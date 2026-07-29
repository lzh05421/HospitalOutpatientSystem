add_executable(protocol_tests
    protocol_tests.cpp
)

target_link_libraries(protocol_tests
    PRIVATE
        hospital_common
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME protocol_tests COMMAND protocol_tests)

if(WIN32)
    get_filename_component(TEST_CXX_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    function(set_hospital_server_test_environment test_name)
        set(path_modifications
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Sql>"
            "PATH=path_list_prepend:${TEST_CXX_RUNTIME_DIR}"
        )
        if(TARGET hiredis)
            list(APPEND path_modifications "PATH=path_list_prepend:$<TARGET_FILE_DIR:hiredis>")
        elseif(TARGET hiredis::hiredis)
            list(APPEND path_modifications "PATH=path_list_prepend:$<TARGET_FILE_DIR:hiredis::hiredis>")
        endif()

        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "${path_modifications}"
        )
    endfunction()

    set_tests_properties(protocol_tests PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;PATH=path_list_prepend:${TEST_CXX_RUNTIME_DIR}"
    )
endif()

add_executable(auth_router_tests
    auth_router_tests.cpp
)

target_link_libraries(auth_router_tests
    PRIVATE
        hospital_server_core
        hospital_common
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME auth_router_tests COMMAND auth_router_tests)

if(WIN32)
    set_hospital_server_test_environment(auth_router_tests)
endif()

add_executable(workflow_rules_tests
    workflow_rules_tests.cpp
)

target_link_libraries(workflow_rules_tests
    PRIVATE
        hospital_server_core
        hospital_common
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME workflow_rules_tests COMMAND workflow_rules_tests)

if(WIN32)
    set_hospital_server_test_environment(workflow_rules_tests)
endif()

add_executable(schedule_rule_engine_tests
    schedule_rule_engine_tests.cpp
)

target_link_libraries(schedule_rule_engine_tests
    PRIVATE
        hospital_server_core
        hospital_common
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME schedule_rule_engine_tests COMMAND schedule_rule_engine_tests)

if(WIN32)
    set_hospital_server_test_environment(schedule_rule_engine_tests)
endif()

add_executable(schedule_service_tests
    schedule_service_tests.cpp
)

target_link_libraries(schedule_service_tests
    PRIVATE
        hospital_server_core
        hospital_common
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME schedule_service_tests COMMAND schedule_service_tests)

if(WIN32)
    set_hospital_server_test_environment(schedule_service_tests)
endif()

add_executable(schedule_batch_transaction_tests
    schedule_batch_transaction_tests.cpp
)

target_link_libraries(schedule_batch_transaction_tests
    PRIVATE
        hospital_server_core
        hospital_common
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME schedule_batch_transaction_tests COMMAND schedule_batch_transaction_tests)

if(WIN32)
    set_hospital_server_test_environment(schedule_batch_transaction_tests)
endif()

add_executable(schedule_smart_rotation_source_tests
    schedule_smart_rotation_source_tests.cpp
)

