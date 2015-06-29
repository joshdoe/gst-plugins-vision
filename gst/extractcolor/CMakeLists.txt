add_definitions (-DHAVE_CONFIG_H)

set (SOURCES
  gstextractcolor.c
  gstextractcolororc-dist.c)
    
set (HEADERS
  gstextractcolor.h)
    
include_directories (AFTER
  ${GSTREAMER_BASE_INCLUDE_DIR}
  ${GSTREAMER_VIDEO_INCLUDE_DIR}
  ${ORC_INCLUDE_DIR})

add_library (libgstextractcolor MODULE
  ${SOURCES}
  ${HEADERS})
  
target_link_libraries (libgstextractcolor
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${ORC_LIBRARIES})
  
install (TARGETS libgstextractcolor
  LIBRARY DESTINATION lib/gstreamer-1.0)
