add_definitions (-DHAVE_CONFIG_H)

set (SOURCES
  gstniimaqdx.c)
    
set (HEADERS
  gstniimaqdx.h)

include_directories (AFTER
  ${NIIMAQDX_INCLUDE_DIR})

set (libname libgstniimaqdx)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

# National Instruments doesn't provide a .lib for 64-bit, so use our own
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set (NIIMAQDX_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/win64/niimaqdx.lib")
endif ()

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${NIIMAQDX_LIBRARIES})
  
set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${libname}.pdb")
install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0 COMPONENT pdb)
install(TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)