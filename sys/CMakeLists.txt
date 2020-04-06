if (APTINA_FOUND)
	add_subdirectory(aptina)
endif (APTINA_FOUND)

if (BITFLOW_FOUND)
	add_subdirectory(bitflow)
endif (BITFLOW_FOUND)

if (EDT_FOUND)
	add_subdirectory(edt)
endif (EDT_FOUND)

if (EURESYS_FOUND)
	add_subdirectory (euresys)
endif (EURESYS_FOUND)

#if (GENICAM_FOUND)
	add_subdirectory (genicam)
#endif (GENICAM_FOUND)

if (IDSUEYE_FOUND)
	add_subdirectory (idsueye)
endif (IDSUEYE_FOUND)

if (KAYA_FOUND)
	add_subdirectory (kaya)
endif (KAYA_FOUND)

if (MATROX_FOUND)
	add_subdirectory (matrox)
endif (MATROX_FOUND)

if (NIIMAQ_FOUND)
	add_subdirectory (niimaq)
endif (NIIMAQ_FOUND)

if (NIIMAQDX_FOUND)
	add_subdirectory (niimaqdx)
endif (NIIMAQDX_FOUND)

if (IMPERX_FLEX_FOUND)
    add_subdirectory (imperxflex)
endif ()

if (IMPERX_SDI_FOUND)
    add_subdirectory (imperxsdi)
endif ()

if (IOTECHDAQX_FOUND)
	add_subdirectory (iotechdaqx)
endif (IOTECHDAQX_FOUND)

if (PHOENIX_FOUND)
	add_subdirectory(phoenix)
endif (PHOENIX_FOUND)

if (Pleora_FOUND)
	add_subdirectory(pleora)
endif (Pleora_FOUND)

if (PYLON_FOUND)
	add_subdirectory(pylon)
endif (PYLON_FOUND)

if (SAPERA_FOUND)
	add_subdirectory(sapera)
endif (SAPERA_FOUND)

if (XCLIB_FOUND)
	add_subdirectory(pixci)
endif (XCLIB_FOUND)
