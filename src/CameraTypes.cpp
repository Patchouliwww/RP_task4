#include "camera/CameraTypes.h"

#include "MvsMapping.h"

// MVS 头文件只允许出现在 src/ 下（这里是其中之一）。
// 公开头文件 include/camera/*.h 里一个 MVS 类型都不能有。
#include "MvCameraControl.h"

namespace camera {

// ───────────────────────── TransportLayer ─────────────────────────

std::string ToString(TransportLayer layer)
{
    switch (layer) {
    case TransportLayer::GigE:       return "GigE";
    case TransportLayer::USB3:       return "USB3";
    case TransportLayer::GenTLGigE:  return "GenTL-GigE";
    case TransportLayer::CameraLink: return "CameraLink";
    case TransportLayer::CoaXPress:  return "CoaXPress";
    case TransportLayer::XoF:        return "XoF";
    case TransportLayer::XoC:        return "XoC";
    case TransportLayer::Virtual:    return "Virtual";
    case TransportLayer::All:        return "All";
    case TransportLayer::Unknown:    break;
    }
    return "Unknown";
}

// ───────────────────────── AccessMode ─────────────────────────

std::string ToString(AccessMode mode)
{
    switch (mode) {
    case AccessMode::Exclusive:                  return "Exclusive";
    case AccessMode::ExclusiveWithSwitch:        return "ExclusiveWithSwitch";
    case AccessMode::Control:                    return "Control";
    case AccessMode::ControlWithSwitch:          return "ControlWithSwitch";
    case AccessMode::ControlSwitchEnable:        return "ControlSwitchEnable";
    case AccessMode::ControlSwitchEnableWithKey: return "ControlSwitchEnableWithKey";
    case AccessMode::Monitor:                    return "Monitor";
    }
    return "Unknown";
}

// ───────────────────────── PixelFormat ─────────────────────────

std::string ToString(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Mono8:     return "Mono8";
    case PixelFormat::Mono10:    return "Mono10";
    case PixelFormat::Mono12:    return "Mono12";
    case PixelFormat::Mono16:    return "Mono16";
    case PixelFormat::BayerRG8:  return "BayerRG8";
    case PixelFormat::BayerGB8:  return "BayerGB8";
    case PixelFormat::BayerGR8:  return "BayerGR8";
    case PixelFormat::BayerBG8:  return "BayerBG8";
    case PixelFormat::BayerRG10: return "BayerRG10";
    case PixelFormat::BayerRG12: return "BayerRG12";
    case PixelFormat::RGB8:      return "RGB8";
    case PixelFormat::BGR8:      return "BGR8";
    case PixelFormat::RGBA8:     return "RGBA8";
    case PixelFormat::BGRA8:     return "BGRA8";
    case PixelFormat::YUV422_8:  return "YUV422_8";
    case PixelFormat::Unknown:   break;
    }
    return "Unknown";
}

bool IsColorFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::BayerRG8:
    case PixelFormat::BayerGB8:
    case PixelFormat::BayerGR8:
    case PixelFormat::BayerBG8:
    case PixelFormat::BayerRG10:
    case PixelFormat::BayerRG12:
    case PixelFormat::RGB8:
    case PixelFormat::BGR8:
    case PixelFormat::RGBA8:
    case PixelFormat::BGRA8:
    case PixelFormat::YUV422_8:
        return true;
    default:
        return false;
    }
}

bool IsBayerFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::BayerRG8:
    case PixelFormat::BayerGB8:
    case PixelFormat::BayerGR8:
    case PixelFormat::BayerBG8:
    case PixelFormat::BayerRG10:
    case PixelFormat::BayerRG12:
        return true;
    default:
        return false;
    }
}

// ───────────────────────── 与 SDK 的映射 ─────────────────────────

namespace detail {

unsigned int ToMvsTransportLayer(TransportLayer layer)
{
    switch (layer) {
    case TransportLayer::GigE:       return MV_GIGE_DEVICE;
    case TransportLayer::USB3:       return MV_USB_DEVICE;
    case TransportLayer::GenTLGigE:  return MV_GENTL_GIGE_DEVICE;
    case TransportLayer::CameraLink: return MV_GENTL_CAMERALINK_DEVICE;
    case TransportLayer::CoaXPress:  return MV_GENTL_CXP_DEVICE;
    case TransportLayer::XoF:        return MV_GENTL_XOF_DEVICE;
    case TransportLayer::XoC:        return MV_GENTL_XOC_DEVICE;
    case TransportLayer::Virtual:    return MV_GENTL_VIR_DEVICE;
    case TransportLayer::Unknown:    return MV_GIGE_DEVICE | MV_USB_DEVICE;
    case TransportLayer::All:
        // 一并枚举 GenTL 各类协议：漏掉它们的后果是插了采集卡或自研网卡的相机
        // 时"设备明明接着却枚举不到"，且没有任何报错可查。
        return MV_GIGE_DEVICE | MV_USB_DEVICE | MV_GENTL_GIGE_DEVICE |
               MV_GENTL_CAMERALINK_DEVICE | MV_GENTL_CXP_DEVICE |
               MV_GENTL_XOF_DEVICE | MV_GENTL_XOC_DEVICE | MV_GENTL_VIR_DEVICE;
    }
    return MV_GIGE_DEVICE | MV_USB_DEVICE;
}

TransportLayer FromMvsTransportLayer(unsigned int tlayerType)
{
    switch (tlayerType) {
    case MV_GIGE_DEVICE:             return TransportLayer::GigE;
    case MV_USB_DEVICE:              return TransportLayer::USB3;
    case MV_GENTL_GIGE_DEVICE:       return TransportLayer::GenTLGigE;
    case MV_GENTL_CAMERALINK_DEVICE: return TransportLayer::CameraLink;
    case MV_GENTL_CXP_DEVICE:        return TransportLayer::CoaXPress;
    case MV_GENTL_XOF_DEVICE:        return TransportLayer::XoF;
    case MV_GENTL_XOC_DEVICE:        return TransportLayer::XoC;
    case MV_GENTL_VIR_DEVICE:        return TransportLayer::Virtual;
    default:                         return TransportLayer::Unknown;
    }
}

unsigned int ToMvsAccessMode(AccessMode mode)
{
    return static_cast<unsigned int>(mode);
}

} // namespace detail
} // namespace camera
