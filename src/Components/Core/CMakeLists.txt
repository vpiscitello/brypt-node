#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Core Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(Core LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    Core.cpp
    RuntimePolicy.cpp)

set(INTERNAL_LIBRARIES 
    Awaitable
    Peer
    Network
    Security
    Configuration
    Processor
    Route
    Event
    Scheduler
    State
    Message
    Identifier)

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${INTERNAL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${spdlog_LIBRARIES} ${Boost_LIBRARIES})

# Compiler options
target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
