#-----------------------------------------------------------------------------------------------------------------------
# Set the C++ Standard properties. 
#-----------------------------------------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_ADDRESS_SANITIZER "Complie with AddressSanitizer and LeakSanitizer. Only applies to Debug builds." OFF)

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# Disallow in-source builds. 
#-----------------------------------------------------------------------------------------------------------------------
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
    "In-source builds not allowed. Please use a designated build directory (e.g. /build).")
endif()

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# Compilier checks. 
#-----------------------------------------------------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.1)
    message(
      FATAL_ERROR
      "GCC 11.1 is required to buld Brypt Node. Found: " ${CMAKE_CXX_COMPILER_VERSION})
  endif()
else()
    message(FATAL_ERROR "The ${CMAKE_CXX_COMPILER_ID} compilier is currently unsupported.")
endif()

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# Build type configuration.  
#-----------------------------------------------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(
      STATUS
      "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build configuration." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Configuring Debug Build...")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(ENABLE_ADDRESS_SANITIZER)
          set(GCC_EXTRA_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
        endif()
        set(CMAKE_CXX_FLAGS_DEBUG "-g3 -ggdb3 -O0 ${GCC_EXTRA_FLAGS}" CACHE STRING "" FORCE)
    endif()
else()
    message(STATUS "Configuring Release Build...")
endif()

#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# Set the install and build paths. 
#-----------------------------------------------------------------------------------------------------------------------
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

#-----------------------------------------------------------------------------------------------------------------------
# Misc.
#-----------------------------------------------------------------------------------------------------------------------
set(CMAKE_COLOR_MAKEFILE ON)

#-----------------------------------------------------------------------------------------------------------------------
