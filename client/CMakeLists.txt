add_executable(hospital_client
    include/client/ApiClient.h
    include/client/EntryDialog.h
    include/client/LoginDialog.h
    include/client/MainWindow.h
    include/client/PatientLoginDialog.h
    include/client/PatientManager.h
    include/client/PaymentNetworkManager.h
    include/client/PaymentSelectionDialog.h
    include/client/UserCenterWindow.h
    include/client/DepartmentCatalog.h
    include/client/PatientAppointmentWindow.h
    include/client/pages/ModulePage.h
    include/client/pages/Pages.h
    src/main.cpp
    src/ApiClient.cpp
    src/EntryDialog.cpp
    src/LoginDialog.cpp
    src/MainWindow.cpp
    src/PatientLoginDialog.cpp
    src/PatientManager.cpp
    src/PaymentNetworkManager.cpp
    src/PaymentSelectionDialog.cpp
    src/UserCenterWindow.cpp
    src/DepartmentCatalog.cpp
    src/PatientAppointmentWindow.cpp
    src/pages/ModulePage.cpp
    src/pages/BillingPage.cpp
    src/pages/ConsultationPage.cpp
    src/pages/DashboardPage.cpp
    src/pages/DepartmentPage.cpp
    src/pages/DrugInventoryPage.cpp
    src/pages/DoctorManagementPage.cpp
    src/pages/ExaminationPage.cpp
    src/pages/PatientPage.cpp
    src/pages/PatientRecordPage.cpp
    src/pages/PermissionAdminPage.cpp
    src/pages/PrescriptionPage.cpp
    src/pages/RegistrationPage.cpp
    src/pages/WaitingQueuePage.cpp
    src/pages/SchedulePage.cpp
    src/pages/StatisticsPage.cpp
    src/pages/OperationLogPage.cpp
)

target_include_directories(hospital_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(hospital_client
    PRIVATE
        HOSPITAL_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(hospital_client
    PRIVATE
        hospital_common
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::PrintSupport
)

if(TARGET Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
    target_compile_definitions(hospital_client PRIVATE HOSPITAL_HAS_WEBENGINE=1)
    target_link_libraries(hospital_client PRIVATE Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
endif()
