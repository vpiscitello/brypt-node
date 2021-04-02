#-----------------------------------------------------------------------------------------------------------------------
message(STATUS "Configuring libbrypt Shared Library...")
#-----------------------------------------------------------------------------------------------------------------------
project(brypt LANGUAGES CXX VERSION ${PROJECT_VERSION})
add_library(${PROJECT_NAME} SHARED)
#-----------------------------------------------------------------------------------------------------------------------

set(
    PROJECT_SOURCES
    ${CMAKE_SOURCE_DIR}/BryptIdentifier/BryptIdentifier.cpp
    ${CMAKE_SOURCE_DIR}/BryptIdentifier/ReservedIdentifiers.cpp
    ${CMAKE_SOURCE_DIR}/BryptMessage/ApplicationMessage.cpp
    ${CMAKE_SOURCE_DIR}/BryptMessage/MessageContext.cpp
    ${CMAKE_SOURCE_DIR}/BryptMessage/MessageHeader.cpp
    ${CMAKE_SOURCE_DIR}/BryptMessage/MessageUtils.cpp
    ${CMAKE_SOURCE_DIR}/BryptMessage/NetworkMessage.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/BryptNode.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/CoordinatorState.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/NetworkState.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/NodeState.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/RuntimePolicy.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/SecurityState.cpp
    ${CMAKE_SOURCE_DIR}/BryptNode/SensorState.cpp
    ${CMAKE_SOURCE_DIR}/Components/Await/ResponseTracker.cpp
    ${CMAKE_SOURCE_DIR}/Components/Await/TrackingManager.cpp
    ${CMAKE_SOURCE_DIR}/Components/BryptPeer/BryptPeer.cpp
    ${CMAKE_SOURCE_DIR}/Components/BryptPeer/EndpointRegistration.cpp
    ${CMAKE_SOURCE_DIR}/Components/BryptPeer/PeerManager.cpp
    ${CMAKE_SOURCE_DIR}/Components/BryptPeer/PeerStatistics.cpp
    ${CMAKE_SOURCE_DIR}/Components/Configuration/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/Components/Configuration/ConfigurationManager.cpp
    ${CMAKE_SOURCE_DIR}/Components/Configuration/PeerPersistor.cpp
    ${CMAKE_SOURCE_DIR}/Components/Handler/Connect.cpp
    ${CMAKE_SOURCE_DIR}/Components/Handler/Election.cpp
    ${CMAKE_SOURCE_DIR}/Components/Handler/Handler.cpp
    ${CMAKE_SOURCE_DIR}/Components/Handler/Information.cpp
    ${CMAKE_SOURCE_DIR}/Components/Handler/Query.cpp
    ${CMAKE_SOURCE_DIR}/Components/MessageControl/AuthorizedProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Components/MessageControl/DiscoveryProtocol.cpp
    ${CMAKE_SOURCE_DIR}/Components/MessageControl/ExchangeProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Components/Network/Endpoint.cpp
    ${CMAKE_SOURCE_DIR}/Components/Network/EndpointManager.cpp
    ${CMAKE_SOURCE_DIR}/Components/Network/Address.cpp
    ${CMAKE_SOURCE_DIR}/Components/Network/LoRa/Endpoint.cpp
    ${CMAKE_SOURCE_DIR}/Components/Network/TCP/Endpoint.cpp
    ${CMAKE_SOURCE_DIR}/Components/Network/TCP/Events.cpp
    ${CMAKE_SOURCE_DIR}/Components/Network/TCP/Session.cpp
    ${CMAKE_SOURCE_DIR}/Components/Security/KeyStore.cpp
    ${CMAKE_SOURCE_DIR}/Components/Security/SecureBuffer.cpp
    ${CMAKE_SOURCE_DIR}/Components/Security/SecurityMediator.cpp
    ${CMAKE_SOURCE_DIR}/Components/Security/SecurityUtils.cpp
    ${CMAKE_SOURCE_DIR}/Components/Security/PostQuantum/NISTSecurityLevelThree.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES} brypt.cpp)

set(EXPORTED_HEADERS brypt.h brypt.hpp)

set_target_properties(
    # Library Name
    ${PROJECT_NAME}
    # Library Properties
    PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${EXPORTED_HEADERS}"
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

target_compile_definitions(${PROJECT_NAME} PRIVATE BRYPT_SHARED)

# Internal include dependencies
target_include_directories(
    ${PROJECT_NAME} SYSTEM PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${LITHIUM_JSON_INCLUDE_DIR} 
    ${OPENSSL_INCLUDE_DIR}
    ${OQS_INCLUDE_DIRS}
    ${OQSCPP_INCLUDE_DIRS} 
    ${spdlog_INCLUDE_DIRS})

# Exported library dependencies
target_link_libraries(
    ${PROJECT_NAME} PRIVATE
    ${Boost_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${OQS_LIBRARIES}
    ${spdlog_LIBRARIES})

# Compilier options
target_link_libraries(${PROJECT_NAME} PUBLIC PROJECT_OPTIONS)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-fdata-sections)
        add_compile_options(-ffunction-sections)
        add_compile_options(-Wl,--gc-sections)
    endif()
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE /EHsc /MTd /W2 /c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN_EXPORT)
    set_target_properties(${PROJECT_NAME} PROPERTIES CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

#-----------------------------------------------------------------------------------------------------------------------

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMENT "Expoting libbrypt headers to ${CMAKE_BINARY_DIR}/include/brypt"
    COMMAND COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include/brypt
    COMMAND COMMAND ${CMAKE_COMMAND} -E copy ${EXPORTED_HEADERS} ${CMAKE_BINARY_DIR}/include/brypt
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

#-----------------------------------------------------------------------------------------------------------------------

install(
    TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/brypt)

#-----------------------------------------------------------------------------------------------------------------------
