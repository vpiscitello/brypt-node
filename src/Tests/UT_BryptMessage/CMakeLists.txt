#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_BryptMessage...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_BryptMessage LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ApplicationMessageTest.cpp
    MessageHeaderTest.cpp
    PlatformMessageTest.cpp
    UT_BryptMessage.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${GTEST_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    BryptIdentifier BryptMessage
    ${GTEST_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
