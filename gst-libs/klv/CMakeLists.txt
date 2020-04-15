add_definitions(-DBUILDING_GST_KLV)

set (SOURCES
  klv.c)
    
set (HEADERS
  klv.h)

include_directories (AFTER
  ${PROJECT_SOURCE_DIR}/common
  )

set (libname gstklv-1.0-0)

add_library (${libname} SHARED
  ${SOURCES}
  ${HEADERS})
  
target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY})

if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif()
install (TARGETS ${libname} LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR})
