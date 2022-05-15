#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Peer Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(Peer LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    Action.cpp
    ProxyStore.cpp
    Proxy.cpp
    Registration.cpp
    Resolver.cpp
    Statistics.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

# Compiler options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
