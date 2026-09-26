cmake_minimum_required(VERSION 3.16)

# 只启用 C++，省掉一个用不到的 C 编译器配置
project(camera_wrapper LANGUAGES CXX)

# 显式钉住标准，让 clangd 从 compile_commands.json 读到的参数与 g++ 实际用的完全一致
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成 compile_commands.json，clangd 智能感知的唯一依据
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 产物统一放到 build/bin 与 build/lib，示例程序不必去猜路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 相机 SDK 的参数类型又窄又杂（unsigned int / unsigned short / unsigned char 混用），
# 隐式转换很容易写出看不出来的 bug，靠警告提前拦下来
add_compile_options(-Wall -Wextra)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# 海康 MVS SDK：头文件 + libMvCameraControl.so
find_package(MVS REQUIRED)

# 本机 OpenCV 4.6.0 为源码编译安装，位于 /usr/local，无 .pc 文件，只能走 config 模式
find_package(OpenCV 4.6 REQUIRED)

add_subdirectory(src)
add_subdirectory(apps)
