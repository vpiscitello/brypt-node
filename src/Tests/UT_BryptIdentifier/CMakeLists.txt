#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_BryptIdentifier...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_BryptIdentifier LANGUAGES CXX)
add_executable(UT_BryptIdentifier)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    BryptIdentifierTest.cpp
    UT_BryptIdentifier.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${GTEST_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    BryptIdentifier
    ${GTEST_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(UT_BryptIdentifier)
#-----------------------------------------------------------------------------------------------------------------------
