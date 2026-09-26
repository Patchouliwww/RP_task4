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

产物在 `build/lib/libcamera_wrapper.so` 与 `build/bin/`。

## 先跑这个：环境自检

```bash
./build/bin/enumerate          # 枚举所有传输层
./build/bin/enumerate gige     # 只看网口相机
./build/bin/enumerate usb      # 只看 USB 相机
```

没插相机时它输出「未发现设备」并以 0 退出 —— **这是正确结果，不是失败**。
它同时证明了 SDK 装好、头文件可见、库能链接、rpath 配好了，
是排查"代码跑不起来"时应当最先执行的一步。

不需要 `LD_LIBRARY_PATH`：SDK 库路径已经通过 `-Wl,-rpath` 写进可执行文件
（`readelf -d build/bin/enumerate` 可以看到 RUNPATH 里的 `/opt/MVS/lib/64`）。
这一点很关键：非交互式 shell 不读 `~/.bashrc`，所以在 IDE、CMake、脚本里
那个环境变量都是空的，靠它会在点"运行"的时候突然报找不到 `.so`。

## 本机 SDK 与参考资料

| 位置 | 内容 |
| --- | --- |
| `/opt/MVS/include/` | 头文件：`MvCameraControl.h`、`CameraParams.h`、`MvErrorDefine.h`、`PixelType.h` |
| `/opt/MVS/lib/64/` | `libMvCameraControl.so`（当前 4.8.2.2） |
| `/opt/MVS/doc/` | **官方开发文档**，含《工业相机Linux SDK开发指南（C）》中文版 |
| `/opt/MVS/Samples/64/C++/` | 官方样例。`General/GrabImage`（轮询取图）、`General/GrabImage_Callback`（回调）、`General/ImageSave`、`General/ParametrizeCamera_LoadAndSave`、`AreaScanCamera/SetParam`、`AreaScanCamera/Trigger_Image` |
| `/opt/MVS/bin/MVS.sh` | 图形客户端。排查设备问题时先开它确认设备可见，再跑自己的代码 |

## 目录结构

```
task4/
├── CMakeLists.txt        顶层：C++17、导出 compile_commands.json、-Wall -Wextra
├── cmake/FindMVS.cmake   定位 MVS SDK；把头文件设为导入目标的系统包含目录，
│                         rpath 也在这里写进去
├── include/camera/       对外头文件（不出现任何 MVS 类型）
│   ├── CameraTypes.h     传输层、权限级别、像素格式枚举
│   ├── CameraError.h     错误码 → 中文说明；CameraException
│   ├── CameraInfo.h      设备描述 CameraInfo / DeviceDescriptor
│   ├── Frame.h           一帧图像、帧率统计
│   └── Camera.h          主类接口
├── src/                  实现，MVS 头文件只在这一层出现
│   ├── MvsMapping.h      本库枚举 ↔ SDK 宏 的转换（私有头）
│   ├── Camera.cpp        句柄 RAII、枚举、设备生命周期
│   ├── CameraInfo.cpp    设备信息格式化
│   ├── CameraError.cpp   错误码表
│   └── CameraTypes.cpp   枚举与字符串转换
├── apps/                 示例程序
│   └── enumerate.cpp     设备枚举（无相机即可运行）
└── docs/                 设计与操作文档
```

## 相关文档

- [`docs/GIT_WORKFLOW.md`](docs/GIT_WORKFLOW.md) —— git 日常操作速查表与提交规范

## 进度

- [x] git 仓库初始化、身份与提交规范配置
- [x] CMake 工程骨架 + clangd / VSCode 智能感知配置
- [x] MVS SDK 安装与环境验证（官方样例编译运行通过）
- [x] 设备枚举程序（无相机即可运行验证）
- [x] `Camera` 类接口设计、错误码映射、句柄 RAII 生命周期
- [ ] 参数读写（曝光 / 增益 / 帧率 / 触发）
- [ ] 取流线程与帧队列
- [ ] 像素格式转换（Bayer → BGR8）
- [ ] 接入相机联调、录制取图视频
- [ ] 设计文档与逐块讲解文档

