#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring UT_BryptLibrary...")
#-----------------------------------------------------------------------------------------------------------------------
project(UT_BryptLibrary LANGUAGES CXX)
add_executable(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------

set(BRYPT_DIRECTORY ${CMAKE_BINARY_DIR})
set(BRYPT_INCLUDE_DIRS ${BRYPT_DIRECTORY}/include)

add_dependencies(UT_BryptLibrary brypt)

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    OptionTest.cpp
    ServiceTest.cpp
    StatusTest.cpp
    UT_BryptLibrary.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${BRYPT_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC brypt ${GTEST_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------

configure_file("config/config.json" "config/config.json" COPYONLY)
configure_file("config/bootstrap.json" "config/bootstrap.json" COPYONLY)

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
