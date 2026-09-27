#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <camera/CameraTypes.h>

namespace camera
{

/// 一台相机的静态描述信息，由枚举结果翻译而来。
///
/// 这里一个 MVS 类型都没有：上层代码只依赖本结构。将来换 SDK 版本或
/// 换相机品牌时，受影响的只有 src/ 里的翻译函数。
///
/// 本结构是纯数据载体（没有行为、没有需要维护的不变量），因此数据成员保持
/// public。这是对规范 2.4「数据成员必须 private/protected」的一处有意偏离，
/// 有意为之的理由：字段全部由 SDK 翻译而来、彼此独立、构造后即可整体读取，
/// 加一层 getter 只增加噪音而不保护任何东西。带行为与不变量的类
/// （Camera、CameraException、SdkRuntime）一律严格 private。
struct CameraInfo
{
    std::string model_name;        ///< 型号，如 MV-CA013-20GM
    std::string serial_number;     ///< 序列号
    std::string user_defined_name; ///< 用户自定义名（可在 MVS 客户端里改）
    std::string device_version;    ///< 固件版本
    std::string vendor_name;       ///< 制造商
    TransportLayer transport = TransportLayer::Unknown;

    // 以下两组字段按传输层二选一，不适用的一组保持为空/零。

    // —— GigE ——
    std::string ip_address;   ///< 相机当前 IP
    std::string subnet_mask;  ///< 相机子网掩码
    std::string gateway;      ///< 相机网关
    std::string host_ip;      ///< 相机所在网卡的主机 IP（判断接在哪个网口上）

    // —— USB3 ——
    std::uint32_t usb_vendor_id = 0;   ///< VID
    std::uint32_t usb_product_id = 0;  ///< PID
    std::uint32_t device_index = 0;    ///< 设备索引号（GigE 无此字段）

    /// 适合打印成一行的摘要，不含敏感或冗长字段。
    std::string summary() const;

    /// 在多个同型号设备里认出某一台：优先用户自定义名，其次序列号，最后 IP。
    /// 实验室里同时接好几台相机时，靠这个在多机程序里指定目标。
    std::string identity() const;
};

/// 打开设备所需的完整信息：人类可读的描述 + 原始设备信息字节块。
///
/// 为什么需要 raw：
///   MV_CC_CreateHandle 的入参是完整的 MV_CC_DEVICE_INFO —— 一个含联合体、
///   几百字节的结构体。若想把它放进公开头文件，使用方就必须 include MVS 头文件，
///   封装立刻就漏了。
///   所以这里用不透明的字节数组承载，由 src/ 负责填充和还原。
///   代价是每台设备多一次 sizeof(MV_CC_DEVICE_INFO) 的拷贝，
///   换来的是使用方完全不需要知道 MVS 的存在。
///
/// 注意：raw 的内容与 SDK 版本绑定，不要序列化到磁盘后跨 SDK 版本使用。
struct DeviceDescriptor
{
    CameraInfo info;
    std::vector<std::uint8_t> raw;
};

} // namespace camera
