#ifdef __APPLE__
/*
 *      Copyright (C) 2005-2009 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "IOSAudioRenderer.h"
#include "AudioContext.h"
#include "GUISettings.h"
#include "Settings.h"
#include "utils/Atomics.h"
#include "utils/log.h"
#include "utils/TimeUtils.h"

//***********************************************************************************************
// Contruction/Destruction
//***********************************************************************************************
CIOSAudioRenderer::CIOSAudioRenderer() :
  m_Pause(false),
  m_Initialized(false),
  m_CurrentVolume(0),
  m_OutputBufferIndex(0),
  m_EnableVolumeControl(true),
  m_AvgBytesPerSec(0),
  m_BufferLen(0),
  m_NumChunks(0),
  m_ChunkSize(0),
  m_packetSize(0)
{
  /*
  CFRunLoopRef theRunLoop = NULL;
  AudioObjectPropertyAddress theAddress = { kAudioHardwarePropertyRunLoop, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
  OSStatus theError = AudioObjectSetPropertyData(kAudioObjectSystemObject, &theAddress, 0, NULL, sizeof(CFRunLoopRef), &theRunLoop);
  if (theError != noErr)
  {
    CLog::Log(LOGERROR, "CIOSAudioRenderer::constructor: kAudioHardwarePropertyRunLoop error.");
  }
  */
}

CIOSAudioRenderer::~CIOSAudioRenderer()
{
  Deinitialize();
}

//***********************************************************************************************
// Initialization
//***********************************************************************************************

bool CIOSAudioRenderer::Initialize(IAudioCallback* pCallback, const CStdString& device, int iChannels, enum PCMChannels *channelMap, unsigned int uiSamplesPerSec, unsigned int uiBitsPerSample, bool bResample, bool bIsMusic /*Useless Legacy Parameter*/, bool bPassthrough)
{
  if (m_Initialized) // Have to clean house before we start again. TODO: Should we return failure instead?
    Deinitialize();

  g_audioContext.SetActiveDevice(CAudioContext::DIRECTSOUND_DEVICE);

  // TODO: If debugging, output information about all devices/streams

  AudioDeviceID outputDevice = CIOSCoreAudioHardware::GetDefaultOutputDevice();
  if (!outputDevice) // Not a lot to be done with no device. TODO: Should we just grab the first existing device?
    return false;

  // TODO: Determine if the device is in-use/locked by another process.

  // Attach our output object to the device
  m_AudioDevice.Open(outputDevice);

  // Create the Output AudioUnit Component
  if (!m_AUOutput.Open(kAudioUnitType_Output, kAudioUnitSubType_RemoteIO, kAudioUnitManufacturer_Apple))
    return false;
  
  CLog::Log(LOGDEBUG, "CIOSAudioRenderer::InitializePCM:    using supplied channel map for audio source");

  // Set the input stream format for the AudioUnit (this is what is being sent to us)
  AudioStreamBasicDescription inputFormat;
  inputFormat.mFormatID = kAudioFormatLinearPCM;			      //	Data encoding format
  inputFormat.mFormatFlags = kAudioFormatFlagsNativeEndian
                           | kLinearPCMFormatFlagIsPacked   // Samples occupy all bits (not left or right aligned)
                           | kAudioFormatFlagIsSignedInteger;
  inputFormat.mChannelsPerFrame = iChannels;                 // Number of interleaved audiochannels
  inputFormat.mSampleRate = (Float64)uiSamplesPerSec;      //	the sample rate of the audio stream
  inputFormat.mBitsPerChannel = uiBitsPerSample;              // Number of bits per sample, per channel
  inputFormat.mBytesPerFrame = (uiBitsPerSample>>3) * iChannels; // Size of a frame == 1 sample per channel		
  inputFormat.mFramesPerPacket = 1;                         // The smallest amount of indivisible data. Always 1 for uncompressed audio 	
  inputFormat.mBytesPerPacket = inputFormat.mBytesPerFrame * inputFormat.mFramesPerPacket;
  inputFormat.mReserved = 0;
  if (!m_AUOutput.SetInputFormat(&inputFormat))
    return false;
  
  m_AvgBytesPerSec = inputFormat.mSampleRate * inputFormat.mBytesPerFrame;      // 1 sample per channel per frame
  m_EnableVolumeControl = true;

  // Configure the maximum number of frames that the AudioUnit will ask to process at one time.
  // If this is not called, there is no guarantee that the callback will ever be called.
  //UInt32 bufferFrames = m_AUOutput.GetBufferFrameSize(); // Size of the output buffer, in Frames
    
  //if (!m_AUOutput.SetMaxFramesPerSlice(bufferFrames))
  //  return false;
    
  UInt32 bufferFrames = 4096;

  m_ChunkSize = bufferFrames;
  m_NumChunks = (m_AvgBytesPerSec + m_ChunkSize - 1) / m_ChunkSize;
  m_BufferLen = m_NumChunks * m_ChunkSize;
  m_Buffer = av_fifo_alloc(m_BufferLen);
  m_packetSize = inputFormat.mFramesPerPacket*inputFormat.mChannelsPerFrame*(inputFormat.mBitsPerChannel/8); 
    
  // Setup the callback function that the AudioUnit will use to request data	
  if (!m_AUOutput.SetRenderProc(RenderCallback, this))
    return false;

  // Initialize the Output AudioUnit
  if (!m_AUOutput.Initialize())
    return false;

  // Log some information about the stream
  AudioStreamBasicDescription inputDesc_end, outputDesc_end;
  CStdString formatString;
  m_AUOutput.GetInputFormat(&inputDesc_end);
  m_AUOutput.GetOutputFormat(&outputDesc_end);
  CLog::Log(LOGDEBUG, "CIOSAudioRenderer::Initialize: Input Stream Format %s", (char*)IOSStreamDescriptionToString(inputDesc_end, formatString));
  CLog::Log(LOGDEBUG, "CIOSAudioRenderer::Initialize: Output Stream Format %s", (char*)IOSStreamDescriptionToString(outputDesc_end, formatString));

  m_Pause = true;                       // Suspend rendering. We will start once we have some data.
  m_Initialized = true;

  SetCurrentVolume(g_settings.m_nVolumeLevel);

  CLog::Log(LOGDEBUG, "CIOSAudioRenderer::Initialize: Renderer Configuration - Chunk Len: %u, Max Cache: %u (%0.0fms).", m_ChunkSize, m_BufferLen, 1000.0 *(float)m_BufferLen/(float)m_AvgBytesPerSec);
  CLog::Log(LOGINFO, "CIOSAudioRenderer::Initialize: Successfully configured audio output.");

  // Make space for remap processing
  // AddPackets will not accept more data than m_MaxCacheLen, so a fixed size buffer should be okay.
  // Do we need to catch memory allocation errors?
  CLog::Log(LOGDEBUG, "CIOSAudioRenderer::Initialize: Allocated %u bytes for channel remapping",m_BufferLen);

  return true;
}

