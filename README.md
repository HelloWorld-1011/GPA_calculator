# GPA 计算器

一个用 C++ / Qt6 写的图形界面 GPA 计算器。一次 **vibe coding 实践**：
由claude code辅助完成。

## 功能
- 按学期管理课程（课程名称 / 学分 / 成绩）
- 分别显示每学期与总计的 GPA、加权均分
- 数据自动保存，下次打开自动载入

## 绩点公式
GPA = 4.0 - 3 × (100 - 成绩)² / 1600

## 编译运行
需要 Qt 6 + MinGW，用 CMake 构建：
```
cmake -G Ninja -B build
cmake --build build
```
