#ifndef KARAOKELYRICSCDG_H
#define KARAOKELYRICSCDG_H

/*
 *      Copyright (C) 2005-2010 Team XBMC
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

// C++ Interface: karaokelyricscdg

#include "cdgdata.h"
#include "karaokelyrics.h"

class CBaseTexture;
typedef uint32_t color_t;

class CKaraokeLyricsCDG : public CKaraokeLyrics
{
  public:
	CKaraokeLyricsCDG( const CStdString& cdgFile );
    ~CKaraokeLyricsCDG();

    //! Parses the lyrics or song file, and loads the lyrics into memory. Returns true if the
    //! lyrics are successfully loaded, false otherwise.
    bool Load();

    //! Virtually all CDG lyrics have some kind of background
    virtual bool HasBackground();

    //! Should return true if the lyrics have video file to play
    virtual bool HasVideo();

    //! Should return video parameters if HasVideo() returned true
    virtual void GetVideoParameters( CStdString& path, __int64& offset  );

    //! This function is called when the karoke visualisation window created. It may
    //! be called after Start(), but is guaranteed to be called before Render()
    //! Default implementation does nothing.
    virtual bool InitGraphics();

    //! This function is called when the karoke visualisation window is destroyed.
    //! Default implementation does nothing.
    virtual void Shutdown();

    //! This function is called to render the lyrics (each frame(?))
    virtual void Render();

  protected:
	void cmdMemoryPreset( const char * data );
	void cmdBorderPreset( const char * data );
	void cmdLoadColorTable( const char * data, int index );
	void cmdTileBlockXor( const char * data );
	bool UpdateBuffer( unsigned int packets_due );
	void RenderIntoBuffer( unsigned char *pixels, unsigned int width, unsigned int height, unsigned int pitch ) const;

  private:
	BYTE getPixel( int x, int y );
	void setPixel( int x, int y, BYTE color );

    //! CDG file name
	CStdString         m_cdgFile;

	typedef struct
	{
		unsigned int	packetnum;
		SubCode			subcode;
	} CDGPacket;

	std::vector<CDGPacket>  m_cdgStream;  //!< Parsed CD+G stream storage
	int                m_streamIdx;       //!< Packet about to be rendered
	DWORD              m_colorTable[16];  //!< CD+G color table; color format is A8R8G8B8
	BYTE			   m_bgColor;         //!< Background color index
	BYTE			   m_cdgScreen[CDG_FULL_WIDTH*CDG_FULL_HEIGHT];	//!< Image state for CD+G stream

    //! Rendering stuff
	CBaseTexture *     m_pCdgTexture;
	color_t            m_bgAlpha;  //!< background alpha
	color_t            m_fgAlpha;  //!< foreground alpha
};

#endif
