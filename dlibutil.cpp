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
// static const qint64 MIN_BLINK_MS = 50;    // 闭眼至少要超过这么久，才算"真的闭上"
// static const qint64 MAX_BLINK_MS = 3000;  // 闭眼最多持续这么久，超过说明是"闭着眼不动"

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
        // 睁眼基线：取历史最大值，并给一个兜底值
    if (avgEar > m_openEar) m_openEar = avgEar;
    if (m_openEar < 0.20)   m_openEar = 0.30;
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
    Q_UNUSED(currentTimeMs);

    const double openEar = (m_openEar > 0.20) ? m_openEar : 0.30;

    // 明显比睁眼时小 → 记下"这次闭过眼"
    if (ear < openEar * 0.72) {
        m_closedSeen = true;
        return false;
    }

    // 之前闭过、现在又睁开了 → 判定为一次眨眼
    if (m_closedSeen && ear > openEar * 0.88) {
        m_closedSeen = false;
        return true;
    }

    return false;
}

// =============================================================================
// 【calculateMAR】计算嘴巴纵横比
//
//   内唇 8 个点（60~67）：
//      60 —— 61 —— 62 —— 63
//      |                  |
//      67 —— 66 —— 65 —— 64
//
//   MAR = (|62-66| + |63-65|) / (2 * |60-64|)
//     · 闭嘴：上下唇贴合，竖直距离小 → MAR 小（约 0.1~0.3）
//     · 张嘴：竖直距离变大        → MAR 明显变大（约 0.5~1.0）
// =============================================================================
double DlibUtil::calculateMAR(const dlib::full_object_detection& shape)
{
    const double A = dlib::length(shape.part(62) - shape.part(66));  // 上内唇中点 -> 下内唇中点
    const double B = dlib::length(shape.part(63) - shape.part(65));  // 偏右的上唇 -> 偏右的下唇
    const double C = dlib::length(shape.part(60) - shape.part(64));  // 左嘴角 -> 右嘴角
    if (C < 1e-6) return 0.0;
    return (A + B) / (2.0 * C);
}

// =============================================================================
// 【detectMouthOpen】张嘴检测
//
//   和眨眼一个套路：必须走完 闭 -> 张 -> 闭 才算一次有效张嘴。
//   只判"张开"会被一张张着嘴的静态照片骗过。
/*
作用 ：

- `calculateMAR` 把嘴的 8 个关键点换算成一个“张开程度”的数字，跟 EAR 一个思路。
- `detectMouthOpen` 是状态机，只有「闭 → 张 → 闭」走完才返回`true` 。
好处 ：

- 静态照片 永远张不了嘴 ，这一项直接挡掉打印照片。
- 挡住“闭眼照片 + 眨眼视频贴纸”这种半静态攻击（照片没法张嘴）。
- MAR 用比例计算，人脸远近、大小都不影响，比固定像素阈值稳。
*/
// =============================================================================
bool DlibUtil::detectMouthOpen(const cv::Mat& image, const cv::Rect& faceBox)
{
    cv::Rect safeBox = faceBox & cv::Rect(0, 0, image.cols, image.rows);
    if (safeBox.area() <= 0) return false;

    bool ok = false;
    dlib::full_object_detection landmarks = landmarksOf(image, safeBox, &ok);
    if (!ok) return false;

    const double mar = calculateMAR(landmarks);
    qDebug() << "MAR 的值:" << mar;

    // 基线：只在"看起来像闭嘴"的帧上更新，防止张嘴帧把基线抬上去
    if (mar < 0.45 && (m_mouthClosedMar <= 0.0 || mar < m_mouthClosedMar)) {
        m_mouthClosedMar = mar;
    }
    if (m_mouthClosedMar < 0.10) m_mouthClosedMar = 0.15;   // 兜底

    // 阈值跟着基线走：不同人/不同距离嘴的大小不一样
    const double openThreshold  = qMax(0.45, m_mouthClosedMar * 2.2);
    const double closeThreshold = qMax(0.30, m_mouthClosedMar * 1.4);

    // 看到张开
    if (mar > openThreshold) {
        m_mouthClosedSeen = true;
        return false;
    }
    // 张开之后又闭上 -> 完成一次张嘴
    if (m_mouthClosedSeen && mar < closeThreshold) {
        m_mouthClosedSeen = false;
        return true;
    }
    return false;
}





