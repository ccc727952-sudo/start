/*
作用：dlib 工具类：具体实现dlib的人脸识别功能// 包含人脸检测、特征提取、眨眼状态机等功能。
函数：
1.void detectFace(const cv::Mat& frame, std::vector<dlib::rectangle>& faces);//人脸检测
2.void extractFeature(const cv::Mat& frame, std::vector<dlib::rectangle>& faces, std::vector<float>& features);//特征提取
3.void detectRealBlink(double ear, qint64 currentTimeMs);//眨眼状态机
4.std::vector<float> getFaceFeatures(const cv::Mat& frame, const std::vector<dlib::rectangle>& faces);//特征提取提取人脸特征向量

*/ 
#ifndef DLIBUTIL_H
#define DLIBUTIL_H

#include <QObject>
#include <QString>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing.h>
#include <dlib/dnn.h>

// =============================================================================
// 【dlib 人脸识别 ResNet 网络结构】
//
//   这段是【必须放在头文件里】的：dlib 的 DNN 是"头文件实现"的模板库，
//   网络结构只有完整可见，编译器才能把这一大坨模板实例化成真正的代码。
//   内容直接来自 dlib 官方示例 dnn_face_recognition_ex.cpp。
//
//   它做的事：输入一张 150×150 的 RGB 人脸图 → 输出 128 维的特征向量。
// =============================================================================
template <template <int, template <typename> class, int, typename> class block,
         int N, template <typename> class BN, typename SUBNET>
using residual = dlib::add_prev1<block<N, BN, 1, dlib::tag1<SUBNET>>>;

template <template <int, template <typename> class, int, typename> class block,
         int N, template <typename> class BN, typename SUBNET>
using residual_down = dlib::add_prev2<dlib::avg_pool<
    2, 2, 2, 2, dlib::skip1<dlib::tag2<block<N, BN, 2, dlib::tag1<SUBNET>>>>>>;

template <int N, template <typename> class BN, int stride, typename SUBNET>
using block = BN<dlib::con<N, 3, 3, 1, 1,
                           dlib::relu<BN<dlib::con<N, 3, 3, stride, stride, SUBNET>>>>>;

template <int N, typename SUBNET>
using ares = dlib::relu<residual<block, N, dlib::affine, SUBNET>>;

template <int N, typename SUBNET>
using ares_down = dlib::relu<residual_down<block, N, dlib::affine, SUBNET>>;

template <typename SUBNET> using alevel0 = ares_down<256, SUBNET>;
template <typename SUBNET> using alevel1 = ares<256, ares<256, ares_down<256, SUBNET>>>;
template <typename SUBNET> using alevel2 = ares<128, ares<128, ares_down<128, SUBNET>>>;
template <typename SUBNET> using alevel3 = ares<64, ares<64, ares<64, ares_down<64, SUBNET>>>>;
template <typename SUBNET> using alevel4 = ares<32, ares<32, ares<32, SUBNET>>>;

using anet_type = dlib::loss_metric<dlib::fc_no_bias<128, dlib::avg_pool_everything<
                                                              alevel0<
                                                                  alevel1<
                                                                      alevel2<
                                                                          alevel3<
                                                                              alevel4<
                                                                                  dlib::max_pool<3, 3, 2, 2, dlib::relu<dlib::affine<dlib::con<32, 7, 7, 2, 2,
                                                                                                                                               dlib::input_rgb_image_sized<150>
                                                                                                                                               >>>>>>>>>>>>;

// =============================================================================
// 【DlibUtil】dlib 工具类 —— 本课最核心的一个类
//
//   它把 dlib 的三件事包成了三个接口：
//     ① showLandmarks()      —— 提取并【画出来】68 个关键点（调试/可视化用）
//     ② dlibFaceLivenessDetect() —— 活体检测：算 EAR + 眨眼状态机
//     ③ getFaceFeatures()    —— 特征提取：人脸对齐 → ResNet → 128 维向量
//
//   ★ 注意 ②③ 都依赖 ① 的关键点，所以 ① 是"公共基础"。
// =============================================================================
class DlibUtil : public QObject
{
    Q_OBJECT
public:
    explicit DlibUtil(QObject* parent = nullptr);

    // ① 提取 68 个关键点，并把点和编号画到图上（图像会被就地修改）
    void showLandmarks(const cv::Mat& bgrImage, const cv::Rect& faceBox);

    // ② 活体检测：判断这一帧里有没有发生一次"完整的眨眼"
    //    返回 true 表示检测到眨眼（真人），false 表示还没眨
    bool dlibFaceLivenessDetect(const cv::Mat& bgrImage, const cv::Rect& faceBox);

    // ③ 提取人脸特征向量（128 维），后面拿去和库里的人脸做比对
    std::vector<float> getFaceFeatures(const cv::Mat& bgrImage, const cv::Rect& faceBox);

private:
    // ---------------------------------------------------------------------
    // 【眨眼状态机】
    //   OPEN    —— 睁着眼，等闭眼
    //   CLOSING —— 刚闭上，还没到 50ms（可能只是眨眼的一瞬间）
    //   CLOSED  —— 闭着眼，等睁开
    //
    //   ★ 为什么不用一个 bool？
    //     因为"看到闭眼"不等于"眨了眼"。
    //     必须走完 睁 → 闭(超过50ms) → 睁(不超过3s) 才算一次有效眨眼，
    //     这样才挡得住"盯着屏幕不眨眼"和"眯眼看"这两种作弊。
    // ---------------------------------------------------------------------
    enum EyeState {
        OPEN,
        CLOSING,
        CLOSED
    };
    EyeState m_eyeState = OPEN;          // 当前状态
    qint64 m_lastStateChangeTime = 0;     // 上一次状态改变的时刻（毫秒）
    bool   m_closedSeen = false;   // 这次眨眼过程中，是否已经看到过"闭眼"
    double m_openEar    = 0.0;     // 这个人的"睁眼基线"，自动学习

    // dlib 的 68 点形状预测器（从 .dat 模型加载）
    dlib::shape_predictor m_sp;

    // dlib 的人脸识别 ResNet（从 .dat 模型加载），输出 128 维向量
    anet_type net;

    // OpenCV 的 Rect -> dlib 的 rectangle（两者坐标表示不一样）
    dlib::rectangle toDlibRect(const cv::Rect& r);

    // 从人脸框里取出 68 个关键点（失败时 ok=false）
    dlib::full_object_detection landmarksOf(const cv::Mat& bgrImage,
                                            const cv::Rect& faceBox,
                                            bool* ok);

    // 计算眼睛纵横比 EAR（36~41 左眼 / 42~47 右眼）
    double calculateEAR(const dlib::full_object_detection& shape, int start, int end);

    // 眨眼状态机：喂进 EAR 值，判断这一帧有没有完成一次眨眼
    bool detectRealBlink(double ear, qint64 currentTimeMs);
};

#endif // DLIBUTIL_H