#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_BryptIdentifier...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_BryptIdentifier LANGUAGES CXX)
add_executable(${PROJECT_NAME})
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
    Identifier
    ${GTEST_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

copy_dependencies(${PROJECT_NAME})

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
