#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Await...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Await LANGUAGES CXX)
add_executable(UT_Await)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    AwaitTest.cpp
    UT_Await.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    Await BryptIdentifier BryptPeer Security MessageControl Network Configuration BryptMessage 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(UT_Await)
#-----------------------------------------------------------------------------------------------------------------------
