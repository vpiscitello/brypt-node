#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Route Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(Route LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    Auxiliary.cpp
    Connect.cpp
    MessageHandler.cpp
    Information.cpp
    Path.cpp
    Router.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${Boost_LIBRARIES})

# Compiler options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
