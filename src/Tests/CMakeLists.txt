#-----------------------------------------------------------------------------------------------------------------------
include(GoogleTest)
#-----------------------------------------------------------------------------------------------------------------------
if(NOT SKIP_BINARY)
    add_subdirectory(UT_Awaitable)
    add_subdirectory(UT_BryptIdentifier)
    add_subdirectory(UT_Message)
    add_subdirectory(UT_Configuration)
    add_subdirectory(UT_Processor)
    add_subdirectory(UT_Network)
    add_subdirectory(UT_Peer)
    add_subdirectory(UT_Route)
    add_subdirectory(UT_Security)
    add_subdirectory(UT_Scheduler)
endif()

#-----------------------------------------------------------------------------------------------------------------------

if(NOT SKIP_SHARED_LIBRARY)
    add_subdirectory(UT_BryptLibrary)
endif()

#-----------------------------------------------------------------------------------------------------------------------
