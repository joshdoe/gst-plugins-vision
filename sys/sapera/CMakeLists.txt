add_definitions (-DHAVE_CONFIG_H)

set (SOURCES
  gstsaperasrc.cpp)
    
set (HEADERS
  gstsaperasrc.h)

include_directories (AFTER
  ${SAPERA_INCLUDE_DIR})

set (libname libgstsapera)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${SAPERA_LIBRARIES})

set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${libname}.pdb")
install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0)
install(TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)
