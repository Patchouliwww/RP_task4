#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <camera/CameraError.h>
#include <camera/CameraInfo.h>
#include <camera/CameraTypes.h>
#include <camera/Frame.h>

namespace camera
{

/// 打开设备时的可选参数。纯数据载体，成员保持 public。
struct OpenOptions
{
    /// 权限级别。独占是默认值，也是最常见的选择；
    /// 如果设备已被 MVS 客户端以控制权限打开，可改用 Monitor 旁观。
    AccessMode access = AccessMode::Exclusive;

    /// 是否把网口相机的包大小调到 SDK 推荐的最优值。
    /// 默认开启：网口相机不调这个，高分辨率下丢包会非常严重。
    /// 对 USB 相机无影响。
    bool optimize_packet_size = true;

    /// SDK 内部图像缓存节点数。
    /// 太小会在主机偶尔卡顿时丢帧，太大则增加端到端延迟。3 是常见折中。
    unsigned image_node_num = 3;
};

/// 一个相机参数的取值范围与当前值。纯数据载体，成员保持 public。
///
/// 下发参数前先查这个能避免静默失败：例如帧率上限受曝光时间约束，
/// 直接把 ExposureTime 设到 100ms 再想把帧率设到 30fps，相机会拒绝后者，
/// 而拒绝是"返回错误码"还是"悄悄改成别的值"因参数而异。
struct ParamRange
{
    double min = 0.0;
    double max = 0.0;
    double step = 0.0;   ///< 最小增量；部分参数为 0 表示连续可调
    double current = 0.0;
};

/// 一台海康工业相机的封装。
///
/// 职责：管理一个 SDK 相机句柄，提供取图与参数读写。
///
/// 三条不可让步的设计：
///
/// 1. **句柄生命周期与对象生命周期绑定（RAII）**
///    构造即打开、析构即关闭。不需要 open()/close() 成对调用，
///    因此也不存在"忘了关"或"异常路径漏了关"的可能。
///    析构顺序是 StopGrabbing → CloseDevice → DestroyHandle —— 顺序错了
///    就是取流线程访问已释放句柄的崩溃。
///
/// 2. **SDK 类型不外泄（Pimpl）**
///    本头文件不含任何 MVS 类型，使用方不需要（也无法）拿到
///    MvCameraControl.h。MVS 的全部调用被关在 Camera.cpp 里，
///    换 SDK 版本或换品牌时受影响的只有那一个文件。
///
/// 3. **取流用自有线程，不用 SDK 回调**
///    MV_CC_RegisterImageCallBackEx 的回调运行在 SDK 自己的线程里：
///    在里面不能阻塞（会拖慢 SDK 收流）、不能抛异常（异常穿过 C 边界是
///    未定义行为），而且回调与对象析构之间存在竞争 —— 对象正在销毁时回调
///    可能仍在执行。改用自有线程轮询 MV_CC_GetImageBuffer，把线程的启停时序
///    完全掌握在类内部。
class Camera
{
public:
    // ─────────────────────────── 静态接口：无需实例 ───────────────────────────

    /// 枚举在线相机。
    ///
    /// @param filter 只要某一类传输层的设备，默认 All 表示不过滤。
    /// @return 设备描述列表。**没有相机时返回空列表，这是正常结果而非错误** ——
    ///         调用方不该把它当异常处理。
    static std::vector<DeviceDescriptor> enumerate(
        TransportLayer filter = TransportLayer::All
    );

    /// 指定设备当前是否可访问（未被别的程序以更高权限占用）。
    ///
    /// 打开设备前先问一句，比直接 OpenDevice 失败后再去猜原因省事得多：
    /// 失败时能区分"设备被 MVS 客户端占着"和"设备/驱动有问题"。
    static bool is_accessible(
        const DeviceDescriptor& device,
        AccessMode mode = AccessMode::Exclusive
    );

    /// SDK 版本号。返回 4 字节，如 0x04080201 表示 V4.8.2.1。
    static std::uint32_t sdk_version();

    /// 释放 SDK 的全局资源。
    ///
    /// **必须在 main 返回之前调用一次。** 这不是本库的约定，而是 MVS 的要求：
    /// MV_CC_Finalize 的文档原文是「main函数退出前调用」。
    ///
    /// 为什么不能替你做这件事（例如放进静态对象的析构函数里自动执行）：
    ///   实测过，把它放在静态对象析构里必定段错误 —— 那份代码运行时 main 已经
    ///   返回，进程正在退出，各动态库的析构顺序不受控，此时进 SDK 会踩到已经被
    ///   拆掉的生产者库（崩在 MvFGProducerGEV.cti 的 GCCloseLib 里）。
    ///   崩溃点看起来和相机毫无关系，排查代价很高。
    ///
    /// 因此本库的选择是：初始化做到全自动（第一次用任何接口时自动完成），
    /// 反初始化保留为一次显式调用，并在所有示例程序的 main 末尾示范。
    /// 忘记调用不会崩溃，只是 SDK 的全局资源由操作系统在进程退出时回收。
    ///
    /// 可以安全地重复调用。调用之后如果继续使用相机接口，会自动重新初始化。
    static void shutdown_sdk();

