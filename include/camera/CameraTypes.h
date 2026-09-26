#pragma once

#include <cstdint>
#include <string>

namespace camera {

/// 相机与主机之间的传输层协议。
///
/// 取值刻意不复用 SDK 的 MV_*_DEVICE 宏，而是自己定义一套：
/// 使用方只依赖本头文件，不需要知道 MVS 的存在。两边的对应关系集中在
/// src/CameraTypes.cpp 里，改 SDK 或换品牌时只动那一处。
enum class TransportLayer {
    Unknown = 0,
    GigE,        ///< GigE Vision：普通网口相机
    USB3,        ///< USB3 Vision
    GenTLGigE,   ///< 自研网卡下的 GigE 相机（走 GenTL 协议）
    CameraLink,
    CoaXPress,
    XoF,
    XoC,
    Virtual,     ///< 采集卡上的虚拟设备
    All,         ///< 不是真实协议，仅作为 enumerate 的“不过滤”参数
};

std::string ToString(TransportLayer layer);

/// 打开设备时申请的权限级别。
///
/// 工业相机允许多个进程同时连接，级别决定别的程序能做什么。
/// 数值与 SDK 的 MV_ACCESS_xxx 保持一致，方便直接转换。
enum class AccessMode {
    Exclusive = 1,                  ///< 独占：其他程序只能读 CCP 寄存器
    ExclusiveWithSwitch = 2,        ///< 可从级别 5 抢占后以独占方式打开
    Control = 3,                    ///< 控制：其他程序可读全部寄存器
    ControlWithSwitch = 4,          ///< 可从级别 5 抢占后以控制方式打开
    ControlSwitchEnable = 5,        ///< 以“可被抢占”的控制权限打开
    ControlSwitchEnableWithKey = 6, ///< 可从级别 5 抢占后以可被抢占方式打开
    Monitor = 7,                    ///< 只读监视：适合设备已被别人控制时旁观
};

std::string ToString(AccessMode mode);

/// 像素格式。
///
/// 相机支持的格式有上百种（含各种位深、打包方式、Bayer 排列），
/// 这里只列实际会用到的常见子集。遇到表里没有的格式统一落到 Unknown，
/// 由调用方决定是报错还是按原始字节处理 —— 不静默当作 Mono8 处理。
enum class PixelFormat {
    Unknown = 0,
    Mono8,          ///< 8 位灰度，可直接交给 OpenCV 当 CV_8UC1
    Mono10,
    Mono12,
    Mono16,
    BayerRG8,       ///< 常见于彩色相机默认输出，需要做去马赛克
    BayerGB8,
    BayerGR8,
    BayerBG8,
    BayerRG10,
    BayerRG12,
    RGB8,
    BGR8,           ///< OpenCV 的原生顺序，imshow 需要它
    RGBA8,
    BGRA8,
    YUV422_8,
};

std::string ToString(PixelFormat format);

/// 是否为彩色格式（Bayer 或 RGB 系）。
bool IsColorFormat(PixelFormat format);

/// 是否为 Bayer 格式（必须做去马赛克才能正确显示）。
bool IsBayerFormat(PixelFormat format);

} // namespace camera
