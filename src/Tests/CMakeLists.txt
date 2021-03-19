#-----------------------------------------------------------------------------------------------------------------------
include(GoogleTest)
#-----------------------------------------------------------------------------------------------------------------------
if(NOT SKIP_BINARY)
    add_subdirectory(UT_Await)
    add_subdirectory(UT_BryptIdentifier)
    add_subdirectory(UT_BryptMessage)
    add_subdirectory(UT_BryptPeer)
    add_subdirectory(UT_Handler)
    add_subdirectory(UT_Configuration)
    add_subdirectory(UT_MessageControl)
    add_subdirectory(UT_Network)
    add_subdirectory(UT_Security)
endif()

#-----------------------------------------------------------------------------------------------------------------------

if(NOT SKIP_SHARED_LIBRARY)
    add_subdirectory(UT_BryptLibrary)
endif()

#-----------------------------------------------------------------------------------------------------------------------
