cmake_minimum_required(VERSION 3.16)

project(GPA_calculator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)   # 自动处理 Q_OBJECT 的 moc

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(GPA_calculator
    main.cpp
    GpaWindow.h
    GpaWindow.cpp
)

target_link_libraries(GPA_calculator PRIVATE Qt6::Widgets)

# Windows 下生成 GUI 程序（不弹出控制台窗口）
if (WIN32)
    set_target_properties(GPA_calculator PROPERTIES WIN32_EXECUTABLE ON)
endif()
