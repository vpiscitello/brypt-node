#------------------------------------------------------------------------------------------------
message(STATUS "Configuring brypt Binary...")
#------------------------------------------------------------------------------------------------
project(brypt-bin LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#------------------------------------------------------------------------------------------------

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

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${INTERNAL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${spdlog_LIBRARIES} ${Boost_LIBRARIES})

# Compilier options
target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#------------------------------------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
#------------------------------------------------------------------------------------------------
