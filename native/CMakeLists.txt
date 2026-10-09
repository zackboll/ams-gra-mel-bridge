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
    src/ir_instrumentation.cpp src/ir_track.cpp src/completion_probe.cpp
    src/rf_data.cpp src/rf_product_rx.cpp src/rf_admin.cpp src/rf_c2.cpp src/rf_virtual_aperture.cpp
    src/rf_job.cpp src/rf_job_interval_status.cpp src/rf_va_status.cpp)
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
# Task 033B: the pinned, byte-identical Task 033A RF DataMEL declaration
# closure (RF MEL + the AMS VITA headers it reaches). SYSTEM because pinned
# TxPowerModeData.h trips Clang -Wignored-qualifiers; first-party sources keep
# the project warnings. Only RF adapter sources
# include RF headers.
target_include_directories(ams_mel_c SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/rf-mel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ams-vita/include)
target_link_libraries(ams_mel_c PRIVATE ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    set(ams_mel_export_map "${CMAKE_CURRENT_SOURCE_DIR}/src/exports.map")
    if(AMS_MEL_BUILD_TESTS)
        # Observation/rescue hooks are private to the contract-test facade.
        # Never extend the production export map or installed C headers.
        file(READ "${ams_mel_export_map}" ams_mel_test_exports)
        string(REPLACE "    global:" "    global:\n        ams_mel_test_va_signal_hold;\n        ams_mel_test_va_signal_waiters;\n        ams_mel_test_va_signal_preset;\n        ams_mel_test_va_signal_last;"
            ams_mel_test_exports "${ams_mel_test_exports}")
        string(REPLACE "    global:" "    global:\n        ams_mel_test_rf_status_hold;\n        ams_mel_test_rf_status_waiters;\n        ams_mel_test_rf_status_preset;\n        ams_mel_test_rf_status_last;"
            ams_mel_test_exports "${ams_mel_test_exports}")
        string(REPLACE "    global:" "    global:\n        ams_mel_test_instrumentation_callbacks;\n        ams_mel_test_track_callbacks;\n        ams_mel_test_instrumentation_observer_from;\n        ams_mel_test_instrumentation_observer_requests;\n        ams_mel_test_instrumentation_observer_close;\n        ams_mel_test_track_observer_from;\n        ams_mel_test_track_observer_requests;\n        ams_mel_test_track_observer_close;"
            ams_mel_test_exports "${ams_mel_test_exports}")
        string(REPLACE "    global:" "    global:\n        ams_mel_test_health_callbacks;\n        ams_mel_test_health_requests;\n        ams_mel_test_admission;\n        ams_mel_test_c2_requests;\n        ams_mel_test_navigation_requests;\n        ams_mel_test_instrumentation_requests;\n        ams_mel_test_track_requests;"
            ams_mel_test_exports "${ams_mel_test_exports}")
        string(REPLACE "    global:" "    global:\n        ams_mel_test_common_from_instrumentation;\n        ams_mel_test_common_from_track;\n        ams_mel_test_common_from_health;\n        ams_mel_test_common_from_c2;\n        ams_mel_test_common_from_stream;\n        ams_mel_test_common_send_keepalive;\n        ams_mel_test_common_submit_comms_test;\n        ams_mel_test_common_close;\n        ams_mel_test_c2_submission;\n        ams_mel_test_completion_snapshot;\n        ams_mel_test_completion_wait;\n        ams_mel_test_completion_owner;\n        ams_mel_test_completion_boundary;\n        ams_mel_test_lost_cleanup_inspect;\n        ams_mel_test_lost_cleanup_rescue;"
            ams_mel_test_exports "${ams_mel_test_exports}")
        string(REPLACE "    global:" "    global:\n        ams_mel_test_rf_rx_observer_from;\n        ams_mel_test_rf_rx_observer_from_last_registration;\n        ams_mel_test_rf_rx_observe;\n        ams_mel_test_rf_rx_waiters;\n        ams_mel_test_rf_rx_drain_waiters;\n        ams_mel_test_rf_rx_preset_counters;\n        ams_mel_test_rf_rx_observer_close;\n        ams_mel_test_rf_rx_failpoint;\n        ams_mel_test_rf_rx_negative_control;\n        ams_mel_test_rf_rx_hold_next;\n        ams_mel_test_rf_rx_permanent_registrations;"
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

    # Task 033A declaration-only probe: the pinned RF MEL DataMEL/RFMFAInfo
    # declaration closure. It implements no RF API and links into nothing.
    # Upstream RF headers stay byte-identical, so every vendored root is a
    # SYSTEM include (pinned TxPowerModeData.h trips Clang -Wignored-qualifiers);
    # the first-party probe itself still compiles with the project warnings.
    set(ams_mel_rf_data_vendor_roots
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/rf-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/common-mel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ams-math/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ams-vita/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-1.83.0)
    add_library(rf_data_header_compile_probe OBJECT tests/rf_data_header_compile_probe.cpp)
    target_compile_features(rf_data_header_compile_probe PRIVATE cxx_std_20)
    target_include_directories(rf_data_header_compile_probe SYSTEM PRIVATE
        ${ams_mel_rf_data_vendor_roots})
    set_target_properties(rf_data_header_compile_probe PROPERTIES CXX_EXTENSIONS OFF)
    ams_mel_warnings(rf_data_header_compile_probe)
    set(ams_mel_rf_data_isystem_args)
    foreach(root IN LISTS ams_mel_rf_data_vendor_roots)
        list(APPEND ams_mel_rf_data_isystem_args --isystem ${root})
    endforeach()
    add_custom_target(check_rf_data_header_closure ALL
        COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_rf_data_closure.py
            --compiler ${CMAKE_CXX_COMPILER}
            --source ${CMAKE_CURRENT_SOURCE_DIR}/tests/rf_data_header_compile_probe.cpp
            --vendor-root ${CMAKE_CURRENT_SOURCE_DIR}/vendor
            ${ams_mel_rf_data_isystem_args}
            --expect rfmel --expect mel --expect math --expect boost --expect vita
        DEPENDS rf_data_header_compile_probe
        VERBATIM)

    # Task 033C declaration-only probe: the pinned RF MEL ProductRxEndpoint
    # receive contract (DataMEL.h + ProductRxEndpoint.h). It implements no RF
    # API, links into nothing, and uses the same SYSTEM vendor roots. The 033A
    # check above is unchanged; this distinct check additionally pins the exact
    # measured RF header set.
    add_library(rf_product_rx_header_compile_probe OBJECT
        tests/rf_product_rx_header_compile_probe.cpp)
    target_compile_features(rf_product_rx_header_compile_probe PRIVATE cxx_std_20)
    target_include_directories(rf_product_rx_header_compile_probe SYSTEM PRIVATE
        ${ams_mel_rf_data_vendor_roots})
    set_target_properties(rf_product_rx_header_compile_probe PROPERTIES CXX_EXTENSIONS OFF)
    ams_mel_warnings(rf_product_rx_header_compile_probe)
    add_custom_target(check_rf_product_rx_header_closure ALL
        COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_rf_product_rx_closure.py
            --compiler ${CMAKE_CXX_COMPILER}
            --source ${CMAKE_CURRENT_SOURCE_DIR}/tests/rf_product_rx_header_compile_probe.cpp
            --vendor-root ${CMAKE_CURRENT_SOURCE_DIR}/vendor
            ${ams_mel_rf_data_isystem_args}
        DEPENDS rf_product_rx_header_compile_probe
        VERBATIM)
    # Task 034B1/034B2A: distinct published Admin/C2/VA/JobDetail roots. Keep the earlier
    # Data and ProductRx closure gates unchanged.
    add_library(rf_admin_c2_header_compile_probe OBJECT
        tests/rf_admin_c2_header_compile_probe.cpp)
    target_compile_features(rf_admin_c2_header_compile_probe PRIVATE cxx_std_20)
    target_include_directories(rf_admin_c2_header_compile_probe SYSTEM PRIVATE
        ${ams_mel_rf_data_vendor_roots})
    set_target_properties(rf_admin_c2_header_compile_probe PROPERTIES CXX_EXTENSIONS OFF)
    ams_mel_warnings(rf_admin_c2_header_compile_probe)
    add_custom_target(check_rf_admin_c2_header_closure ALL
        COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_rf_admin_c2_closure.py
            --compiler ${CMAKE_CXX_COMPILER}
            --source ${CMAKE_CURRENT_SOURCE_DIR}/tests/rf_admin_c2_header_compile_probe.cpp
            --vendor-root ${CMAKE_CURRENT_SOURCE_DIR}/vendor
            ${ams_mel_rf_data_isystem_args}
        DEPENDS rf_admin_c2_header_compile_probe
        VERBATIM)
    add_executable(test_rf_interval_headers tests/test_rf_interval_headers.cpp)
    target_compile_features(test_rf_interval_headers PRIVATE cxx_std_20)
    target_include_directories(test_rf_interval_headers SYSTEM PRIVATE ${ams_mel_rf_data_vendor_roots})
    ams_mel_warnings(test_rf_interval_headers)
    add_test(NAME rf-interval-pinned-headers COMMAND test_rf_interval_headers)
    add_test(NAME c_abi COMMAND test_c_abi)

    # Task 033B: dedicated mock RF provider (createDataMEL + TEST-only
    # observations) and a loadable RF-shaped DSO without createDataMEL. Both
    # use the pinned 033A closure; neither shares code with the IR mock.
    foreach(rf_provider IN ITEMS mock_rf_provider missing_symbol_rf_provider)
        add_library(${rf_provider} SHARED tests/${rf_provider}.cpp)
        target_compile_features(${rf_provider} PRIVATE cxx_std_20)
        target_include_directories(${rf_provider} SYSTEM PRIVATE
            ${ams_mel_rf_data_vendor_roots})
        set_target_properties(${rf_provider} PROPERTIES
            CXX_EXTENSIONS OFF CXX_VISIBILITY_PRESET hidden
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-providers")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${rf_provider} PRIVATE -fno-gnu-unique)
        endif()
        ams_mel_warnings(${rf_provider})
    endforeach()

    add_executable(test_rf_physical tests/test_rf_physical.c)
    target_compile_features(test_rf_physical PRIVATE c_std_11)
    set_target_properties(test_rf_physical PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_physical PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_physical PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_physical mock_rf_provider)
    ams_mel_warnings(test_rf_physical)
    add_test(NAME rf-physical COMMAND test_rf_physical)

    add_executable(test_rf_tx_power tests/test_rf_tx_power.c)
    target_compile_features(test_rf_tx_power PRIVATE c_std_11)
    set_target_properties(test_rf_tx_power PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_tx_power PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_tx_power PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_tx_power mock_rf_provider)
    ams_mel_warnings(test_rf_tx_power)
    add_test(NAME rf-tx-power COMMAND test_rf_tx_power)

    add_executable(test_rf_data tests/test_rf_data.c)
    target_compile_features(test_rf_data PRIVATE c_std_11)
    set_target_properties(test_rf_data PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_data PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>"
        AMS_MEL_TEST_MISSING_SYMBOL_RF_PROVIDER="$<TARGET_FILE:missing_symbol_rf_provider>")
    target_link_libraries(test_rf_data PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_data mock_rf_provider missing_symbol_rf_provider)
    ams_mel_warnings(test_rf_data)
    add_executable(test_rf_admin tests/test_rf_admin.c)
    target_compile_features(test_rf_admin PRIVATE c_std_11)
    set_target_properties(test_rf_admin PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_admin PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>"
        AMS_MEL_TEST_MISSING_SYMBOL_RF_PROVIDER="$<TARGET_FILE:missing_symbol_rf_provider>")
    target_link_libraries(test_rf_admin PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_admin mock_rf_provider missing_symbol_rf_provider)
    ams_mel_warnings(test_rf_admin)
    add_test(NAME rf-admin COMMAND test_rf_admin)
    add_executable(test_rf_c2 tests/test_rf_c2.c)
    target_compile_features(test_rf_c2 PRIVATE c_std_11)
    set_target_properties(test_rf_c2 PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_c2 PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>"
        AMS_MEL_TEST_MISSING_SYMBOL_RF_PROVIDER="$<TARGET_FILE:missing_symbol_rf_provider>")
    target_link_libraries(test_rf_c2 PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_c2 mock_rf_provider missing_symbol_rf_provider)
    ams_mel_warnings(test_rf_c2)
    add_test(NAME rf-c2-lifecycle COMMAND test_rf_c2)
    add_executable(test_rf_va tests/test_rf_va.c)
    target_compile_features(test_rf_va PRIVATE c_std_11)
    set_target_properties(test_rf_va PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_va PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_va PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_va mock_rf_provider)
    ams_mel_warnings(test_rf_va)
    add_test(NAME rf-va-request COMMAND test_rf_va)
    add_executable(test_rf_va_queries tests/test_rf_va_queries.c)
    target_compile_features(test_rf_va_queries PRIVATE c_std_11)
    set_target_properties(test_rf_va_queries PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_va_queries PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_va_queries PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_va_queries mock_rf_provider)
    ams_mel_warnings(test_rf_va_queries)
    add_test(NAME rf-va-live-queries COMMAND test_rf_va_queries)
    add_executable(test_rf_local_functions tests/test_rf_local_functions.c)
    target_compile_features(test_rf_local_functions PRIVATE c_std_11)
    set_target_properties(test_rf_local_functions PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_local_functions PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_local_functions PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_local_functions mock_rf_provider)
    ams_mel_warnings(test_rf_local_functions)
    add_test(NAME rf-va-local-functions COMMAND test_rf_local_functions)

    add_executable(test_rf_transmit_power tests/test_rf_transmit_power.c)
    target_compile_features(test_rf_transmit_power PRIVATE c_std_11)
    set_target_properties(test_rf_transmit_power PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_transmit_power PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_transmit_power PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_transmit_power mock_rf_provider)
    ams_mel_warnings(test_rf_transmit_power)
    add_test(NAME rf-va-transmit-power COMMAND test_rf_transmit_power)
    set_tests_properties(rf-va-local-functions PROPERTIES TIMEOUT 30)
    add_executable(test_rf_element_groups tests/test_rf_element_groups.c)
    add_executable(test_rf_data_pipes tests/test_rf_data_pipes.c)
    target_compile_features(test_rf_data_pipes PRIVATE c_std_11)
    set_target_properties(test_rf_data_pipes PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_data_pipes PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_data_pipes PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_data_pipes mock_rf_provider)
    ams_mel_warnings(test_rf_data_pipes)
    add_test(NAME rf-va-data-pipes COMMAND test_rf_data_pipes)
    set_tests_properties(rf-va-data-pipes PROPERTIES TIMEOUT 30)
    target_compile_features(test_rf_element_groups PRIVATE c_std_11)
    set_target_properties(test_rf_element_groups PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_element_groups PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_element_groups PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_element_groups mock_rf_provider)
    ams_mel_warnings(test_rf_element_groups)
    add_test(NAME rf-element-group-snapshots COMMAND test_rf_element_groups)
    set_tests_properties(rf-element-group-snapshots PROPERTIES TIMEOUT 60)
    set_tests_properties(rf-va-live-queries PROPERTIES TIMEOUT 30)
    add_executable(test_rf_va_notifications tests/test_rf_va_notifications.c)
    target_compile_features(test_rf_va_notifications PRIVATE c_std_11)
    set_target_properties(test_rf_va_notifications PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_va_notifications PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_va_notifications PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS} Threads::Threads)
    add_dependencies(test_rf_va_notifications mock_rf_provider)
    ams_mel_warnings(test_rf_va_notifications)
    foreach(case IN ITEMS c2:va-notify-zero c2:va-notify-max c2:va-notify-copy
            c2:va-notify-reference c2:va-notify-sync c2:va-notify-remove-callback
            preparation stored-throw removal-errors races job-lifetime)
        add_test(NAME rf-va-notifications-${case} COMMAND test_rf_va_notifications ${case})
        set_tests_properties(rf-va-notifications-${case} PROPERTIES TIMEOUT 25)
    endforeach()
    add_executable(test_rf_job tests/test_rf_job.c)
    target_compile_features(test_rf_job PRIVATE c_std_11)
    set_target_properties(test_rf_job PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job mock_rf_provider)
    ams_mel_warnings(test_rf_job)
    add_test(NAME rf-job-request COMMAND test_rf_job)
    add_executable(test_rf_job_v2 tests/test_rf_job_v2.c)
    target_compile_features(test_rf_job_v2 PRIVATE c_std_11)
    set_target_properties(test_rf_job_v2 PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_v2 PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_v2 PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_v2 mock_rf_provider)
    ams_mel_warnings(test_rf_job_v2)
    add_test(NAME rf-job-request-v2 COMMAND test_rf_job_v2)
    set_tests_properties(rf-job-request-v2 PROPERTIES TIMEOUT 30)
    add_executable(test_rf_job_pointing tests/test_rf_job_pointing.c)
    target_compile_features(test_rf_job_pointing PRIVATE c_std_11)
    set_target_properties(test_rf_job_pointing PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_pointing PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_pointing PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_pointing mock_rf_provider)
    ams_mel_warnings(test_rf_job_pointing)
    add_test(NAME rf-job-pointing COMMAND test_rf_job_pointing)
    set_tests_properties(rf-job-pointing PROPERTIES TIMEOUT 30)
    add_executable(test_rf_job_tx_groups tests/test_rf_job_tx_groups.c)
    target_compile_features(test_rf_job_tx_groups PRIVATE c_std_11)
    set_target_properties(test_rf_job_tx_groups PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_tx_groups PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_tx_groups PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_tx_groups mock_rf_provider)
    ams_mel_warnings(test_rf_job_tx_groups)
    add_test(NAME rf-job-tx-groups COMMAND test_rf_job_tx_groups)
    set_tests_properties(rf-job-tx-groups PROPERTIES TIMEOUT 30)
    add_executable(test_rf_job_interval_spatial tests/test_rf_job_interval_spatial.c)
    target_compile_features(test_rf_job_interval_spatial PRIVATE c_std_11)
    set_target_properties(test_rf_job_interval_spatial PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_interval_spatial PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_interval_spatial PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_interval_spatial mock_rf_provider)
    ams_mel_warnings(test_rf_job_interval_spatial)
    add_test(NAME rf-job-interval-spatial COMMAND test_rf_job_interval_spatial)
    set_tests_properties(rf-job-interval-spatial PROPERTIES TIMEOUT 30)
    add_executable(test_rf_job_interval_event_controls tests/test_rf_job_interval_event_controls.c)
    target_compile_features(test_rf_job_interval_event_controls PRIVATE c_std_11)
    set_target_properties(test_rf_job_interval_event_controls PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_interval_event_controls PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_interval_event_controls PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_interval_event_controls mock_rf_provider)
    ams_mel_warnings(test_rf_job_interval_event_controls)
    add_test(NAME rf-job-interval-event-controls COMMAND test_rf_job_interval_event_controls)
    add_executable(test_rf_job_pulse_detection tests/test_rf_job_pulse_detection.c)
    target_compile_features(test_rf_job_pulse_detection PRIVATE c_std_11)
    set_target_properties(test_rf_job_pulse_detection PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_pulse_detection PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_pulse_detection PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_pulse_detection mock_rf_provider)
    ams_mel_warnings(test_rf_job_pulse_detection)
    add_test(NAME rf-job-pulse-detection COMMAND test_rf_job_pulse_detection)
    add_executable(test_rf_job_interval_lf_commands tests/test_rf_job_interval_lf_commands.c)
    target_compile_features(test_rf_job_interval_lf_commands PRIVATE c_std_11)
    set_target_properties(test_rf_job_interval_lf_commands PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_interval_lf_commands PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_interval_lf_commands PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_interval_lf_commands mock_rf_provider)
    ams_mel_warnings(test_rf_job_interval_lf_commands)
    add_test(NAME rf-job-interval-lf-commands COMMAND test_rf_job_interval_lf_commands)
    add_executable(test_rf_job_product_stream_params tests/test_rf_job_product_stream_params.c)
    target_compile_features(test_rf_job_product_stream_params PRIVATE c_std_11)
    set_target_properties(test_rf_job_product_stream_params PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_product_stream_params PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_product_stream_params PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_product_stream_params mock_rf_provider)
    ams_mel_warnings(test_rf_job_product_stream_params)
    add_test(NAME rf-job-product-stream-params COMMAND test_rf_job_product_stream_params)
    add_executable(test_rf_job_endpoint_map tests/test_rf_job_endpoint_map.c)
    target_compile_features(test_rf_job_endpoint_map PRIVATE c_std_11)
    set_target_properties(test_rf_job_endpoint_map PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_endpoint_map PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_endpoint_map PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_endpoint_map mock_rf_provider)
    ams_mel_warnings(test_rf_job_endpoint_map)
    add_test(NAME rf-job-endpoint-map COMMAND test_rf_job_endpoint_map)
    add_executable(test_rf_job_tx_events tests/test_rf_job_tx_events.c)
    target_compile_features(test_rf_job_tx_events PRIVATE c_std_11)
    set_target_properties(test_rf_job_tx_events PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_tx_events PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_tx_events PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_rf_job_tx_events mock_rf_provider)
    ams_mel_warnings(test_rf_job_tx_events)
    add_test(NAME rf-job-tx-events COMMAND test_rf_job_tx_events)
    set_tests_properties(rf-job-interval-event-controls PROPERTIES TIMEOUT 30)
    add_executable(test_rf_job_interval_status tests/test_rf_job_interval_status.c)
    target_compile_features(test_rf_job_interval_status PRIVATE c_std_11)
    set_target_properties(test_rf_job_interval_status PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_interval_status PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_interval_status PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS} pthread)
    add_dependencies(test_rf_job_interval_status mock_rf_provider)
    ams_mel_warnings(test_rf_job_interval_status)
    add_test(NAME rf-job-interval-status COMMAND test_rf_job_interval_status)
    add_executable(test_rf_job_extension tests/test_rf_job_extension.c)
    target_compile_features(test_rf_job_extension PRIVATE c_std_11)
    set_target_properties(test_rf_job_extension PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_job_extension PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    target_link_libraries(test_rf_job_extension PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS} Threads::Threads)
    add_dependencies(test_rf_job_extension mock_rf_provider)
    ams_mel_warnings(test_rf_job_extension)
    add_test(NAME rf-job-event-extension COMMAND test_rf_job_extension)
    set_tests_properties(rf-job-event-extension PROPERTIES TIMEOUT 60)
    # One process per case: lifetime/retention evidence cannot leak.
    foreach(case IN ITEMS open version version-long snapshot two-snapshots
            snapshot-independent snapshot-throw inconsistent-faces future-format
            missing-symbol null-factory factory-throw close shutdown-throw
            shutdown-throw-unknown invalid allocation quantize quantize-failure)
        add_test(NAME rf-data-${case} COMMAND test_rf_data ${case})
        set_tests_properties(rf-data-${case} PROPERTIES TIMEOUT 30)
    endforeach()

    # Task 033D RF ProductRxEndpoint ComplexINT16 receive (C11). One process
    # per case: permanent callback-registration retention and lifetime
    # evidence cannot leak between cases. Negative controls fork.
    add_executable(test_rf_product_rx tests/test_rf_product_rx.c)
    target_compile_features(test_rf_product_rx PRIVATE c_std_11)
    set_target_properties(test_rf_product_rx PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_rf_product_rx PRIVATE
        AMS_MEL_TEST_MOCK_RF_PROVIDER="$<TARGET_FILE:mock_rf_provider>")
    find_package(Threads REQUIRED)
    target_link_libraries(test_rf_product_rx PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS}
        Threads::Threads)
    add_dependencies(test_rf_product_rx mock_rf_provider)
    ams_mel_warnings(test_rf_product_rx)
    foreach(case IN ITEMS receive fail-closed malformed queue-full create-failures
            long-diagnostic invalid claim-unique parent-first pending-parent-first unclaimed
            unclaimed-parent-first never-ready sync-callback registration-throw-before
            registration-throw-after registration-throw-active mid-callback-close
            late-copy late-reference late-move negative-no-library-pin
            negative-no-exact-lvalue negative-old-close-order no-callback-unload
            multiple-endpoints concurrent receive-close saturation deferred-shutdown-throw
            worker-shutdown-throw worker-launch-failure)
        add_test(NAME rf-product-rx-${case} COMMAND test_rf_product_rx ${case})
        set_tests_properties(rf-product-rx-${case} PROPERTIES TIMEOUT 60)
    endforeach()

    add_executable(test_common_channel_view tests/test_common_channel_view.c)
    target_compile_features(test_common_channel_view PRIVATE c_std_11)
    set_target_properties(test_common_channel_view PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_common_channel_view PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    add_dependencies(test_common_channel_view mock_ir_provider)
    ams_mel_warnings(test_common_channel_view)
    foreach(case IN ITEMS c2-return image-return c2-comms image-comms
            c2-weak image-weak c2-close-first image-close-first image-running
            c2-parent image-parent c2-throw image-throw image-comms-throw
            image-finish image-cross image-cross-reverse c2-cross image-race image-return-allocation image-return-worker-launch
            image-comms-allocation image-comms-worker-launch c2-return-allocation)
        add_test(NAME common_${case} COMMAND test_common_channel_view ${case})
        set_tests_properties(common_${case} PROPERTIES TIMEOUT 30)
    endforeach()

    find_package(Threads REQUIRED)
    target_link_libraries(test_common_channel_view PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS} Threads::Threads)

    # Task 032B1: the installed public common Channel ABI, kept distinct from
    # the 032A internal/test-only common-view suites above.
    add_executable(test_public_common_channel tests/test_public_common_channel.c)
    target_compile_features(test_public_common_channel PRIVATE c_std_11)
    set_target_properties(test_public_common_channel PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_public_common_channel PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_public_common_channel PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_public_common_channel mock_ir_provider)
    ams_mel_warnings(test_public_common_channel)
    foreach(family IN ITEMS c2 image health instrumentation track)
        foreach(case IN ITEMS lifecycle weak close-first pending multiple snapshot invalid allocation)
            add_test(NAME public_channel_${family}_${case}
                COMMAND test_public_common_channel ${family}-${case})
            set_tests_properties(public_channel_${family}_${case} PROPERTIES TIMEOUT 60)
        endforeach()
    endforeach()
    foreach(case IN ITEMS keepalive keepalive-fail keepalive-reject comms comms-reject refusal)
        add_test(NAME public_channel_c2_legacy_${case}
            COMMAND test_public_common_channel c2-legacy-${case})
        set_tests_properties(public_channel_c2_legacy_${case} PROPERTIES TIMEOUT 60)
    endforeach()
    add_executable(test_common_channel_health tests/test_common_channel_health.c)
    target_compile_features(test_common_channel_health PRIVATE c_std_11)
    set_target_properties(test_common_channel_health PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_common_channel_health PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_common_channel_health PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS} Threads::Threads)
    add_dependencies(test_common_channel_health mock_ir_provider)
    ams_mel_warnings(test_common_channel_health)
    foreach(case IN ITEMS return comms enabled-return enabled-comms weak close-first
            parent-attached parent-enabled metadata-stop metadata-inflight
            return-throw close-wins-throw close-wins-attached-throw comms-throw cross cross-reverse
            return-allocation return-worker-launch comms-allocation comms-worker-launch
            finish-exception deferred-detach-fail)
        add_test(NAME common_health_${case} COMMAND test_common_channel_health ${case})
        set_tests_properties(common_health_${case} PROPERTIES TIMEOUT 30)
    endforeach()
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
    add_executable(test_common_channel_instrumentation_track
        tests/test_common_channel_instrumentation_track.c)
    target_compile_features(test_common_channel_instrumentation_track PRIVATE c_std_11)
    set_target_properties(test_common_channel_instrumentation_track PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(test_common_channel_instrumentation_track PRIVATE
        AMS_MEL_TEST_MOCK_PROVIDER="$<TARGET_FILE:mock_ir_provider>")
    target_link_libraries(test_common_channel_instrumentation_track
        PRIVATE ams_mel::ams_mel_c ${CMAKE_DL_LIBS})
    add_dependencies(test_common_channel_instrumentation_track mock_ir_provider)
    ams_mel_warnings(test_common_channel_instrumentation_track)
    foreach(family instrumentation track)
        foreach(case weak observer-idle close-first attached-keepalive attached-comms
                     enabled-keepalive enabled-comms enabled-parent attached-parent
                     finish detach throw comms-throw shared-open shared-closed metadata-held
                     close-wins post-return-allocation post-return-worker-launch
                     post-comms-allocation post-comms-worker-launch)
            add_test(NAME common_${family}_${case}
                COMMAND test_common_channel_instrumentation_track ${family}-${case})
            set_tests_properties(common_${family}_${case} PROPERTIES TIMEOUT 30)
        endforeach()
    endforeach()
    add_test(NAME common_instrumentation_comms-close-wins
        COMMAND test_common_channel_instrumentation_track instrumentation-comms-close-wins)
    set_tests_properties(common_instrumentation_comms-close-wins PROPERTIES TIMEOUT 30)
    foreach(case instrumentation-cross-common-first instrumentation-cross-typed-first
                 track-cross-common-first track-cross-typed-first)
        add_test(NAME common_${case}
            COMMAND test_common_channel_instrumentation_track ${case})
        set_tests_properties(common_${case} PROPERTIES TIMEOUT 30)
    endforeach()
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
foreach(upstream IN ITEMS common-mel ir-mel ams-math rf-mel ams-vita)
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
