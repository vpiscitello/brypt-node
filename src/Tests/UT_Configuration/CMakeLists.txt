#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Configuration...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Configuration LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

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
    Peer Security Configuration MessageControl Network Event BryptMessage BryptIdentifier 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC  PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------

configure_file("files/good/config.json" "files/good/config.json" COPYONLY)
configure_file("files/good/bootstrap.json" "files/good/bootstrap.json" COPYONLY)
configure_file("files/malformed/config.json" "files/malformed/config.json" COPYONLY)
configure_file("files/malformed/bootstrap.json" "files/malformed/bootstrap.json" COPYONLY)
configure_file("files/missing/bootstrap.json" "files/missing/bootstrap.json" COPYONLY)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
