#pragma once

// 本头文件是 src/ 内部用的，不会被安装、也不会被 apps/ 引用。
// 按规范 1.8，内部头文件用 "" 包含，且路径以当前源文件所在目录为起点。
//
// 职责：把本库自己的枚举与 MVS SDK 的宏互相转换。
// 这样做的好处是：MvCameraControl.h 只在 src/ 下出现，公开头文件与使用方
// 都不受 SDK 影响；将来换 SDK 版本或换品牌，只需要改这一层的对应关系。

#include <camera/CameraTypes.h>

namespace camera::detail
{

/// 本库的传输层枚举 → SDK 的 MV_*_DEVICE 位掩码。
/// All 会被展开成"所有已知协议"的或值，直接交给 MV_CC_EnumDevices。
unsigned int to_mvs_transport_layer(TransportLayer layer);

/// SDK 的 MV_*_DEVICE 值 → 本库的传输层枚举。
/// 认不出来的协议返回 Unknown，不猜、不落到某个默认值上。
TransportLayer from_mvs_transport_layer(unsigned int tlayer_type);

/// 本库的权限枚举 → SDK 的 MV_ACCESS_* 数值。
/// 两者的数值本来就是一一对应的，但显式转换能防止将来 SDK 调整取值。
unsigned int to_mvs_access_mode(AccessMode mode);

} // namespace camera::detail
