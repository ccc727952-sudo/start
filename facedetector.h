/*
 * @brief 人脸检测器// 基于 YOLO-face 的 OpenCV DNN 人脸检测器
 * 
 */

#ifndef FACEDETECTOR_H
#define FACEDETECTOR_H

#include <QObject>
#include <QString>
#include <QRectF>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

// =============================================================================
// 【FaceDetection】一次人脸检测的结果
// =============================================================================
struct FaceDetection {
    cv::Rect box;        // 在输入图像坐标系下的矩形（左上角 + 宽高）
    float confidence;    // 置信度 0~1
};

// =============================================================================
// 【FaceDetector】基于 YOLO-face 的 OpenCV DNN 人脸检测器
//
//   ★ 为什么用 YOLO 而不是 OpenCV 自带的 Haar 级联？
//     · Haar 级联是老方法，误检率高、框也偏
//     · YOLO 是深度学习模型，框更准、对正脸/侧脸都稳
//     ★ 但 YOLO 输出的是【特征图】格式，需要自己解码 —— 这正是本类要干的事
//
//   【一个完整的检测流程分 5 步】
//     ① 预处理：把图缩放到 640×640、归一化、BGR→RGB  （blobFromImage）
//     ② 前向推理：m_net.forward() 拿到 [1, 5, 8400] 的原始输出
//     ③ 解码：把每个候选框的 cx,cy,w,h,score 换成原图坐标
//     ④ 去重：NMS 抑制掉同一个人的重复框
//     ⑤ 组装：返回 QVector<FaceDetection>
// =============================================================================
class FaceDetector
{
public:
    // onnxPath：YOLO-face 模型路径；inputSize：模型输入边长（YOLOv5 是 640）
    explicit FaceDetector(const QString& onnxPath = QString(),
                          int inputSize = 640);
    ~FaceDetector();

    // 对 BGR 图像做检测，返回所有检测到的人脸
    QVector<FaceDetection> detect(const cv::Mat& bgrImage,
                                  float confThreshold = 0.25f,
                                  float nmsThreshold = 0.45f);

private:
    cv::dnn::Net m_net;        // OpenCV DNN 网络
    int m_inputSize = 640;     // 模型输入边长
    bool m_loaded = false;     // 模型是否加载成功

    // YOLOv5 后处理：解码【一个尺度】的输出（多尺度模型才用得上）
    void decodeOutput(const cv::Mat& output, int stride,
                      const cv::Size& inputSize,
                      const cv::Size& originalSize,
                      float confThreshold,
                      QVector<cv::Rect>& boxes,
                      QVector<float>& confidences);

    // 通用 NMS（非极大值抑制）：把重叠的重复框只留置信度最高的那个
    void nms(QVector<cv::Rect>& boxes, QVector<float>& confidences,
             float nmsThreshold);
};

#endif // FACEDETECTOR_H