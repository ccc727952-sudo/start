#@brief 人脸录入系统// 负责管理人脸录入模块的运行// 包含登录管理器、人脸录入管理器等功能。
 
#cmake_minimum_required 用于指定 CMake 的最低版本要求
cmake_minimum_required(VERSION 3.16)# 最低要求 CMake 版本为 3.16
#project 函数用于定义项目，参数依次为：
#   项目名
#   项目版本号
#   项目语言
#   项目描述
project(accessControlSystem_faceInput VERSION 0.1 LANGUAGES CXX)
#set 函数用于设置变量，参数依次为：
#   变量名
#   变量值
set(CMAKE_CXX_STANDARD_REQUIRED ON)# 开启 C++ 标准要求
set(CMAKE_CXX_STANDARD 17)# 设置 C++ 标准为 17
# OpenCV 的安装目录 —— 按自己机器的实际路径改
set(OpenCV_ROOT_DIR "D:/WorkProgram/opencv/opencv/opencv-4.8.0/build/install")# 设置 OpenCV 安装目录

#find_package 函数用于查找指定的库或头文件
find_package(Qt6 REQUIRED COMPONENTS Quick Multimedia TextToSpeech Network)# 查找 Qt6 库
# dlib路径和查找
set(dlib_DIR "D:/WorkProgram/dlib/build/install/lib/cmake/dlib")# 设置 dlib 安装目录
#find_package 函数用于查找指定的库或头文件
find_package(dlib REQUIRED)# 查找 dlib 库


#qt_standard_project_setup 函数用于设置 Qt 项目的标准配置
qt_standard_project_setup(REQUIRES 6.8)
# 坑：中文注释会"吃掉"代码MSVC 默认按系统 ANSI 代码页（中文机器 = GBK）解析源码。
# 我们写的注释是 UTF-8 的，某些汉字的字节在 GBK 里会被当成反斜杠 + 换行的转义 —— 后面的代码就被当成注释吞掉了，
# 报错还报在莫名其妙的地方。
# 解决办法（CMakeLists.txt 里加一行）：
if(MSVC)# 如果是 MSVC 编译
    add_compile_options(/utf-8)# 添加 UTF-8 编译选项
endif()

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appaccessControlSystem_faceInput
    main.cpp
)

# qt_add_qml_module 函数用于添加 QML 模块
#具体参数说明：
#   URI：QML 模块的 URI，用于在 QML 中引用
#   QML_FILES：QML 文件列表
#   SOURCES：C++ 源文件列表
#   RESOURCES：资源文件列表
#   QML_FILES：QML 模块文件列表
#   SOURCES：C++ 源文件列表
#   RESOURCES：资源文件列表
qt_add_qml_module(appaccessControlSystem_faceInput
    URI accessControlSystem_faceInput
    QML_FILES
        Main.qml
SOURCES loginmanager.cpp loginmanager.h
QML_FILES LabeledTextField.qml Login.qml
QML_FILES Register.qml#自研注册页面的qml
QML_FILES FaceInput.qml
SOURCES captureworker.h captureworker.cpp
SOURCES faceinputmanager.h faceinputmanager.cpp
SOURCES facehandleworker.h facehandleworker.cpp
RESOURCES resources.qrc
SOURCES facedetector.cpp facedetector.h modelpaths.cpp modelpaths.h
SOURCES dlibutil.cpp dlibutil.h
SOURCES speechutils.h speechutils.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appaccessControlSystem_faceInput PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appaccessControlSystem_faceInput
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)
# 【接 OpenCV】—— 兼容"标准安装"和"扁平安装"两种情况
find_package(OpenCV QUIET)

if(OpenCV_FOUND)
    # 情况一：标准安装 —— find_package 找到了，直接用,注意项目名称用实际的项目名称
    message(STATUS "OpenCV: 使用 find_package 的结果 (版本 ${OpenCV_VERSION})")
    target_include_directories(appaccessControlSystem_faceInput PRIVATE
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(appaccessControlSystem_faceInput  PRIVATE
        ${OpenCV_LIBS}
    )
else()
    # 情况二：扁平安装 —— find_package 找不到，手动指定
    message(STATUS "OpenCV: find_package 没找到，改用手动指定路径")
    target_include_directories(appaccessControlSystem_faceInput  PRIVATE
        "${OpenCV_ROOT_DIR}/include"
    )
    target_link_directories(appaccessControlSystem_faceInput PRIVATE
        "${OpenCV_ROOT_DIR}/lib"
    )
    # 本工程用到的 OpenCV 模块：
    #   core     —— cv::Mat / cv::Scalar / cv::Point / 画图函数
    #   imgproc  —— cv::cvtColor（颜色空间转换）
    #   imgcodecs—— cv::imread / cv::imwrite（读写图片）
    #   videoio  —— cv::VideoCapture（打开摄像头）
    #   highgui  —— cv::imshow（调试时弹窗看画面）
    target_link_libraries(appaccessControlSystem_faceInput PRIVATE
        opencv_core480
        opencv_imgproc480
        opencv_imgcodecs480
        opencv_videoio480
        opencv_highgui480
        opencv_dnn480
        opencv_imgcodecs480
    )
endif()

# 【模型目录】编译进程序（modelpaths.cpp 里再用环境变量 FACE_MODEL_DIR 覆盖）
target_compile_definitions(appaccessControlSystem_faceInput PRIVATE
    FACE_MODEL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/model"
)
target_link_libraries(appaccessControlSystem_faceInput
    PRIVATE Qt6::Quick
    Qt6::Multimedia
    dlib::dlib
    Qt6::TextToSpeech
    Qt6::Network
)

install(TARGETS appaccessControlSystem_faceInput
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
# 【运行时能找到 OpenCV 的 DLL】
#   Windows 上编译好的 exe 需要 DLL 在 PATH 里。
#   下面这一句把 OpenCV 的 bin 目录加进【构建产物的 PATH】，
#   这样在 build 目录里直接运行 exe 就能找到 DLL，不用手动配环境变量。
set(ENV{PATH} "${OpenCV_ROOT_DIR}/bin;$ENV{PATH}")# 设置 PATH 环境变量