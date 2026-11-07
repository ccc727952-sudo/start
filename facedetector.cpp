#include "facedetector.h"
#include "modelpaths.h"

#include <QDebug>
#include <QFileInfo>
#include <algorithm>

// =============================================================================
// 【FaceDetector.cpp】YOLO-face 人脸检测实现
// =============================================================================

FaceDetector::FaceDetector(const QString& onnxPath, int inputSize)
    : m_inputSize(inputSize)
{
    // 没传路径就用统一的模型路径管理
    const QString path = onnxPath.isEmpty() ? ModelPaths::yoloFace() : onnxPath;

    if (!QFileInfo::exists(path)) {
        qWarning() << "人脸检测模型不存在:" << path
                   << "（请检查 resources/model 目录，或设置环境变量 FACE_MODEL_DIR）";
        return;
    }

    try {
        m_net = cv::dnn::readNetFromONNX(path.toStdString());
    } catch (const cv::Exception& e) {
        qWarning() << "加载 ONNX 失败:" << e.what();
        return;
    }

    if (m_net.empty()) {
        qWarning() << "ONNX 模型为空:" << path;
        return;
    }

    // 如果用 GPU 可以打开下面两行（需要 OpenCV 编译时带 CUDA 支持）
    // m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
    // m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);

    m_loaded = true;
    qDebug() << "人脸检测模型加载成功:" << path << "输入尺寸:" << inputSize;
}

FaceDetector::~FaceDetector() {}

// =============================================================================
// 【detect】检测人脸
// =============================================================================
QVector<FaceDetection> FaceDetector::detect(const cv::Mat& bgrImage,
                                            float confThreshold,
                                            float nmsThreshold)
{
    QVector<FaceDetection> results;
    if (!m_loaded || bgrImage.empty()) {
        return results;
    }

    // ------------------------------------------------------------------
    // ① 预处理：必须和训练时一模一样，否则检测效果会崩
    //     · 缩放到 640×640
    //     · 除以 255 归一化到 0~1
    //     · swapRB=true 表示 BGR → RGB（模型训练时用的是 RGB）
    // ------------------------------------------------------------------
    cv::Mat blob;
    cv::dnn::blobFromImage(bgrImage, blob,
                           1.0 / 255.0,
                           cv::Size(m_inputSize, m_inputSize),
                           cv::Scalar(0, 0, 0),
                           true,   // swapRB: BGR -> RGB
                           false);

    m_net.setInput(blob);

    // ------------------------------------------------------------------
    // ② 前向推理：一次前向拿到所有候选框
    // ------------------------------------------------------------------
    std::vector<cv::Mat> outputs;
    m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());
    if (outputs.empty()) {
        return results;
    }

    // YOLOv5 人脸模型的输出形状是 [1, 5, 8400]
    //   5 = cx, cy, w, h, score（单类别，所以没有额外的类别分数）
    //   8400 = 80×80 + 40×40 + 20×20 三张特征图上的候选点总数
    cv::Mat output = outputs[0];
    const int C = output.size[1];        // 5
    const int N = output.size[2];        // 8400

    // 转置成 [8400, 5]，这样一行就是一张候选框的 5 个数
    cv::Mat tmp = output.reshape(1, C);   // [5, 8400]
    cv::Mat transposed;
    cv::transpose(tmp, transposed);       // [8400, 5]

    qDebug() << "output 0 dims:" << output.dims
             << "size[0] =" << output.size[0]
             << "size[1] =" << output.size[1]
             << "size[2] =" << output.size[2];

    QVector<cv::Rect> boxes;
    QVector<float> confidences;

    // 坐标是在 640 输入空间下的像素值，要按比例缩回原图
    const float scaleX = static_cast<float>(bgrImage.cols) / m_inputSize;
    const float scaleY = static_cast<float>(bgrImage.rows) / m_inputSize;

    // ------------------------------------------------------------------
    // ③ 解码：逐个候选框换算坐标
    // ------------------------------------------------------------------
    for (int i = 0; i < transposed.rows; ++i) {
        const float* row = transposed.ptr<float>(i);

        const float cx = row[0];
        const float cy = row[1];
        const float w = row[2];
        const float h = row[3];
        const float score = row[4];

        if (score < confThreshold) {
            continue;   // 置信度不够，丢掉
        }

        // 中心点 + 宽高 → 左上角 + 宽高
        int left = static_cast<int>((cx - w / 2.0f) * scaleX);
        int top = static_cast<int>((cy - h / 2.0f) * scaleY);
        int width = static_cast<int>(w * scaleX);
        int height = static_cast<int>(h * scaleY);

        // 裁剪到图像范围内
        left = std::max(0, left);
        top = std::max(0, top);
        width = std::min(width, bgrImage.cols - left);
        height = std::min(height, bgrImage.rows - top);
        if (width <= 0 || height <= 0) {
            continue;
        }

        boxes.append(cv::Rect(left, top, width, height));
        confidences.append(score);
    }

    // ------------------------------------------------------------------
    // ④ NMS 去重：同一个人可能有好几个重叠的框，只留最好的那个
    // ------------------------------------------------------------------
    nms(boxes, confidences, nmsThreshold);

    // ------------------------------------------------------------------
    // ⑤ 组装结果
    // ------------------------------------------------------------------
    for (int i = 0; i < boxes.size(); ++i) {
        FaceDetection d;
        d.box = boxes[i];
        d.confidence = confidences[i];
        results.append(d);
    }

    return results;
}

