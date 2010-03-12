add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gstniimaq.c )
    
set ( HEADERS
    gstniimaq.h )

include_directories ( AFTER
  .
  ${NIIMAQ_INCLUDE_DIR} )

add_library ( libgstimaq MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgstimaq
  general ${GLIB2_LIBRARIES}
  general ${GSTREAMER_LIBRARIES}
  general ${NIIMAQ_LIBRARY} )