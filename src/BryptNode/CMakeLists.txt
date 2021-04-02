#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring brypt Binary...")
#-----------------------------------------------------------------------------------------------------------------------
project(brypt-bin LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    BryptNode.cpp
    CoordinatorState.cpp
    NetworkState.cpp
    NodeState.cpp
    RuntimePolicy.cpp
    SecurityState.cpp
    SensorState.cpp
    StartupOptions.cpp
    main.cpp)

set(INTERNAL_LIBRARIES 
    Await
    Handler
    BryptPeer
    Network
    Security
    Configuration
    MessageControl
    BryptMessage
    BryptIdentifier)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME brypt)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    # TODO: Implement the install for the brypt binary 
    set(MACOS_INSTALL_RPATH
        "@executable_path/../lib/"
        "@executable_path/../external/lib/")

    set_target_properties(brypt-bin
        PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH_USE_LINK_PATH TRUE
        INSTALL_RPATH "${MACOS_INSTALL_RPATH}")

    # Fixup the binary on MacOS to run without installing
    add_custom_command(TARGET brypt-bin 
        POST_BUILD
        COMMAND 
            ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../external/boost/${BOOST_VERSION}/lib/" $<TARGET_FILE:brypt-bin>
        COMMAND 
            ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../external/liboqs/${OQS_VERSION}/lib/" $<TARGET_FILE:brypt-bin>
        COMMAND 
            ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../external/openssl/${OPENSSL_VERSION}/lib/" $<TARGET_FILE:brypt-bin>
        COMMAND 
            ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../external/spdlog/${SPDLOG_VERSION}/lib/" $<TARGET_FILE:brypt-bin>)
endif()

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${INTERNAL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${spdlog_LIBRARIES} ${Boost_LIBRARIES})

# Compilier options
target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
#-----------------------------------------------------------------------------------------------------------------------
