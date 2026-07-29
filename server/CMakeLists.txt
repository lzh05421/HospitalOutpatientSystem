include(FetchContent)

option(HOSPITAL_ENABLE_REDIS "Build Redis-backed booking demo support" ON)

if(HOSPITAL_ENABLE_REDIS)
    find_package(hiredis CONFIG QUIET)

    if(NOT TARGET hiredis::hiredis AND TARGET hiredis_static)
        add_library(hiredis::hiredis ALIAS hiredis_static)
    endif()

    if(NOT TARGET hiredis::hiredis AND TARGET hiredis)
        add_library(hiredis::hiredis ALIAS hiredis)
    endif()

    if(NOT TARGET hiredis::hiredis)
        message(STATUS "hiredis package not found; fetching hiredis from GitHub")
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version for legacy dependencies" FORCE)
        FetchContent_Declare(
            hiredis
            GIT_REPOSITORY https://github.com/redis/hiredis.git
            GIT_TAG v1.2.0
        )
        FetchContent_MakeAvailable(hiredis)

        if(TARGET hiredis_static)
            add_library(hiredis::hiredis ALIAS hiredis_static)
        elseif(TARGET hiredis)
            add_library(hiredis::hiredis ALIAS hiredis)
        endif()
    endif()

    if(NOT TARGET hiredis::hiredis)
        message(FATAL_ERROR "hiredis dependency is required but no CMake target was found")
    endif()
else()
    message(STATUS "Redis support disabled; building without hiredis dependency")
endif()

set(HOSPITAL_SERVER_CORE_SOURCES
    include/server/AppConfig.h
    include/server/AuthorizationService.h
    include/server/booking/BookingRepository.h
    include/server/booking/ScheduleService.h
    include/server/ClientConnection.h
    include/server/DatabaseManager.h
    include/server/DemoRepository.h
    include/server/HospitalServer.h
    include/server/RequestRouter.h
    include/server/RedisManager.h
    include/server/SessionManager.h
    include/server/ScheduleRuleEngine.h
    include/server/SqlJson.h
    include/server/WorkflowRules.h
    include/server/modules/AuthService.h
    include/server/modules/ModuleServices.h
    include/server/outbox/OutboxConsumer.h
    src/AppConfig.cpp
    src/AuthorizationService.cpp
    src/booking/BookingRepository.cpp
    src/booking/ScheduleService.cpp
    src/ClientConnection.cpp
    src/DatabaseManager.cpp
    src/DemoRepository.cpp
    src/HospitalServer.cpp
    src/RequestRouter.cpp
    src/RedisManager.cpp
    src/SessionManager.cpp
    src/ScheduleRuleEngine.cpp
    src/SqlJson.cpp
    src/WorkflowRules.cpp
    src/modules/AuthService.cpp
    src/modules/BillingService.cpp
    src/modules/ConsultationService.cpp
    src/modules/DashboardService.cpp
    src/modules/DepartmentService.cpp
    src/modules/DoctorService.cpp
    src/modules/ExaminationService.cpp
    src/modules/InventoryService.cpp
    src/modules/PatientService.cpp
    src/modules/PatientRecordService.cpp
    src/modules/PermissionAdminService.cpp
    src/modules/PrescriptionService.cpp
    src/modules/RegistrationService.cpp
    src/modules/ScheduleService.cpp
    src/modules/StatisticsService.cpp
    src/modules/OperationLogService.cpp
    src/outbox/OutboxConsumer.cpp
)

add_library(hospital_server_core STATIC
    ${HOSPITAL_SERVER_CORE_SOURCES}
)

target_include_directories(hospital_server_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(hospital_server_core
    PUBLIC
        hospital_common
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Sql
)

if(HOSPITAL_ENABLE_REDIS)
    target_compile_definitions(hospital_server_core PUBLIC HOSPITAL_ENABLE_REDIS=1)
    target_link_libraries(hospital_server_core PUBLIC hiredis::hiredis)
endif()

add_executable(hospital_server
    src/main.cpp
)

target_link_libraries(hospital_server
    PRIVATE
        hospital_server_core
)

if(WIN32 AND TARGET hiredis)
    add_custom_command(TARGET hospital_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:hiredis>
            $<TARGET_FILE_DIR:hospital_server>
        COMMENT "Copying hiredis runtime library next to hospital_server"
    )
endif()
