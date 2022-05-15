#-----------------------------------------------------------------------------------------------------------------------
# A function to copy the project dependencies to the build folder. 
#-----------------------------------------------------------------------------------------------------------------------
function(copy_dependencies target_name)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LIBRARY_BUILD_SUFFIX "d")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(LIBRARY_BUILD_FOLDER "debug")
            set(BOOST_LIBRARY_TAGS "vc143-mt-gd-x64-1_79")
        else()
            set(LIBRARY_BUILD_FOLDER "release")
            set(BOOST_LIBRARY_TAGS "vc143-mt-x64-1_79")
        endif()

         set(OPENSSL_LIBRARY_TAGS "3-x64")

        file(GLOB_RECURSE dependency_files
            "${BOOST_DIRECTORY}/lib/boost_chrono-${BOOST_LIBRARY_TAGS}.${SHARED_EXTENSION}"
            "${BOOST_DIRECTORY}/lib/boost_program_options-${BOOST_LIBRARY_TAGS}.${SHARED_EXTENSION}"
            "${BOOST_DIRECTORY}/lib/boost_system-${BOOST_LIBRARY_TAGS}.${SHARED_EXTENSION}"
            "${BOOST_DIRECTORY}/lib/boost_timer-${BOOST_LIBRARY_TAGS}.${SHARED_EXTENSION}"
            "${OPENSSL_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/bin/libcrypto-${OPENSSL_LIBRARY_TAGS}.${SHARED_EXTENSION}"
            "${OQS_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/bin/oqs.${SHARED_EXTENSION}"
            "${SPDLOG_DIRECTORY}/${LIBRARY_BUILD_FOLDER}/bin/spdlog${LIBRARY_BUILD_SUFFIX}.${SHARED_EXTENSION}")

        foreach(dependency_file ${dependency_files})
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${dependency_file}
                    $<TARGET_FILE_DIR:${target_name}>)
        endforeach(dependency_file)
    endif()
endfunction()

#-----------------------------------------------------------------------------------------------------------------------
