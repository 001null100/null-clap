cmake_minimum_required(VERSION 3.24)
project(null-clap VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
include(GNUInstallDirs)

option(NULLCLAP_BUILD_EXAMPLES "Build null-clap example plugins" ON)
option(NULLCLAP_BUILD_TESTS "Build null-clap unit tests" ON)

set(CLAP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG a47f6badb49d948fd009998f28309cdab78979c9
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(clap)

set(CLAP_HELPERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CLAP_HELPERS_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    clap_helpers
    GIT_REPOSITORY https://github.com/free-audio/clap-helpers.git
    GIT_TAG c35dd4906bd8efbb900cb2b89e680fed463cc8b1
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(clap_helpers)

add_library(nullclap STATIC
    src/ParameterStore.cpp
    src/State.cpp
    src/Plugin.cpp
)
add_library(nullclap::nullclap ALIAS nullclap)

target_compile_features(nullclap PUBLIC cxx_std_20)
target_link_libraries(nullclap PUBLIC clap clap-helpers)
target_include_directories(nullclap
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(nullclap PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(nullclap PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(nullclap PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(cmake/NullClapPlugin.cmake)

if(NULLCLAP_BUILD_EXAMPLES)
    add_subdirectory(examples/MinimalEffect)
endif()

if(NULLCLAP_BUILD_TESTS)
    enable_testing()
    add_executable(nullclap_tests tests/ParameterStoreTests.cpp)
    target_link_libraries(nullclap_tests PRIVATE nullclap::nullclap)
    target_compile_features(nullclap_tests PRIVATE cxx_std_20)
    add_test(NAME nullclap_parameter_store COMMAND nullclap_tests)
endif()

install(TARGETS nullclap EXPORT nullclapTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT nullclapTargets NAMESPACE nullclap:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/null-clap)
