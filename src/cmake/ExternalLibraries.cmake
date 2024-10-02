#-----------------------------------------------------------------------------------------------------------------------

include(ExternalProject)
include(FetchContent)

if(BUILD_EXTERNAL)
    add_custom_target(external)
endif()

set(DEPENDENCY_DIRECTORY ${CMAKE_SOURCE_DIR}/../external)
set(DEPENDENCY_TEMP_DIRECTORY ${DEPENDENCY_DIRECTORY}/temp)
set(DEPENDENCY_DOWNLOAD_DIRECTORY ${DEPENDENCY_DIRECTORY}/download)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LIBRARY_BUILD_SUFFIX "d")
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(SHARED_EXTENSION dll)
    set(STATIC_EXTENSION lib)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LIBRARY_BUILD_FOLDER "debug")
    else()
        set(LIBRARY_BUILD_FOLDER "release")
    endif()
    
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(SHARED_EXTENSION dylib)
    set(STATIC_EXTENSION a)
else()
    set(SHARED_EXTENSION so)
    set(STATIC_EXTENSION a)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(COMPILER_OVERRIDES CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER})
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_C_COMPILER ${CMAKE_CXX_COMPILER})
endif()

#-----------------------------------------------------------------------------------------------------------------------
# Boost Dependency
#-----------------------------------------------------------------------------------------------------------------------

set(BOOST_VERSION "1.86.0")
set(BOOST_DOWNLOAD_TARGET "boost_1_86_0.tar.gz")
set(BOOST_URL "https://archives.boost.io/release/${BOOST_VERSION}/source/${BOOST_DOWNLOAD_TARGET}")
set(BOOST_HASH "SHA256=2575e74ffc3ef1cd0babac2c1ee8bdb5782a0ee672b1912da40e5b4b591ca01f")
set(BOOST_DIRECTORY ${DEPENDENCY_DIRECTORY}/boost/${BOOST_VERSION})
set(BOOST_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/boost/${BOOST_VERSION})

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_RELEASE_LIBS ON)
set(Boost_USE_MULTITHREADED ON)

find_package(Boost ${BOOST_VERSION} COMPONENTS container json program_options system HINTS ${BOOST_DIRECTORY} QUIET)
if (NOT Boost_FOUND AND BUILD_EXTERNAL)
    if (NOT EXISTS ${BOOST_DOWNLOAD_DIRECTORY})
        message(STATUS "Boost was not found. Downloading now...")

        FetchContent_Declare(
            boost
            URL ${BOOST_URL}
            URL_HASH ${BOOST_HASH}
            TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
            DOWNLOAD_DIR ${BOOST_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${BOOST_DOWNLOAD_DIRECTORY}
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE)

        FetchContent_Populate(boost)
        
        message(STATUS "Boost downloaded to ${BOOST_DIRECTORY}")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(BOOST_CONFIGURE "./bootstrap.sh")
        set(BOOST_TOOLSET gcc)
        string(REGEX MATCHALL "[0-9]+" GCC_VERSION_COMPONENTS ${CMAKE_CXX_COMPILER_VERSION})
        list(GET GCC_VERSION_COMPONENTS 0 GCC_MAJOR)
        set(BOOST_TOOLSET_CONFIGURATION
            echo "using gcc : ${GCC_MAJOR} : ${CMAKE_CXX_COMPILER} $<SEMICOLON> " >> ${BOOST_DOWNLOAD_DIRECTORY}/tools/build/src/user-config.jam)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(BOOST_CONFIGURE "./bootstrap.bat")
        set(BOOST_TOOLSET msvc)
    endif()

    set(BOOST_CONFIGURE_PARAMS
        --prefix=${BOOST_DIRECTORY}
        --libdir=${BOOST_DIRECTORY}/lib
        --with-toolset=${BOOST_TOOLSET}
        --with-python=python3
        --with-libraries=chrono,json,program_options,system,timer)

    set(BOOST_BUILD "./b2")
    set(BOOST_BUILD_PARAMS
        --prefix=${BOOST_DIRECTORY}
        --libdir=${BOOST_DIRECTORY}/lib
        --build-type=minimal
        --no-cmake-config
        --with-chrono
        --with-json
        --with-program_options
        --with-system
        --with-timer
        toolset=${BOOST_TOOLSET}
        threading=multi
        runtime-link=shared
        address-model=64
        -sNO_LZMA=1
        -sNO_ZSTD=1
        -j4)
    
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(BOOST_FIXUP_DYLIBS
            bash ${CMAKE_SOURCE_DIR}/cmake/MacOSFixupDylibRpaths.sh ${BOOST_DIRECTORY}/lib)
    endif()
    
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
            ${COMPILER_OVERRIDES}
            ${BOOST_COMPILER} ${BOOST_CONFIGURE} ${BOOST_CONFIGURE_PARAMS}
        COMMAND
            ${BOOST_TOOLSET_CONFIGURATION}
        BUILD_COMMAND
            ${BOOST_BUILD} install ${BOOST_BUILD_PARAMS}
        COMMAND
            ${BOOST_FIXUP_DYLIBS}
        INSTALL_COMMAND ""
        INSTALL_DIR ${BOOST_DIRECTORY}
        EXCLUDE_FROM_ALL TRUE)

    add_dependencies(external boost)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BOOST_LIBRARY_TAGS "-vc143-mt-gd-x64-1_86")
    else()
        set(BOOST_LIBRARY_TAGS "-vc143-mt-x64-1_86")
    endif()
