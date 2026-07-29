add_executable(hospital_launcher
    src/main.cpp
)

target_compile_definitions(hospital_launcher
    PRIVATE
        HOSPITAL_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(hospital_launcher
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
)
