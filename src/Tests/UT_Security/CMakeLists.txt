#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Security...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Security LANGUAGES CXX)
add_executable(UT_Security)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    PQNISTL3StrategyTest.cpp
    SecurityMediatorTest.cpp
    UT_Security.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    BryptPeer Security MessageControl Network Configuration BryptMessage BryptIdentifier 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(UT_Security)
#-----------------------------------------------------------------------------------------------------------------------
