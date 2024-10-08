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
    CipherPackage.cpp
    CipherService.cpp
    "PackageSynchronizer.cpp"
    KeyStore.cpp
    SecureBuffer.cpp
    SecurityUtils.cpp
    SynchronizerContext.cpp
    "Classical/FiniteFieldDiffieHellmanModel.cpp"
    Classical/EllipticCurveDiffieHellmanModel.cpp
    PostQuantum/KeyEncapsulationModel.cpp)

# Exported include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${OQS_INCLUDE_DIRS} ${OQSCPP_INCLUDE_DIRS})

# Internal include dependencies
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${OPENSSL_CRYPTO_LIBRARY} ${OQS_LIBRARIES})

# Compiler options
target_link_libraries(${PROJECT_NAME} PRIVATE PROJECT_OPTIONS)

#-----------------------------------------------------------------------------------------------------------------------
