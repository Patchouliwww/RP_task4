#include <camera/Camera.h>

#include "MvsMapping.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include <MvCameraControl.h>

namespace camera
{

namespace
{

// ───────────────────────── SDK 进程级初始化 ─────────────────────────

/// 一对按"进程"而非按"句柄"计数的函数：MV_CC_Initialize / MV_CC_Finalize。
///
/// 为什么需要这个类：
///   SDK 内部有全局资源需要显式初始化。如果把它散落在每个公开接口里，
///   就会遇到"谁负责初始化、反复调用怎么办"这一连串问题。集中到一个对象里最省事。
class SdkRuntime
{
public:
    /// 整个进程唯一的实例。
    /// 函数内静态对象在 C++11 起保证初始化线程安全，多线程同时首次调用
    /// 也只会构造一次。
    static SdkRuntime& instance()
    {
        static SdkRuntime runtime;
        return runtime;
    }

    /// 释放 SDK 全局资源。反复调用安全。
    static void shutdown() { instance().do_shutdown(); }

    /// 确保 SDK 已初始化，第一次调用时真正初始化。
    void ensure_initialized()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized)
            return;

        // 初始化失败就抛异常，且保持 m_initialized 为 false ——
        // 于是"SDK 坏了"会稳定地表现为每次调用都抛错，
        // 而不是悄悄用未初始化的状态继续跑。
        throw_if_failed(MV_CC_Initialize(), "MV_CC_Initialize");
        m_version = MV_CC_GetSDKVersion();
        m_initialized = true;
    }

    std::uint32_t version() const noexcept { return m_version; }

private:
    // 构造与析构都刻意留空。特别是析构里**不能**调用 MV_CC_Finalize：
    //   实测过，把它放在静态对象的析构里必定段错误 —— 那份代码执行时 main 已经
    //   返回，进程正在退出，各动态库的析构顺序不受控，此时进 SDK 会踩到已被拆掉的
    //   生产者库（崩在 MvFGProducerGEV.cti 的 GCCloseLib 里）。
    //   而 MVS 文档对 MV_CC_Finalize 的要求本就是「main函数退出前调用」。
    // 所以遵守契约：反初始化由 Camera::shutdown_sdk() 显式触发。
    SdkRuntime() = default;
    ~SdkRuntime() = default;

    void do_shutdown()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized)
            return;

        MV_CC_Finalize();
        m_initialized = false;
    }

    std::mutex m_mutex;
    bool m_initialized = false;
    std::uint32_t m_version = 0;
};


/// 取一个已完成初始化的 SDK 实例。所有要调用 SDK 的接口都先走这一步。
SdkRuntime& sdk_runtime()
{
    SdkRuntime& runtime = SdkRuntime::instance();
    runtime.ensure_initialized();
    return runtime;
}


// ─────────────────── SDK 结构体 → 本库结构 ───────────────────

/// 定长字符数组 → std::string。
///
/// SDK 里的名字字段是 unsigned char[N]，可能刚好填满而没有结尾的 '\0'。
/// 直接当 C 字符串用会读越界，所以必须显式带上长度、并在 NUL 处停下。
std::string fixed_string(const unsigned char* data, std::size_t size)
{
    std::size_t length = 0;
    while (length < size && data[length] != '\0')
        ++length;

    return std::string(reinterpret_cast<const char*>(data), length);
}


/// SDK 的 IP 表示（unsigned int）→ 点分十进制字符串。
///
/// 字节序按官方样例的做法：最高位字节是 IP 的第一段，
/// 即 0xC0A8010A 表示 192.168.1.10。不能想当然按主机字节序处理，
/// 否则打印出来的地址恰好反着，很容易被误判成"网段没配对"。
std::string ip_to_string(unsigned int ip)
{
    char buffer[16];
    std::snprintf(
        buffer, sizeof(buffer), "%u.%u.%u.%u",
        (ip >> 24) & 0xFFu, (ip >> 16) & 0xFFu, (ip >> 8) & 0xFFu, ip & 0xFFu
    );
    return buffer;
}


