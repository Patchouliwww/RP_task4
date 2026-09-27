#include <camera/CameraInfo.h>

#include <cstdio>

namespace camera
{

std::string CameraInfo::summary() const
{
    std::string line = to_string(transport);
    line += " | ";

    if (!model_name.empty())
        line += model_name;
    else
        line += "<未知型号>";

    if (!serial_number.empty())
        line += " | SN " + serial_number;

    // 只在与该传输层相关时追加地址信息，避免打印一串空字段
    if (transport == TransportLayer::GigE || transport == TransportLayer::GenTLGigE)
    {
        line += " | IP " + (ip_address.empty() ? std::string("<未获取>") : ip_address);
    }
    else if (transport == TransportLayer::USB3 && usb_vendor_id != 0)
    {
        char buffer[32];
        std::snprintf(
            buffer, sizeof(buffer), " | VID:PID %04X:%04X", usb_vendor_id, usb_product_id
        );
        line += buffer;
    }

    if (!user_defined_name.empty())
        line += " | 名称 " + user_defined_name;

    return line;
}


std::string CameraInfo::identity() const
{
    // 优先用户自定义名：实验室里通常按安装位置给相机改名，这是最不容易认错的标识。
    // 其次是序列号，全局唯一。最后才退到 IP —— 它是可变的（DHCP 或换网段后会变），
    // 只能当兜底。
    if (!user_defined_name.empty())
        return user_defined_name;

    if (!serial_number.empty())
        return serial_number;

    return ip_address;
}

} // namespace camera
