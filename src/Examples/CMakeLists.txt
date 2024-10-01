#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Examples...")
#-----------------------------------------------------------------------------------------------------------------------
project(examples LANGUAGES CXX)

set(ALPHA_BINARY alpha)
set(OMEGA_BINARY omega)

add_executable(${ALPHA_BINARY})
add_executable(${OMEGA_BINARY})
#-----------------------------------------------------------------------------------------------------------------------

set(BRYPT_DIRECTORY ${CMAKE_BINARY_DIR})
set(BRYPT_INCLUDE_DIRS ${BRYPT_DIRECTORY}/include)

add_dependencies(${ALPHA_BINARY} brypt)
target_sources(${ALPHA_BINARY} PRIVATE alpha.cpp)
target_include_directories(${ALPHA_BINARY} SYSTEM PUBLIC ${BRYPT_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})
target_link_libraries(${ALPHA_BINARY} PUBLIC brypt)
target_link_libraries(${ALPHA_BINARY} PUBLIC PROJECT_OPTIONS)
copy_dependencies(${ALPHA_BINARY})

add_dependencies(${OMEGA_BINARY} brypt)
target_sources(${OMEGA_BINARY} PRIVATE omega.cpp)
target_include_directories(${OMEGA_BINARY} SYSTEM PUBLIC ${BRYPT_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})
target_link_libraries(${OMEGA_BINARY} PUBLIC brypt)
target_link_libraries(${OMEGA_BINARY} PUBLIC PROJECT_OPTIONS)
copy_dependencies(${OMEGA_BINARY})

#-----------------------------------------------------------------------------------------------------------------------

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BRYPT_LIBRARY_SUFFIX "d")
    endif()

    add_custom_command(TARGET ${ALPHA_BINARY} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/brypt${BRYPT_LIBRARY_SUFFIX}.${SHARED_EXTENSION}
        $<TARGET_FILE_DIR:${ALPHA_BINARY}>)

    add_custom_command(TARGET ${OMEGA_BINARY} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/brypt${BRYPT_LIBRARY_SUFFIX}.${SHARED_EXTENSION}
        $<TARGET_FILE_DIR:${OMEGA_BINARY}>)
endif()

#-----------------------------------------------------------------------------------------------------------------------
