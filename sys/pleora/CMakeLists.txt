if (ENABLE_KLV)
  add_definitions(-DGST_PLUGINS_VISION_ENABLE_KLV)
endif ()

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

set (LIBRARIES
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  )

if (ENABLE_KLV)
  set (LIBRARIES ${LIBRARIES} gstklv-1.0-0)
endif ()

target_link_libraries (${libname}
  ${LIBRARIES}
  )

if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(
  TARGETS ${libname} 
  LIBRARY DESTINATION lib COMPONENT ${PLUGIN_INSTALL_DIR}
  )
