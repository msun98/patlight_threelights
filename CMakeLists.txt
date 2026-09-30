cmake_minimum_required(VERSION 3.16)

project(patlite_lr6_qt
    VERSION 1.0.0
    DESCRIPTION "PATLITE LR6-USB signal tower controller (Qt Widgets)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 메타오브젝트 도구 자동 실행 (moc / uic / rcc)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# 의존성
# ---------------------------------------------------------------------------
# Qt6가 있으면 Qt6, 없으면 Qt5로 폴백
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# libusb-1.0 (pkg-config 기반 → IMPORTED target 사용)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

message(STATUS "Qt version    : ${QT_VERSION}")
message(STATUS "libusb version: ${LIBUSB_VERSION}")

# ---------------------------------------------------------------------------
# 타깃
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/PatliteTypes.h
    src/PatliteTypes.cpp
    src/PatliteDevice.h
    src/PatliteLR6.h
    src/PatliteLR6.cpp
    src/PatliteLR5Lan.h
    src/PatliteLR5Lan.cpp
    src/TowerWidget.h
    src/TowerWidget.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/MainWindow.ui
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    PkgConfig::LIBUSB
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# ---------------------------------------------------------------------------
# 설치
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES udev/99-patlite-lr6.rules
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/udev/rules.d
        OPTIONAL)
