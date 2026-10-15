# Popup placement probe for docs/wayland.md; built by run.sh, not part of the Patchy build.
cmake_minimum_required(VERSION 3.16)
project(wayland_popup_probe CXX)
set(CMAKE_CXX_STANDARD 17)
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)
# Qt 6.10+ splits the private module out; older Qt provides it with Gui.
find_package(Qt6 QUIET COMPONENTS GuiPrivate)
set(PATCHY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
add_executable(popup_probe main.cpp ${PATCHY_SRC}/ui/wayland_popup_anchor.cpp)
target_include_directories(popup_probe PRIVATE ${PATCHY_SRC})
target_link_libraries(popup_probe Qt6::Widgets Qt6::GuiPrivate)
