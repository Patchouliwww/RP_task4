/// 设备枚举示例。
///
/// 这是本工程唯一一个**没有相机也能正常运行**的程序：没插相机时它打印
/// “未发现设备”并以 0 退出。因此它可以当作环境自检工具 ——
/// 它跑通就说明 SDK 装好、头文件可见、库能链接、rpath 配对了。
///
/// 用法：
///   ./enumerate            枚举所有传输层
///   ./enumerate gige       只看网口相机
///   ./enumerate usb        只看 USB 相机
///
/// 退出码：
///   0  执行成功（包含“未发现设备”这种情况 —— 那不是错误）
///   1  参数错误
///   2  SDK 调用失败（异常会打印具体错误码）

#include <camera/Camera.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace
{

void print_usage(const char* program)
{
    std::cout << "用法: " << program << " [gige|usb|all]\n"
              << "  gige  只枚举网口相机\n"
              << "  usb   只枚举 USB3 相机\n"
              << "  all   枚举所有传输层（默认）\n";
}


bool parse_filter(const std::string& text, camera::TransportLayer& out)
{
    if (text == "all")
    {
        out = camera::TransportLayer::All;
        return true;
    }
    if (text == "gige")
    {
        out = camera::TransportLayer::GigE;
        return true;
    }
    if (text == "usb")
    {
        out = camera::TransportLayer::USB3;
        return true;
    }
    return false;
}


/// 4 字节版本号 → "V4.8.2.1"。
/// 编码方式：主|次|修正|测试，各占 8 bit。
std::string format_sdk_version(std::uint32_t version)
{
    char buffer[32];
    std::snprintf(
        buffer, sizeof(buffer), "V%u.%u.%u.%u",
        (version >> 24) & 0xFFu, (version >> 16) & 0xFFu,
        (version >> 8) & 0xFFu, version & 0xFFu
    );
    return buffer;
}


/// 实际的枚举逻辑。
///
/// 与 main 分开是为了让"释放 SDK 资源"这个必须执行的动作只有一处，
/// 并且不会被中途的 return 跳过。
int run_enumeration(camera::TransportLayer filter)
{
    try
    {
        std::cout << "MVS SDK 版本: " << format_sdk_version(camera::Camera::sdk_version())
                  << "\n";
        std::cout << "枚举传输层:   " << camera::to_string(filter) << "\n\n";

        const std::vector<camera::DeviceDescriptor> devices =
            camera::Camera::enumerate(filter);

        if (devices.empty())
        {
            // 注意：这不是错误。没有相机时枚举到 0 台是预期结果，
            // 也是验证 SDK 环境是否正常的最简方式。
            std::cout << "未发现设备。\n";
            if (filter == camera::TransportLayer::All)
            {
                std::cout << "若相机已接上却看不到，按顺序排查：\n"
                          << "  1. 网口相机：网卡是否与相机同网段、MTU 是否 9000、\n"
                          << "     是否执行过 MVS 的 set_rp_filter.sh（多网卡必做）\n"
                          << "  2. USB 相机：是否插在蓝色 USB3 口，lsusb -t 是否显示 5000M\n"
                          << "  3. 先用 MVS 客户端（/opt/MVS/bin/MVS.sh）确认设备可见\n";
            }
            return 0;
        }

        std::cout << "发现 " << devices.size() << " 台设备：\n\n";

        for (std::size_t i = 0; i < devices.size(); ++i)
        {
            const camera::DeviceDescriptor& device = devices[i];
            const camera::CameraInfo& info = device.info;

            std::cout << "[" << i << "] " << info.summary() << "\n";

            if (!info.device_version.empty())
                std::cout << "    固件版本: " << info.device_version << "\n";

            if (!info.vendor_name.empty())
                std::cout << "    制造商:   " << info.vendor_name << "\n";

            if (!info.host_ip.empty() && info.host_ip != "0.0.0.0")
                std::cout << "    主机网口: " << info.host_ip << "\n";

            // 打开前先问一句能否访问。设备被 MVS 客户端以独占方式打开时
            // 这里会是 false，能把原因提前说清楚，不必等 OpenDevice 失败后靠猜。
            const bool accessible = camera::Camera::is_accessible(device);
            std::cout << "    可访问:   "
                      << (accessible ? "是" : "否（可能已被其他程序占用）") << "\n\n";
        }

        return 0;
    }
    catch (const camera::CameraException& error)
    {
        std::cerr << "\n相机错误: " << error.what() << "\n";
        return 2;
    }
    catch (const std::exception& error)
    {
        std::cerr << "\n未预期的异常: " << error.what() << "\n";
        return 2;
    }
}

} // namespace

int main(int argc, char** argv)
{
    camera::TransportLayer filter = camera::TransportLayer::All;

    if (argc > 1)
    {
        const std::string argument = argv[1];
        if (argument == "-h" || argument == "--help")
        {
            print_usage(argv[0]);
            return 0;
        }
        if (!parse_filter(argument, filter))
        {
            std::cerr << "未知参数: " << argument << "\n\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    // 参数解析在动用 SDK 之前完成，所以上面几个提前返回不需要反初始化。

    const int result = run_enumeration(filter);

    // MVS 要求 MV_CC_Finalize 在 main 返回之前调用，这个动作不能省
    // （原因见 Camera::shutdown_sdk 的注释：放到静态对象析构里会在退出时段错误）。
    camera::Camera::shutdown_sdk();

    return result;
}
