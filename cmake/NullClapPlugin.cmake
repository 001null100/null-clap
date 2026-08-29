include(CMakeParseArguments)

function(nullclap_add_plugin target)
    set(options)
    set(oneValueArgs OUTPUT_NAME)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(NCP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT NCP_SOURCES)
        message(FATAL_ERROR "nullclap_add_plugin(${target}) requires SOURCES")
    endif()

    add_library(${target} MODULE ${NCP_SOURCES})
    target_link_libraries(${target} PRIVATE nullclap::nullclap)
    target_compile_features(${target} PRIVATE cxx_std_20)

    if(NCP_OUTPUT_NAME)
        set(_nullclap_output_name "${NCP_OUTPUT_NAME}")
    else()
        set(_nullclap_output_name "${target}")
    endif()

    set(_nullclap_output_dir "${CMAKE_BINARY_DIR}/clap")
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        SUFFIX ".clap"
        OUTPUT_NAME "${_nullclap_output_name}"
        LIBRARY_OUTPUT_DIRECTORY "${_nullclap_output_dir}"
        RUNTIME_OUTPUT_DIRECTORY "${_nullclap_output_dir}"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )

    foreach(config Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${config}" config_upper)
        set_target_properties(${target} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${_nullclap_output_dir}"
            RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${_nullclap_output_dir}"
        )
    endforeach()

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
