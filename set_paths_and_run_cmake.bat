rem ########################################################################
set GSTREAMER_DIR=C:\gstreamer
set LIBXML2_DIR=%GSTREAMER_DIR%
set LIBICONV_DIR=%GSTREAMER_DIR%
set GLIB2_DIR=%GSTREAMER_DIR%
set NIIMAQ_DIR=C:\Program Files\National Instruments
set EURESYS_DIR=C:\Program Files\Euresys\MultiCam
set OpenCV_DIR=C:\Users\joshua.doe\Apps\opencv

rem Plugins will be installed under %CMAKE_PREFIX_PATH%\lib\gstreamer-0.10
set CMAKE_PREFIX_PATH=C:\gstreamer

rem cd mingw32
rem del *ache* && cmake -G "MinGW Makefiles" ..

rem cd vs9
rem del *ache* && cmake -G "Visual Studio 9 2008" ..

rem cmd
rem ########################################################################

cmake-gui