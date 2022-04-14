#-----------------------------------------------------------------------------------------------------------------------
# A function to set the required compiler warnings, features, and system library flags. 
#-----------------------------------------------------------------------------------------------------------------------
function(set_project_options project_name)
    set(GCC_WARNINGS
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

    set(GCC_FEATURES -fcoroutines)
    set(GCC_LINKS -pthread)
    
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(PROJECT_WARNINGS ${GCC_WARNINGS})
        set(PROJECT_FEATURES ${GCC_FEATURES})
        set(PROJECT_LINKS ${GCC_LINKS})
    endif()

    target_compile_options(${project_name} INTERFACE ${PROJECT_WARNINGS})
    target_compile_options(${project_name} INTERFACE ${PROJECT_FEATURES})
    target_link_options(${project_name} INTERFACE ${PROJECT_LINKS})
    target_compile_definitions(${project_name} INTERFACE SPDLOG_COMPILED_LIB)
endfunction()

#-----------------------------------------------------------------------------------------------------------------------
