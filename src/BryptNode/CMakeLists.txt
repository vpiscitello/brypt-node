#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring brypt Binary...")
#-----------------------------------------------------------------------------------------------------------------------
project(brypt-executable LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    BryptNode.cpp
    RuntimePolicy.cpp
    StartupOptions.cpp
    main.cpp)

set(INTERNAL_LIBRARIES 
    Awaitable
    Peer
    Network
    Security
    Configuration
    MessageControl
    Route
    Event
    Scheduler
    State
    BryptMessage
    BryptIdentifier)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME brypt)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # TODO: Implement the install for the brypt executable 
    set(MACOS_INSTALL_RPATH
        "@executable_path/../lib/"
        "@executable_path/../external/lib/")

    set_target_properties(brypt-executable
        PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH_USE_LINK_PATH TRUE
        INSTALL_RPATH "${MACOS_INSTALL_RPATH}")

    set(MACOS_EXTERNAL_DIR "@executable_path/../../external/")

    # Fixup the binary on MacOS to run without installing
    add_custom_command(TARGET brypt-executable 
        POST_BUILD
        COMMAND 
            ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "${MACOS_EXTERNAL_DIR}boost/${BOOST_VERSION}/lib/" $<TARGET_FILE:brypt-executable>
        COMMAND 
            ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "${MACOS_EXTERNAL_DIR}liboqs/${OQS_VERSION}/lib/" $<TARGET_FILE:brypt-executable>
        COMMAND 
            ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "${MACOS_EXTERNAL_DIR}openssl/${OPENSSL_VERSION}/lib/" $<TARGET_FILE:brypt-executable>
        COMMAND 
            ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "${MACOS_EXTERNAL_DIR}spdlog/${SPDLOG_VERSION}/lib/" $<TARGET_FILE:brypt-executable>)
endif()

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${INTERNAL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${spdlog_LIBRARIES} ${Boost_LIBRARIES} Boost::program_options)

# Compiler options
target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
#-----------------------------------------------------------------------------------------------------------------------
