#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Awaitable...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Awaitable LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    AggregateTrackerTest.cpp
    TrackingServiceTest.cpp
    UT_Awaitable.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${GTEST_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    Awaitable BryptIdentifier Peer Security MessageControl Network Configuration Event Scheduler State BryptMessage 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
