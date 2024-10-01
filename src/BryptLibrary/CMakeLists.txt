#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring libbrypt Shared Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(brypt LANGUAGES CXX VERSION ${PROJECT_VERSION})
add_library(${PROJECT_NAME} SHARED)
#-----------------------------------------------------------------------------------------------------------------------

set(C_INTERFACE_EXPORTED_HEADERS brypt.h)

set(CXX20_EXPORTED_HEADERS 
    LanguageBindings/cpp20/action.hpp
    LanguageBindings/cpp20/brypt.hpp
    LanguageBindings/cpp20/helpers.hpp
    LanguageBindings/cpp20/identifier.hpp
    LanguageBindings/cpp20/options.hpp
    LanguageBindings/cpp20/peer.hpp
    LanguageBindings/cpp20/protocol.hpp
    LanguageBindings/cpp20/result.hpp
    LanguageBindings/cpp20/status.hpp)
    
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${CMAKE_BINARY_DIR}/include/)

target_sources(
    ${PROJECT_NAME} PRIVATE
    ${C_INTERFACE_EXPORTED_HEADERS} ${CXX20_EXPORTED_HEADERS}
    brypt.cpp
    service.cpp)

set(EXPORTED_HEADERS ${C_INTERFACE_EXPORTED_HEADERS} ${CXX20_EXPORTED_HEADERS})

set_target_properties(
    # Library Name
    ${PROJECT_NAME}
    # Library Properties
    PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${EXPORTED_HEADERS}"
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    DEBUG_POSTFIX d)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DBRYPT_SHARED)

target_link_libraries(${PROJECT_NAME} PUBLIC Core)

# Compiler options
target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMENT "Expoting libbrypt headers to ${CMAKE_BINARY_DIR}/include/brypt"
    COMMAND COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include/brypt
    COMMAND COMMAND ${CMAKE_COMMAND} -E copy ${EXPORTED_HEADERS} ${CMAKE_BINARY_DIR}/include/brypt
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

#-----------------------------------------------------------------------------------------------------------------------

install(
    TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/brypt)

#-----------------------------------------------------------------------------------------------------------------------
