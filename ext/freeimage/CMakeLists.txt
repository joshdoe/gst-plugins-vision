add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
  gstfreeimage.c
  gstfreeimagedec.c
  gstfreeimageenc.c
  gstfreeimageutils.c)
    
set ( HEADERS
  gstfreeimage.h
  gstfreeimagedec.h
  gstfreeimageenc.h
  gstfreeimageutils.h)

include_directories ( AFTER
  ${FREEIMAGE_INCLUDE_DIR} )

add_library ( libgstfreeimage MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgstfreeimage
  general ${GLIB2_LIBRARIES}
  general ${GSTREAMER_LIBRARIES}
  general ${GSTREAMER_BASE_LIBRARY}
  general ${GSTREAMER_INTERFACE_LIBRARY}
  general ${GSTREAMER_VIDEO_LIBRARY}
  general ${FREEIMAGE_LIBRARIES} )
  
install (TARGETS libgstfreeimage
  LIBRARY DESTINATION lib/gstreamer-0.10)