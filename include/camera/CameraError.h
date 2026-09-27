#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace camera
{

/// 把 MVS 错误码翻译成可读说明。
///
/// 表里没有的码不会被丢掉，而是返回带十六进制原值的字符串 —— 排查问题时
/// 一个确切的 0x80000004 比一句“未知错误”有用得多。
std::string error_code_to_string(std::int32_t code);

/// MVS C 接口调用失败时抛出的异常。
///
/// 设计取舍：C 接口全部用 int 返回错误码，而一次完整操作往往要串起
/// 5~8 个 API（枚举 → 建句柄 → 开设备 → 配参数 → 启流）。逐个 if 判断会让
/// 主体逻辑淹没在错误处理里，而且极易漏检某一层 —— 漏检的后果是拿着无效句柄
/// 继续往下跑，最后在完全无关的地方崩溃。
/// 抛出异常能让正常路径和错误路径彻底分开，且异常里自带出错的 API 名，
/// 排查时不用回去数行号。
/// C++17 没有 std::expected，异常是这条链路上最合适的选择。
class CameraException : public std::runtime_error
{
public:
    /// @param code MVS 返回的错误码
    /// @param api  出错的 API 名，必须是字符串字面量（异常只存指针，不拷贝）
    CameraException(std::int32_t code, const char* api);

    std::int32_t code() const noexcept { return m_code; }
    const char* api() const noexcept { return m_api; }

private:
    /// 拼出形如 "MVS 调用失败：MV_CC_OpenDevice() → 错误或无效的句柄 (0x80000000)" 的说明。
    static std::string build_message(std::int32_t code, const char* api);

    std::int32_t m_code;
    const char* m_api;
};

/// 检查 MVS 返回值，非 MV_OK 时抛 CameraException。
///
/// api 只接受字符串字面量：异常内部保存的是指针而不是 std::string，
/// 这样可以避免构造异常时再做一次内存分配。
void throw_if_failed(std::int32_t ret, const char* api);

} // namespace camera
