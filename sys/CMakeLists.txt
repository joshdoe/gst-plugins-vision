if (BITFLOW_FOUND)
	add_subdirectory(bitflow)
endif (BITFLOW_FOUND)

if (EDT_FOUND)
	add_subdirectory(edt)
endif (EDT_FOUND)

if (EURESYS_FOUND)
	add_subdirectory (euresys)
endif (EURESYS_FOUND)

if (NIIMAQ_FOUND)
	add_subdirectory (niimaq)
endif (NIIMAQ_FOUND)

if (NIIMAQDX_FOUND)
	add_subdirectory (niimaqdx)
endif (NIIMAQDX_FOUND)

if (IMPERX_FLEX_FOUND)
    add_subdirectory (imperx)
endif ()

if (IOTECHDAQX_FOUND)
	add_subdirectory (iotechdaqx)
endif (IOTECHDAQX_FOUND)

if (PHOENIX_FOUND)
	add_subdirectory(phoenix)
endif (PHOENIX_FOUND)

if (SAPERA_FOUND)
	add_subdirectory(sapera)
endif (SAPERA_FOUND)

if (XCLIB_FOUND)
	add_subdirectory(pixci)
endif (XCLIB_FOUND)
