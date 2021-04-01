#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_BryptPeer...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_BryptPeer LANGUAGES CXX)
add_executable(UT_BryptPeer)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    PeerManagerTest.cpp
    UT_BryptPeer.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${GTEST_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    BryptPeer Network Security MessageControl Network Configuration BryptMessage BryptIdentifier
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(UT_BryptPeer)
#-----------------------------------------------------------------------------------------------------------------------
