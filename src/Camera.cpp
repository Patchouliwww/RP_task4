#include "camera/Camera.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

// MVS 头文件只在本文件（及 src/ 下其它文件）出现。
// 公开头文件 include/camera/*.h 里没有任何 MVS 类型 —— 这是整套封装的根基：
// 使用方不需要知道海康 SDK 的存在，将来替换 SDK 版本或品牌时，改动被限制在 src/。
#include "MvCameraControl.h"

#include "MvsMapping.h"

namespace camera {

namespace {

// ───────────────────────── SDK 进程级初始化 ─────────────────────────

/// SDK 进程级初始化。
///
/// 为什么需要它：
///   这对函数是按“进程”而非按“句柄”计数的，SDK 内部有全局资源需要显式初始化。
///   如果把它散落在每个公开接口里，就会遇到“谁负责初始化、反复调用怎么办”
///   这一连串问题。集中到一个对象里最省事。
///
/// 为什么析构函数里**不**调用 MV_CC_Finalize：
///   实测（见 apps/enumerate 的开发过程）把它放进静态对象析构必定段错误：
///   那份代码运行时 main 已经返回，进程正在退出，各动态库的析构顺序不受控，
///   此时进 SDK 会踩到已被拆掉的生产者库 —— 崩溃点在 MvFGProducerGEV.cti 的
///   GCCloseLib 里，和相机看起来毫无关系。
///   而 MVS 文档对 MV_CC_Finalize 的要求本来就是「main函数退出前调用」。
///   所以这里遵守契约：反初始化由 Camera::shutdownSdk() 显式触发。
class SdkRuntime {
public:
    static SdkRuntime& instance()
    {
        SdkRuntime* runtime = instancePtr();
        runtime->ensureInitialized();
        return *runtime;
    }

    /// 反复调用安全；调用后若再次使用接口会自动重新初始化。
    static void shutdown()
    {
        SdkRuntime& runtime = *instancePtr();
        std::lock_guard<std::mutex> lock(runtime.m_mutex);
        if (!runtime.m_initialized) {
            return;
        }
        MV_CC_Finalize();
        runtime.m_initialized = false;
    }

    /// SDK 版本号，4 字节：主|次|修正|测试 各占 8 bit。
    uint32_t version() { return m_version; }

private:
    SdkRuntime() = default;

    static SdkRuntime* instancePtr()
    {
        // 刻意用 new 而不是静态对象：
        //   * 静态对象会引入跨动态库的析构顺序问题（正是上面说的坑）；
        //   * 这个对象本身没有需要回收的资源，进程退出时由系统一并回收。
        // 换来的是“永远不会在退出阶段被调到”的确定性。
        // 注意：整个进程只应有这一个实例，因此 instance() 也必须走这个函数，
        // 不能各自声明一份函数内静态变量。
        static SdkRuntime* runtime = new SdkRuntime();
        return runtime;
    }

    void ensureInitialized()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) {
            return;
        }
        // 初始化失败就抛异常。此时 m_initialized 仍为 false，
        // 于是“SDK 坏了”会稳定地表现为每次调用都抛错，
        // 而不是悄悄用未初始化的状态继续跑。
        ThrowIfFailed(MV_CC_Initialize(), "MV_CC_Initialize");
        m_version = MV_CC_GetSDKVersion();
        m_initialized = true;
    }

    std::mutex m_mutex;
    bool m_initialized = false;
    uint32_t m_version = 0;
};

// ─────────────────── SDK 结构体 → 本库结构 ───────────────────

/// 定长字符数组 → std::string。
///
/// SDK 里的名字字段是 unsigned char[N]，可能刚好填满而没有结尾的 '\0'。
/// 直接当 C 字符串用会读越界，所以必须显式带上长度、并在 NUL 处停下。
std::string FixedString(const unsigned char* data, size_t size)
{
    size_t length = 0;
    while (length < size && data[length] != '\0') {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(data), length);
}

/// SDK 的 IP 表示（unsigned int）→ 点分十进制字符串。
///
/// 字节序按官方样例的做法：最高位字节是 IP 的第一段。
/// 即 0xC0A8010A 表示 192.168.1.10。这里不能想当然按主机字节序处理，
/// 否则打印出来的地址会恰好反着，而且很容易被误判成“网段没配对”。
std::string IpToString(unsigned int ip)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
                  (ip >> 24) & 0xFFu, (ip >> 16) & 0xFFu,
                  (ip >> 8) & 0xFFu, ip & 0xFFu);
    return buffer;
}