// =============================================================================
// 【detectHeadShake】摇头检测
//
//   原理：鼻尖（30 号点）在脸正中间。头一偏，鼻尖相对"两眼中心"就横移。
//   用【两眼间距】做归一化，这样人脸远近都不影响判定。
//
//   方向变化：左 -> 右 算一次翻转，右 -> 左 再算一次，
//   翻转次数够 SHAKE_TURNS_REQUIRED 就算完成一次摇头。
// =============================================================================
/*
作用 ：用鼻尖 30 号点相对两眼中心的横向偏移判断头有没有左右摆，摆够次数就返回`true` 。

好处 ：

- 挡住 平面照片/手机屏幕回放 ——照片里的鼻子不会相对眼睛移动，转不动。
- 转头时人脸是 3D 结构变化，2D 照片无法伪造。
- 用“两眼间距”归一化，所以人脸大小、距离远近都不影响，比固定像素阈值稳得多。
*/
bool DlibUtil::detectHeadShake(const cv::Mat& image, const cv::Rect& faceBox)
{
    cv::Rect safeBox = faceBox & cv::Rect(0, 0, image.cols, image.rows);
    if (safeBox.area() <= 0) return false;

    bool ok = false;
    dlib::full_object_detection landmarks = landmarksOf(image, safeBox, &ok);
    if (!ok) return false;

    // 左眼中心、右眼中心、两眼间距
    const double leftEyeX  = (landmarks.part(36).x() + landmarks.part(39).x()) / 2.0;
    const double rightEyeX = (landmarks.part(42).x() + landmarks.part(45).x()) / 2.0;
    const double eyeCenterX = (leftEyeX + rightEyeX) / 2.0;
    const double eyeDist    = std::abs(rightEyeX - leftEyeX);
    if (eyeDist < 1.0) return false;   // 关键点异常，这一帧丢掉

    // 鼻尖相对两眼中心的偏移，用两眼间距归一化
    const double offset = (landmarks.part(30).x() - eyeCenterX) / eyeDist;
    qDebug() << "鼻尖偏移:" << offset;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 超过 3 秒没动作，清空重来（防止用户不动卡死）
    if (m_lastShakeTime > 0 && now - m_lastShakeTime > 3000) {
        m_shakeDir = 0;
        m_shakeTurnCount = 0;
    }

    // 死区：偏移太小当"正中间"，避免关键点抖动误判
    const double deadZone = 0.06;
    int dir = 0;
    if (offset < -deadZone) dir = -1;
    else if (offset > deadZone) dir = 1;

    if (dir != 0 && dir != m_shakeDir) {
        // 方向翻转
        if (m_shakeDir != 0) {
            const qint64 dt = now - m_lastShakeTime;
            // 太快是抖动，太慢是慢慢挪，都不算有效翻转
            if (dt > 120 && dt < 3000) ++m_shakeTurnCount;
            else                       m_shakeTurnCount = 0;
        }
        m_shakeDir = dir;
        m_lastShakeTime = now;
    }

    // 需要来回摆两次（左->右->左）才算"摇头"，一次可能是正常转头
    const int SHAKE_TURNS_REQUIRED = 2;
    if (m_shakeTurnCount >= SHAKE_TURNS_REQUIRED) {
        m_shakeTurnCount = 0;
        m_shakeDir = 0;
        return true;
    }
    return false;
}
//换人、重来时把上一次残留的状态清掉。不清的话，上一个人的基线/计数会带到下一个人身上，直接导致判不出来。
void DlibUtil::resetLivenessState()
{
    // 眨眼
    m_eyeState = OPEN;
    m_lastStateChangeTime = 0;
    m_closedSeen = false;
    m_openEar = 0.0;
    // 张嘴
    m_mouthClosedSeen = false;
    m_mouthClosedMar = 0.0;
    // 摇头
    m_shakeDir = 0;
    m_shakeTurnCount = 0;
    m_lastShakeTime = 0;
}