bool CIOSAudioRenderer::Deinitialize()
{
  // Stop rendering
  Stop();
  // Reset our state
  CLog::Log(LOGDEBUG, "CIOSAudioRenderer::Deinitialize: deleted remapping buffer");
  
  m_AUOutput.Close();
  Sleep(10);
  m_AudioDevice.Close();
  m_Initialized = false;
  m_EnableVolumeControl = true;
  m_AvgBytesPerSec = 0;
  m_BufferLen = 0;
  m_NumChunks = 0;
  m_ChunkSize = 0;
  av_fifo_free(m_Buffer);
  m_Buffer = NULL;

  g_audioContext.SetActiveDevice(CAudioContext::DEFAULT_DEVICE);

  CLog::Log(LOGINFO, "CIOSAudioRenderer::Deinitialize: Renderer has been shut down.");

  return true;
}

//***********************************************************************************************
// Transport control methods
//***********************************************************************************************
bool CIOSAudioRenderer::Pause()
{
  if (!m_Pause)
  {
    m_AUOutput.Stop();
    m_Pause = true;
  }
  return true;
}

bool CIOSAudioRenderer::Resume()
{
  if (m_Pause)
  {
    m_AUOutput.Start();
    m_Pause = false;
  }
  return true;
}

bool CIOSAudioRenderer::Stop()
{
  m_AUOutput.Stop();

  m_Pause = true;
  av_fifo_reset(m_Buffer);

  return true;
}

//***********************************************************************************************
// Volume control methods
//***********************************************************************************************
LONG CIOSAudioRenderer::GetCurrentVolume() const
{
  return m_CurrentVolume;
}

void CIOSAudioRenderer::Mute(bool bMute)
{
  if (bMute)
    SetCurrentVolume(0);
  else
    SetCurrentVolume(m_CurrentVolume);
}

