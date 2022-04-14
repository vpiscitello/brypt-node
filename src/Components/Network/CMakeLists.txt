#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Network Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(Network LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

set(BASE_ENDPOINT_SOURCES Endpoint.cpp Manager.cpp Address.cpp)
set(LORA_ENDPOINT_SOURCES LoRa/Endpoint.cpp)
set(TCP_ENDPOINT_SOURCES TCP/Endpoint.cpp TCP/Events.cpp TCP/Session.cpp)

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ${BASE_ENDPOINT_SOURCES} ${LORA_ENDPOINT_SOURCES} ${TCP_ENDPOINT_SOURCES})

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${Boost_LIBRARIES})

# Compiler options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
