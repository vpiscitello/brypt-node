#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Network...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Network LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    AddressTest.cpp
    ConnectionDetailsTest.cpp
    NetworkManagerTest.cpp
    MessageSinkStub.cpp
    SinglePeerMediatorStub.cpp
    TcpEndpointTest.cpp
    UT_Network.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    Peer Network Security MessageControl Configuration Event Scheduler BryptMessage BryptIdentifier 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
