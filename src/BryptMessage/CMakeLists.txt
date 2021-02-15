#------------------------------------------------------------------------------------------------
message(STATUS "Configuring Brypt Message Library...")
#------------------------------------------------------------------------------------------------
project(BryptMessage LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ApplicationMessage.cpp
    MessageContext.cpp
    MessageHeader.cpp
    MessageUtils.cpp
    NetworkMessage.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${spdlog_INCLUDE_DIRS})

# Compilier options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#------------------------------------------------------------------------------------------------
