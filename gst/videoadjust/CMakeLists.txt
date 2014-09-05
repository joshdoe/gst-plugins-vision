add_definitions (-DHAVE_CONFIG_H)

set (SOURCES
	gstvideoadjust.c
    gstvideolevels.c)
    
set (HEADERS
    gstvideolevels.h)
    
include_directories(AFTER
  . )

add_library (libgstvideoadjust MODULE
  ${SOURCES}
  ${HEADERS})
  
target_link_libraries (libgstvideoadjust
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY})
  
install (TARGETS libgstvideoadjust
    LIBRARY DESTINATION lib/gstreamer-0.10)
