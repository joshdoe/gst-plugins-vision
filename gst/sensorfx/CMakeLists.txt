add_definitions (-DHAVE_CONFIG_H)

set (SOURCES
  gstsensorfx.c
  gstsensorfx3dnoise.c)
    
set (HEADERS
    gstsensorfx3dnoise.h)

include_directories (
    .
    C:/gstreamer/include/gstreamer-0.10/gst/opencv
    ${OpenCV_INCLUDE_DIR})

add_library (libgstsensorfx MODULE
  ${SOURCES}
  ${HEADERS})
  
target_link_libraries (libgstsensorfx
  general ${GLIB2_LIBRARIES}
  general ${GSTREAMER_LIBRARIES}
  general ${GSTREAMER_BASE_LIBRARY}
  general ${GSTREAMER_VIDEO_LIBRARY}
  ${OpenCV_LIBS}
  C:/gstreamer/lib/libgstopencv-0.10.lib)
  
install (TARGETS libgstsensorfx
    LIBRARY DESTINATION lib/gstreamer-0.10)