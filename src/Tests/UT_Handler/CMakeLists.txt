#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Handler...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Handler LANGUAGES CXX)
add_executable(UT_Handler)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    HandlerTest.cpp
    UT_Handler.cpp)

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ${CMAKE_SOURCE_DIR}/BryptNode/BryptNode.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/CoordinatorState.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/NetworkState.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/NodeState.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/SecurityState.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/SensorState.cpp)    

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    Await Handler BryptPeer Network Security Configuration MessageControl BryptMessage BryptIdentifier 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(UT_Handler)
#-----------------------------------------------------------------------------------------------------------------------
