#-----------------------------------------------------------------------------------------------------------------------

include(ExternalProject)
include(FetchContent)

set(DEPENDENCY_DIRECTORY ${CMAKE_SOURCE_DIR}/external)
set(DEPENDENCY_TEMP_DIRECTORY ${DEPENDENCY_DIRECTORY}/temp)
set(DEPENDENCY_DOWNLOAD_DIRECTORY ${DEPENDENCY_DIRECTORY}/download)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(SHARED_EXTENSION dll)
    set(STATIC_EXTENSION lib)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(SHARED_EXTENSION dylib)
    set(STATIC_EXTENSION a)
else()
    set(SHARED_EXTENSION so)
    set(STATIC_EXTENSION a)
endif()

#-----------------------------------------------------------------------------------------------------------------------
# Boost Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(BOOST_VERSION "1.75.0")
set(BOOST_DOWNLOAD_TARGET "boost_1_75_0.tar.gz")
set(BOOST_URL "https://dl.bintray.com/boostorg/release/${BOOST_VERSION}/source/${BOOST_DOWNLOAD_TARGET}")
set(BOOST_HASH "SHA256=aeb26f80e80945e82ee93e5939baebdca47b9dee80a07d3144be1e1a6a66dd6a")
set(BOOST_DIRECTORY ${DEPENDENCY_DIRECTORY}/boost/${BOOST_VERSION})
set(BOOST_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/boost/${BOOST_VERSION})

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_RELEASE_LIBS ON)
set(Boost_USE_MULTITHREADED ON)

