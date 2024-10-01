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
    ResultTest.cpp
    UT_BryptLibrary.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GTEST_INCLUDE_DIRS} ${BRYPT_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC brypt ${GTEST_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

copy_dependencies(${PROJECT_NAME})

#-----------------------------------------------------------------------------------------------------------------------

configure_file("config/config.json" "config/config.json" COPYONLY) 
 
#-----------------------------------------------------------------------------------------------------------------------

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BRYPT_LIBRARY_SUFFIX "d")
    endif()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/brypt${BRYPT_LIBRARY_SUFFIX}.${SHARED_EXTENSION}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()

#-----------------------------------------------------------------------------------------------------------------------
gtest_discover_tests(${PROJECT_NAME})
#-----------------------------------------------------------------------------------------------------------------------
