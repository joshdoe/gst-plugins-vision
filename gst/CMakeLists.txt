if (OPENCV_FOUND)
  add_subdirectory (sensorfx)
endif (OPENCV_FOUND)

add_subdirectory (bayerutils)
add_subdirectory (extractcolor)

if (ENABLE_KLV)
  add_subdirectory (klv)
endif ()

add_subdirectory (misb)
add_subdirectory (select)
add_subdirectory (videoadjust)