find_package(Boost ${BOOST_VERSION} COMPONENTS system program_options HINTS ${BOOST_DIRECTORY} QUIET)
if (NOT Boost_FOUND)
    if (NOT EXISTS ${BOOST_DOWNLOAD_DIRECTORY})
        message(STATUS "Boost was not found. Downloading now...")

        FetchContent_Declare(
            boost
            URL ${BOOST_URL}
            URL_HASH ${BOOST_HASH}
            TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
            DOWNLOAD_DIR ${BOOST_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${BOOST_DOWNLOAD_DIRECTORY})

        if(NOT boost_POPULATED)
            FetchContent_Populate(boost)
        endif()
        
        message(STATUS "Boost downloaded to ${BOOST_DIRECTORY}")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(BOOST_TOOLSET gcc)
        string(REGEX MATCHALL "[0-9]+" GCC_VERSION_COMPONENTS ${CMAKE_CXX_COMPILER_VERSION})
        list(GET GCC_VERSION_COMPONENTS 0 GCC_MAJOR)
        set(BOOST_TOOLSET_CONFIGURATION
            echo "using gcc : ${GCC_MAJOR} : ${CMAKE_CXX_COMPILER} $<SEMICOLON> " >> ${BOOST_DOWNLOAD_DIRECTORY}/tools/build/src/user-config.jam)
    endif()

    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(BOOST_FIXUP_DYLIBS
            bash ${CMAKE_SOURCE_DIR}/cmake/MacOSFixupDylibRpaths.sh ${BOOST_DIRECTORY}/lib)
    endif()
    
    set(BOOST_CONFIGURE "./bootstrap.sh")
    set(BOOST_CONFIGURE_PARAMS
        --prefix=${BOOST_DIRECTORY}
        --libdir=${BOOST_DIRECTORY}/lib
        --with-toolset=${BOOST_TOOLSET}
        --with-python=python3
        --with-libraries=chrono,program_options,system,timer)

    set(BOOST_BUILD "./b2")
    set(BOOST_BUILD_PARAMS
        --toolset=${BOOST_TOOLSET}
        --threading=multi
        --link=shared
        --variant=release
        --layout=system
        --build-type=minimal
        -sNO_LZMA=1
        -sNO_ZSTD=1
        -j4)

    ExternalProject_Add(
        boost
        PREFIX ${BOOST_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${BOOST_DOWNLOAD_DIRECTORY}
        BUILD_IN_SOURCE true
        URL ""
        DOWNLOAD_DIR ""
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND
            CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
            ${BOOST_COMPILIER} ${BOOST_CONFIGURE} ${BOOST_CONFIGURE_PARAMS}
        COMMAND
            ${BOOST_TOOLSET_CONFIGURATION}
        BUILD_COMMAND
            ${BOOST_BUILD} install ${BOOST_BUILD_PARAMS}
        COMMAND
            ${BOOST_FIXUP_DYLIBS}
        INSTALL_COMMAND ""
        INSTALL_DIR ${BOOST_DIRECTORY})

    set(Boost_INCLUDE_DIRS ${BOOST_DIRECTORY}/include)
    set(Boost_LIBRARIES
        ${BOOST_DIRECTORY}/lib/libboost_system.${SHARED_EXTENSION}
        ${BOOST_DIRECTORY}/lib/libboost_program_options.${SHARED_EXTENSION})
endif()

message(DEBUG "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(DEBUG "Boost_LIBRARIES: ${Boost_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# Googletest Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(GOOGLETEST_VERSION "1.10.0")
set(GOOGLETEST_URL "https://github.com/google/googletest/archive/release-${GOOGLETEST_VERSION}.tar.gz")
set(GOOGLETEST_HASH "SHA256=9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb")
set(GOOGLETEST_DIRECTORY ${DEPENDENCY_DIRECTORY}/googletest/${GOOGLETEST_VERSION})
set(GOOGLETEST_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/googletest/${GOOGLETEST_VERSION})

if(NOT DEFINED GTEST_ROOT)
    set(GTEST_ROOT ${GOOGLETEST_DIRECTORY})
endif()

find_package(GTest ${GOOGLETEST_VERSION} QUIET)
if (NOT GTEST_FOUND)
    if (NOT EXISTS ${GOOGLETEST_DOWNLOAD_DIRECTORY})
        message(STATUS "Googletest was not found. Downloading now...")

        FetchContent_Declare(
            googletest
            URL ${GOOGLETEST_URL}
            URL_HASH ${GOOGLETEST_HASH}
            TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
            DOWNLOAD_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY})

        if(NOT googletest_POPULATED)
            FetchContent_Populate(googletest)
        endif()
        
        message(STATUS "Googletest downloaded to ${GOOGLETEST_DIRECTORY}")
    endif()

    set(GOOGLETEST_CONFIGURE_PARAMS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} 
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${GOOGLETEST_DIRECTORY})
    
    ExternalProject_Add(
        googletest
        PREFIX ${GOOGLETEST_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY}
        BUILD_IN_SOURCE true
        URL ""
        DOWNLOAD_DIR ""
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CMAKE_ARGS ${GOOGLETEST_CONFIGURE_PARAMS}
        INSTALL_DIR ${GOOGLETEST_DIRECTORY})

    set(GTEST_INCLUDE_DIRS ${GOOGLETEST_DIRECTORY}/include)
    set(GTEST_LIBRARIES ${GOOGLETEST_DIRECTORY}/lib/libgtest.${STATIC_EXTENSION})
endif()

message(DEBUG "GTEST_INCLUDE_DIRS: ${GTEST_INCLUDE_DIRS}")
message(DEBUG "GTEST_LIBRARIES: ${GTEST_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# Lithium JSON Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(LITHIUM_JSON_VERSION "0.0.0")
set(LITHIUM_JSON_FILE "lithium_json.hh")
set(LITHIUM_JSON_URL "https://raw.githubusercontent.com/matt-42/lithium/master/single_headers/${LITHIUM_JSON_FILE}")
set(LITHIUM_JSON_DIRECTORY ${DEPENDENCY_DIRECTORY}/lithium/json/${LITHIUM_JSON_VERSION})
set(LITHIUM_JSON_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/lithium/json/${LITHIUM_JSON_VERSION})

if (NOT EXISTS "${LITHIUM_JSON_DIRECTORY}/${LITHIUM_JSON_FILE}")
    message(STATUS "Lithium JSON was not found. Downloading now...")

    FetchContent_Declare(
        lithium_json
        URL ${LITHIUM_JSON_URL}
        TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
        DOWNLOAD_DIR ${LITHIUM_JSON_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${LITHIUM_JSON_DIRECTORY}
        DOWNLOAD_NO_EXTRACT true)

    if(NOT lithium_json_POPULATED)
        FetchContent_Populate(lithium_json)
    endif()

    message(STATUS "Lithium JSON downloaded to ${LITHIUM_JSON_DIRECTORY}")
endif()

# Always set the Lithium JSON include directory given it is not findable. 
set(LITHIUM_JSON_INCLUDE_DIR ${LITHIUM_JSON_DIRECTORY})
message(DEBUG "LITHIUM_JSON_INCLUDE_DIR: ${LITHIUM_JSON_INCLUDE_DIR}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# OpenSSL Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(OPENSSL_VERSION "1.1.1")
set(OPENSSL_PATCH "i")
set(OPENSSL_URL "https://www.openssl.org/source/openssl-${OPENSSL_VERSION}${OPENSSL_PATCH}.tar.gz")
set(OPENSSL_HASH "SHA256=e8be6a35fe41d10603c3cc635e93289ed00bf34b79671a3a4de64fcee00d5242")
set(OPENSSL_DIRECTORY ${DEPENDENCY_DIRECTORY}/openssl/${OPENSSL_VERSION}${OPENSSL_PATCH})
set(OPENSSL_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/openssl/${OPENSSL_VERSION}${OPENSSL_PATCH})

if(NOT DEFINED OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR ${OPENSSL_DIRECTORY})
endif()

find_package(OpenSSL ${OPENSSL_VERSION} QUIET)
if (NOT OPENSSL_FOUND)
    set(OPENSSL_BUILD_REQUIRED true)

    if (NOT EXISTS ${OPENSSL_DOWNLOAD_DIRECTORY})
        message(STATUS "OpenSSL was not found. Downloading now...")

        FetchContent_Declare(
            openssl
            URL ${OPENSSL_URL}
            URL_HASH ${OPENSSL_HASH}
            TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
            DOWNLOAD_DIR ${OPENSSL_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${OPENSSL_DOWNLOAD_DIRECTORY})

        if(NOT openssl_POPULATED)
            FetchContent_Populate(openssl)
        endif()
        
        message(STATUS "OpenSSL downloaded to ${OPENSSL_DIRECTORY}")
    endif()

    set(OPENSSL_CONFIGURE "./config")
    set(OPENSSL_CONFIGURE_MODULES
        no-cast no-md2 no-md4 no-mdc2 no-rc4 no-rc5 no-engine no-idea no-mdc2 no-rc5
        no-camellia no-ssl3 no-heartbeats no-gost no-deprecated no-capieng no-comp no-dtls
        no-psk no-srp no-dso no-dsa no-rc2 no-des)
    set(OPENSSL_CONFIGURE_PARAMS
        --prefix=${OPENSSL_DIRECTORY} --openssldir=${OPENSSL_DIRECTORY} --libdir=lib)

    set(OPENSSL_BUILD_TARGET build_generated)
    set(OPENSSL_INSTALL_TARGET install_sw)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(OPENSSL_KERNEL_BITS KERNEL_BITS=64)
        set(OPENSSL_MAKE_TARGET build_libs)
        set(OPENSSL_INSTALL_TARGET install_dev)
    endif()

    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(OPENSSL_FIXUP_DYLIBS
            bash ${CMAKE_SOURCE_DIR}/cmake/MacOSFixupDylibRpaths.sh ${OPENSSL_DIRECTORY}/lib)
    endif()

    find_program(OPENSSL_BUILD make REQUIRED)

    ExternalProject_Add(
        openssl
        PREFIX ${OPENSSL_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${OPENSSL_DOWNLOAD_DIRECTORY}
        BUILD_IN_SOURCE true
        URL ""
        DOWNLOAD_DIR ""
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND
            CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} ${OPENSSL_KERNEL_BITS}
        COMMAND
            ${OPENSSL_CONFIGURE} ${OPENSSL_CONFIGURE_MODULES} ${OPENSSL_CONFIGURE_PARAMS}
        BUILD_COMMAND
            ${OPENSSL_BUILD} ${OPENSSL_MAKE_TARGET}
        INSTALL_COMMAND
            ${OPENSSL_BUILD} ${OPENSSL_INSTALL_TARGET}
        COMMAND
            ${OPENSSL_FIXUP_DYLIBS}
        INSTALL_DIR ${OPENSSL_DIRECTORY})

    set(OPENSSL_INCLUDE_DIR ${OPENSSL_DIRECTORY}/include)
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_DIRECTORY}/lib/libcrypto.${SHARED_EXTENSION})    
endif()

message(DEBUG "OPENSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR}")
message(DEBUG "OPENSSL_CRYPTO_LIBRARY: ${OPENSSL_CRYPTO_LIBRARY}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# OQS Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(OQS_VERSION "0.4.0")
set(OQS_URL "https://github.com/open-quantum-safe/liboqs/archive/${OQS_VERSION}.tar.gz")
set(OQS_HASH "SHA256=05836cd2b5c70197b3b6eed68b97d0ccb2c445061d5c19c15aef7c959842de0b")
set(OQS_DIRECTORY ${DEPENDENCY_DIRECTORY}/liboqs/${OQS_VERSION})
set(OQS_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/liboqs/${OQS_VERSION})

if (NOT EXISTS ${OQS_DOWNLOAD_DIRECTORY})
    set(OQS_BUILD_REQUIRED true)

    message(STATUS "Open Quantum Safe (liboqs) was not found. Downloading now...")

    FetchContent_Declare(
        liboqs
        URL ${OQS_URL}
        URL_HASH ${OQS_HASH}
        TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
        DOWNLOAD_DIR ${OQS_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${OQS_DOWNLOAD_DIRECTORY})

    if(NOT liboqs_POPULATED)
        FetchContent_Populate(liboqs)
    endif()
    
    message(STATUS "Open Quantum Safe (liboqs) downloaded to ${DEPENDENCY_DOWNLOAD_DIRECTORY}")
endif()

if (NOT EXISTS ${OQS_DIRECTORY}/lib/liboqs.${SHARED_EXTENSION})
    set(OQS_CONFIGURE_GENERATOR "Ninja")
    set(OQS_CONFIGURE_PARAMS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DBUILD_SHARED_LIBS=ON
        -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
        -DCMAKE_INSTALL_PREFIX=${OQS_DIRECTORY})
    find_program(OQS_BUILD ninja REQUIRED)

    ExternalProject_Add(
        liboqs
        PREFIX ${OQS_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${OQS_DOWNLOAD_DIRECTORY}
        BINARY_DIR ${OQS_DOWNLOAD_DIRECTORY}/build
        URL ""
        DOWNLOAD_DIR ""
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CMAKE_GENERATOR ${OQS_CONFIGURE_GENERATOR}
        CMAKE_ARGS ${OQS_CONFIGURE_PARAMS}
        BUILD_COMMAND ${OQS_BUILD}
        INSTALL_DIR ${OQS_DIRECTORY})

    # If we are building OpenSSL, we must add a dependency to liboqs to ensure it is using the correct version
    if (OPENSSL_BUILD_REQUIRED)
        add_dependencies(liboqs openssl)
    endif()
endif()

set(OQS_INCLUDE_DIRS ${OQS_DIRECTORY}/include)
set(OQS_LIBRARIES ${OQS_DIRECTORY}/lib/liboqs.${SHARED_EXTENSION})    

message(DEBUG "OQS_INCLUDE_DIRS: ${OQS_INCLUDE_DIRS}")
message(DEBUG "OQS_LIBRARIES: ${OQS_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# OQS CPP Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(OQSCPP_VERSION "0.4.0")
set(OQSCPP_URL "https://github.com/open-quantum-safe/liboqs-cpp/archive/${OQSCPP_VERSION}.tar.gz")
set(OQSCPP_HASH "SHA256=4f1db339632a3478c210d267ef2d2e325c93a4b0af6523e9cbddaf3390d38080")
set(OQSCPP_DIRECTORY ${DEPENDENCY_DIRECTORY}/liboqs-cpp/${OQSCPP_VERSION})
set(OQSCPP_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/liboqs-cpp/${OQSCPP_VERSION})

if (NOT EXISTS ${OQSCPP_DIRECTORY})
    message(STATUS "Open Quantum Safe (liboqs-cpp) was not found. Downloading now...")

    FetchContent_Declare(
        liboqs-cpp
        URL ${OQSCPP_URL}
        URL_HASH ${OQSCPP_HASH}
        TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
        DOWNLOAD_DIR ${OQSCPP_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${OQSCPP_DOWNLOAD_DIRECTORY})

    if(NOT liboqs-cpp_POPULATED)
        FetchContent_Populate(liboqs-cpp)
    endif()

    file(COPY ${OQSCPP_DOWNLOAD_DIRECTORY}/include/ DESTINATION ${OQSCPP_DIRECTORY}/include/oqscpp)
    
    message(STATUS "Open Quantum Safe (liboqs-cpp) downloaded to ${OQSCPP_DIRECTORY}")
endif()

set(OQSCPP_INCLUDE_DIRS ${OQSCPP_DIRECTORY}/include)

message(DEBUG "OQSCPP_INCLUDE_DIRS: ${OQSCPP_INCLUDE_DIRS}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# spdlog Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(SPDLOG_VERSION "1.8.2")
set(SPDLOG_URL "https://github.com/gabime/spdlog/archive/v${SPDLOG_VERSION}.tar.gz")
set(SPDLOG_HASH "SHA256=e20e6bd8f57e866eaf25a5417f0a38a116e537f1a77ac7b5409ca2b180cec0d5")
set(SPDLOG_DIRECTORY ${DEPENDENCY_DIRECTORY}/spdlog/${SPDLOG_VERSION})
set(SPDLOG_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/spdlog/${SPDLOG_VERSION})

if (NOT EXISTS ${SPDLOG_DOWNLOAD_DIRECTORY})
    message(STATUS "spdlog was not found. Downloading now...")

    FetchContent_Declare(
        spdlog
        URL ${SPDLOG_URL}
        URL_HASH ${SPDLOG_HASH}
        TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
        DOWNLOAD_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${SPDLOG_DOWNLOAD_DIRECTORY})

    if(NOT spdlog_POPULATED)
        FetchContent_Populate(spdlog)
    endif()
    
    message(STATUS "spdlog downloaded to ${SPDLOG_DIRECTORY}")
endif()
    
if (NOT EXISTS ${SPDLOG_DIRECTORY}/lib/libspdlog.${SHARED_EXTENSION})
    set(SPDLOG_CONFIGURE_PARAMS
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${SPDLOG_DIRECTORY}
        -DSPDLOG_BUILD_SHARED=ON)
    
    ExternalProject_Add(
        spdlog
        PREFIX ${SPDLOG_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}
        BUILD_IN_SOURCE true
        URL ""
        DOWNLOAD_DIR ""
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CMAKE_ARGS ${SPDLOG_CONFIGURE_PARAMS}
        INSTALL_DIR ${SPDLOG_DIRECTORY})
endif()

set(spdlog_INCLUDE_DIRS ${SPDLOG_DIRECTORY}/include)
set(spdlog_LIBRARIES ${SPDLOG_DIRECTORY}/lib/libspdlog.${SHARED_EXTENSION})

message(DEBUG "spdlog_INCLUDE_DIRS: ${spdlog_INCLUDE_DIRS}")
message(DEBUG "spdlog_LIBRARIES: ${spdlog_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------
