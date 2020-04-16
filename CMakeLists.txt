cmake_minimum_required(VERSION "2.8.0")

project(gst-plugins-vision)

option(ENABLE_KLV "Whether to enable KLV support" OFF)

set(CMAKE_SHARED_MODULE_PREFIX "lib")
set(CMAKE_SHARED_LIBRARY_PREFIX "lib")

# add local cmake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# get git version info
include(GetGitRevisionDescription)
git_describe(GIT_DESCRIBE --dirty)

# values used when registering plugins
add_definitions(-DGST_PACKAGE_VERSION="${GIT_DESCRIBE}")
add_definitions(-DGST_PACKAGE_LICENSE="LGPL")
add_definitions(-DGST_PACKAGE_ORIGIN="Unknown package origin")
add_definitions(-DGST_PACKAGE_NAME="${CMAKE_PROJECT_NAME}")
add_definitions(-DPACKAGE="${CMAKE_PROJECT_NAME} package")

# configure CPack
set(CPACK_GENERATOR "ZIP")
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING IGNORE)
if (WIN32)
    set(CPACK_COMPONENTS_ALL pdb Unspecified)
endif (WIN32)
set(CPACK_PACKAGE_VERSION ${GIT_DESCRIBE})
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-x86_64")
else ()
  set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-x86")
