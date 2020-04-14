set (SOURCES
  gstkayasrc.c
  )
    
set (HEADERS
  gstkayasrc.h)

include_directories (AFTER
  ${GSTREAMER_INCLUDE_DIR}/..
  ${KAYA_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/common
  )

set (libname gstkaya)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${KAYA_LIBRARIES}
  )

if (WIN32)
  set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/lib${libname}.pdb")
  install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0 COMPONENT pdb)
endif ()
install(TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)
