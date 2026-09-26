#include "camera/CameraError.h"

#include <cstdio>

namespace camera {

namespace {

/// MVS 错误码表。
///
/// 数值来自 SDK 的 MvErrorDefine.h，不能凭记忆写 —— 每个版本都可能新增。
/// 这里刻意不列全 SDK 定义的所有码（有上百个，其中大部分是采集卡专用），
/// 只覆盖面阵相机开发中真正会遇到的；查不到的码会在 ToString 里回退成
/// 带十六进制原值的字符串，信息不会丢。
struct ErrorEntry {
    uint32_t code;
    const char* text;
};

constexpr ErrorEntry kErrorTable[] = {
    { 0x00000000, "成功" },

    // —— SDK 通用错误 ——
    { 0x80000000, "错误或无效的句柄（对象已析构？或 CreateHandle 没成功就直接用了）" },
    { 0x80000001, "不支持的功能（该型号相机没有这个功能节点）" },
    { 0x80000002, "缓存已满" },
    { 0x80000003, "函数调用顺序错误（例如没打开设备就启流）" },
    { 0x80000004, "错误的参数（功能名拼错、或参数值类型不匹配）" },
    { 0x80000006, "资源申请失败（内存不足）" },
    { 0x80000007, "超时，未收到数据（相机没出图：检查触发模式、曝光、镜头盖）" },
    { 0x80000008, "前置条件有误，或运行环境已发生变化" },
    { 0x80000009, "版本不匹配（头文件与运行时 .so 版本不一致）" },
    { 0x8000000A, "内存空间不足（传入的缓冲区偏小）" },
    { 0x8000000B, "异常图像，可能因丢包导致不完整" },
    { 0x8000000C, "动态加载库失败（LD_LIBRARY_PATH / rpath 没配好）" },
    { 0x8000000D, "没有可输出的缓存" },
    { 0x8000000E, "加密错误" },
    { 0x8000000F, "打开文件出错" },
    { 0x80000010, "缓存地址已被使用" },
    { 0x80000011, "无效的缓存地址" },
    { 0x80000012, "缓存对齐异常" },
    { 0x80000013, "缓存个数不足" },
    { 0x80000014, "串口被占用" },
    { 0x80000015, "解码错误（SDK 校验图像异常）" },
    { 0x80000016, "图像大小超出 unsigned int 可表示范围，该接口不支持" },
    { 0x80000017, "图像高度异常，残帧被丢弃" },
    { 0x80000018, "采集卡 DDR 缓存不足" },
    { 0x80000019, "采集卡流通道不足" },
    { 0x8000001A, "设备无响应" },
    { 0x8000001B, "写文件异常" },
    { 0x8000001C, "读文件异常" },
    { 0x8000001D, "文件长度异常" },
    { 0x8000001E, "事件创建失败" },
    { 0x8000001F, "线程创建失败" },
    { 0x80000020, "设备已掉线（网线松动、相机断电）" },
    { 0x80000021, "设备不支持该操作" },
    { 0x80000022, "当前平台未实现该功能" },
    { 0x80000023, "设备串口缓存空间不足" },
    { 0x80000024, "流通道索引无效" },
    { 0x80000025, "参数超出范围限制（先用 range() 查一下上下限）" },
    { 0x80000026, "IO 资源异常" },
    { 0x80000027, "图像信息异常（宽、高、长度不合法）" },
    { 0x80000028, "请求的资源（采集卡、设备、流等）已被占用" },

    // —— 设备端拒绝 ——
    { 0x80000041, "命令在设备中未实现" },
    { 0x80000042, "命令参数无效或超出范围" },
    { 0x80000043, "尝试访问不存在的寄存器地址" },
    { 0x80000044, "尝试写入只读寄存器" },
};

constexpr const char* kUnknownErrorText = "未知错误码";

} // namespace

std::string ErrorCodeToString(int32_t code)
{
    const uint32_t raw = static_cast<uint32_t>(code);

    for (const ErrorEntry& entry : kErrorTable) {
        if (entry.code == raw) {
            return entry.text;
        }
    }

    // 表里没有也要把原值带出去：一个确切的十六进制码可以拿去查文档，
    // 而"未知错误"四个字什么都做不了。
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s 0x%08X", kUnknownErrorText, raw);
    return buffer;
}

CameraException::CameraException(int32_t code, const char* api)
    : std::runtime_error(buildMessage(code, api))
    , m_code(code)
    , m_api(api)
{
}

std::string CameraException::buildMessage(int32_t code, const char* api)
{
    std::string message = "MVS 调用失败：";
    message += api;
    message += "() → ";
    message += ErrorCodeToString(code);

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), " (0x%08X)", static_cast<uint32_t>(code));
    message += buffer;

    return message;
}

void ThrowIfFailed(int32_t ret, const char* api)
{
    if (ret != 0) {  // MV_OK == 0
        throw CameraException(ret, api);
    }
}

} // namespace camera
