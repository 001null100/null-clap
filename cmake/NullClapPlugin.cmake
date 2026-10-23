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
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )

    # ELF modules export every default-visibility symbol, including code pulled
    # in from static libraries (null-clap, clap-helpers) and bundled C libraries
    # (e.g. JUCE's zlib/libpng). Two plug-ins in one host process would then
    # interpose each other's copies. CLAP only needs clap_entry.
    if(UNIX AND NOT APPLE)
        set(_nullclap_exports_map "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/NullClapExports.map")
        target_link_options(${target} PRIVATE "LINKER:--version-script=${_nullclap_exports_map}")
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${_nullclap_exports_map}")
    endif()

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