/// MV_CC_DEVICE_INFO → CameraInfo。
///
/// 两种传输层的字段名和长度都不一样（GigE 的序列号只有 16 字节，USB 有 64；
/// GigE 没有设备索引号），所以必须按 nTLayerType 分支处理，不能"取公共字段"了事。
CameraInfo translate_device_info(const MV_CC_DEVICE_INFO& sdk_info)
{
    CameraInfo info;
    info.transport = detail::from_mvs_transport_layer(sdk_info.nTLayerType);

    switch (sdk_info.nTLayerType)
    {
    case MV_GIGE_DEVICE:
    case MV_GENTL_GIGE_DEVICE:
    {
        const MV_GIGE_DEVICE_INFO& gige = sdk_info.SpecialInfo.stGigEInfo;
        info.model_name = fixed_string(gige.chModelName, sizeof(gige.chModelName));
        info.serial_number = fixed_string(gige.chSerialNumber, sizeof(gige.chSerialNumber));
        info.user_defined_name =
            fixed_string(gige.chUserDefinedName, sizeof(gige.chUserDefinedName));
        info.device_version =
            fixed_string(gige.chDeviceVersion, sizeof(gige.chDeviceVersion));
        info.vendor_name =
            fixed_string(gige.chManufacturerName, sizeof(gige.chManufacturerName));
        info.ip_address = ip_to_string(gige.nCurrentIp);
        info.subnet_mask = ip_to_string(gige.nCurrentSubNetMask);
        info.gateway = ip_to_string(gige.nDefultGateWay);  // SDK 里拼写就是 Defult
        info.host_ip = ip_to_string(gige.nNetExport);
        break;
    }
    case MV_USB_DEVICE:
    {
        const MV_USB3_DEVICE_INFO& usb = sdk_info.SpecialInfo.stUsb3VInfo;
        info.model_name = fixed_string(usb.chModelName, sizeof(usb.chModelName));
        info.serial_number = fixed_string(usb.chSerialNumber, sizeof(usb.chSerialNumber));
        info.user_defined_name =
            fixed_string(usb.chUserDefinedName, sizeof(usb.chUserDefinedName));
        info.device_version =
            fixed_string(usb.chDeviceVersion, sizeof(usb.chDeviceVersion));
        info.vendor_name = fixed_string(usb.chVendorName, sizeof(usb.chVendorName));
        info.usb_vendor_id = usb.idVendor;
        info.usb_product_id = usb.idProduct;
        info.device_index = usb.nDeviceNumber;
        break;
    }
    default:
        // 采集卡类协议（CameraLink / CoaXPress / XoF / XoC）和虚拟设备：
        // 目前没有设备可验证其字段布局，因此不猜、不填 —— 只保留传输层类型。
        // 将来接上这类设备时在这里补，比现在凭结构体定义硬写更可靠。
        break;
    }

    return info;
}

} // namespace

// ───────────────────────────── 静态接口 ─────────────────────────────

std::uint32_t Camera::sdk_version()
{
    return sdk_runtime().version();
}


void Camera::shutdown_sdk()
{
    SdkRuntime::shutdown();
}


std::vector<DeviceDescriptor> Camera::enumerate(TransportLayer filter)
{
    sdk_runtime();  // 触发 SDK 初始化（第一次调用时生效）

    // 这个结构体有 256 个指针成员。{} 保证全部清零 ——
    // 不清零的话 nDeviceNum 会是栈上的随机值，直接导致越界访问。
    MV_CC_DEVICE_INFO_LIST sdk_list{};

    throw_if_failed(
        MV_CC_EnumDevices(detail::to_mvs_transport_layer(filter), &sdk_list),
        "MV_CC_EnumDevices"
    );

    std::vector<DeviceDescriptor> devices;
    devices.reserve(sdk_list.nDeviceNum);

    for (unsigned int i = 0; i < sdk_list.nDeviceNum; ++i)
    {
        const MV_CC_DEVICE_INFO* sdk_info = sdk_list.pDeviceInfo[i];
        // 正常情况不会为空；为空时跳过而不是解引用，
        // 免得 SDK 的异常状态变成我们的段错误。
        if (sdk_info == nullptr)
            continue;

        DeviceDescriptor device;
        device.info = translate_device_info(*sdk_info);

        // 把整个结构体按字节复制一份带走。
        // 原因：sdk_list 里的指针指向 SDK 内部缓存，其生命周期只保证到下一次
        // MV_CC_EnumDevices 调用为止。想"枚举一次、后面慢慢用"，就必须自己留底。
        device.raw.resize(sizeof(MV_CC_DEVICE_INFO));
        std::memcpy(device.raw.data(), sdk_info, sizeof(MV_CC_DEVICE_INFO));

        devices.push_back(std::move(device));
    }

    return devices;
}


bool Camera::is_accessible(const DeviceDescriptor& device, AccessMode mode)
{
    sdk_runtime();

    // 描述信息不完整说明这个 descriptor 不是由 enumerate() 产出的
    // （比如被手工构造、或跨 SDK 版本序列化过），没有可信答案，只能返回 false。
    if (device.raw.size() != sizeof(MV_CC_DEVICE_INFO))
        return false;

    MV_CC_DEVICE_INFO sdk_info{};
    std::memcpy(&sdk_info, device.raw.data(), sizeof(sdk_info));

    return MV_CC_IsDeviceAccessible(&sdk_info, detail::to_mvs_access_mode(mode));
}

// ───────────────────────────── 生命周期 ─────────────────────────────