endif()

if (NOT TARGET Boost::container)
    add_library(Boost::container STATIC IMPORTED)
    set(BOOST_CONTAINER_IMPORT_LOCATION ${BOOST_DIRECTORY}/lib/libboost_container${BOOST_LIBRARY_TAGS}.${STATIC_EXTENSION})
    set_target_properties(Boost::container PROPERTIES IMPORTED_LOCATION ${BOOST_CONTAINER_IMPORT_LOCATION})
endif()

if (NOT TARGET Boost::json)
    add_library(Boost::json STATIC IMPORTED)
    set(BOOST_JSON_IMPORT_LOCATION ${BOOST_DIRECTORY}/lib/libboost_json${BOOST_LIBRARY_TAGS}.${STATIC_EXTENSION})
    set_target_properties(Boost::json PROPERTIES IMPORTED_LOCATION ${BOOST_JSON_IMPORT_LOCATION})
endif()

if (NOT TARGET Boost::program_options)
    add_library(Boost::program_options STATIC IMPORTED)
    set(BOOST_PROGRAM_OPTIONS_IMPORT_LOCATION ${BOOST_DIRECTORY}/lib/libboost_program_options${BOOST_LIBRARY_TAGS}.${STATIC_EXTENSION})
    set_target_properties(Boost::program_options PROPERTIES IMPORTED_LOCATION ${BOOST_PROGRAM_OPTIONS_IMPORT_LOCATION})
endif()