add_test(NAME schedule_smart_rotation_source_tests COMMAND schedule_smart_rotation_source_tests)
set_tests_properties(schedule_smart_rotation_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(schedule_smart_rotation_source_tests)
endif()

add_executable(patient_identity_display_source_tests
    patient_identity_display_source_tests.cpp
)

add_test(NAME patient_identity_display_source_tests COMMAND patient_identity_display_source_tests)
set_tests_properties(patient_identity_display_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(patient_identity_display_source_tests)
endif()

add_executable(pharmacy_workflow_source_tests
    pharmacy_workflow_source_tests.cpp
)

add_test(NAME pharmacy_workflow_source_tests COMMAND pharmacy_workflow_source_tests)
set_tests_properties(pharmacy_workflow_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(pharmacy_workflow_source_tests)
endif()

add_executable(module_table_copy_source_tests
    module_table_copy_source_tests.cpp
)

add_test(NAME module_table_copy_source_tests COMMAND module_table_copy_source_tests)
set_tests_properties(module_table_copy_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(module_table_copy_source_tests)
endif()

add_executable(doctor_cascade_filter_source_tests
    doctor_cascade_filter_source_tests.cpp
)

add_test(NAME doctor_cascade_filter_source_tests COMMAND doctor_cascade_filter_source_tests)
set_tests_properties(doctor_cascade_filter_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(doctor_cascade_filter_source_tests)
endif()

add_executable(structured_medical_record_source_tests
    structured_medical_record_source_tests.cpp
)

add_test(NAME structured_medical_record_source_tests COMMAND structured_medical_record_source_tests)
set_tests_properties(structured_medical_record_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(structured_medical_record_source_tests)
endif()

add_executable(permission_admin_source_tests
    permission_admin_source_tests.cpp
)

add_test(NAME permission_admin_source_tests COMMAND permission_admin_source_tests)
set_tests_properties(permission_admin_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(permission_admin_source_tests)
endif()

add_executable(redis_manager_compile_tests
    redis_manager_compile_tests.cpp
)

target_link_libraries(redis_manager_compile_tests
    PRIVATE
        hospital_server_core
        hospital_common
)

# Compile/link check only. It intentionally does not touch a live Redis service.
add_test(NAME redis_manager_compile_tests COMMAND redis_manager_compile_tests)

if(WIN32)
    set_hospital_server_test_environment(redis_manager_compile_tests)
endif()

add_executable(database_compatibility_schema_tests
    database_compatibility_schema_tests.cpp
)

add_test(NAME database_compatibility_schema_tests COMMAND database_compatibility_schema_tests)
set_tests_properties(database_compatibility_schema_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(database_compatibility_schema_tests)
endif()

add_executable(payment_security_source_tests
    payment_security_source_tests.cpp
)

add_test(NAME payment_security_source_tests COMMAND payment_security_source_tests)
set_tests_properties(payment_security_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(payment_security_source_tests)
endif()

add_executable(refund_workflow_source_tests
    refund_workflow_source_tests.cpp
)

add_test(NAME refund_workflow_source_tests COMMAND refund_workflow_source_tests)
set_tests_properties(refund_workflow_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(refund_workflow_source_tests)
endif()

add_executable(examination_workflow_source_tests
    examination_workflow_source_tests.cpp
)

add_test(NAME examination_workflow_source_tests COMMAND examination_workflow_source_tests)
set_tests_properties(examination_workflow_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(examination_workflow_source_tests)
endif()

add_executable(emergency_priority_source_tests
    emergency_priority_source_tests.cpp
)

add_test(NAME emergency_priority_source_tests COMMAND emergency_priority_source_tests)
set_tests_properties(emergency_priority_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(emergency_priority_source_tests)
endif()

add_executable(waiting_consultation_today_filter_source_tests
    waiting_consultation_today_filter_source_tests.cpp
)

add_test(NAME waiting_consultation_today_filter_source_tests COMMAND waiting_consultation_today_filter_source_tests)
set_tests_properties(waiting_consultation_today_filter_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(waiting_consultation_today_filter_source_tests)
endif()

add_executable(ui_workstation_polish_source_tests
    ui_workstation_polish_source_tests.cpp
)

add_test(NAME ui_workstation_polish_source_tests COMMAND ui_workstation_polish_source_tests)
set_tests_properties(ui_workstation_polish_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(ui_workstation_polish_source_tests)
endif()

add_executable(consultation_waiting_ui_source_tests
    consultation_waiting_ui_source_tests.cpp
)

add_test(NAME consultation_waiting_ui_source_tests COMMAND consultation_waiting_ui_source_tests)
set_tests_properties(consultation_waiting_ui_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(consultation_waiting_ui_source_tests)
endif()

add_executable(entry_login_dashboard_ui_source_tests
    entry_login_dashboard_ui_source_tests.cpp
)

add_test(NAME entry_login_dashboard_ui_source_tests COMMAND entry_login_dashboard_ui_source_tests)
set_tests_properties(entry_login_dashboard_ui_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(entry_login_dashboard_ui_source_tests)
endif()

add_executable(main_shell_module_ui_source_tests
    main_shell_module_ui_source_tests.cpp
)

add_test(NAME main_shell_module_ui_source_tests COMMAND main_shell_module_ui_source_tests)
set_tests_properties(main_shell_module_ui_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(main_shell_module_ui_source_tests)
endif()

add_executable(dashboard_visualization_source_tests
    dashboard_visualization_source_tests.cpp
)

add_test(NAME dashboard_visualization_source_tests COMMAND dashboard_visualization_source_tests)
set_tests_properties(dashboard_visualization_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(dashboard_visualization_source_tests)
endif()

add_executable(payment_dialog_resilience_source_tests
    payment_dialog_resilience_source_tests.cpp
)

add_test(NAME payment_dialog_resilience_source_tests COMMAND payment_dialog_resilience_source_tests)
set_tests_properties(payment_dialog_resilience_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(payment_dialog_resilience_source_tests)
endif()

add_executable(payment_router_tests
    payment_router_tests.cpp
)

target_link_libraries(payment_router_tests
    PRIVATE
        hospital_server_core
        hospital_common
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME payment_router_tests COMMAND payment_router_tests)

if(WIN32)
    set_hospital_server_test_environment(payment_router_tests)
endif()

add_executable(patient_account_source_tests
    patient_account_source_tests.cpp
)

add_test(NAME patient_account_source_tests COMMAND patient_account_source_tests)
set_tests_properties(patient_account_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(patient_account_source_tests)
endif()

add_executable(registration_insurance_source_tests
    registration_insurance_source_tests.cpp
)

add_test(NAME registration_insurance_source_tests COMMAND registration_insurance_source_tests)
set_tests_properties(registration_insurance_source_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(registration_insurance_source_tests)
endif()

add_executable(outbox_consumer_mock_tests
    outbox_consumer_mock_tests.cpp
)

target_link_libraries(outbox_consumer_mock_tests
    PRIVATE
        hospital_server_core
        hospital_common
)

add_test(NAME outbox_consumer_mock_tests COMMAND outbox_consumer_mock_tests)

if(WIN32)
    set_hospital_server_test_environment(outbox_consumer_mock_tests)
endif()

add_executable(linux_example_config_tests
    linux_example_config_tests.cpp
)

target_link_libraries(linux_example_config_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME linux_example_config_tests COMMAND linux_example_config_tests)
set_tests_properties(linux_example_config_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    set_hospital_server_test_environment(linux_example_config_tests)
endif()
