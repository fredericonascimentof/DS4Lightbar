cmake_minimum_required(VERSION 3.20)

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "F:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "F:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain file")
endif()

if (WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "vcpkg target triplet")
endif()

set(QT_CREATOR_SKIP_VCPKG_SETUP ON CACHE BOOL "Disable Qt Creator package manager auto-setup")

project(DS4Lightbar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Widgets)
find_package(SDL3 CONFIG REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(DS4Lightbar
    main.cpp
    LightbarController.cpp
    LightbarController.h
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(DS4Lightbar PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

qt_add_qml_module(DS4Lightbar
    URI DS4Lightbar
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    RESOURCES
        assets/icon.png
)

if (TARGET SDL3::SDL3-shared)
    set(SDL3_LINK_TARGET SDL3::SDL3-shared)
else()
    set(SDL3_LINK_TARGET SDL3::SDL3)
endif()

target_link_libraries(DS4Lightbar PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    ${SDL3_LINK_TARGET}
)

if (WIN32)
    target_link_libraries(DS4Lightbar PRIVATE setupapi hid)
endif()

target_compile_definitions(DS4Lightbar PRIVATE
    DS4LIGHTBAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_definitions(DS4Lightbar PRIVATE
    $<$<CONFIG:Debug>:DS4LIGHTBAR_DEV_RELOAD>
)

add_custom_command(TARGET DS4Lightbar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:DS4Lightbar>/assets"
)

get_target_property(SDL3_TARGET_TYPE ${SDL3_LINK_TARGET} TYPE)
if (WIN32 AND NOT SDL3_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
    add_custom_command(TARGET DS4Lightbar POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${SDL3_LINK_TARGET}>
            $<TARGET_FILE_DIR:DS4Lightbar>
    )
endif()

find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt
    HINTS "${Qt6_DIR}/../../../bin"
)

if (WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET DS4Lightbar POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
            --no-translations
            "$<TARGET_FILE:DS4Lightbar>"
    )
endif()
