#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring State Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(State LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    CoordinatorState.cpp
    NetworkState.cpp
    NodeState.cpp
    SecurityState.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

# Compiler options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
