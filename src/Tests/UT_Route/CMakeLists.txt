#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Route...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Route LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ConnectHandlerTest.cpp
    PathTest.cpp
    RouterTest.cpp
    UT_Route.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    Route BryptMessage BryptIdentifier 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})
    
target_link_libraries(${PROJECT_NAME} PUBLIC
    Awaitable Route Peer Network Security Configuration MessageControl State Event Scheduler BryptMessage BryptIdentifier 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})
    
target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