struct Camera::Impl
{
    void* handle = nullptr;  ///< SDK 相机句柄；nullptr 表示未打开
    CameraInfo info;         ///< 设备信息快照

    /// 保护句柄。
    /// 取流线程接入后，它会与主线程并发访问句柄（主线程可能正在析构），
    /// 这里先把锁准备好，避免到时候忘记加。
    mutable std::mutex mutex;

    bool grabbing = false;  ///< 是否处于取流状态（取流实现在后续提交接入）
};


Camera::Camera(const DeviceDescriptor& device, const OpenOptions& options)
    : m_impl(std::make_unique<Impl>())
{
    sdk_runtime();

    if (device.raw.size() != sizeof(MV_CC_DEVICE_INFO))
        throw CameraException(MV_E_PARAMETER, "Camera::Camera");

    // 还原本地副本再交给 SDK。
    // 不能把 device.raw.data() 直接 reinterpret_cast 成 MV_CC_DEVICE_INFO*：
    // 该结构体含有联合体和多种整型，对齐要求高于 std::vector<std::uint8_t> 的
    // 默认分配对齐，强转后在部分平台上属于未定义行为。
    MV_CC_DEVICE_INFO sdk_info{};
    std::memcpy(&sdk_info, device.raw.data(), sizeof(sdk_info));

    throw_if_failed(MV_CC_CreateHandle(&m_impl->handle, &sdk_info), "MV_CC_CreateHandle");

    try
    {
        throw_if_failed(
            MV_CC_OpenDevice(m_impl->handle, detail::to_mvs_access_mode(options.access)),
            "MV_CC_OpenDevice"
        );
    }
    catch (...)
    {
        // 构造函数抛异常时析构函数不会被调用 —— 对象还不算"建成"。
        // 半成品的资源只能在这里自己回收，否则每失败一次就漏一个句柄，
        // 攒够几次之后相机就再也打不开了。
        MV_CC_DestroyHandle(m_impl->handle);
        m_impl->handle = nullptr;
        throw;
    }

    m_impl->info = device.info;

    // 打开之后重新读一次设备信息。
    // 原因：枚举拿到的是打开之前的快照。网口相机在打开过程中可能被重新分配 IP
    // （DHCP 或 IP 冲突时会这样），沿用旧快照会让后续按 IP 判断的逻辑失效。
    MV_CC_DEVICE_INFO opened_info{};
    if (MV_CC_GetDeviceInfo(m_impl->handle, &opened_info) == MV_OK)
    {
        CameraInfo refreshed = translate_device_info(opened_info);
        // 只在翻译成功时覆盖，避免把已知信息替换成空壳
        if (refreshed.transport != TransportLayer::Unknown)
            m_impl->info = std::move(refreshed);
    }

    // 网口相机：把包大小调到 SDK 算出的最优值。
    // 不做这一步，高分辨率下丢包会非常严重 —— 而且症状是"图像下半部分是花的"，
    // 很容易被误判成相机或网线的问题。
    const bool is_gige = (m_impl->info.transport == TransportLayer::GigE ||
                          m_impl->info.transport == TransportLayer::GenTLGigE);
    if (options.optimize_packet_size && is_gige)
    {
        const int optimal_packet_size = MV_CC_GetOptimalPacketSize(m_impl->handle);
        // 失败不算致命（部分相机不允许改这个值），只忽略返回值、不抛异常。
        if (optimal_packet_size > 0)
        {
            MV_CC_SetIntValue(
                m_impl->handle, "GevSCPSPacketSize",
                static_cast<unsigned int>(optimal_packet_size)
            );
        }
    }

    // SDK 内部缓存几帧。调小可以减少端到端延迟，调大可以抗住主机的短暂卡顿。
    if (options.image_node_num > 0)
        MV_CC_SetImageNodeNum(m_impl->handle, options.image_node_num);
}


Camera::~Camera()
{
    if (m_impl == nullptr || m_impl->handle == nullptr)
        return;

    // 析构顺序不能变：停取流 → 关设备 → 销毁句柄。
    // 顺序反了，取流线程会拿着已经销毁的句柄继续调 SDK，也就是访问已释放内存 ——
    // 症状是随机崩溃，而且崩溃点看起来跟问题毫无关系。
    //
    // "停取流"这一步在取流线程那次提交里接入（需要先让线程退出并 join）。
    // 目前还没有取流线程，因此这里直接关设备，步骤位置已经留好。
    MV_CC_CloseDevice(m_impl->handle);
    MV_CC_DestroyHandle(m_impl->handle);
    m_impl->handle = nullptr;
}


const CameraInfo& Camera::info() const
{
    return m_impl->info;
}


bool Camera::is_connected() const
{
    if (m_impl->handle == nullptr)
        return false;

    return MV_CC_IsDeviceConnected(m_impl->handle);
}

} // namespace camera
