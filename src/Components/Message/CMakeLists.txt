#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Brypt Message Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(Message LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    ApplicationMessage.cpp
    Extension.cpp
    MessageContext.cpp
    MessageHeader.cpp
    MessageUtils.cpp
    PlatformMessage.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${spdlog_INCLUDE_DIRS})

# Compiler options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
