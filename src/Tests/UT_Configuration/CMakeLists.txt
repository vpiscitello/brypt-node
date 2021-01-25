#------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Configuration...")
#------------------------------------------------------------------------------------------------
project(UT_Configuration LANGUAGES CXX)
add_executable(UT_Configuration)
#------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ConfigurationTest.cpp
    PeerPersistorTest.cpp
    UT_Configuration.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    BryptPeer Security Configuration MessageControl BryptMessage BryptIdentifier 
    ${GTEST_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC  PROJECT_OPTIONS)

#------------------------------------------------------------------------------------------------

configure_file("files/good/config.json" "files/good/config.json" COPYONLY)
configure_file("files/good/peers.json" "files/good/peers.json" COPYONLY)
configure_file("files/malformed/config.json" "files/malformed/config.json" COPYONLY)
configure_file("files/malformed/peers.json" "files/malformed/peers.json" COPYONLY)
configure_file("files/missing/peers.json" "files/missing/peers.json" COPYONLY)

#------------------------------------------------------------------------------------------------
gtest_discover_tests(UT_Configuration)
#------------------------------------------------------------------------------------------------
