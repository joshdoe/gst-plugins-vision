add_definitions(-DHAVE_CONFIG_H)

set(SOURCES
    gstpixcisrc.c)
    
set(HEADERS
    gstpixcisrc.h)

include_directories(AFTER
  ${XCLIB_INCLUDE_DIR}
  .)

add_library(libgstpixci MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries(libgstpixci
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${XCLIB_LIBRARIES})
  
install(TARGETS libgstpixci
  LIBRARY DESTINATION lib/gstreamer-0.10)
