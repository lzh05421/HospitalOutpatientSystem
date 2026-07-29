add_library(hospital_common STATIC
    src/Protocol.cpp
)

target_include_directories(hospital_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(hospital_common
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
)
