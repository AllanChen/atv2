#!/bin/sh

if [ ! -d XBMC.app ]; then
  echo "XBMC.app not found! copy it from build dir to here -> `pwd`"
  exit 1
fi
if [ -f "/usr/bin/sudo" ]; then
  SUDO="/usr/bin/sudo"
fi

PACKAGE=org.xbmc.xbmc-ios

VERSION=10.0
REVISION=0
ARCHIVE=${PACKAGE}_${VERSION}-${REVISION}_iphoneos-arm.deb

echo Creating $PACKAGE package version $VERSION revision $REVISION
${SUDO} rm -rf $PACKAGE
${SUDO} rm -rf $ARCHIVE

# create debian control file.
mkdir -p $PACKAGE/DEBIAN
echo "Package: $PACKAGE"                          >  $PACKAGE/DEBIAN/control
echo "Priority: Extra"                            >> $PACKAGE/DEBIAN/control
echo "Name: XBMC-iOS"                             >> $PACKAGE/DEBIAN/control
echo "Version: $VERSION-$REVISION"                >> $PACKAGE/DEBIAN/control
echo "Architecture: iphoneos-arm"                 >> $PACKAGE/DEBIAN/control
echo "Description: XBMC Multimedia center for 4.x iOS" >> $PACKAGE/DEBIAN/control
echo "Homepage: http://xbmc.org/"                 >> $PACKAGE/DEBIAN/control
echo "Maintainer: Scott Davilla, Edgar Hucek"     >> $PACKAGE/DEBIAN/control
echo "Author: TeamXBMC"                           >> $PACKAGE/DEBIAN/control
echo "Section: Multimedia"                        >> $PACKAGE/DEBIAN/control
echo "Icon: file:///Applications/XBMC.app/cydia.png" >> $PACKAGE/DEBIAN/control

# prerm: called on remove and upgrade - get rid of existing bits.
echo "#!/bin/sh"                                  >  $PACKAGE/DEBIAN/prerm
echo "rm -rf /Applications/XBMC.app"               >> $PACKAGE/DEBIAN/prerm
chmod +x $PACKAGE/DEBIAN/prerm

# postinst: nothing for now.
echo "#!/bin/sh"                                  >  $PACKAGE/DEBIAN/postinst
chmod +x $PACKAGE/DEBIAN/postinst

# prep XBMC.app
mkdir -p $PACKAGE/Applications
cp -r XBMC.app $PACKAGE/Applications/
find $PACKAGE/Applications/ -name '.svn' -exec rm -rf {} \;
find $PACKAGE/Applications/ -name '.gitignore' -exec rm -rf {} \;

# set ownership to root:root
${SUDO} chown -R 0:0 $PACKAGE

echo Packaging $PACKAGE
# Tell tar, pax, etc. on Mac OS X 10.4+ not to archive
# extended attributes (e.g. resource forks) to ._* archive members.
# Also allows archiving and extracting actual ._* files.
export COPYFILE_DISABLE=true
export COPY_EXTENDED_ATTRIBUTES_DISABLE=true
#
../../ios-depends/build/bin/dpkg-deb -b $PACKAGE $ARCHIVE
../../ios-depends/build/bin/dpkg-deb --info $ARCHIVE
../../ios-depends/build/bin/dpkg-deb --contents $ARCHIVE

# clean up by removing package dir
${SUDO} rm -rf $PACKAGE