endif ()
set(CPACK_ARCHIVE_PDB_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-pdbs")
set(CPACK_ARCHIVE_UNSPECIFIED_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}")
include(CPack)

include(MacroLogFeature)

find_package(GStreamer REQUIRED COMPONENTS base)
macro_log_feature(GSTREAMER_FOUND "GStreamer" "Required to build gst-plugins-vision" "http://gstreamer.freedesktop.org/" TRUE "1.2.0")
macro_log_feature(GSTREAMER_BASE_LIBRARY_FOUND "GStreamer base library" "Required to build most plugins" "http://gstreamer.freedesktop.org/" FALSE "1.2.0")

find_package(GStreamerPluginsBase COMPONENTS video)
macro_log_feature(GSTREAMER_VIDEO_LIBRARY_FOUND "GStreamer video library" "Required to build several video plugins" "http://gstreamer.freedesktop.org/" FALSE "1.2.0")

find_package(GLIB2 REQUIRED)
macro_log_feature(GLIB2_FOUND "GLib" "Required to build gst-plugins-vision" "http://www.gtk.org/" TRUE)

find_package(GObject REQUIRED)
macro_log_feature(GOBJECT_FOUND "GObject" "Required to build gst-plugins-vision" "http://www.gtk.org/" TRUE)

find_package(Orc REQUIRED)
macro_log_feature(ORC_FOUND "Orc" "Required library to improve performance" "http://code.entropywave.com/orc/" TRUE)

find_package(Bitflow)
macro_log_feature(BITFLOW_FOUND "BitFlow" "Required to build BitFlow source element" "http://www.bitflow.com/" FALSE)

find_package(NIIMAQ)
macro_log_feature(NIIMAQ_FOUND "NI-IMAQ" "Required to build National Instruments IMAQ source element" "http://www.ni.com/" FALSE)

find_package(NIIMAQdx)
macro_log_feature(NIIMAQDX_FOUND "NI-IMAQdx" "Required to build National Instruments IMAQdx source element" "http://www.ni.com/" FALSE)

find_package(FreeImage)
macro_log_feature(FREEIMAGE_FOUND "FreeImage" "Required to build FreeImage plugin" "http://freeimage.sourceforge.net/" FALSE)

find_package(OpenCV)
macro_log_feature(OPENCV_FOUND "OpenCV" "Required to build sensorfx plugin" "http://opencv.willowgarage.com/" FALSE)

find_package(Aptina)
macro_log_feature(APTINA_FOUND "Aptina" "Required to build aptinasrc source element" "http://www.onsemi.com/" FALSE)

find_package(EDT)
macro_log_feature(EDT_FOUND "EDT" "Required to build EDT PDV source element" "http://www.edt.com/" FALSE)

find_package(Euresys)
macro_log_feature(EURESYS_FOUND "Euresys" "Required to build Euresys source element" "http://www.euresys.com/" FALSE)

find_package(GigESim)
macro_log_feature(GIGESIM_FOUND "GigESim" "Required to build A&B Soft GigESim sink element" "http://www.ab-soft.com/" FALSE)

find_package(IDSuEye)
macro_log_feature(IDSUEYE_FOUND "IDS uEye" "Required to build IDS uEye source element" "http://en.ids-imaging.com/" FALSE)

find_package(Imperx)
macro_log_feature(IMPERX_FLEX_FOUND "Imperx FrameLink Express" "Required to build Imperx FrameLink Express source element" "http://www.imperx.com/" FALSE)

find_package(ImperxSDI)
macro_log_feature(IMPERX_SDI_FOUND "Imperx HD-SDI Express" "Required to build Imperx HD-SDI Express source element" "http://www.imperx.com/" FALSE)

find_package(IOtechDaqX)
macro_log_feature(IOTECHDAQX_FOUND "IOtech DaqX" "Required to build IOtech DaqX source element" "http://www.mccdaq.com/" FALSE)

find_package(KAYA)
macro_log_feature(KAYA_FOUND "KAYA Vision Point" "Required to build KAYA source element" "http://www.kayainstruments.com/" FALSE)

find_package(Matrox)
macro_log_feature(MATROX_FOUND "Matrox MIL" "Required to build Matrox MIL source element" "http://www.matrox.com/imaging/" FALSE)

find_package(Phoenix)
macro_log_feature(PHOENIX_FOUND "Active Silicon Phoenix" "Required to build Active Silicon Phoenix source element" "http://www.activesilicon.com/" FALSE)

find_package(Pleora)
macro_log_feature(Pleora_FOUND "Pleora eBUS" "Required to build Pleora eBUS source element" "http://www.pleora.com/" FALSE)

find_package(Pylon)
macro_log_feature(PYLON_FOUND "Basler Pylon" "Required to build Basler Pylon source element" "http://www.baslerweb.com/" FALSE)

find_package(Sapera)
macro_log_feature(SAPERA_FOUND "Teledyne DALSA Sapera" "Required to build Teledyne DALSA Sapera source element" "http://www.teledynedalsa.com/" FALSE)

find_package(XCLIB)
macro_log_feature(XCLIB_FOUND "EPIX PIXCI" "Required to build EPIX PIXCI source element" "http://www.epixinc.com/" FALSE)


# Setup common environment
include_directories(
    .
    ${GSTREAMER_INCLUDE_DIR}
    ${GLIB2_INCLUDE_DIR})

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if (WIN32)
    get_filename_component(_PREFIX "${GSTREAMER_PLUGIN_DIR}/../" REALPATH)
  else ()
    set(_PREFIX "/usr/lib")
  endif ()
  set(CMAKE_INSTALL_PREFIX "${_PREFIX}"
      CACHE PATH "Common prefix for all installed files (e.g., plugins will be installed under CMAKE_INSTALL_PREFIX/gstreamer-1.0)" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

if (NOT LIBRARY_INSTALL_DIR)
  set(LIBRARY_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/"
      CACHE PATH "Location to install libraries (e.g., libgstklv-1.0-0.so/dll)")
endif ()
if (NOT PLUGIN_INSTALL_DIR)
  set(PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/gstreamer-1.0/"
      CACHE PATH "Location to install plugins (e.g., libgstpylon.so/dll)")
endif()
if (WIN32 AND NOT PDB_INSTALL_DIR)
  set(PDB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/gstreamer-1.0/"
      CACHE PATH "Location to install PDB debug files (e.g., libgstpylon.pdb)")
endif()

add_subdirectory(gst-libs)

# Build the plugins
add_subdirectory(ext)
add_subdirectory(gst)
add_subdirectory(sys)

macro_display_feature_log()
