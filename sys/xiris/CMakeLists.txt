set (SOURCES
  gstxirissrc.cpp
  )
    
set (HEADERS
  gstxirissrc.h)

include_directories (AFTER
  ${GSTREAMER_INCLUDE_DIR}/..
  ${XIRIS_INCLUDE_DIR}
  )

set (libname gstxiris)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${XIRIS_LIBRARIES}
  stdc++fs
  )

target_compile_options(${libname} PRIVATE -Wall)

install(TARGETS ${libname} LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR})
