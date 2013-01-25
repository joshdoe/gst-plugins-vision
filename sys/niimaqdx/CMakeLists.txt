add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gstniimaqdx.c )
    
set ( HEADERS
    gstniimaqdx.h )

include_directories ( AFTER
  .
  ${NIIMAQDX_INCLUDE_DIR} )

add_library ( libgstimaqdx MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgstimaqdx
  general ${GLIB2_LIBRARIES}
  general ${GSTREAMER_LIBRARIES}
  general ${GSTREAMER_BASE_LIBRARY}
  general ${GSTREAMER_INTERFACE_LIBRARY}
  general ${NIIMAQDX_LIBRARIES} )
  
install (TARGETS libgstimaqdx
  LIBRARY DESTINATION lib/gstreamer-0.10)
