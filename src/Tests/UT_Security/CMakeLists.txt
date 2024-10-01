#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_Security...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_Security LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    
    UT_Security.cpp "CipherServiceTest.cpp" "CipherPackageTest.cpp" "QuantumSynchronizerTest.cpp" "NamedSecurityTypesTest.cpp" "KeyStoreTest.cpp" "PackageSynchronizerTest.cpp" "EllipticCurveSynchronizerTest.cpp" "FiniteFieldSynchronizerTest.cpp" )

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
    Awaitable Peer Security Processor Network Configuration Route Event Scheduler State Message Identifier 
    ${GTEST_LIBRARIES} ${spdlog_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

copy_dependencies(${PROJECT_NAME})

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
