# 查找海康机器人 MVS SDK。
#
# 按以下顺序搜索，找到即停：
#   1. CMake 变量 MVS_ROOT（-DMVS_ROOT=/some/where 手动指定）
#   2. 环境变量 MVCAM_SDK_PATH —— MVS 安装脚本会把它写进 ~/.bashrc
#   3. 环境变量 MVCAM_COMMON_RUNENV 的上一级目录 —— 即 <SDK>/lib/64 的 <SDK>
#   4. 默认安装路径 /opt/MVS
#
# 为什么不能只靠环境变量：
#   非交互式 shell 不读 ~/.bashrc。在终端里手敲命令时 MVCAM_SDK_PATH 是有的，
#   但 CMake、IDE、CI 脚本里都是空的。所以必须给默认路径兜底。
#
# 定义结果：
#   MVS_FOUND
#   MVS_INCLUDE_DIR     含 MvCameraControl.h 的目录
#   MVS_LIBRARY         libMvCameraControl.so 的完整路径
#   导入目标 MVS::MvCameraControl

set(_mvs_hints "")

if(MVS_ROOT)
    list(APPEND _mvs_hints "${MVS_ROOT}")
endif()

if(DEFINED ENV{MVCAM_SDK_PATH} AND NOT "$ENV{MVCAM_SDK_PATH}" STREQUAL "")
    list(APPEND _mvs_hints "$ENV{MVCAM_SDK_PATH}")
endif()

if(DEFINED ENV{MVCAM_COMMON_RUNENV} AND NOT "$ENV{MVCAM_COMMON_RUNENV}" STREQUAL "")
    get_filename_component(_mvs_runenv_parent "$ENV{MVCAM_COMMON_RUNENV}" DIRECTORY)
    list(APPEND _mvs_hints "${_mvs_runenv_parent}")
endif()

list(APPEND _mvs_hints "/opt/MVS")

find_path(MVS_INCLUDE_DIR
    NAMES MvCameraControl.h
    PATH_SUFFIXES include
    HINTS ${_mvs_hints})

# MVS 的 64 位库放在 lib/64 下，带不带这一级都试一次
find_library(MVS_LIBRARY
    NAMES MvCameraControl
    PATH_SUFFIXES lib/64 lib
    HINTS ${_mvs_hints})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MVS
    REQUIRED_VARS MVS_INCLUDE_DIR MVS_LIBRARY
    FAIL_MESSAGE "未找到海康 MVS SDK。请安装 MVS（完整客户端包，Runtime 包不含头文件），或用 -DMVS_ROOT=<SDK 根目录> 指定。")

if(MVS_FOUND AND NOT TARGET MVS::MvCameraControl)
    get_filename_component(MVS_LIBRARY_DIR "${MVS_LIBRARY}" DIRECTORY)

    add_library(MVS::MvCameraControl SHARED IMPORTED)
    set_target_properties(MVS::MvCameraControl PROPERTIES
        IMPORTED_LOCATION "${MVS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MVS_INCLUDE_DIR}")

    # 把 SDK 库目录直接写进可执行文件的 rpath。
    # 这样程序不依赖 LD_LIBRARY_PATH —— 原因见文件开头：非交互式环境里
    # 那个变量根本不存在，靠它会在 IDE 里点运行时突然报找不到 .so。
    # 官方样例的 Makefile 用的也是同一招（-Wl,-rpath=$(MVCAM_COMMON_RUNENV)/64）。
    target_link_options(MVS::MvCameraControl INTERFACE "-Wl,-rpath,${MVS_LIBRARY_DIR}")
endif()
