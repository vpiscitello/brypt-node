#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Processor...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Processor LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    AuthorizedProcessorTest.cpp
    ExchangeProcessorTest.cpp
    UT_Processor.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})
    
target_link_libraries(${PROJECT_NAME} PUBLIC
    Awaitable
    Route
    Peer
    Network
    Security
    Configuration
    Processor
    Network
    Event
    Scheduler
    State
    Message
    Identifier 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

copy_dependencies(${PROJECT_NAME})

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
