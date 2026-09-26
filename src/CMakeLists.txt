add_library(camera_wrapper SHARED
    Camera.cpp
    CameraError.cpp
    CameraInfo.cpp
    CameraTypes.cpp
)

# PUBLIC：公开头文件里出现了 cv::Mat（Frame::image），使用方必须能看到 OpenCV 头文件
target_include_directories(camera_wrapper
    PUBLIC ${PROJECT_SOURCE_DIR}/include
           ${OpenCV_INCLUDE_DIRS}
)

# PRIVATE：MVS 只是实现细节。
# 使用方既不 include 它的头文件，也不需要链接它 —— 这正是 Pimpl 想要的效果。
target_link_libraries(camera_wrapper
    PUBLIC ${OpenCV_LIBS}
    PRIVATE MVS::MvCameraControl
)