/// MV_CC_DEVICE_INFO → CameraInfo。
///
/// 两种传输层的字段名和长度都不一样（GigE 的序列号只有 16 字节，USB 有 64；
/// GigE 没有设备索引号），所以必须按 nTLayerType 分支处理，不能"取公共字段"了事。
CameraInfo TranslateDeviceInfo(const MV_CC_DEVICE_INFO& sdkInfo)
{
    CameraInfo info;
    info.transport = detail::FromMvsTransportLayer(sdkInfo.nTLayerType);

    switch (sdkInfo.nTLayerType) {
    case MV_GIGE_DEVICE:
    case MV_GENTL_GIGE_DEVICE: {
        const MV_GIGE_DEVICE_INFO& gigE = sdkInfo.SpecialInfo.stGigEInfo;
        info.modelName = FixedString(gigE.chModelName, sizeof(gigE.chModelName));
        info.serialNumber = FixedString(gigE.chSerialNumber, sizeof(gigE.chSerialNumber));
        info.userDefinedName =
            FixedString(gigE.chUserDefinedName, sizeof(gigE.chUserDefinedName));
        info.deviceVersion = FixedString(gigE.chDeviceVersion, sizeof(gigE.chDeviceVersion));
        info.vendorName =
            FixedString(gigE.chManufacturerName, sizeof(gigE.chManufacturerName));
        info.ipAddress = IpToString(gigE.nCurrentIp);
        info.subnetMask = IpToString(gigE.nCurrentSubNetMask);
        info.gateway = IpToString(gigE.nDefultGateWay);  // SDK 里的拼写就是 Defult
        info.hostIp = IpToString(gigE.nNetExport);
        break;
    }
    case MV_USB_DEVICE: {
        const MV_USB3_DEVICE_INFO& usb = sdkInfo.SpecialInfo.stUsb3VInfo;
        info.modelName = FixedString(usb.chModelName, sizeof(usb.chModelName));
        info.serialNumber = FixedString(usb.chSerialNumber, sizeof(usb.chSerialNumber));
        info.userDefinedName =
            FixedString(usb.chUserDefinedName, sizeof(usb.chUserDefinedName));
        info.deviceVersion = FixedString(usb.chDeviceVersion, sizeof(usb.chDeviceVersion));
        info.vendorName = FixedString(usb.chVendorName, sizeof(usb.chVendorName));
        info.usbVendorId = usb.idVendor;
        info.usbProductId = usb.idProduct;
        info.deviceIndex = usb.nDeviceNumber;
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

uint32_t Camera::sdkVersion()
{
    return SdkRuntime::instance().version();
}

void Camera::shutdownSdk()
{
    SdkRuntime::shutdown();
}

std::vector<DeviceDescriptor> Camera::enumerate(TransportLayer filter)
{
    // 触发 SDK 初始化（第一次调用时生效）
    SdkRuntime::instance();

    MV_CC_DEVICE_INFO_LIST sdkList;
    // 这个结构体有 256 个指针成员，必须清零。不 zero 的话
    // nDeviceNum 会是栈上的随机值，直接导致越界访问。
    std::memset(&sdkList, 0, sizeof(sdkList));

    ThrowIfFailed(
        MV_CC_EnumDevices(detail::ToMvsTransportLayer(filter), &sdkList),
        "MV_CC_EnumDevices");

    std::vector<DeviceDescriptor> devices;
    devices.reserve(sdkList.nDeviceNum);

    for (unsigned int i = 0; i < sdkList.nDeviceNum; ++i) {
        const MV_CC_DEVICE_INFO* sdkInfo = sdkList.pDeviceInfo[i];
        if (sdkInfo == nullptr) {
            // 正常情况不会出现；出现就跳过而不是解引用，
            // 免得 SDK 的异常状态变成我们的段错误。
            continue;
        }

        DeviceDescriptor device;
        device.info = TranslateDeviceInfo(*sdkInfo);

        // 把整个结构体按字节复制一份带走。
        // 原因：sdkList 里的指针指向 SDK 内部缓存，其生命周期只保证到下一次
        // MV_CC_EnumDevices 调用为止。想要"枚举一次、后面慢慢用"，就必须自己留底。
        device.raw.resize(sizeof(MV_CC_DEVICE_INFO));
        std::memcpy(device.raw.data(), sdkInfo, sizeof(MV_CC_DEVICE_INFO));

        devices.push_back(std::move(device));
    }

    return devices;
}

bool Camera::isAccessible(const DeviceDescriptor& device, AccessMode mode)
{
    SdkRuntime::instance();

    if (device.raw.size() != sizeof(MV_CC_DEVICE_INFO)) {
        // 描述信息不完整说明这个 descriptor 不是由 enumerate() 产出的
        // （比如被手工构造或跨 SDK 版本序列化过），没有可信答案，只能返回 false。
        return false;
    }

    MV_CC_DEVICE_INFO sdkInfo;
    std::memcpy(&sdkInfo, device.raw.data(), sizeof(sdkInfo));

    return MV_CC_IsDeviceAccessible(&sdkInfo, detail::ToMvsAccessMode(mode));
}

// ───────────────────────────── 生命周期 ─────────────────────────────

struct Camera::Impl {
    void* handle = nullptr;   ///< SDK 相机句柄；nullptr 表示未打开
    CameraInfo info;          ///< 设备信息快照

    /// 保护句柄。
    /// 取流线程接入后，它会与主线程并发访问句柄（主线程可能正在析构），
    /// 这里先把锁准备好，避免到时候忘记加。
    mutable std::mutex mutex;

    bool grabbing = false;    ///< 是否处于取流状态（取流实现在后续提交接入）
};

Camera::Camera(const DeviceDescriptor& device, const OpenOptions& options)
    : m_impl(new Impl())
{
    SdkRuntime::instance();

    if (device.raw.size() != sizeof(MV_CC_DEVICE_INFO)) {
        throw CameraException(MV_E_PARAMETER, "Camera::Camera");
    }

    // 还原本地副本再传给 SDK。
    // 不能把 device.raw.data() 直接 reinterpret_cast 成 MV_CC_DEVICE_INFO*：
    // 该结构体含有联合体和多种整型，对齐要求高于 std::vector<uint8_t> 的
    // 默认分配对齐，强转后在部分平台上属于未定义行为。
    MV_CC_DEVICE_INFO sdkInfo;
    std::memcpy(&sdkInfo, device.raw.data(), sizeof(sdkInfo));

    ThrowIfFailed(MV_CC_CreateHandle(&m_impl->handle, &sdkInfo), "MV_CC_CreateHandle");

    try {
        ThrowIfFailed(
            MV_CC_OpenDevice(m_impl->handle, detail::ToMvsAccessMode(options.access)),
            "MV_CC_OpenDevice");
    } catch (...) {
        // 构造函数抛异常时，析构函数不会被调用 —— 对象还不算"建成"。
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
    MV_CC_DEVICE_INFO openedInfo;
    std::memset(&openedInfo, 0, sizeof(openedInfo));
    if (MV_CC_GetDeviceInfo(m_impl->handle, &openedInfo) == MV_OK) {
        CameraInfo refreshed = TranslateDeviceInfo(openedInfo);
        // 只在翻译成功时覆盖，避免把已知信息替换成空壳
        if (refreshed.transport != TransportLayer::Unknown) {
            m_impl->info = std::move(refreshed);
        }
    }

    // 网口相机：把包大小调到 SDK 算出的最优值。
    // 不做这一步，高分辨率下丢包会非常严重 —— 而且症状是"图像下半部分是花的"，
    // 很容易被误判成相机或网线问题。
    const bool isGigE = (m_impl->info.transport == TransportLayer::GigE ||
                         m_impl->info.transport == TransportLayer::GenTLGigE);
    if (options.optimizePacketSize && isGigE) {
        const int optimal = MV_CC_GetOptimalPacketSize(m_impl->handle);
        if (optimal > 0) {
            // 失败不算致命（部分相机不允许改这个值），因此只忽略返回值不抛异常。
            MV_CC_SetIntValue(m_impl->handle, "GevSCPSPacketSize",
                              static_cast<unsigned int>(optimal));
        }
    }

    if (options.imageNodeNum > 0) {
        // SDK 内部缓存几帧。调小可以减少端到端延迟，调大可以抗住主机的短暂卡顿。
        MV_CC_SetImageNodeNum(m_impl->handle, options.imageNodeNum);
    }
}

Camera::~Camera()
{
    if (m_impl == nullptr || m_impl->handle == nullptr) {
        return;
    }

    // 析构顺序不能变：停取流 → 关设备 → 销毁句柄。
    // 顺序反了，取流线程会拿着已经销毁的句柄继续调 SDK，也就是访问已释放内存 ——
    // 症状是随机崩溃，且崩溃点看起来跟问题毫无关系。
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

bool Camera::isConnected() const
{
    if (m_impl->handle == nullptr) {
        return false;
    }
    return MV_CC_IsDeviceConnected(m_impl->handle);
}

} // namespace camera
