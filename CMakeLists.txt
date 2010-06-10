cmake_minimum_required (VERSION "2.8.0")

list (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

project ( gst-plugins-vision )

find_package (Gstreamer REQUIRED)
find_package (Glib2 REQUIRED)
find_package (NIIMAQ)
find_package (FreeImage)

# conditional required packages
include (CheckIncludeFiles)
CHECK_INCLUDE_FILES ("${GSTREAMER_INCLUDE_DIR}/gst/gstconfig.h" GST_USING_BINARY_REGISTRY)
if (NOT GST_USING_BINARY_REGISTRY)
    find_package (LIBXML2)
    find_package (LIBICONV)
    include_directories (AFTER
        ${LIBXML2_INCLUDE_DIR}
        ${LIBICONV_INCLUDE_DIR})
endif (NOT GST_USING_BINARY_REGISTRY)


include_directories (
	.
    ${GSTREAMER_INCLUDE_DIR}
    ${GLIB2_INCLUDE_DIR} )

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    file (TO_CMAKE_PATH "${GSTREAMER_INCLUDE_DIR}/../.." PREFIX)
    set (CMAKE_INSTALL_PREFIX
        ${PREFIX} CACHE PATH "Directory to install plugins, under lib/gstreamer-0.10" FORCE)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

add_subdirectory (ext)
add_subdirectory (gst)
add_subdirectory (sys)
