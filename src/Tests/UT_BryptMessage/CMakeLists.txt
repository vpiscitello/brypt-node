#------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_BryptMessage...")
#------------------------------------------------------------------------------------------------
project(UT_BryptMessage LANGUAGES CXX)
add_executable(UT_BryptMessage)
#------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ApplicationMessageTest.cpp
    MessageHeaderTest.cpp
    NetworkMessageTest.cpp
    UT_BryptMessage.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${GTEST_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    BryptIdentifier BryptMessage
    ${GTEST_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#------------------------------------------------------------------------------------------------
gtest_discover_tests(UT_BryptMessage)
#------------------------------------------------------------------------------------------------
