add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gstpixcisrc.c )
    
set ( HEADERS
    gstpixcisrc.h )

include_directories ( AFTER
  ${PIXCI_INCLUDE_DIR}
  .)

add_library ( libgstpixci MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgstpixci
  ${GLIB2_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_INTERFACE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${PIXCI_LIBRARIES})
  
install (TARGETS libgstpixci
  LIBRARY DESTINATION lib/gstreamer-0.10)