// =============================================================================
// 【decodeOutput】多尺度 YOLOv5 输出的解码（备用）
//
//   导出的模型如果带了 Detect 层（--include onnx），输出就是上面那种
//   [1, 5, 8400] 的格式，detect() 里直接解就行。
//   如果导出的是【不带 Detect 层】的原始输出，形状是 [1, N, C]（C=6），
//   每个格子里的 cx/cy 是【相对网格】的偏移，要按下面的公式还原：
//       cx = (row[0] * 2 - 0.5 + gridX) * stride
//   这份代码就是为那种导出方式准备的，按需调用。
// =============================================================================
void FaceDetector::decodeOutput(const cv::Mat& output, int stride,
                                const cv::Size& inputSize,
                                const cv::Size& originalSize,
                                float confThreshold,
                                QVector<cv::Rect>& boxes,
                                QVector<float>& confidences)
{
    const int numClasses = output.size[2] - 5;
    const int numAnchors = output.size[1];

    const float* data = output.ptr<float>();

    // 输入空间 -> 原图空间
    const float scaleX = static_cast<float>(originalSize.width) / inputSize.width;
    const float scaleY = static_cast<float>(originalSize.height) / inputSize.height;

    for (int a = 0; a < numAnchors; ++a) {
        const float* row = data + a * output.size[2];

        const float objConf = row[4];
        if (objConf < confThreshold) {
            continue;
        }

        // 找最大的类别分数
        float maxCls = 0.f;
        for (int c = 0; c < numClasses; ++c) {
            maxCls = std::max(maxCls, row[5 + c]);
        }
        const float score = objConf * maxCls;
        if (score < confThreshold) {
            continue;
        }

        const float cx = row[0];
        const float cy = row[1];
        const float w = row[2];
        const float h = row[3];

        const float x = (cx - w / 2.f) * scaleX;
        const float y = (cy - h / 2.f) * scaleY;
        const float bw = w * scaleX;
        const float bh = h * scaleY;

        int left = std::max(0, static_cast<int>(x));
        int top = std::max(0, static_cast<int>(y));
        int right = std::min(originalSize.width, static_cast<int>(x + bw));
        int bottom = std::min(originalSize.height, static_cast<int>(y + bh));
        if (right <= left || bottom <= top) {
            continue;
        }

        boxes.append(cv::Rect(left, top, right - left, bottom - top));
        confidences.append(score);
    }
}

// =============================================================================
// 【nms】非极大值抑制
//
//   YOLO 对同一个人脸会输出好几个重叠的框（比如一张脸分成好几个格子都命中）。
//   NMS 的做法：按置信度从高到低，保留最高分的框，
//   把和它重叠超过阈值的其它框全部删掉。删完就只剩"人脸几个"。
// =============================================================================
void FaceDetector::nms(QVector<cv::Rect>& boxes,
                       QVector<float>& confidences,
                       float nmsThreshold)
{
    if (boxes.isEmpty()) {
        return;
    }

    // 转成 OpenCV DNN 认识的标准容器
    std::vector<cv::Rect> b(boxes.begin(), boxes.end());
    std::vector<float> c(confidences.begin(), confidences.end());

    std::vector<int> indices;
    cv::dnn::NMSBoxes(b, c, 0.0f, nmsThreshold, indices);

    QVector<cv::Rect> outBoxes;
    QVector<float> outConf;
    outBoxes.reserve(indices.size());
    outConf.reserve(indices.size());
    for (int idx : indices) {
        outBoxes.append(b[idx]);
        outConf.append(c[idx]);
    }

    boxes = outBoxes;
    confidences = outConf;
}