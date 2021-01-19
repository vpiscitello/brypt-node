#------------------------------------------------------------------------------------------------
message(STATUS "Configuring Handler Library...")
#------------------------------------------------------------------------------------------------
project(Handler LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    Connect.cpp
    Election.cpp
    Handler.cpp
    Information.cpp
    Query.cpp)

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${Boost_INCLUDE_DIRS} ${LITHIUM_JSON_INCLUDE_DIR})

# Compilier options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#------------------------------------------------------------------------------------------------
