#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring Security Static Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(Security LANGUAGES CXX)
add_library(${PROJECT_NAME} STATIC)
#-----------------------------------------------------------------------------------------------------------------------

target_sources(
    # Library Name
    ${PROJECT_NAME}
    # Source Type
    PRIVATE
    # Source Files
    KeyStore.cpp
    SecureBuffer.cpp
    SecurityMediator.cpp
    SecurityUtils.cpp
    PostQuantum/NISTSecurityLevelThree.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${OQS_INCLUDE_DIRS} ${OQSCPP_INCLUDE_DIRS})

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${OPENSSL_CRYPTO_LIBRARY} ${OQS_LIBRARIES})

# Compilier options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
