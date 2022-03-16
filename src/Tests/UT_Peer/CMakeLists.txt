#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Peer...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Peer LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ActionTest.cpp
    ExchangeTest.cpp
    PeerManagerTest.cpp
    ResolverTest.cpp
    UT_Peer.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${GTEST_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    Awaitable Peer Network Security MessageControl Network Configuration Event Scheduler State BryptMessage BryptIdentifier
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
