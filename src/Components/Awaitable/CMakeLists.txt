#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Awaitable Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(Awaitable LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    Tracker.cpp
    TrackingService.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${OPENSSL_INCLUDE_DIR} ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${Boost_LIBRARIES})

# Compiler options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
