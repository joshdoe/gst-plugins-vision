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
  general ${GLIB2_LIBRARIES}
  general ${GSTREAMER_LIBRARIES}
  general ${GSTREAMER_BASE_LIBRARY}
  general ${GSTREAMER_VIDEO_LIBRARY})
  
install (TARGETS libgstvideoadjust
    LIBRARY DESTINATION lib/gstreamer-0.10)