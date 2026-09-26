#pragma once

#include <chrono>
#include <cstdint>

#include <opencv2/core.hpp>

#include "camera/CameraTypes.h"

namespace camera {

/// 一帧图像及其元数据。
///
/// image 里的像素数据是**本对象独占的拷贝**，不指向 SDK 的内部缓存。
/// 这点很关键：SDK 的帧缓存要在取到下一帧之前归还
/// （MV_CC_GetImageBuffer 必须配对 MV_CC_FreeImageBuffer），
/// 如果 Frame 只持有裸指针，一旦归还或对象析构，image 就变成悬空引用。
/// 多花一次拷贝换取"Frame 可以安全地跨线程传递、可以随意存活"，
/// 对上层是划算的。
struct Frame {
    cv::Mat image;  ///< 已转成可显示的格式：彩色为 BGR8，单色为 Mono8

    uint64_t frameNumber = 0;  ///< 相机给的帧号，可用于判断是否丢帧
    uint64_t timestampNs = 0;  ///< 相机时间戳（纳秒）。部分相机不提供，为 0

    /// 主机收到这一帧的时刻。相机时间戳可能不准或为 0，
    /// 做帧率统计、超时判断时用这个更可靠。
    std::chrono::steady_clock::time_point hostTime{};

    PixelFormat sourceFormat = PixelFormat::Unknown;  ///< 相机输出的原始格式
    uint32_t width = 0;
    uint32_t height = 0;

    /// 这一帧数据不完整（收到长度小于期望长度）。
    /// 网口相机在带宽不足或丢包时会出现，此时图像下半部分可能是花屏。
    bool incomplete = false;

    /// 相机统计的丢包数（仅部分网口相机提供，其余恒为 0）。
    uint32_t lostPacket = 0;

    /// 是否取到了有效图像。
    explicit operator bool() const { return !image.empty(); }

    /// 像素总数，用于估算带宽或打印诊断信息。
    size_t pixelCount() const
    {
        return static_cast<size_t>(width) * static_cast<size_t>(height);
    }
};

/// 一段时间内的帧率统计。
///
/// 单独抽出来是因为：只报告"当前 FPS"会非常飘（相邻两帧抖动很大），
/// 而工程上真正关心的是"平均帧率是否达到设定值"和"总共丢了多少帧"。
struct FrameStatistics {
    uint64_t received = 0;  ///< 实际收到的帧数
    uint64_t dropped = 0;   ///< 因消费不过来被主动丢弃的帧数
    double averageFps = 0.0;

    double dropRate() const
    {
        const uint64_t total = received + dropped;
        return total == 0 ? 0.0 : static_cast<double>(dropped) / static_cast<double>(total);
    }
};

} // namespace camera
