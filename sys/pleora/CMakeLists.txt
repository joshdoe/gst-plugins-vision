add_definitions(-D_XKEYCHECK_H)

set (SOURCES
  gstpleora.cpp
  gstpleorasrc.cpp)
    
set (HEADERS
  gstpleorasrc.h)

if (Pleora_VERSION_MAJOR GREATER 5)
  set (SOURCES
    ${SOURCES}
    gstpleorasink.cpp
    streamingchannelsource.cpp)
  set (HEADERS
    ${HEADERS}
    gstpleorasink.h
    streamingchannelsource.h)
endif ()

include_directories (AFTER
  ${Pleora_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/common
  ${PROJECT_SOURCE_DIR}/gst-libs/klv
  )

link_directories(${Pleora_LIBRARY_DIR})

set (libname gstpleora${Pleora_VERSION_MAJOR})

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  gstklv-1.0-0)

if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(TARGETS ${libname} LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR})