    // ────────────────────────────── 生命周期 ──────────────────────────────

    /// 打开指定设备。失败抛 CameraException。
    /// 典型失败原因：设备被人独占、网段不通、USB 权限不足。
    explicit Camera(
        const DeviceDescriptor& device,
        const OpenOptions& options = {}
    );

    /// 停止取流（如果正在取）→ 关闭设备 → 释放句柄。
    /// 平凡析构：内部已处理所有清理，不会向外抛异常。
    ~Camera();

    // 一个对象 = 一台相机 = 一个句柄。
    // 若允许拷贝，两个对象析构时会重复销毁同一个句柄 —— 直接禁用。
    // 移动也不提供：取流线程持有 this 指针，移动会让它指向已失效的对象。
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    Camera(Camera&&) = delete;
    Camera& operator=(Camera&&) = delete;

    // ─────────────────────────────── 状态 ───────────────────────────────

    const CameraInfo& info() const;

    /// 设备是否仍在连接。相机掉线后（拔出网线、断电）会变 false。
    bool is_connected() const;

    bool is_grabbing() const;

    /// 设备是否支持某个功能节点，如 "ExposureTime"、"GevSCPSPacketSize"。
    /// 不同型号支持的参数不一样，调用前先问一句可以避免盲目下发。
    bool has_feature(const std::string& node) const;

    // ────────────────────────────── 参数读写 ──────────────────────────────
    //
    // 统一约定：set 之后会读回相机实际生效的值。
    // 原因是相机可能因为参数间约束（曝光与帧率互相限制）而拒绝或调整请求值，
    // 只发不读会出现"我以为设成了 10ms，实际还是 30ms"这种静默偏差。

    void set_exposure_time(double microseconds);
    double exposure_time() const;

    void set_gain(double decibel);
    double gain() const;

    void set_frame_rate(double fps);
    double frame_rate() const;

    void set_auto_exposure(bool enable);
    void set_auto_gain(bool enable);

    /// 一次性自动曝光/增益调整（相当于按一下"自动"按钮后锁定结果）。
    void set_auto_exposure_once();

    void set_pixel_format(PixelFormat format);
    PixelFormat pixel_format() const;

    void set_trigger_mode(bool enable);      ///< 是否启用触发模式
    bool trigger_mode() const;

    void set_trigger_source(const std::string& source);  ///< "Software" / "Line0" ...
    /// 软触发一次（仅在触发模式且源为 Software 时有效）。
    void trigger_once();

    /// 查询参数范围与当前值。node 是相机功能名，如 "ExposureTime"。
    /// 数值型参数用这个，无需为每个参数单独写一个 range 接口。
    ParamRange range(const std::string& node) const;

    // ─────────────────────────────── 取流 ───────────────────────────────

    /// 启动取流：起一个自有线程不断把帧从 SDK 取到内部队列。
    /// 重复调用是安全的（已在取流则直接返回）。
    /// 注意：进入取流状态后再改像素格式、宽高等采集相关参数通常会被相机拒绝，
    /// 需要先 stop_grabbing()。
    void start_grabbing();

    /// 停止取流并回收线程。
    /// 阻塞直到取流线程真正退出 —— 析构里依赖这一点来保证句柄安全。
    void stop_grabbing();

    /// 阻塞取一帧，最多等 timeout。超时返回的 Frame 为 false（image 为空）。
    /// 需要先 start_grabbing()。
    Frame get_frame(std::chrono::milliseconds timeout = std::chrono::seconds(1));

    /// 取最新一帧，不阻塞：队列为空时立刻返回无效 Frame。
    /// 适合"只关心当前画面、宁可丢帧也不要延迟"的显示循环。
    Frame get_latest_frame();

    /// 注册帧回调。回调在**取流线程**里执行，因此必须快速返回：
    /// 不要在里面做 imshow、写文件等耗时操作，否则会拖慢取流导致丢帧。
    /// 传 nullptr 取消注册。
    void set_frame_callback(std::function<void(const Frame&)> callback);

    /// 自上次 start_grabbing 起的统计信息。
    FrameStatistics statistics() const;

    /// 清空内部队列里积压的帧，避免重新开始时消费到旧画面。
    void flush_queue();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace camera
