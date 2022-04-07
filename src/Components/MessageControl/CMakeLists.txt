#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Message Control Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(MessageControl LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    AuthorizedProcessor.cpp
    ExchangeProcessor.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${LITHIUM_JSON_INCLUDE_DIR})

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${Boost_INCLUDE_DIRS} ${OQS_INCLUDE_DIRS} ${OQSCPP_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS})

# Compilier options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
