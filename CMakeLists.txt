cmake_minimum_required(VERSION 3.16)
project(linux-power-tray LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt6 with fallback to Qt5
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Gui Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Gui Core REQUIRED)

# --- Main tray application ---
add_executable(linux-power-tray main.cpp)
target_link_libraries(linux-power-tray PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
)
install(TARGETS linux-power-tray DESTINATION bin)

# --- Privileged helper (no Qt dependency) ---
add_executable(set-governor set-governor.cpp)
install(TARGETS set-governor DESTINATION lib/linux-power-tray)

# --- Install desktop entry for application launcher ---
install(FILES linux-power-tray.desktop DESTINATION share/applications)
