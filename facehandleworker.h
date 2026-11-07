/*
 * @brief 人脸处理线程// 负责处理一帧图像的人脸识别任务// 包含人脸检测、活体检测、特征提取等功能。
 * 
*/
#ifndef FACEHANDLEWORKER_H
#define FACEHANDLEWORKER_H

#include <QObject>
#include <opencv2/opencv.hpp>
#include "facedetector.h"
#include "dlibutil.h"

//前向声明，占位
// class FaceDetector;
// class DlibUtil;

class FaceHandleWorker : public QObject
{
    Q_OBJECT
public:
    explicit FaceHandleWorker(QObject *parent = nullptr);
    //人脸录入流程的状态
        enum WorkState{
        START,              //等待检测到人脸
        FACE_DELECTED,      //已检测到人脸，下一步做活体检测(眨眼)
        BLINK_PASSED,       //眨眼已通过，下一步张嘴
        MOUTH_PASSED,       //张嘴已通过，下一步摇头
        LIVENESS_DELECTED,  //活体检测全部通过，下一步提取特征
        FEATURES_EXITRACTED //特征已提取，等待用户提交
    };
signals:
    //告诉采集线程，这一帧处理完了，等用户提交才能继续处理下一帧
    void handleFrameFinished(bool isFinished);
public slots:
    //收到一帧图像，做  人脸识别  -> 活体检测  ->特征提取
    void handleFrame(const cv::Mat& image);
private:
    //人脸检测  YOLO-face
    FaceDetector *faceDetector=nullptr;
    //关键点/活体/特征提取
    DlibUtil* dlibUtil=nullptr;
    //当前处于流程的那一步
    WorkState m_workState=START;
};

#endif // FACEHANDLEWORKER_H
