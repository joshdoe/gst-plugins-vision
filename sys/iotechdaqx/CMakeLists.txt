add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gstiotechdaqx.c )
    
set ( HEADERS
    gstiotechdaqx.h )

include_directories ( AFTER
  .
  ${WAVEBOOK_INCLUDE_DIR} )

add_library ( libgstiotechdaqx MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgstiotechdaqx
  ${GLIB2_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_INTERFACE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${IOTECHDAQX_LIBRARIES})
  
install (TARGETS libgstiotechdaqx
  LIBRARY DESTINATION lib/gstreamer-0.10)