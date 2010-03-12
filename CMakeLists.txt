cmake_minimum_required ( VERSION "2.6.0" )

list ( APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

project ( gst-plugins-vision )

find_package ( Glib2 REQUIRED )
find_package ( Gstreamer COMPONENTS gstbase )
find_package ( LibXml2 )
find_package ( LibIconv )
find_package ( NIIMAQ REQUIRED ) # make this optional

function ( check_package name LEVEL )
    if ( ${name}_FOUND )
        message ( STATUS "Found ${name}... YES" )
    else ( ${name}_FOUND )
        message ( STATUS "Found ${name}... NO" )
        if ( LEVEL )
            if ( WIN32 )
                message ( FATAL_ERROR "You should set the env variable ${name}_DIR" )
            else ( WIN32 )
                message ( FATAL_ERROR "You should install ${name} dev package" )
            endif ( WIN32 )
        endif ( LEVEL )
    endif ( ${name}_FOUND )
endfunction ( check_package )

# required packages
check_package ( GLIB2 1 )
check_package ( GSTREAMER 1 )

# conditionnal required packages
INCLUDE ( CheckIncludeFiles )
CHECK_INCLUDE_FILES ( "${GSTREAMER_gstconfig_INCLUDE_DIR}/gst/gstconfig.h" GST_USING_BINARY_REGISTRY )
if ( NOT GST_USING_BINARY_REGISTRY )
    check_package ( LIBXML2 1 )
    check_package ( LIBICONV 1 )
endif ( NOT GST_USING_BINARY_REGISTRY )

# debug
message ( "****" ${GLIB2_INCLUDE_DIR} )
message ( "****" ${GLIB2_LIBRARIES} )
message ( "****" ${GSTREAMER_INCLUDE_DIR} )
message ( "****" ${GSTREAMER_LIBRARIES} )
message ( "****" ${NIIMAQ_INCLUDE_DIR} )
message ( "****" ${NIIMAQ_LIBRARIES} )

include_directories (
	.
    ${GSTREAMER_INCLUDE_DIR}
    ${GLIB2_INCLUDE_DIR} )

if ( NOT GST_USING_BINARY_REGISTRY )
    include_directories ( AFTER
        ${LIBXML2_INCLUDE_DIR}
        ${LIBICONV_INCLUDE_DIR} )
endif ( NOT GST_USING_BINARY_REGISTRY )

add_subdirectory ( gst )
add_subdirectory ( sys )