if (NOT TARGET Boost::system)
    add_library(Boost::system STATIC IMPORTED)
    set(BOOST_SYSTEM_IMPORT_LOCATION ${BOOST_DIRECTORY}/lib/libboost_system${BOOST_LIBRARY_TAGS}.${STATIC_EXTENSION})
    set_target_properties(Boost::system PROPERTIES IMPORTED_LOCATION ${BOOST_SYSTEM_IMPORT_LOCATION})
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(Boost_INCLUDE_DIRS ${BOOST_DIRECTORY}/include CACHE FILEPATH "" FORCE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(Boost_INCLUDE_DIRS ${BOOST_DIRECTORY}/include/boost-1_86 CACHE FILEPATH "" FORCE)
endif()

set(Boost_LIBRARIES Boost::json Boost::container Boost::system CACHE FILEPATH "" FORCE)

message(DEBUG "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(DEBUG "Boost_LIBRARIES: ${Boost_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# Googletest Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(GOOGLETEST_VERSION "1.15.2")
set(GOOGLETEST_URL "https://github.com/google/googletest/releases/download/v${GOOGLETEST_VERSION}/googletest-${GOOGLETEST_VERSION}.tar.gz")
set(GOOGLETEST_HASH "SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926")
set(GOOGLETEST_DIRECTORY ${DEPENDENCY_DIRECTORY}/googletest/${GOOGLETEST_VERSION})
set(GOOGLETEST_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/googletest/${GOOGLETEST_VERSION})

find_package(GTest ${GOOGLETEST_VERSION} HINTS ${GOOGLETEST_DIRECTORY} QUIET)
if (NOT GTEST_FOUND AND BUILD_EXTERNAL)
    if (NOT EXISTS ${GOOGLETEST_DOWNLOAD_DIRECTORY})
        message(STATUS "Googletest was not found. Downloading now...")

        FetchContent_Declare(
            googletest
            URL ${GOOGLETEST_URL}
            URL_HASH ${GOOGLETEST_HASH}
            TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
            DOWNLOAD_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY}
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE)

        FetchContent_Populate(googletest)
        
        message(STATUS "Googletest downloaded to ${GOOGLETEST_DIRECTORY}")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(GOOGLETEST_CONFIGURE_PARAMS
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} 
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_INSTALL_PREFIX=${GOOGLETEST_DIRECTORY}
            -DCMAKE_C_VISIBILITY_PRESET=hidden
            -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON)
    
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
            INSTALL_DIR ${GOOGLETEST_DIRECTORY}
            EXCLUDE_FROM_ALL TRUE)

        add_dependencies(external googletest)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        find_program(GOOGLETEST_BUILD msbuild REQUIRED)

        set(GOOGLETEST_CONFIGURE_PARAMS
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} 
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -Dgtest_force_shared_crt=ON)

        ExternalProject_Add(
            googletest-release
            PREFIX ${GOOGLETEST_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY}
            BINARY_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY}/build/release
            BUILD_IN_SOURCE false
            URL ""
            DOWNLOAD_DIR ""
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CMAKE_ARGS ${GOOGLETEST_CONFIGURE_PARAMS}
                -DCMAKE_INSTALL_PREFIX=${GOOGLETEST_DIRECTORY}/release
                -DCMAKE_BUILD_TYPE=Release
                -Dgtest_force_shared_crt=ON
                
                -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
            INSTALL_DIR ${GOOGLETEST_DIRECTORY}/release
            INSTALL_COMMAND 
                ${GOOGLETEST_BUILD}
                ${GOOGLETEST_DOWNLOAD_DIRECTORY}/build/release/INSTALL.vcxproj
                /property:Configuration=Release
            EXCLUDE_FROM_ALL TRUE)

        ExternalProject_Add(
            googletest-debug
            PREFIX ${GOOGLETEST_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY}
            BINARY_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY}/build/debug
            BUILD_IN_SOURCE false
            URL ""
            DOWNLOAD_DIR ""
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CMAKE_ARGS ${GOOGLETEST_CONFIGURE_PARAMS}
                -DCMAKE_INSTALL_PREFIX=${GOOGLETEST_DIRECTORY}/debug
                -DCMAKE_BUILD_TYPE=Debug
                -Dgtest_force_shared_crt=ON
                -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL
            INSTALL_DIR ${GOOGLETEST_DIRECTORY}/debug
            INSTALL_COMMAND 
                ${GOOGLETEST_BUILD}
                ${GOOGLETEST_DOWNLOAD_DIRECTORY}/build/debug/INSTALL.vcxproj
                /property:Configuration=Debug
            EXCLUDE_FROM_ALL TRUE)

        add_dependencies(external googletest-release)
        add_dependencies(external googletest-debug)
    endif()
endif()

