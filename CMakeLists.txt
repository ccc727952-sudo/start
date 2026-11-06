cmake_minimum_required(VERSION 3.16)

project(accessControlSystem_faceInput VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)
#设置C++版本
set(CMAKE_CXX_STANDARD 17)
# OpenCV 的安装目录 —— 按自己机器的实际路径改
set(OpenCV_ROOT_DIR "D:/WorkProgram/opencv/opencv/opencv-4.8.0/build/install")

find_package(Qt6 REQUIRED COMPONENTS Quick Multimedia)

qt_standard_project_setup(REQUIRES 6.8)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appaccessControlSystem_faceInput
    main.cpp
)

qt_add_qml_module(appaccessControlSystem_faceInput
    URI accessControlSystem_faceInput
    QML_FILES
        Main.qml
SOURCES loginmanager.cpp loginmanager.h
QML_FILES LabeledTextField.qml Login.qml
QML_FILES FaceInput.qml
SOURCES captureworker.h captureworker.cpp
SOURCES faceinputmanager.h faceinputmanager.cpp
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
    )
endif()
target_link_libraries(appaccessControlSystem_faceInput
    PRIVATE Qt6::Quick
    Qt6::Multimedia
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
set(ENV{PATH} "${OpenCV_ROOT_DIR}/bin;$ENV{PATH}")