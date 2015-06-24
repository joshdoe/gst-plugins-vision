add_definitions(-DHAVE_CONFIG_H)

set(SOURCES
    gstniimaqdx.c)
    
set(HEADERS
    gstniimaqdx.h)

include_directories(AFTER
  .
  ${NIIMAQDX_INCLUDE_DIR})

add_library(libgstniimaqdx MODULE
  ${SOURCES}
  ${HEADERS} )

# National Instruments doesn't provide a .lib for 64-bit, so use our own
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(NIIMAQDX_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/win64/niimaqdx.lib")
endif()

target_link_libraries(libgstniimaqdx
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${NIIMAQDX_LIBRARIES})
  
install (TARGETS libgstniimaqdx
  LIBRARY DESTINATION lib/gstreamer-0.10)
