#-----------------------------------------------------------------------------------------------------------------------

include(ExternalProject)
include(FetchContent)

if(BUILD_EXTERNAL)
    add_custom_target(external)
endif()

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

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(COMPILER_OVERRIDES CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER})
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_C_COMPILER ${CMAKE_CXX_COMPILER})
endif()

#-----------------------------------------------------------------------------------------------------------------------
# Boost Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(BOOST_VERSION "1.79.0")
set(BOOST_DOWNLOAD_TARGET "boost_1_79_0.tar.gz")
set(BOOST_URL "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/${BOOST_DOWNLOAD_TARGET}")
set(BOOST_HASH "SHA256=273f1be93238a068aba4f9735a4a2b003019af067b9c183ed227780b8f36062c")
set(BOOST_DIRECTORY ${DEPENDENCY_DIRECTORY}/boost/${BOOST_VERSION})
set(BOOST_DOWNLOAD_DIRECTORY ${DEPENDENCY_DOWNLOAD_DIRECTORY}/boost/${BOOST_VERSION})

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_RELEASE_LIBS ON)
set(Boost_USE_MULTITHREADED ON)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options system HINTS ${BOOST_DIRECTORY} QUIET)
if (NOT Boost_FOUND AND BUILD_EXTERNAL)
    if (NOT EXISTS ${BOOST_DOWNLOAD_DIRECTORY})
        message(STATUS "Boost was not found. Downloading now...")

        FetchContent_Declare(
            boost
            URL ${BOOST_URL}
            URL_HASH ${BOOST_HASH}
            TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
            DOWNLOAD_DIR ${BOOST_DOWNLOAD_DIRECTORY}
            SOURCE_DIR ${BOOST_DOWNLOAD_DIRECTORY})

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
        --with-libraries=chrono,program_options,system,timer)

    set(BOOST_BUILD "./b2")
    set(BOOST_BUILD_PARAMS
        --prefix=${BOOST_DIRECTORY}
        --libdir=${BOOST_DIRECTORY}/lib
        --build-type=minimal
        --no-cmake-config
        --with-chrono
        --with-program_options
        --with-system
        --with-timer
        toolset=${BOOST_TOOLSET}
        threading=multi
        link=shared
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

if (NOT TARGET Boost::program_options)
    add_library(Boost::program_options SHARED IMPORTED)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(Boost_INCLUDE_DIRS ${BOOST_DIRECTORY}/include CACHE FILEPATH "" FORCE)

        set_target_properties(Boost::program_options PROPERTIES
            IMPORTED_LOCATION ${BOOST_DIRECTORY}/lib/libboost_program_options.${SHARED_EXTENSION}
            IMPORTED_IMPLIB ${BOOST_DIRECTORY}/lib/libboost_program_options.${STATIC_EXTENSION})

        set(Boost_LIBRARIES "-Wl,-rpath,${BOOST_DIRECTORY}/lib/"
            Boost::program_options Boost::system CACHE FILEPATH "" FORCE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(Boost_INCLUDE_DIRS ${BOOST_DIRECTORY}/include/boost-1_79 CACHE FILEPATH "" FORCE)

        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set_target_properties(Boost::program_options PROPERTIES
                IMPORTED_LOCATION ${BOOST_DIRECTORY}/lib/boost_program_options-vc143-mt-gd-x64-1_79.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${BOOST_DIRECTORY}/lib/boost_program_options-vc143-mt-gd-x64-1_79.${STATIC_EXTENSION})
        else()
            set_target_properties(Boost::program_options PROPERTIES
                IMPORTED_LOCATION ${BOOST_DIRECTORY}/lib/boost_program_options-vc143-mt-x64-1_79.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${BOOST_DIRECTORY}/lib/boost_program_options-vc143-mt-x64-1_79.${STATIC_EXTENSION})
        endif()
    endif()
endif()

if (NOT TARGET Boost::system)
    add_library(Boost::system SHARED IMPORTED)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(Boost_INCLUDE_DIRS ${BOOST_DIRECTORY}/include CACHE FILEPATH "" FORCE)

        set_target_properties(Boost::system PROPERTIES
            IMPORTED_LOCATION ${BOOST_DIRECTORY}/lib/libboost_system.${SHARED_EXTENSION}
            IMPORTED_IMPLIB ${BOOST_DIRECTORY}/lib/libboost_system.${STATIC_EXTENSION})

        set(Boost_LIBRARIES "-Wl,-rpath,${BOOST_DIRECTORY}/lib/"
            Boost::system Boost::system CACHE FILEPATH "" FORCE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(Boost_INCLUDE_DIRS ${BOOST_DIRECTORY}/include/boost-1_79 CACHE FILEPATH "" FORCE)

        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set_target_properties(Boost::system PROPERTIES
                IMPORTED_LOCATION ${BOOST_DIRECTORY}/lib/boost_system-vc143-mt-gd-x64-1_79.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${BOOST_DIRECTORY}/lib/boost_system-vc143-mt-gd-x64-1_79.${STATIC_EXTENSION})
        else()
            set_target_properties(Boost::system PROPERTIES
                IMPORTED_LOCATION ${BOOST_DIRECTORY}/lib/boost_system-vc143-mt-x64-1_79.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${BOOST_DIRECTORY}/lib/boost_system-vc143-mt-x64-1_79.${STATIC_EXTENSION})
        endif()
    endif()
endif()


set(Boost_LIBRARIES Boost::system CACHE FILEPATH "" FORCE)

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
            SOURCE_DIR ${GOOGLETEST_DOWNLOAD_DIRECTORY})

        FetchContent_Populate(googletest)
        
        message(STATUS "Googletest downloaded to ${GOOGLETEST_DIRECTORY}")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
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
        set(GTEST_INCLUDE_DIRS ${GOOGLETEST_DIRECTORY}/include CACHE FILEPATH "" FORCE)

        set_target_properties(GTest::gtest PROPERTIES
            IMPORTED_LOCATION ${GOOGLETEST_DIRECTORY}/lib/libgtest.${STATIC_EXTENSION})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(GTEST_INCLUDE_DIRS ${GOOGLETEST_DIRECTORY}/debug/include CACHE FILEPATH "" FORCE)

            set_target_properties(GTest::gtest PROPERTIES
                IMPORTED_LOCATION ${GOOGLETEST_DIRECTORY}/debug/lib/gtestd.${STATIC_EXTENSION})
        else()
            set(GTEST_INCLUDE_DIRS ${GOOGLETEST_DIRECTORY}/release/include CACHE FILEPATH "" FORCE)

            set_target_properties(GTest::gtest PROPERTIES
                IMPORTED_LOCATION ${GOOGLETEST_DIRECTORY}/release/lib/gtest.${STATIC_EXTENSION})
        endif()
    endif()
endif()

set(GTEST_LIBRARIES GTest::gtest CACHE FILEPATH "" FORCE)

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

if (NOT EXISTS "${LITHIUM_JSON_DIRECTORY}/${LITHIUM_JSON_FILE}" AND BUILD_EXTERNAL)
    message(STATUS "Lithium JSON was not found. Downloading now...")

    FetchContent_Declare(
        lithium_json
        URL ${LITHIUM_JSON_URL}
        TMP_DIR ${DEPENDENCY_TEMP_DIRECTORY}
        DOWNLOAD_DIR ${LITHIUM_JSON_DOWNLOAD_DIRECTORY}
        SOURCE_DIR ${LITHIUM_JSON_DIRECTORY}
        DOWNLOAD_NO_EXTRACT true)

    FetchContent_Populate(lithium_json)

    message(STATUS "Lithium JSON downloaded to ${LITHIUM_JSON_DIRECTORY}")
endif()

# Always set the Lithium JSON include directory given it is not findable. 
set(LITHIUM_JSON_INCLUDE_DIR ${LITHIUM_JSON_DIRECTORY} CACHE FILEPATH "" FORCE)
message(DEBUG "LITHIUM_JSON_INCLUDE_DIR: ${LITHIUM_JSON_INCLUDE_DIR}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# OpenSSL Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(OPENSSL_VERSION "3.0.3")
set(OPENSSL_URL "https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz")
set(OPENSSL_HASH "SHA256=ee0078adcef1de5f003c62c80cc96527721609c6f3bb42b7795df31f8b558c0b")
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
            SOURCE_DIR ${OPENSSL_DOWNLOAD_DIRECTORY})

        FetchContent_Populate(openssl)
        
        message(STATUS "OpenSSL downloaded to ${OPENSSL_DIRECTORY}")
    endif()

    set(OPENSSL_CONFIGURE_MODULES
        no-cast no-md2 no-md4 no-mdc2 no-rc4 no-rc5 no-engine no-idea no-mdc2 no-rc5
        no-camellia no-ssl3 no-heartbeats no-gost no-deprecated no-capieng no-comp no-dtls
        no-psk no-srp no-dso no-dsa no-rc2 no-des)

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

if (NOT TARGET OpenSSL::Crypto)
    add_library(OpenSSL::Crypto SHARED IMPORTED)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(OPENSSL_INCLUDE_DIR ${OPENSSL_DIRECTORY}/include CACHE FILEPATH "" FORCE)

        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION ${OPENSSL_DIRECTORY}/lib/libcrypto.${SHARED_EXTENSION}
            IMPORTED_IMPLIB ${OPENSSL_DIRECTORY}/lib/libcrypto.${STATIC_EXTENSION})
        set(OPENSSL_CRYPTO_LIBRARY "-Wl,-rpath,${OPENSSL_DIRECTORY}/lib/" OpenSSL::Crypto CACHE FILEPATH "" FORCE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(OPENSSL_INCLUDE_DIR ${OPENSSL_DIRECTORY}/debug/include CACHE FILEPATH "" FORCE)

            set_target_properties(OpenSSL::Crypto PROPERTIES
                IMPORTED_LOCATION ${OPENSSL_DIRECTORY}/debug/bin/libcrypto-1_1-x64.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${OPENSSL_DIRECTORY}/debug/lib/libcrypto.${STATIC_EXTENSION})
        else()
            set(OPENSSL_INCLUDE_DIR ${OPENSSL_DIRECTORY}/release/include CACHE FILEPATH "" FORCE)

            set_target_properties(OpenSSL::Crypto PROPERTIES
                IMPORTED_LOCATION ${OPENSSL_DIRECTORY}/release/bin/libcrypto-1_1-x64.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${OPENSSL_DIRECTORY}/release/lib/libcrypto.${STATIC_EXTENSION})
        endif()
    endif()
endif()

set(OPENSSL_CRYPTO_LIBRARY OpenSSL::Crypto CACHE FILEPATH "" FORCE)

message(DEBUG "OPENSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR}")
message(DEBUG "OPENSSL_CRYPTO_LIBRARY: ${OPENSSL_CRYPTO_LIBRARY}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# OQS Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(OQS_VERSION "0.7.1")
set(OQS_URL "https://github.com/open-quantum-safe/liboqs/archive/${OQS_VERSION}.tar.gz")
set(OQS_HASH "SHA256=c8a1ffcfd4facc90916557c0efae9a28c46e803b088d0cb32ee7b0b010555d3a")
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
        SOURCE_DIR ${OQS_DOWNLOAD_DIRECTORY})

    FetchContent_Populate(liboqs)
    
    message(STATUS "Open Quantum Safe (liboqs) downloaded to ${DEPENDENCY_DOWNLOAD_DIRECTORY}")
endif()

if ((NOT EXISTS ${OQS_DIRECTORY}/lib/liboqs.${SHARED_EXTENSION} AND NOT DEFINED CACHE{OQS_LIBRARIES}) AND BUILD_EXTERNAL)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(OQS_CONFIGURE_GENERATOR "Ninja")
        set(OQS_CONFIGURE_PARAMS
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DBUILD_SHARED_LIBS=ON
            -DOQS_BUILD_ONLY_LIB=ON
            -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
            -DOQS_USE_SHA3_OPENSSL=ON
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
        set(OQS_INCLUDE_DIRS ${OQS_DIRECTORY}/include CACHE FILEPATH "" FORCE)

        set_target_properties(OQS::oqs PROPERTIES
            IMPORTED_LOCATION ${OQS_DIRECTORY}/lib/liboqs.${SHARED_EXTENSION}
            IMPORTED_IMPLIB ${OQS_DIRECTORY}/lib/liboqs.${STATIC_EXTENSION})
        set(OQS_LIBRARIES "-Wl,-rpath,${OQS_DIRECTORY}/lib/" OQS::oqs CACHE FILEPATH "" FORCE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(OQS_INCLUDE_DIRS ${OQS_DIRECTORY}/debug/include CACHE FILEPATH "" FORCE)

            set_target_properties(OQS::oqs PROPERTIES
                IMPORTED_LOCATION ${OQS_DIRECTORY}/debug/bin/oqs.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${OQS_DIRECTORY}/debug/lib/oqs.${STATIC_EXTENSION})
        else()
            set(OQS_INCLUDE_DIRS ${OQS_DIRECTORY}/release/include CACHE FILEPATH "" FORCE)

            set_target_properties(OQS::oqs PROPERTIES
                IMPORTED_LOCATION ${OQS_DIRECTORY}/release/bin/oqs.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${OQS_DIRECTORY}/release/lib/oqs.${STATIC_EXTENSION})
        endif()
    endif()
endif()

set(OQS_LIBRARIES OQS::oqs CACHE FILEPATH "" FORCE)

message(DEBUG "OQS_INCLUDE_DIRS: ${OQS_INCLUDE_DIRS}")
message(DEBUG "OQS_LIBRARIES: ${OQS_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# OQS CPP Dependency
#-----------------------------------------------------------------------------------------------------------------------
set(OQSCPP_VERSION "0.7.1")
set(OQSCPP_URL "https://github.com/open-quantum-safe/liboqs-cpp/archive/${OQSCPP_VERSION}.tar.gz")
set(OQSCPP_HASH "SHA256=86ea3fbeec2fd69639065cc1b537fbe5c156bc0adb9fde7e0e3dc4b5c92b8e12")
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
        SOURCE_DIR ${OQSCPP_DOWNLOAD_DIRECTORY})

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
set(SPDLOG_VERSION "1.10.0")
set(SPDLOG_URL "https://github.com/gabime/spdlog/archive/v${SPDLOG_VERSION}.tar.gz")
set(SPDLOG_HASH "SHA256=697f91700237dbae2326b90469be32b876b2b44888302afbc7aceb68bcfe8224")
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
        SOURCE_DIR ${SPDLOG_DOWNLOAD_DIRECTORY})

    FetchContent_Populate(spdlog)
    
    message(STATUS "spdlog downloaded to ${SPDLOG_DIRECTORY}")
endif()
    
if ((NOT EXISTS ${SPDLOG_DIRECTORY}/lib/libspdlog.${SHARED_EXTENSION} AND NOT DEFINED CACHE{spdlog_LIBRARIES}) AND BUILD_EXTERNAL)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(SPDLOG_CONFIGURE_PARAMS
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_INSTALL_PREFIX=${SPDLOG_DIRECTORY}
            -DSPDLOG_BUILD_SHARED=ON)
    
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
            -DSPDLOG_BUILD_SHARED=ON
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
    add_library(Spdlog::spdlog SHARED IMPORTED)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(spdlog_INCLUDE_DIRS ${SPDLOG_DIRECTORY}/include CACHE FILEPATH "" FORCE)

        set_target_properties(Spdlog::spdlog PROPERTIES
            IMPORTED_LOCATION ${SPDLOG_DIRECTORY}/lib/libspdlog.${SHARED_EXTENSION}
            IMPORTED_IMPLIB ${SPDLOG_DIRECTORY}/lib/libspdlog.${STATIC_EXTENSION})
        set(spdlog_LIBRARIES "-Wl,-rpath,${SPDLOG_DIRECTORY}/lib/" Spdlog::spdlog CACHE FILEPATH "" FORCE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(spdlog_INCLUDE_DIRS ${SPDLOG_DIRECTORY}/debug/include CACHE FILEPATH "" FORCE)

            set_target_properties(Spdlog::spdlog PROPERTIES
                IMPORTED_LOCATION ${SPDLOG_DIRECTORY}/debug/bin/spdlogd.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${SPDLOG_DIRECTORY}/debug/lib/spdlogd.${STATIC_EXTENSION})
        else()
            set(spdlog_INCLUDE_DIRS ${SPDLOG_DIRECTORY}/release/include CACHE FILEPATH "" FORCE)

            set_target_properties(Spdlog::spdlog PROPERTIES
                IMPORTED_LOCATION ${SPDLOG_DIRECTORY}/release/bin/spdlog.${SHARED_EXTENSION}
                IMPORTED_IMPLIB ${SPDLOG_DIRECTORY}/release/lib/spdlog.${STATIC_EXTENSION})
        endif()
    endif()
endif()

set(spdlog_LIBRARIES Spdlog::spdlog CACHE FILEPATH "" FORCE)

message(DEBUG "spdlog_INCLUDE_DIRS: ${spdlog_INCLUDE_DIRS}")
message(DEBUG "spdlog_LIBRARIES: ${spdlog_LIBRARIES}")

#-----------------------------------------------------------------------------------------------------------------------
