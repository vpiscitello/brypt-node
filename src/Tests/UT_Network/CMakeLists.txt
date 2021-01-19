#------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Network...")
#------------------------------------------------------------------------------------------------
project(UT_Network LANGUAGES CXX)
add_executable(UT_Network)
#------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ConnectionDetailsTest.cpp
    EndpointManagerTest.cpp
    MessageSinkStub.cpp
    SinglePeerMediatorStub.cpp
    TcpEndpointTest.cpp
    UT_Network.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${GTEST_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    BryptPeer Network Security MessageControl BryptMessage BryptIdentifier 
    ${GTEST_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#------------------------------------------------------------------------------------------------
gtest_discover_tests(UT_Network)
#------------------------------------------------------------------------------------------------
