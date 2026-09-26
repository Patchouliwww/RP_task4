# Task4 —— 基于海康 MVS SDK 的相机类封装

用 C++ 把海康（HIKROBOT）MVS SDK 那套面向过程的 C 接口封装成一个面向对象的
`Camera` 类：一个类管理一台相机，把相机句柄的生命周期与对象的生命周期绑定。

## 任务要求

- 阅读海康官方文档中「示例程序」部分，掌握 MVS SDK 的调用方法；
- 用 C++ 封装 C 接口，对外只暴露更抽象、交互性更强的接口，而不是把
  `MV_CC_*` 函数原样转发一遍；
- 必须体现面向对象思想：**一个类管理一个相机，句柄生命周期与对象生命周期绑定**；
- 涉及多线程（取流）；
- 用 git 管理代码版本并上传 GitHub，提交要原子化、符合规范。

## 环境依赖

| 依赖 | 版本 / 位置 | 说明 |
| --- | --- | --- |
| MVS SDK | `/opt/MVS`（`include/` + `lib/64/libMvCameraControl.so`） | 海康官方 Linux 安装包，安装后才有头文件 |
| OpenCV | 4.6.0，`/usr/local` | 源码编译安装，走 config 模式 `find_package` |
| 编译器 | g++ 11 | 与 `.clangd` 里钉住的 libstdc++ 路径一致 |
| CMake | ≥ 3.10 | |
| C++ 标准 | C++17 | 在 CMake 中显式钉死，保证与 clangd 读到的参数一致 |

## 构建

```bash
cmake -S . -B build
cmake --build build -j4
```

> 工程骨架尚未搭建，以上命令在 CMakeLists.txt 落地后才可用。

运行时若 MVS 库不在系统搜索路径内，可以不依赖 `LD_LIBRARY_PATH`：
CMake 里用 `BUILD_RPATH` 指向 `/opt/MVS/lib/64`，同时在 `main()` 开头
`setenv("MVCAM_COMMON_RUNENV", ...)` 兜底。

## 计划中的目录结构

```
task4/
├── CMakeLists.txt        顶层：C++17、导出 compile_commands.json
├── cmake/FindMVS.cmake   定位 MVS SDK，找不到时给出清晰报错
├── include/camera/       对外头文件（不出现任何 MVS SDK 类型）
├── src/                  实现，MVS 头文件只在这里出现（Pimpl）
├── apps/                 示例程序：枚举设备、单帧取图、连续取图、参数读写
└── docs/                 设计文档、逐步讲解、git 操作手册
```

## 相关文档

- [`docs/GIT_WORKFLOW.md`](docs/GIT_WORKFLOW.md) —— git 日常操作速查表与提交规范

## 进度

- [x] git 仓库初始化、身份与提交规范配置
- [ ] CMake 工程骨架 + clangd / VSCode 智能感知配置
- [ ] MVS SDK 安装与环境验证
- [ ] 设备枚举程序（无相机即可运行验证）
- [ ] `Camera` 类接口设计与实现
- [ ] 取流线程与帧队列
- [ ] 参数读写与软触发
- [ ] 接入相机联调、录制取图视频
- [ ] 设计文档与提交记录整理
