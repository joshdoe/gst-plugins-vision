add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gsteuresys.c )
    
set ( HEADERS
    gsteuresys.h )

include_directories ( AFTER
  .
  ${EURESYS_INCLUDE_DIR} )

add_library ( libgsteuresys MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgsteuresys
  ${GLIB2_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_INTERFACE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY})
  
install (TARGETS libgsteuresys
  LIBRARY DESTINATION lib/gstreamer-0.10)