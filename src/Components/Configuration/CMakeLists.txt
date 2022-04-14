#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Configuration Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(Configuration LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    BootstrapService.cpp
    Options.cpp
    Parser.cpp)

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${Boost_INCLUDE_DIRS} ${LITHIUM_JSON_INCLUDE_DIR} ${spdlog_INCLUDE_DIRS})

# Compiler options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
