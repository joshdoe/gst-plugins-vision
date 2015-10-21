add_definitions(-DHAVE_CONFIG_H)

set(SOURCES
  gstedt.c
  gstedtpdvsink.c
  gstedtpdvsrc.c)

set(HEADERS
  gstedtpdvsink.h
  gstedtpdvsrc.h)

include_directories(AFTER
  ${EDT_INCLUDE_DIR})

add_library(libgstedt MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries(libgstedt
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${EDT_LIBRARIES})
  
install(TARGETS libgstedt
  LIBRARY DESTINATION lib/gstreamer-1.0)