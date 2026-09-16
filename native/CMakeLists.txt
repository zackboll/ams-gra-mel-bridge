cmake_minimum_required(VERSION 3.20)
project(ams_mel_c VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(AMS_MEL_BUILD_TESTS "Build native facade and mock-provider tests" OFF)

add_library(ams_mel_c SHARED src/abi.cpp src/provider.cpp)
add_library(ams_mel::ams_mel_c ALIAS ams_mel_c)
target_compile_features(ams_mel_c PRIVATE cxx_std_20)
target_compile_definitions(ams_mel_c PRIVATE AMS_MEL_BUILDING_LIBRARY)
target_include_directories(ams_mel_c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/common-mel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ir-mel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ams-math/include)
target_link_libraries(ams_mel_c PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(ams_mel_c PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    VERSION ${PROJECT_VERSION}
    SOVERSION 0)
# The external GPR import names this directory; GPR never compiles this library.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/obj")

function(ams_mel_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
ams_mel_warnings(ams_mel_c)

if(AMS_MEL_BUILD_TESTS)
    enable_testing()
    add_library(mock_ir_provider SHARED tests/mock_provider.cpp)
    target_compile_features(mock_ir_provider PRIVATE cxx_std_20)
    target_include_directories(mock_ir_provider PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/common-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ir-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ams-math/include)
    set_target_properties(mock_ir_provider PROPERTIES
        CXX_EXTENSIONS OFF CXX_VISIBILITY_PRESET hidden
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-providers")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(mock_ir_provider PRIVATE -fno-gnu-unique)
    endif()
    ams_mel_warnings(mock_ir_provider)

    add_library(missing_symbol_ir_provider SHARED
        tests/missing_symbol_provider.cpp)
    target_compile_features(missing_symbol_ir_provider PRIVATE cxx_std_20)
    target_include_directories(missing_symbol_ir_provider PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/common-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ir-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ams-math/include)
    set_target_properties(missing_symbol_ir_provider PROPERTIES
        CXX_EXTENSIONS OFF CXX_VISIBILITY_PRESET hidden
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-providers")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(missing_symbol_ir_provider PRIVATE -fno-gnu-unique)
    endif()
    ams_mel_warnings(missing_symbol_ir_provider)

    add_executable(test_c_abi tests/test_c_abi.c)
    target_compile_features(test_c_abi PRIVATE c_std_11)
    set_target_properties(test_c_abi PROPERTIES C_EXTENSIONS OFF)
    target_link_libraries(test_c_abi PRIVATE ams_mel::ams_mel_c)
    ams_mel_warnings(test_c_abi)
    add_test(NAME c_abi COMMAND test_c_abi)

    add_executable(test_cpp_header tests/test_cpp_header.cpp)
    target_compile_features(test_cpp_header PRIVATE cxx_std_20)
    target_link_libraries(test_cpp_header PRIVATE ams_mel::ams_mel_c)
    ams_mel_warnings(test_cpp_header)
    add_test(NAME cpp_header COMMAND test_cpp_header)

    add_executable(test_provider tests/test_provider.c)
    target_compile_features(test_provider PRIVATE c_std_11)
    set_target_properties(test_provider PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_provider PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>"
        AMS_MEL_TEST_MISSING_SYMBOL_PROVIDER="$<TARGET_FILE:missing_symbol_ir_provider>"
        AMS_MEL_TEST_LIFETIME_LOG="${CMAKE_CURRENT_BINARY_DIR}/provider-lifetime.log")
    target_link_libraries(test_provider PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_provider mock_ir_provider missing_symbol_ir_provider)
    ams_mel_warnings(test_provider)
    add_test(NAME provider_contract COMMAND test_provider)
    set_tests_properties(provider_contract PROPERTIES
        ENVIRONMENT "AMS_MEL_TEST_LIFETIME_LOG=${CMAKE_CURRENT_BINARY_DIR}/provider-lifetime.log")
endif()

install(TARGETS ams_mel_c EXPORT ams_mel_cTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/ams_mel_c)
foreach(upstream IN ITEMS common-mel ir-mel ams-math)
    install(FILES vendor/${upstream}/LICENSE vendor/${upstream}/INTENT.md
        DESTINATION
        ${CMAKE_INSTALL_DATADIR}/licenses/ams_mel_c/upstream/${upstream})
endforeach()
install(EXPORT ams_mel_cTargets NAMESPACE ams_mel::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ams_mel_c)
configure_package_config_file(cmake/ams_mel_cConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ams_mel_cConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ams_mel_c)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ams_mel_cConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY ExactVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ams_mel_cConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ams_mel_cConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ams_mel_c)