bool CIOSAudioRenderer::SetCurrentVolume(LONG nVolume)
{
  if (m_EnableVolumeControl) // Don't change actual volume for encoded streams
  {
    // Convert milliBels to percent
    Float32 volPct = pow(10.0f, (float)nVolume/2000.0f);

    // Try to set the volume. If it fails there is not a lot to be done.
    if (!m_AUOutput.SetCurrentVolume(volPct))
      return false;
  }
  m_CurrentVolume = nVolume; // Store the volume setpoint. We need this to check for 'mute'
  return true;
}

//***********************************************************************************************
// Data management methods
//***********************************************************************************************
unsigned int CIOSAudioRenderer::GetSpace()
{
  return m_BufferLen - av_fifo_size(m_Buffer);
}

unsigned int CIOSAudioRenderer::AddPackets(const void* data, DWORD len)
{

  int free = m_BufferLen - av_fifo_size(m_Buffer);

  // Require at least one 'chunk'. This allows us at least some measure of control over efficiency
  // TODO 
  /*
  if (len < m_ChunkSize || len > free)
    return 0;
  */
  
  //fprintf(stderr, "CIOSAudioRenderer::AddPackets len %d free %d\n", len, free); 
  if (len > free) len = free;

  av_fifo_generic_write(m_Buffer, (unsigned char *)data, len, NULL);
  
  Resume();
  
  return len;
}

float CIOSAudioRenderer::GetDelay()
{
  
  //fprintf(stderr, "CIOSAudioRenderer::GetDelay %f\n", (float)av_fifo_size(m_Buffer)/(float)m_AvgBytesPerSec); 

  //return 0;
  return (float)av_fifo_size(m_Buffer)/(float)m_AvgBytesPerSec;
}

float CIOSAudioRenderer::GetCacheTime()
{
  //fprintf(stderr, "CIOSAudioRenderer::GetCacheTime %f\n", (float)av_fifo_size(m_Buffer)/(float)m_AvgBytesPerSec); 
  return GetDelay();
}

float CIOSAudioRenderer::GetCacheTotal()
{
  //fprintf(stderr, "CIOSAudioRenderer::GetCacheTotal %f\n", (float)m_BufferLen / m_AvgBytesPerSec); 
  return (float)m_BufferLen / m_AvgBytesPerSec;
}

unsigned int CIOSAudioRenderer::GetChunkLen()
{
  //fprintf(stderr, "CIOSAudioRenderer::GetChunkLen %u\n", m_ChunkSize); 
  return m_ChunkSize;
}

void CIOSAudioRenderer::WaitCompletion()
{
  //fprintf(stderr, "CIOSAudioRenderer::WaitCompletion %u\n", av_fifo_size(m_Buffer)); 

  if (av_fifo_size(m_Buffer) == 0) // The cache is already empty. There is nothing to wait for.
    return;

  long long timeleft=(1000000LL*av_fifo_size(m_Buffer))/m_AvgBytesPerSec;
  usleep((int)timeleft);

  Stop();
}

//***********************************************************************************************
// Rendering Methods
//***********************************************************************************************
OSStatus CIOSAudioRenderer::OnRender(AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
  if (!m_Initialized)
    CLog::Log(LOGERROR, "CIOSAudioRenderer::OnRender: Callback to de/unitialized renderer.");

  int amt=av_fifo_size(m_Buffer);
  int req=(inNumberFrames)*m_packetSize;

  //fprintf(stderr, "CIOSAudioRenderer::OnRender req %d amt %d m_packetSize %d\n", req, amt, m_packetSize); 
  
  if(amt>req)
    amt=req;

  if(amt) {
    int buffered = av_fifo_size(m_Buffer);
    int len = amt;
    
    if (len > buffered) len = buffered;
    av_fifo_generic_read(m_Buffer, (unsigned char *)ioData->mBuffers[m_OutputBufferIndex].mData, len, NULL);
  } else {
    //fprintf(stderr, "CIOSAudioRenderer::OnRender Pause\n"); 
    Pause();
  }

  if (!m_EnableVolumeControl && m_CurrentVolume <= VOLUME_MINIMUM)
    ioData->mBuffers[m_OutputBufferIndex].mDataByteSize = 0;
  else
    ioData->mBuffers[m_OutputBufferIndex].mDataByteSize = amt;

  return noErr;
}

// Static Callback from AudioUnit
OSStatus CIOSAudioRenderer::RenderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
  return ((CIOSAudioRenderer*)inRefCon)->OnRender(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

#endif


