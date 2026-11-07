/*
作用：dlib 工具类：具体实现dlib的人脸识别功能// 包含人脸检测、特征提取、眨眼状态机等功能。
函数：
1.void detectFace(const cv::Mat& frame, std::vector<dlib::rectangle>& faces);//人脸检测
2.void extractFeature(const cv::Mat& frame, std::vector<dlib::rectangle>& faces, std::vector<float>& features);//特征提取
3.void detectRealBlink(double ear, qint64 currentTimeMs);//眨眼状态机
4.std::vector<float> getFaceFeatures(const cv::Mat& frame, const std::vector<dlib::rectangle>& faces);//特征提取提取人脸特征向量
.
.
.

*/


#include "dlibutil.h"
#include "modelpaths.h"

#include <QDateTime>
#include <QFileInfo>

// =============================================================================
// 【DlibUtil.cpp】dlib 工具类实现
// =============================================================================

// 眨眼判定的两个时间阈值（毫秒）
static const qint64 MIN_BLINK_MS = 50;    // 闭眼至少要超过这么久，才算"真的闭上"
static const qint64 MAX_BLINK_MS = 3000;  // 闭眼最多持续这么久，超过说明是"闭着眼不动"

DlibUtil::DlibUtil(QObject* parent)
    : QObject{parent}
{
    // ------------------------------------------------------------------
    // 加载两个模型文件
    //   ① shape_predictor_68_face_landmarks.dat —— 68 点形状预测器
    //   ② dlib_face_recognition_resnet_model_v1.dat —— 特征提取网络
    //
    //   ★ dlib::deserialize 失败会【抛异常】，不接住的话程序直接崩，
    //     所以这里必须 try/catch。
    // ------------------------------------------------------------------
    const QString spPath = ModelPaths::landmarks();
    const QString netPath = ModelPaths::resnet();

    try {
        if (!QFileInfo::exists(spPath)) {
            qWarning() << "关键点模型不存在:" << spPath;
        } else {
            dlib::deserialize(spPath.toStdString()) >> m_sp;
        }

        if (!QFileInfo::exists(netPath)) {
            qWarning() << "特征提取模型不存在:" << netPath;
        } else {
            dlib::deserialize(netPath.toStdString()) >> net;
        }

        qDebug() << "dlib 模型文件加载成功............";
    } catch (const std::exception& e) {
        qWarning() << "dlib 模型加载失败:" << e.what();
    }
}

// =============================================================================
// 【showLandmarks】提取并绘制 68 个关键点
//
//   用途：先可视化验证关键点是否正确，再往下做活体检测/特征提取。
//   （不能想象着就用，得先看图确认点落在对的位置上）
// =============================================================================
void DlibUtil::showLandmarks(const cv::Mat& image, const cv::Rect& faceBox)
{
    // 裁剪到图像范围内，防止越界
    cv::Rect safeBox = faceBox & cv::Rect(0, 0, image.cols, image.rows);
    if (safeBox.area() <= 0) {
        qDebug() << "坐标越界!";
        return;
    }

    try {
        // 把 OpenCV 的图像对象转成 dlib 的图像对象
        dlib::cv_image<dlib::bgr_pixel> dlibImg(image);
        // 把 OpenCV 的坐标转成 dlib 的坐标对象
        dlib::rectangle dlibRect = toDlibRect(safeBox);

        // 通过形状预测器，从人脸区域里提取 68 个关键点
        dlib::full_object_detection landmarks = m_sp(dlibImg, dlibRect);

        // 把点和编号画到图上：绿点 + 红编号
        for (int i = 0; i < 68; ++i) {
            cv::circle(image,
                       cv::Point(landmarks.part(i).x(), landmarks.part(i).y()),
                       2, cv::Scalar(0, 255, 0), -1);   // 画绿色圆点
            cv::putText(image, std::to_string(i),
                        cv::Point(landmarks.part(i).x(), landmarks.part(i).y()),
                        cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0, 0, 255), 1);
        }
    } catch (const std::exception& e) {
        qWarning() << "关键点提取失败:" << e.what();
    }
}

// =============================================================================
// 【dlibFaceLivenessDetect】活体检测（眨眼）
//
//   三步：
//     ① 拿到 68 个关键点
//     ② 用眼睛的 6 个点算 EAR（眼睛纵横比）
//     ③ 把 EAR 喂给眨眼状态机，判断这一帧是不是"眨完眼"的那一帧
// =============================================================================
bool DlibUtil::dlibFaceLivenessDetect(const cv::Mat& image, const cv::Rect& faceBox)
{
    // 先把脸框裁到图像范围内
    cv::Rect safeBox = faceBox & cv::Rect(0, 0, image.cols, image.rows);
    if (safeBox.area() <= 0) {
        return false;
    }

    // ① 拿关键点
    bool ok = false;
    dlib::full_object_detection landmarks = landmarksOf(image, safeBox, &ok);
    if (!ok) {
        return false;
    }

    // ② 分别算左右眼的 EAR，再取平均
    //   左眼 36~41，右眼 42~47
    const double leftEar = calculateEAR(landmarks, 36, 41);
    const double rightEar = calculateEAR(landmarks, 42, 47);
    const double avgEar = (leftEar + rightEar) / 2.0;
    qDebug() << "EAR 的值:" << avgEar;

    // ③ 喂给眨眼状态机
    //   眨眼 = 睁眼 -> 闭眼 -> 睁眼 的完整过程
    return detectRealBlink(avgEar, QDateTime::currentMSecsSinceEpoch());//返回是否是眨眼状态机的判断结果
}