if (NOT TARGET GTest::gtest)
    add_library(GTest::gtest STATIC IMPORTED)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(GTEST_IMPORT_LOCATION ${GOOGLETEST_DIRECTORY}/lib/libgtest.${STATIC_EXTENSION})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(GTEST_IMPORT_LOCATION ${GOOGLETEST_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/lib/gtest.${STATIC_EXTENSION})
    endif()

    set_target_properties(GTest::gtest PROPERTIES IMPORTED_LOCATION ${GTEST_IMPORT_LOCATION})
endif()

set(GTEST_INCLUDE_DIRS ${GOOGLETEST_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/include CACHE FILEPATH "" FORCE)
set(GTEST_LIBRARIES GTest::gtest CACHE FILEPATH "" FORCE)

message(DEBUG "GTEST_INCLUDE_DIRS: ${GTEST_INCLUDE_DIRS}")
message(DEBUG "GTEST_LIBRARIES: ${GTEST_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# OpenSSL Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(OPENSSL_VERSION "3.3.2")
set(OPENSSL_URL "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz")
set(OPENSSL_HASH "SHA256=2e8a40b01979afe8be0bbfb3de5dc1c6709fedb46d6c89c10da114ab5fc3d281")
set(OPENSSL_DIRECTORY ${DEPENDENCY_DIRECTORY}/openssl/${OPENSSL_VERSION}${OPENSSL_PATCH})
set(OPENSSL_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/openssl/${OPENSSL_VERSION}${OPENSSL_PATCH})

if(NOT DEFINED OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR ${OPENSSL_DIRECTORY})
endif()

find_package(OpenSSL ${OPENSSL_VERSION} HINTS ${OPENSSL_DIRECTORY} QUIET)
# I'm still figuring out how to properly use CMake given the constraints of wanting pre-fetch, building if not found, 
# and setting the rpath for the custom builds. 
# Basically, OPENSSL_CUSTOM_BUILD acts a canary such that we can re-add the custom OpenSSL target. Otherwise, CMake 
# won't know what OpenSSL::Crypto is on the second run. The intention of overriding find_package's exports is to make 
# the ExternalProject custom builds act the same as if it was found with find_package.  
if ((NOT OPENSSL_FOUND OR DEFINED CACHE{OPENSSL_CUSTOM_BUILD}) AND BUILD_EXTERNAL)
    if (NOT EXISTS ${OPENSSL_DOWNLOAD_DIRECTORY})
        message(STATUS "OpenSSL was not found. Downloading now...")

        FetchContent_Declare(
            openssl
            URL ${OPENSSL_URL}
            URL_HASH ${OPENSSL_HASH}
            TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
            DOWNLOAD_DIR ${OPENSSL_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${OPENSSL_DOWNLOAD_DIRECTORY}
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE)

        FetchContent_Populate(openssl)
        
        message(STATUS "OpenSSL downloaded to ${OPENSSL_DIRECTORY}")
    endif()

    set(OPENSSL_CONFIGURE_MODULES
        no-engine no-ssl3 no-gost no-deprecated
        no-capieng no-comp no-dtls no-psk no-srp no-dso no-dsa)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(OPENSSL_CONFIGURE "./config")
        find_program(OPENSSL_BUILD make REQUIRED)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set (PERL_POSSIBLE_BIN_PATHS ${PERL_POSSIBLE_BIN_PATHS}
            "C:/Perl/bin" 
            "C:/Strawberry/perl/bin")
        find_program(PERL_EXECUTABLE perl REQUIRED PATHS ${PERL_POSSIBLE_BIN_PATHS})
        set(OPENSSL_CONFIGURE ${PERL_EXECUTABLE} ${OPENSSL_DOWNLOAD_DIRECTORY}/configure VC-WIN64A)
        find_program(OPENSSL_BUILD nmake REQUIRED)
    endif()

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

    set(OPENSSL_CUSTOM_BUILD TRUE CACHE BOOL INTERNAL)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
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
                ${COMPILER_OVERRIDES} ${OPENSSL_KERNEL_BITS}
            COMMAND
                ${OPENSSL_CONFIGURE} ${OPENSSL_CONFIGURE_MODULES}
                --prefix=${OPENSSL_DIRECTORY} --openssldir=${OPENSSL_DIRECTORY} --libdir=lib
            BUILD_COMMAND
                ${OPENSSL_BUILD} ${OPENSSL_MAKE_TARGET}
            INSTALL_COMMAND
                ${OPENSSL_BUILD} ${OPENSSL_INSTALL_TARGET}
            COMMAND
                ${OPENSSL_FIXUP_DYLIBS}
            INSTALL_DIR ${OPENSSL_DIRECTORY}
            EXCLUDE_FROM_ALL TRUE)
        
        add_dependencies(external openssl)    
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        ExternalProject_Add(
            openssl-release
            PREFIX ${OPENSSL_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${OPENSSL_DOWNLOAD_DIRECTORY}
            BINARY_DIR ${OPENSSL_DOWNLOAD_DIRECTORY}/build/release
            BUILD_IN_SOURCE false
            URL ""
            DOWNLOAD_DIR ""
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CONFIGURE_COMMAND
                ${COMPILER_OVERRIDES} ${OPENSSL_KERNEL_BITS}
            COMMAND
                ${OPENSSL_CONFIGURE} ${OPENSSL_CONFIGURE_MODULES} ${OPENSSL_CONFIGURE_PARAMS}
                --prefix=${OPENSSL_DIRECTORY}/release --openssldir=${OPENSSL_DIRECTORY}/release --libdir=lib
            BUILD_COMMAND
                ${OPENSSL_BUILD} ${OPENSSL_MAKE_TARGET}
            INSTALL_COMMAND
                ${OPENSSL_BUILD} ${OPENSSL_INSTALL_TARGET}
            COMMAND
                ${OPENSSL_FIXUP_DYLIBS}
            INSTALL_DIR ${OPENSSL_DIRECTORY}/release
            EXCLUDE_FROM_ALL TRUE)

        ExternalProject_Add(
            openssl-debug
            PREFIX ${OPENSSL_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${OPENSSL_DOWNLOAD_DIRECTORY}
            BINARY_DIR ${OPENSSL_DOWNLOAD_DIRECTORY}/build/debug
            BUILD_IN_SOURCE false
            URL ""
            DOWNLOAD_DIR ""
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CONFIGURE_COMMAND
                ${COMPILER_OVERRIDES} ${OPENSSL_KERNEL_BITS}
            COMMAND
                ${OPENSSL_CONFIGURE} ${OPENSSL_CONFIGURE_MODULES} ${OPENSSL_CONFIGURE_PARAMS}
                --prefix=${OPENSSL_DIRECTORY}/debug --openssldir=${OPENSSL_DIRECTORY}/debug --libdir=lib  --debug
            BUILD_COMMAND
                ${OPENSSL_BUILD} ${OPENSSL_MAKE_TARGET}
            INSTALL_COMMAND
                ${OPENSSL_BUILD} ${OPENSSL_INSTALL_TARGET}
            COMMAND
                ${OPENSSL_FIXUP_DYLIBS}
            INSTALL_DIR ${OPENSSL_DIRECTORY}/debug
            EXCLUDE_FROM_ALL TRUE)
        
        add_dependencies(external openssl-release)
        add_dependencies(external openssl-debug)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(OPENSSL_LIBRARY_TAGS "3-x64")
endif()

if (NOT TARGET OpenSSL::Crypto)
    add_library(OpenSSL::Crypto SHARED IMPORTED)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(OPENSSL_IMPORT_LOCATION ${OPENSSL_DIRECTORY}/lib/libcrypto.${SHARED_EXTENSION})
        set(OPENSSL_IMPORT_IMPLIB ${OPENSSL_DIRECTORY}/lib/libcrypto.${STATIC_EXTENSION})

        # set(OPENSSL_CRYPTO_LIBRARY "-Wl,-rpath,${OPENSSL_DIRECTORY}/lib/" OpenSSL::Crypto CACHE FILEPATH "" FORCE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(OPENSSL_IMPORT_LOCATION ${OPENSSL_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/bin/libcrypto-${OPENSSL_LIBRARY_TAGS}.${SHARED_EXTENSION})
        set(OPENSSL_IMPORT_IMPLIB ${OPENSSL_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/lib/libcrypto.${STATIC_EXTENSION})

        # set(OPENSSL_CRYPTO_LIBRARY OpenSSL::Crypto CACHE FILEPATH "" FORCE)
    endif()

    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION ${OPENSSL_IMPORT_LOCATION} IMPORTED_IMPLIB ${OPENSSL_IMPORT_IMPLIB})
endif()

set(OPENSSL_INCLUDE_DIR ${OPENSSL_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/include CACHE FILEPATH "" FORCE)
set(OPENSSL_CRYPTO_LIBRARY OpenSSL::Crypto CACHE FILEPATH "" FORCE)

message(DEBUG "OPENSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR}")
message(DEBUG "OPENSSL_CRYPTO_LIBRARY: ${OPENSSL_CRYPTO_LIBRARY}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# OQS Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(OQS_VERSION "0.11.0")
set(OQS_URL "https://github.com/open-quantum-safe/liboqs/archive/refs/tags/${OQS_VERSION}.tar.gz")
set(OQS_HASH "SHA256=f77b3eff7dcd77c84a7cd4663ef9636c5c870f30fd0a5b432ad72f7b9516b199")
set(OQS_DIRECTORY ${DEPENDENCY_DIRECTORY}/liboqs/${OQS_VERSION})
set(OQS_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/liboqs/${OQS_VERSION})

if (NOT EXISTS ${OQS_DOWNLOAD_DIRECTORY} AND BUILD_EXTERNAL)
    set(OQS_BUILD_REQUIRED true)

    message(STATUS "Open Quantum Safe (liboqs) was not found. Downloading now...")

    FetchContent_Declare(
        liboqs
        URL ${OQS_URL}
        URL_HASH ${OQS_HASH}
        TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
        DOWNLOAD_DIR ${OQS_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${OQS_DOWNLOAD_DIRECTORY}
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE)

    FetchContent_Populate(liboqs)
    
    message(STATUS "Open Quantum Safe (liboqs) downloaded to ${DEPENDENCY_DOWNLOAD_DIRECTORY}")
endif()

if (NOT EXISTS ${OQS_DIRECTORY}/lib/liboqs.${SHARED_EXTENSION} AND BUILD_EXTERNAL)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(OQS_CONFIGURE_GENERATOR "Ninja")
        set(OQS_CONFIGURE_PARAMS
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DBUILD_SHARED_LIBS=ON
            -DOQS_BUILD_ONLY_LIB=ON
            -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
            -DOQS_USE_SHA3_OPENSSL=ON
            -DCMAKE_C_VISIBILITY_PRESET=hidden
            -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON
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
            INSTALL_DIR ${OQS_DIRECTORY}
            DEPENDS openssl # Ensures this project is built after openssl
            EXCLUDE_FROM_ALL TRUE)

        add_dependencies(external liboqs)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(OQS_CONFIGURE_PARAMS
            -DBUILD_SHARED_LIBS=ON
            -DOQS_BUILD_ONLY_LIB=ON
            -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR})
        find_program(OQS_BUILD msbuild REQUIRED)

        ExternalProject_Add(
            liboqs-release
            PREFIX ${OQS_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${OQS_DOWNLOAD_DIRECTORY}
            BINARY_DIR ${OQS_DOWNLOAD_DIRECTORY}/build/release
            URL ""
            DOWNLOAD_DIR ""
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CMAKE_GENERATOR "Visual Studio 17 2022"
            CMAKE_ARGS 
                ${OQS_CONFIGURE_PARAMS}
                -DCMAKE_INSTALL_PREFIX=${OQS_DIRECTORY}/release
                -DCMAKE_BUILD_TYPE=Release
                -DOQS_DEBUG_BUILD=OFF
            BUILD_COMMAND 
                ${OQS_BUILD}
                ${OQS_DOWNLOAD_DIRECTORY}/build/release/ALL_BUILD.vcxproj
                /property:Configuration=Release
            INSTALL_DIR ${OQS_DIRECTORY}/release
            INSTALL_COMMAND 
                ${OQS_BUILD}
                ${OQS_DOWNLOAD_DIRECTORY}/build/release/INSTALL.vcxproj
                /property:Configuration=Release
            DEPENDS openssl-release # Ensures this project is built after openssl
            EXCLUDE_FROM_ALL TRUE)

        ExternalProject_Add(
            liboqs-debug
            PREFIX ${OQS_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${OQS_DOWNLOAD_DIRECTORY}
            BINARY_DIR ${OQS_DOWNLOAD_DIRECTORY}/build/debug
            URL ""
            DOWNLOAD_DIR ""
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CMAKE_GENERATOR "Visual Studio 17 2022"
            CMAKE_ARGS 
                ${OQS_CONFIGURE_PARAMS}
                -DCMAKE_INSTALL_PREFIX=${OQS_DIRECTORY}/debug
                -DCMAKE_BUILD_TYPE=Debug
                -DOQS_DEBUG_BUILD=ON
            BUILD_COMMAND
                ${OQS_BUILD}
                ${OQS_DOWNLOAD_DIRECTORY}/build/debug/ALL_BUILD.vcxproj
                /property:Configuration=Debug
            INSTALL_DIR ${OQS_DIRECTORY}/debug
            INSTALL_COMMAND 
                ${OQS_BUILD}
                ${OQS_DOWNLOAD_DIRECTORY}/build/debug/INSTALL.vcxproj
                /property:Configuration=Debug
            DEPENDS openssl-debug # Ensures this project is built after openssl
            EXCLUDE_FROM_ALL TRUE)

        add_dependencies(external liboqs-release)
        add_dependencies(external liboqs-debug)
    endif()
endif()

if (NOT TARGET OQS::oqs)
    add_library(OQS::oqs SHARED IMPORTED)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(OQS_IMPORT_LOCATION ${OQS_DIRECTORY}/lib/liboqs.${SHARED_EXTENSION})
        set(OQS_IMPORT_IMPLIB ${OQS_DIRECTORY}/lib/liboqs.${STATIC_EXTENSION})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(OQS_IMPORT_LOCATION ${OQS_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/bin/oqs.${SHARED_EXTENSION})
        set(OQS_IMPORT_IMPLIB ${OQS_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/lib/oqs.${STATIC_EXTENSION})
    endif()

    set_target_properties(OQS::oqs PROPERTIES
        IMPORTED_LOCATION ${OQS_IMPORT_LOCATION} IMPORTED_IMPLIB ${OQS_IMPORT_IMPLIB})
endif()

set(OQS_INCLUDE_DIRS ${OQS_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/include CACHE FILEPATH "" FORCE)
set(OQS_LIBRARIES OQS::oqs CACHE FILEPATH "" FORCE)

message(DEBUG "OQS_INCLUDE_DIRS: ${OQS_INCLUDE_DIRS}")
message(DEBUG "OQS_LIBRARIES: ${OQS_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# OQS CPP Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(OQSCPP_VERSION "0.10.0")
set(OQSCPP_URL "https://github.com/open-quantum-safe/liboqs-cpp/archive/${OQSCPP_VERSION}.tar.gz")
set(OQSCPP_HASH "SHA256=076d8f6fd18afb2db66fea1d1f237d560ee8cb87715b88397862183f2275f0d5")
set(OQSCPP_DIRECTORY ${DEPENDENCY_DIRECTORY}/liboqs-cpp/${OQSCPP_VERSION})
set(OQSCPP_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/liboqs-cpp/${OQSCPP_VERSION})

if (NOT EXISTS ${OQSCPP_DIRECTORY} AND BUILD_EXTERNAL)
    message(STATUS "Open Quantum Safe (liboqs-cpp) was not found. Downloading now...")

    FetchContent_Declare(
        liboqs-cpp
        URL ${OQSCPP_URL}
        URL_HASH ${OQSCPP_HASH}
        TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
        DOWNLOAD_DIR ${OQSCPP_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${OQSCPP_DOWNLOAD_DIRECTORY}
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE)

    FetchContent_Populate(liboqs-cpp)

    file(COPY ${OQSCPP_DOWNLOAD_DIRECTORY}/include/ DESTINATION ${OQSCPP_DIRECTORY}/include/oqscpp)
    
    message(STATUS "Open Quantum Safe (liboqs-cpp) downloaded to ${OQSCPP_DIRECTORY}")
endif()

set(OQSCPP_INCLUDE_DIRS ${OQSCPP_DIRECTORY}/include CACHE FILEPATH "" FORCE)

message(DEBUG "OQSCPP_INCLUDE_DIRS: ${OQSCPP_INCLUDE_DIRS}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# spdlog Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(SPDLOG_VERSION "1.14.1")
set(SPDLOG_URL "https://github.com/gabime/spdlog/archive/refs/tags/v${SPDLOG_VERSION}.tar.gz")
set(SPDLOG_HASH "SHA256=1586508029a7d0670dfcb2d97575dcdc242d3868a259742b69f100801ab4e16b")
set(SPDLOG_DIRECTORY ${DEPENDENCY_DIRECTORY}/spdlog/${SPDLOG_VERSION})
set(SPDLOG_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/spdlog/${SPDLOG_VERSION})

if (NOT EXISTS ${SPDLOG_DOWNLOAD_DIRECTORY} AND BUILD_EXTERNAL)
    message(STATUS "spdlog was not found. Downloading now...")

    FetchContent_Declare(
        spdlog
        URL ${SPDLOG_URL}
        URL_HASH ${SPDLOG_HASH}
        TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
        DOWNLOAD_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE)

    FetchContent_Populate(spdlog)
    
    message(STATUS "spdlog downloaded to ${SPDLOG_DIRECTORY}")
endif()
    
if (NOT EXISTS ${SPDLOG_DIRECTORY}/lib/libspdlog.${SHARED_EXTENSION} AND BUILD_EXTERNAL)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(SPDLOG_CONFIGURE_PARAMS
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_INSTALL_PREFIX=${SPDLOG_DIRECTORY}
            -DSPDLOG_BUILD_SHARED=OFF
            -DSPDLOG_BUILD_EXAMPLE=OFF
            -DSPDLOG_BUILD_EXAMPLE_HO=OFF
            -DSPDLOG_BUILD_TESTS=OFF
            -DCMAKE_CXX_VISIBILITY_PRESET=hidden
            -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON)
    
        ExternalProject_Add(
            spdlog
            PREFIX ${SPDLOG_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}
            BINARY_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}/build
            BUILD_IN_SOURCE false
            URL ""
            DOWNLOAD_DIR ""
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CMAKE_ARGS ${SPDLOG_CONFIGURE_PARAMS} -DCMAKE_BUILD_TYPE=Release
            INSTALL_DIR ${SPDLOG_DIRECTORY}
            EXCLUDE_FROM_ALL TRUE)

        add_dependencies(external spdlog)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        find_program(SPDLOG_BUILD msbuild REQUIRED)

        set(SPDLOG_CONFIGURE_PARAMS
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DSPDLOG_BUILD_SHARED=OFF
            -DSPDLOG_WCHAR_SUPPORT=ON
            -DSPDLOG_WCHAR_FILENAMES=ON
            -DSPDLOG_BUILD_EXAMPLE=OFF
            -DSPDLOG_BUILD_EXAMPLE_HO=OFF
            -DSPDLOG_BUILD_TESTS=OFF)
    
        ExternalProject_Add(
            spdlog-release
            PREFIX ${SPDLOG_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}
            BINARY_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}/build/release
            BUILD_IN_SOURCE false
            URL ""
            DOWNLOAD_DIR ""
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CMAKE_ARGS ${SPDLOG_CONFIGURE_PARAMS}
                -DCMAKE_INSTALL_PREFIX=${SPDLOG_DIRECTORY}/release
                -DCMAKE_BUILD_TYPE=Release
            INSTALL_DIR ${SPDLOG_DIRECTORY}/release
            INSTALL_COMMAND 
                ${SPDLOG_BUILD}
                ${SPDLOG_DOWNLOAD_DIRECTORY}/build/release/INSTALL.vcxproj
                /property:Configuration=Release
            EXCLUDE_FROM_ALL TRUE)

        ExternalProject_Add(
            spdlog-debug
            PREFIX ${SPDLOG_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}
            BINARY_DIR ${SPDLOG_DOWNLOAD_DIRECTORY}/build/debug
            BUILD_IN_SOURCE false
            URL ""
            DOWNLOAD_DIR ""
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CMAKE_ARGS ${SPDLOG_CONFIGURE_PARAMS}
            ${CMAKE_INSTALL_LIBDIR}
                -DCMAKE_INSTALL_PREFIX=${SPDLOG_DIRECTORY}/debug
                -DCMAKE_BUILD_TYPE=Debug
            INSTALL_DIR ${SPDLOG_DIRECTORY}/debug
            INSTALL_COMMAND 
                ${SPDLOG_BUILD}
                ${SPDLOG_DOWNLOAD_DIRECTORY}/build/debug/INSTALL.vcxproj
                /property:Configuration=Debug
            EXCLUDE_FROM_ALL TRUE)

        add_dependencies(external spdlog-release)
        add_dependencies(external spdlog-debug)
    endif()
endif()

if (NOT TARGET Spdlog::spdlog)
    add_library(Spdlog::spdlog STATIC IMPORTED)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(spdlog_IMPORT_LOCATION ${SPDLOG_DIRECTORY}/lib/libspdlog.${STATIC_EXTENSION})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(spdlog_IMPORT_LOCATION ${SPDLOG_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/lib/spdlog${LIBRARY_BUILD_SUFFIX}.${STATIC_EXTENSION})
    endif()

    set_target_properties(Spdlog::spdlog PROPERTIES IMPORTED_LOCATION ${spdlog_IMPORT_LOCATION})
endif()

set(spdlog_INCLUDE_DIRS ${SPDLOG_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/include CACHE FILEPATH "" FORCE)
set(spdlog_LIBRARIES Spdlog::spdlog CACHE FILEPATH "" FORCE)

message(DEBUG "spdlog_INCLUDE_DIRS: ${spdlog_INCLUDE_DIRS}")
message(DEBUG "spdlog_LIBRARIES: ${spdlog_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# A function to copy the project dependencies to the build folder. 
#-----------------------------------------------------------------------------------------------------------------------
function(copy_dependencies target_name)
    file(GLOB_RECURSE dependency_files
        "${OPENSSL_IMPORT_LOCATION}"
        "${OQS_IMPORT_LOCATION}"
        "${SPDLOG_IMPORT_LOCATION}")

    foreach(dependency_file ${dependency_files})
        set(OUTPUT_FILEDIR $<TARGET_FILE_DIR:${target_name}>)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${dependency_file}
                $<TARGET_FILE_DIR:${target_name}>)
    endforeach(dependency_file)
endfunction()

#-----------------------------------------------------------------------------------------------------------------------
