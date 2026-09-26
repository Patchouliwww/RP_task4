#include "camera/CameraInfo.h"

#include <cstdio>

namespace camera {

std::string CameraInfo::summary() const
{
    std::string line = ToString(transport);
    line += " | ";

    if (!modelName.empty()) {
        line += modelName;
    } else {
        line += "<未知型号>";
    }

    if (!serialNumber.empty()) {
        line += " | SN " + serialNumber;
    }

    // 只在与该传输层相关时追加地址信息，避免打印一串空字段
    if (transport == TransportLayer::GigE || transport == TransportLayer::GenTLGigE) {
        line += " | IP " + (ipAddress.empty() ? std::string("<未获取>") : ipAddress);
    } else if (transport == TransportLayer::USB3 && usbVendorId != 0) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), " | VID:PID %04X:%04X",
                      usbVendorId, usbProductId);
        line += buffer;
    }

    if (!userDefinedName.empty()) {
        line += " | 名称 " + userDefinedName;
    }

    return line;
}

std::string CameraInfo::identity() const
{
    // 优先用户自定义名：实验室里通常按安装位置给相机改名，这是最不容易认错的标识。
    // 其次是序列号，全局唯一。最后才退到 IP —— 它是可变的（DHCP 或换网段后会变），
    // 只能当兜底。
    if (!userDefinedName.empty()) {
        return userDefinedName;
    }
    if (!serialNumber.empty()) {
        return serialNumber;
    }
    return ipAddress;
}

} // namespace camera
