/*
 * @brief 主程序// 负责初始化 Qt 应用、加载 QML 文件、处理事件等。
 * 
*/
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <opencv2/opencv.hpp>
#include <QDebug>
#include "loginmanager.h"
#include "faceinputmanager.h"
#include <QDebug>
#include <QVector>
#include <QFile>
#include <QByteArray>

// -----------------------------------------------------------------------------
// 【读图 / 存图】—— 绕开 OpenCV 不支持中文路径的坑
//
//   ★ cv::imread 在 Windows 上是用 ANSI 接口打开文件的，
//     路径里带中文就会失败（报 "can't open/read file"，但路径看着没问题）。
//   办法：先用 Qt 的 QFile 把字节读出来，再用 cv::imdecode 解码；
//         写图反过来：cv::imencode + QFile::write。
// -----------------------------------------------------------------------------
static cv::Mat readImage(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "图片打不开:" << path;
        return {};
    }
    const QByteArray bytes = file.readAll();
    // 把字节数组包成 1 行 N 列的 Mat，交给 imdecode 解码
    const cv::Mat buf(1, bytes.size(), CV_8UC1,
                      const_cast<char*>(bytes.constData()));
    return cv::imdecode(buf, cv::IMREAD_COLOR);
}

static void writeImage(const QString& path, const cv::Mat& image)
{
    std::vector<uchar> buf;                 // imencode 的结果写到这里（出参）
    cv::imencode(".jpg", image, buf);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "图片写不进去:" << path;
        return;
    }
    file.write(reinterpret_cast<const char*>(buf.data()),
               static_cast<qint64>(buf.size()));
}

// -----------------------------------------------------------------------------
// 【test2】用静态图片离线验证：人脸检测 + 特征提取
//
//   这不是"测试代码"，而是【调试手段】：
//   写完人脸检测，第一个要确认的就是"框框得对不对、向量出不出来"。
// -----------------------------------------------------------------------------
void test2()
{
    const QString imagePath="C:/Users/34976/Downloads/images/dlrb1.jpeg";
    qDebug() << "=== 离线自测开始 ===" << imagePath;

    // ① 读一张人脸照片
    const cv::Mat src = readImage(imagePath);
    if (src.empty()) {
        qWarning() << "图片读不到，请检查路径:" << imagePath;
        return;
    }

    // ② 人脸检测（YOLO-face）
    FaceDetector faceDetector;
    const auto faces = faceDetector.detect(src);
    qDebug() << "检测到人脸数量:" << faces.size();
    if (faces.isEmpty()) {
        qWarning() << "没有检测到人脸，检查模型是否正确";
        return;
    }
    for (const auto& f : faces) {
        qDebug() << "  位置:" << f.box.x << f.box.y << f.box.width << f.box.height
                 << "置信度:" << f.confidence;
    }

    // ③ 关键点可视化（在图上画 68 个点和编号）
    DlibUtil dlibUtil;
    cv::Mat show = src.clone();
    dlibUtil.showLandmarks(show, faces[0].box);
    writeImage("landmarks_result.jpg", show);   // 看看点有没有落在对的位置
    qDebug() << "关键点可视化已保存: landmarks_result.jpg";

    // ④ 提取 128 维特征向量
    const std::vector<float> features = dlibUtil.getFaceFeatures(src, faces[0].box);
    qDebug() << "特征向量长度:" << features.size();
    // 只打印前 10 个值看看（128 个数刷屏没意义）
    const int showCount = qMin(10, int(features.size()));
    qDebug() << "特征向量前 10 个值:"
             << QVector<float>(features.begin(), features.begin() + showCount);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    // 登录管理器 —— 注册给 QML 用（登录案例要用）
    qmlRegisterType<LoginManager>("cn.rh", 1, 0, "LoginManager");
    qmlRegisterType<FaceInputManager>("cn.rh", 1, 0, "FaceInputManager");
    engine.loadFromModule("accessControlSystem_faceInput", "Main");
    qDebug()<<"OpenCV 版本："<<CV_VERSION;
    test2();
    return QGuiApplication::exec();
}
