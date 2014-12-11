add_definitions(-DHAVE_CONFIG_H)

set(SOURCES
    gstsaperasrc.cpp)
    
set(HEADERS
    gstsaperasrc.h)

include_directories(AFTER
  ${SAPERA_INCLUDE_DIR}
  .)

add_library(libgstsapera MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries(libgstsapera
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${SAPERA_LIBRARIES})
  
install(TARGETS libgstsapera
  LIBRARY DESTINATION lib/gstreamer-1.0)
