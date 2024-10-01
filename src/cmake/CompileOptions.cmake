#-----------------------------------------------------------------------------------------------------------------------
# A function to set the required compiler warnings, features, and system library flags. 
#-----------------------------------------------------------------------------------------------------------------------
function(set_project_options project_name)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(PROJECT_WARNINGS
            -Wall
            -Wextra
            -Wpedantic
            -Wextra-semi
            -Wformat-security
            -Wnull-dereference
            -Wduplicated-branches
            -Wduplicated-cond
            -Wconversion
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wuseless-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wdouble-promotion
            -Wlogical-op
            -Wno-non-template-friend
            -Wno-missing-field-initializers
            -Wno-missing-braces
            -Wformat=2)
        
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            set(PROJECT_FEATURES 
                -fdata-sections
                -ffunction-sections
                -Wl,--gc-sections,--stack,8388608)
        endif()

        set(PROJECT_FEATURES 
            -fcoroutines
            -fvisibility=hidden
            -fvisibility-inlines-hidden
            ${EXTRA_PROJECT_FEATURES})

        set(PROJECT_LINKS -pthread)

        set(PROJECT_DEFINITIONS 
            -DSPDLOG_DISABLE_DEFAULT_LOGGER=1
            -DSPDLOG_NO_THREAD_ID=1)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(PROJECT_FEATURES /EHsc /c /Zc:preprocessor /Zc:__cplusplus /await:strict /sdl /w34996)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8388608")
        set(PROJECT_LINKS "/STACK:8388608")

        macro(get_version_tag version)
            string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" ${version} ${CMAKE_SYSTEM_VERSION})
            math(EXPR ${version} "(${CMAKE_MATCH_1} << 8) + ${CMAKE_MATCH_2}" OUTPUT_FORMAT HEXADECIMAL)
        endmacro()
        
        get_version_tag(windows_version_tag)

        set(PROJECT_DEFINITIONS 
            WIN_EXPORT
            -D_WIN32_WINNT=${windows_version_tag}
            -DNOMINMAX
            -DBOOST_ALL_NO_LIB
            -DSPDLOG_DISABLE_DEFAULT_LOGGER
            -DSPDLOG_NO_THREAD_ID
            -DSPDLOG_WCHAR_TO_UTF8_SUPPORT)

        set_target_properties(${project_name} PROPERTIES CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(PROJECT_DEFINITIONS ${PROJECT_DEFINITIONS} -DDEBUG)
    else()
        set(PROJECT_DEFINITIONS ${PROJECT_DEFINITIONS} -DNDEBUG)
    endif()

    target_compile_options(${project_name} INTERFACE ${PROJECT_WARNINGS})
    target_compile_options(${project_name} INTERFACE ${PROJECT_FEATURES})
    target_link_options(${project_name} INTERFACE ${PROJECT_LINKS})
    target_compile_definitions(${project_name} INTERFACE -DSPDLOG_COMPILED_LIB ${PROJECT_DEFINITIONS})
endfunction()

#-----------------------------------------------------------------------------------------------------------------------
