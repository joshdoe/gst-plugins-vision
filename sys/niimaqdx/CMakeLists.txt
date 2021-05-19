set (SOURCES
  gstniimaqdx.c)
    
set (HEADERS
  gstniimaqdx.h)

include_directories (AFTER
  ${NIIMAQDX_INCLUDE_DIR})

set (libname gstniimaqdx)

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
  
if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(
  TARGETS ${libname} 
  LIBRARY DESTINATION lib COMPONENT ${PLUGIN_INSTALL_DIR}
  )
