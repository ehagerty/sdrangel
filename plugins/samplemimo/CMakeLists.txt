project(samplemimo)

if(ENABLE_BLADERF AND LIBBLADERF_FOUND)
    add_subdirectory(bladerf2mimo)
endif()

if(ENABLE_LIMESUITE AND LIMESUITE_FOUND)
    add_subdirectory(limesdrmimo)
endif()

if(ENABLE_XTRX AND LIBXTRX_FOUND)
    add_subdirectory(xtrxmimo)
endif()

if(ENABLE_IIO AND LIBIIO_FOUND)
    add_subdirectory(plutosdrmimo)
endif()

if(ENABLE_USRP AND UHD_FOUND)
    add_subdirectory(usrpmimo)
endif()

add_subdirectory(metismiso)
add_subdirectory(testmi)
add_subdirectory(testmosync)
