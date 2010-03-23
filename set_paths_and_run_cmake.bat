rem ########################################################################
set GSTREAMER_DIR=C:\gstreamer
set LIBXML2_DIR=%GSTREAMER_DIR%
set LIBICONV_DIR=%GSTREAMER_DIR%
set GLIB2_DIR=%GSTREAMER_DIR%
set NIIMAQ_DIR=C:\Program Files\National Instruments

rem cd mingw32
rem del *ache* && cmake -G "MinGW Makefiles" ..

rem cd vs8
rem del *ache* && cmake -G "Visual Studio 8 2005" ..

cd vs9
del *ache* && cmake -G "Visual Studio 9 2008" ..

rem cd codeblocks
rem del *ache* && cmake -G "CodeBlocks - MinGW Makefiles" ..

cmd
rem ########################################################################
