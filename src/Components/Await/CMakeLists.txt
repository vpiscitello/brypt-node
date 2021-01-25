#------------------------------------------------------------------------------------------------
message(STATUS "Configuring Await Library...")
#------------------------------------------------------------------------------------------------
project(Await LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ResponseTracker.cpp
    TrackingManager.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${LITHIUM_JSON_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${spdlog_INCLUDE_DIRS})

# Compilier options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#------------------------------------------------------------------------------------------------