// =============================================================================
// 【getFaceFeatures】提取人脸特征向量（128 维）
//
//   三步：
//     ① 拿 68 个关键点
//     ② 人脸对齐：按关键点把人脸"摆正"，裁成 150×150 的标准人脸图
//        （不做对齐的话，歪一点的脸算出来的向量就不可比了）
//     ③ 送进 ResNet 前向推理，得到 128 维特征向量
// =============================================================================
std::vector<float> DlibUtil::getFaceFeatures(const cv::Mat& image, const cv::Rect& faceBox)
{
    // 裁剪到图像范围内
    cv::Rect safeBox = faceBox & cv::Rect(0, 0, image.cols, image.rows);
    if (safeBox.area() <= 0) {
        return {};
    }

    // ① 关键点
    bool ok = false;
    dlib::full_object_detection landmarks = landmarksOf(image, safeBox, &ok);
    if (!ok) {
        return {};
    }

    // ② 人脸对齐 → 150×150 的 face chip
    //   get_face_chip_details 的第二个参数是输出尺寸（必须 150，和网络结构对应）
    //   第三个参数是 padding（往外多留一点，避免切到下巴）
    dlib::cv_image<dlib::bgr_pixel> dlibImg(image);
    dlib::matrix<dlib::rgb_pixel> face_chip;
    dlib::extract_image_chip(dlibImg,
                             dlib::get_face_chip_details(landmarks, 150, 0.25),
                             face_chip);

    // ③ 前向推理 → 128 维描述子
    dlib::matrix<float, 0, 1> descriptor = net(face_chip);

    // 转成 std::vector<float>，方便存进数据库或做比对
    std::vector<float> feature(descriptor.size());
    for (long i = 0; i < descriptor.size(); ++i) {
        feature[i] = descriptor(i);
    }
    return feature;
}

// =============================================================================
// 【landmarksOf】从人脸框里取出 68 个关键点
// =============================================================================
dlib::full_object_detection DlibUtil::landmarksOf(const cv::Mat& bgrImage,
                                                  const cv::Rect& faceBox,
                                                  bool* ok)
{
    *ok = false;
    dlib::full_object_detection landmarks;

    try {
        dlib::cv_image<dlib::bgr_pixel> dlibImg(bgrImage);
        dlib::rectangle dlibRect = toDlibRect(faceBox);
        landmarks = m_sp(dlibImg, dlibRect);
        *ok = true;
    } catch (const std::exception& e) {
        qWarning() << "关键点提取失败:" << e.what();
    }

    return landmarks;
}

// =============================================================================
// 【toDlibRect】OpenCV 的 Rect -> dlib 的 rectangle
//
//   ★ 两者坐标表示不一样：
//     OpenCV：左上角 (x, y) + 宽高 (w, h)
//     dlib  ：左上角 (x1, y1) + 右下角 (x2, y2)
// =============================================================================
dlib::rectangle DlibUtil::toDlibRect(const cv::Rect& r)
{
    return dlib::rectangle(
        static_cast<long>(r.x),
        static_cast<long>(r.y),
        static_cast<long>(r.x + r.width - 1),
        static_cast<long>(r.y + r.height - 1));
}

// =============================================================================
// 【calculateEAR】计算眼睛纵横比
//
//   眼睛 6 个点（36~41 左眼）：
//        2 —— 3
//    1 ——     —— 5      A = |1-5|（竖直距离）
//        6 —— 4
//              C = |0-3|（水平距离）
//
//   EAR = (A + B) / (2 * C)
//     · 睁眼：竖直距离大 → EAR 大（一般 > 0.25）
//     · 闭眼：竖直距离趋近 0 → EAR 显著变小
// =============================================================================
double DlibUtil::calculateEAR(const dlib::full_object_detection& shape,
                              int start, int end)
{
    std::vector<dlib::point> eyePoints;
    for (int i = start; i <= end; ++i) {
        eyePoints.push_back(shape.part(i));
    }

    const double A = dlib::length(eyePoints[1] - eyePoints[5]);
    const double B = dlib::length(eyePoints[2] - eyePoints[4]);
    const double C = dlib::length(eyePoints[0] - eyePoints[3]);

    return (A + B) / (2.0 * C);
}

// =============================================================================
// 【detectRealBlink】眨眼状态机
//
//   OPEN   → 看到闭眼，切到 CLOSING，记下时间
//   CLOSING→ 一直闭着且超过 50ms，切到 CLOSED
//   CLOSED → 又睁开了：
//            · 闭眼时长没超过 3s → 判定为【一次有效眨眼】，返回 true
//            · 超过 3s → 说明是闭眼不动，不算，重新计时
// =============================================================================
bool DlibUtil::detectRealBlink(double ear, qint64 currentTimeMs)
{
    // EAR > 0.25 认为是睁眼，否则是闭眼
    const bool isOpen = (ear > 0.25);
    bool blinkDetected = false;

    switch (m_eyeState) {
    case OPEN:
        if (!isOpen) {
            m_eyeState = CLOSING;
            m_lastStateChangeTime = currentTimeMs;
        }
        break;

    case CLOSING:
        if (!isOpen) {
            // 闭眼时间要超过 50ms 才算数
            if (currentTimeMs - m_lastStateChangeTime > MIN_BLINK_MS) {
                m_eyeState = CLOSED;
            }
        } else {
            m_eyeState = OPEN;   // 只是眨眼的一瞬间，回到睁眼
        }
        break;

    case CLOSED:
        if (isOpen) {
            const qint64 closedMs = currentTimeMs - m_lastStateChangeTime;
            if (closedMs <= MAX_BLINK_MS) {
                // 闭眼时间在 3 秒以内 → 是一次有效眨眼
                blinkDetected = true;
            } else {
                // 闭眼太久 → 这次不算，重新开始计时
                m_lastStateChangeTime = currentTimeMs;
            }
            m_eyeState = OPEN;
        }
        break;

    default:
        break;
    }

    return blinkDetected;
}