cmake_minimum_required(VERSION 3.20)
project(ams_mel_c VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(AMS_MEL_BUILD_TESTS "Build native facade and mock-provider tests" OFF)
# Scripts use this marker to reset build trees created before runtime-path
# sanitization was introduced.
set(AMS_MEL_SANITIZED_BUILD_ENVIRONMENT ON CACHE BOOL
    "Native build was configured without caller LD_RUN_PATH" FORCE)

add_library(ams_mel_c SHARED src/abi.cpp src/provider.cpp src/ir_stream.cpp src/ir_image.cpp
    src/ir_channel.cpp src/ir_c2.cpp src/common_channel_requests.cpp src/ir_health.cpp src/ir_navigation.cpp
    src/ir_instrumentation.cpp src/ir_track.cpp src/completion_probe.cpp)
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
target_include_directories(ams_mel_c SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-1.83.0)
target_link_libraries(ams_mel_c PRIVATE ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    set(ams_mel_export_map "${CMAKE_CURRENT_SOURCE_DIR}/src/exports.map")
    if(AMS_MEL_BUILD_TESTS)
        # Observation/rescue hooks are private to the contract-test facade.
        # Never extend the production export map or installed C headers.
        file(READ "${ams_mel_export_map}" ams_mel_test_exports)
        string(REPLACE "    global:" "    global:\n        ams_mel_test_admission;\n        ams_mel_test_c2_requests;\n        ams_mel_test_navigation_requests;\n        ams_mel_test_instrumentation_requests;\n        ams_mel_test_track_requests;"
            ams_mel_test_exports "${ams_mel_test_exports}")
        string(REPLACE "    global:" "    global:\n        ams_mel_test_c2_submission;\n        ams_mel_test_completion_snapshot;\n        ams_mel_test_completion_wait;\n        ams_mel_test_completion_owner;\n        ams_mel_test_completion_boundary;\n        ams_mel_test_lost_cleanup_inspect;\n        ams_mel_test_lost_cleanup_rescue;"
            ams_mel_test_exports "${ams_mel_test_exports}")
        set(ams_mel_export_map "${CMAKE_CURRENT_BINARY_DIR}/test-exports.map")
        file(WRITE "${ams_mel_export_map}" "${ams_mel_test_exports}")
    endif()
    target_link_options(ams_mel_c PRIVATE
        "LINKER:--version-script=${ams_mel_export_map}")
    set_property(TARGET ams_mel_c APPEND PROPERTY LINK_DEPENDS "${ams_mel_export_map}")
endif()
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
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    enable_testing()
    target_compile_definitions(ams_mel_c PRIVATE AMS_MEL_ENABLE_TEST_FAILPOINTS)
    add_library(mock_ir_provider SHARED tests/mock_provider.cpp)
    target_compile_features(mock_ir_provider PRIVATE cxx_std_20)
    target_include_directories(mock_ir_provider PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/common-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ir-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ams-math/include)
    # The mock provider derives from TrackChannel, whose published declaration
    # closure includes Boost. Only the vendored pinned Boost is visible.
    target_include_directories(mock_ir_provider SYSTEM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-1.83.0)
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

    # Declaration-only probe: TrackChannel's published closure includes
    # TrackDataUpdate declarations, but this target implements no Track API.
    add_library(track_header_compile_probe OBJECT tests/track_header_compile_probe.cpp)
    target_compile_features(track_header_compile_probe PRIVATE cxx_std_20)
    target_include_directories(track_header_compile_probe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/common-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ir-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ams-math/include)
    target_include_directories(track_header_compile_probe SYSTEM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-1.83.0)
    set_target_properties(track_header_compile_probe PROPERTIES CXX_EXTENSIONS OFF)
    ams_mel_warnings(track_header_compile_probe)
    add_custom_target(check_track_header_boost_closure ALL
        COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_track_boost_closure.py
            --compiler ${CMAKE_CXX_COMPILER}
            --source ${CMAKE_CURRENT_SOURCE_DIR}/tests/track_header_compile_probe.cpp
            --boost-root ${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-1.83.0
            --include ${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-1.83.0
            --include ${CMAKE_CURRENT_SOURCE_DIR}/vendor/common-mel/include
            --include ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ir-mel/include
            --include ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ams-math/include
        DEPENDS track_header_compile_probe
        VERBATIM)
    add_test(NAME c_abi COMMAND test_c_abi)

    find_package(Threads REQUIRED)
    add_executable(test_c2_submission tests/test_c2_submission.c)
    target_compile_features(test_c2_submission PRIVATE c_std_11)
    set_target_properties(test_c2_submission PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_c2_submission PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_c2_submission PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS} Threads::Threads)
    add_dependencies(test_c2_submission mock_ir_provider)
    ams_mel_warnings(test_c2_submission)
    add_test(NAME c2_submission COMMAND test_c2_submission)
    set_tests_properties(c2_submission PROPERTIES TIMEOUT 120)
    add_executable(test_c2_finish_exception tests/test_c2_finish_exception.c)
    target_compile_features(test_c2_finish_exception PRIVATE c_std_11)
    set_target_properties(test_c2_finish_exception PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_c2_finish_exception PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_c2_finish_exception PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_c2_finish_exception mock_ir_provider)
    ams_mel_warnings(test_c2_finish_exception)
    foreach(family IN ITEMS 1 2)
        add_test(NAME c2_finish_exception_${family} COMMAND test_c2_finish_exception ${family})
        set_tests_properties(c2_finish_exception_${family} PROPERTIES TIMEOUT 30)
    endforeach()
    foreach(family RANGE 0 2)
        foreach(failure allocation worker-launch)
            add_test(NAME c2_submission_${family}_${failure}
                COMMAND test_c2_submission ${family} ${failure})
            set_tests_properties(c2_submission_${family}_${failure} PROPERTIES TIMEOUT 30)
        endforeach()
    endforeach()

    add_executable(test_completion_scalability tests/test_completion_scalability.c)
    foreach(scenario options-zero bounded-one bounded-eight)
        add_test(NAME completion_${scenario} COMMAND test_completion_scalability ${scenario})
        set_tests_properties(completion_${scenario} PROPERTIES TIMEOUT 120)
    endforeach()
    target_compile_features(test_completion_scalability PRIVATE c_std_11)
    set_target_properties(test_completion_scalability PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_completion_scalability PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_completion_scalability PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_completion_scalability mock_ir_provider)
    ams_mel_warnings(test_completion_scalability)
    add_test(NAME completion_scalability COMMAND test_completion_scalability)

    add_executable(test_completion_cross_family tests/test_completion_cross_family.c)
    target_compile_features(test_completion_cross_family PRIVATE c_std_11)
    set_target_properties(test_completion_cross_family PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_completion_cross_family PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_completion_cross_family PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_completion_cross_family mock_ir_provider)
    ams_mel_warnings(test_completion_cross_family)
    add_test(NAME completion_cross_family COMMAND test_completion_cross_family)
    foreach(type RANGE 0 6)
        foreach(failure allocation worker-launch)
            add_test(NAME admission_retention_${type}_${failure}
                COMMAND test_completion_cross_family failure ${type} ${failure})
            set_tests_properties(admission_retention_${type}_${failure} PROPERTIES TIMEOUT 30)
        endforeach()
    endforeach()
    foreach(type 0 3 4 5)
        add_test(NAME admission_send_throw_${type}
            COMMAND test_completion_cross_family failure ${type} send-throw)
        set_tests_properties(admission_send_throw_${type} PROPERTIES TIMEOUT 60)
    endforeach()
    foreach(type 0 3)
        add_test(NAME admission_exception_${type}
            COMMAND test_completion_cross_family failure ${type} exception)
        set_tests_properties(admission_exception_${type} PROPERTIES TIMEOUT 60)
    endforeach()
    add_test(NAME admission_provider_rejection
        COMMAND test_completion_cross_family failure 1 rejection)
    set_tests_properties(admission_provider_rejection PROPERTIES TIMEOUT 60)
    add_test(NAME completion_cross_family_admission COMMAND test_completion_cross_family admission)
    add_test(NAME completion_parent_admission COMMAND test_completion_cross_family parent-admission)
    add_test(NAME completion_concurrent_admission COMMAND test_completion_cross_family concurrent-admission)
    set_tests_properties(completion_concurrent_admission PROPERTIES TIMEOUT 120)
    add_test(NAME completion_isolated_admission COMMAND test_completion_cross_family isolated-admission)
    set_tests_properties(completion_isolated_admission PROPERTIES TIMEOUT 120)
    set_tests_properties(completion_parent_admission PROPERTIES TIMEOUT 120)
    add_test(NAME completion_seven_type_bounded COMMAND test_completion_cross_family bounded-workload)
    set_tests_properties(completion_seven_type_bounded PROPERTIES TIMEOUT 120)
    set_tests_properties(completion_cross_family_admission PROPERTIES TIMEOUT 120)
    add_test(NAME completion_cross_family_exception COMMAND test_completion_cross_family exception)
    add_test(NAME completion_retained COMMAND test_completion_cross_family retained)
    add_test(NAME completion_retained_exception COMMAND test_completion_cross_family retained exception)
    set_tests_properties(completion_cross_family completion_cross_family_exception
        completion_retained completion_retained_exception
        PROPERTIES TIMEOUT 120)

    add_executable(request_completion_scalability bench/request_completion_scalability.cpp)
    target_compile_features(request_completion_scalability PRIVATE cxx_std_20)
    target_compile_definitions(request_completion_scalability PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(request_completion_scalability PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(request_completion_scalability mock_ir_provider)
    ams_mel_warnings(request_completion_scalability)

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
        AMS_MEL_TEST_MISSING_SYMBOL_PROVIDER="$<TARGET_FILE:missing_symbol_ir_provider>")
    target_link_libraries(test_provider PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_provider mock_ir_provider missing_symbol_ir_provider)
    ams_mel_warnings(test_provider)
    add_test(NAME provider_contract COMMAND test_provider)

    add_executable(test_ir_stream tests/test_ir_stream.c)
    target_compile_features(test_ir_stream PRIVATE c_std_11)
    set_target_properties(test_ir_stream PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_stream PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    # Task 030B resolves the mock provider's deterministic backpressure probes
    # with dlsym/RTLD_NOLOAD against the already-loaded provider image.
    target_link_libraries(test_ir_stream PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_ir_stream mock_ir_provider)
    ams_mel_warnings(test_ir_stream)
    add_test(NAME ir_stream_contract COMMAND test_ir_stream)

    add_executable(test_ir_image_metadata tests/test_ir_image_metadata.c)
    target_compile_features(test_ir_image_metadata PRIVATE c_std_11)
    set_target_properties(test_ir_image_metadata PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_image_metadata PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_ir_image_metadata PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_ir_image_metadata mock_ir_provider)
    ams_mel_warnings(test_ir_image_metadata)
    add_test(NAME ir_image_metadata_contract COMMAND test_ir_image_metadata)

    add_executable(test_ir_c2 tests/test_ir_c2.c)
    target_compile_features(test_ir_c2 PRIVATE c_std_11)
    set_target_properties(test_ir_c2 PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_c2 PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_ir_c2 PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_ir_c2 mock_ir_provider)
    ams_mel_warnings(test_ir_c2)
    add_test(NAME ir_c2_contract COMMAND test_ir_c2)

    add_executable(test_ir_navigation tests/test_ir_navigation.c)
    target_compile_features(test_ir_navigation PRIVATE c_std_11)
    set_target_properties(test_ir_navigation PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_navigation PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    # The deterministic completion-vs-Close teardown race regressions drive a
    # concurrent public Close from a second thread.
    find_package(Threads REQUIRED)
    target_link_libraries(test_ir_navigation PRIVATE ams_mel::ams_mel_c Threads::Threads)
    add_dependencies(test_ir_navigation mock_ir_provider)
    ams_mel_warnings(test_ir_navigation)
    add_test(NAME ir_navigation_contract COMMAND test_ir_navigation)

    add_executable(test_ir_health tests/test_ir_health.c)
    target_compile_features(test_ir_health PRIVATE c_std_11)
    set_target_properties(test_ir_health PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_health PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_ir_health PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_ir_health mock_ir_provider)
    ams_mel_warnings(test_ir_health)
    add_test(NAME ir_health_contract COMMAND test_ir_health)

    add_executable(test_ir_instrumentation tests/test_ir_instrumentation.c)
    target_compile_features(test_ir_instrumentation PRIVATE c_std_11)
    set_target_properties(test_ir_instrumentation PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_instrumentation PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_ir_instrumentation PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_ir_instrumentation mock_ir_provider)
    ams_mel_warnings(test_ir_instrumentation)
    add_test(NAME ir_instrumentation_contract COMMAND test_ir_instrumentation)

    add_executable(test_ir_track_foundation tests/test_ir_track_foundation.c)
    target_compile_features(test_ir_track_foundation PRIVATE c_std_11)
    set_target_properties(test_ir_track_foundation PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_track_foundation PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_ir_track_foundation PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_ir_track_foundation mock_ir_provider)
    ams_mel_warnings(test_ir_track_foundation)
    add_test(NAME ir_track_foundation_contract COMMAND test_ir_track_foundation)

    add_executable(test_ir_track_metadata tests/test_ir_track_metadata.c)
    target_compile_features(test_ir_track_metadata PRIVATE c_std_11)
    set_target_properties(test_ir_track_metadata PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_track_metadata PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_ir_track_metadata PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_ir_track_metadata mock_ir_provider)
    ams_mel_warnings(test_ir_track_metadata)
    add_test(NAME ir_track_metadata_contract COMMAND test_ir_track_metadata)

    add_executable(test_ir_track_update tests/test_ir_track_update.c)
    target_compile_features(test_ir_track_update PRIVATE c_std_11)
    set_target_properties(test_ir_track_update PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_track_update PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_ir_track_update PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_ir_track_update mock_ir_provider)
    ams_mel_warnings(test_ir_track_update)
    add_test(NAME ir_track_update_contract COMMAND test_ir_track_update)

    add_executable(test_ir_track_system_response
        tests/test_ir_track_system_response.c)
    target_compile_features(test_ir_track_system_response PRIVATE c_std_11)
    set_target_properties(test_ir_track_system_response
        PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_track_system_response PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_ir_track_system_response
        PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_ir_track_system_response mock_ir_provider)
    ams_mel_warnings(test_ir_track_system_response)
    add_test(NAME ir_track_system_response_contract
        COMMAND test_ir_track_system_response)

    add_executable(test_ir_track_request_system_data
        tests/test_ir_track_request_system_data.c)
    target_compile_features(test_ir_track_request_system_data PRIVATE c_std_11)
    set_target_properties(test_ir_track_request_system_data
        PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_ir_track_request_system_data PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_ir_track_request_system_data
        PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_ir_track_request_system_data mock_ir_provider)
    ams_mel_warnings(test_ir_track_request_system_data)
    add_test(NAME ir_track_request_system_data_contract
        COMMAND test_ir_track_request_system_data)


    add_executable(test_diagnostic_allocation
        tests/test_diagnostic_allocation.cpp)
    target_compile_features(test_diagnostic_allocation PRIVATE cxx_std_20)
    target_compile_definitions(test_diagnostic_allocation PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_diagnostic_allocation PRIVATE ams_mel::ams_mel_c)
    add_dependencies(test_diagnostic_allocation mock_ir_provider)
    ams_mel_warnings(test_diagnostic_allocation)
    add_test(NAME diagnostic_allocation COMMAND test_diagnostic_allocation)
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
install(FILES vendor/boost-1.83.0/LICENSE_1_0.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/ams_mel_c/upstream/boost-1.83.0)
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
