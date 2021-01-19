#------------------------------------------------------------------------------------------------
message(STATUS "Configuring Brypt Peer Library...")
#------------------------------------------------------------------------------------------------
project(BryptPeer LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    BryptPeer.cpp
    EndpointRegistration.cpp
    PeerManager.cpp
    PeerStatistics.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

# Compilier options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#------------------------------------------------------------------------------------------------
