if (EURESYS_FOUND)
	add_subdirectory (euresys)
endif (EURESYS_FOUND)

if (NIIMAQ_FOUND)
	add_subdirectory (niimaq)
endif (NIIMAQ_FOUND)

if (IOTECHDAQX_FOUND)
	add_subdirectory (iotechdaqx)
endif (IOTECHDAQX_FOUND)

add_subdirectory (decklink)
