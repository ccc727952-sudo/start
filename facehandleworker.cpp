
/*
 * @brief 人脸处理线程// 负责处理一帧图像的人脸识别任务// 包含人脸检测、活体检测、特征提取等功能。
 * 
*/
#include "facehandleworker.h"
#include <QDebug>
#include <algorithm>//排序
#include "speechutils.h"

FaceHandleWorker::FaceHandleWorker(QObject *parent)
    : QObject{parent}
{
    //在这里创建 faceDetector和dlibUtil
    this->faceDetector=new FaceDetector;
    this->dlibUtil=new DlibUtil;
    SpeechUtils::getInstance().speak("开始采集，请正对摄像头!",2500);
}
//发引导提示：同一句话只发一次，避免每帧刷屏
void FaceHandleWorker::emitPrompt(const QString &text)
{
    if (text == m_lastPrompt) return;
    m_lastPrompt = text;
    emit livenessPrompt(text);
}
//收到一帧图像，做  人脸识别  -> 活体检测  ->特征提取
//状态机实现
void FaceHandleWorker::handleFrame(const cv::Mat& image){
    bool isFinished=false;
    //人脸录入流程： 人脸检测->活体检测->特征向量提取->之后保存
    auto faces=this->faceDetector->detect(image);
    std::sort(faces.begin(), faces.end(),
              [](const FaceDetection& a, const FaceDetection& b){
                  return a.box.area() > b.box.area();
              });
    qDebug()<<"(检测)本帧人脸数："<<faces.size()<<
        "当前状态(0=等检测  1=已检测  2=活体通过  3=特征已提取)"<<int(m_workState);

    switch (m_workState) {
        case START:
        //先来做人脸加内存
        if(!faces.isEmpty()){
            qDebug()<<"人脸检测已通过...";
            this->dlibUtil->resetLivenessState();   // ★ 新人来了，状态机清零
            m_workState=FACE_DELECTED;
            this->emitPrompt("请眨一下眼");          // ★ 提示
        } else {
            this->emitPrompt("请将人脸放入框内");     // ★ 提示
        }
        break;
    case FACE_DELECTED:
        if(faces.isEmpty()){
            this->emitPrompt("请将人脸放入框内");
            emit this->handleFrameFinished(false);
            return;
        }
        if(this->dlibUtil->dlibFaceLivenessDetect(image, faces[0].box)){
            qDebug()<<"眨眼通过，请张嘴...";
            m_workState=BLINK_PASSED;
            this->emitPrompt("请张一下嘴");          // ★ 提示
        }
        break;
    case BLINK_PASSED:
        if(faces.isEmpty()){
            this->emitPrompt("请将人脸放入框内");
            emit this->handleFrameFinished(false);
            return;
        }
        if(this->dlibUtil->detectMouthOpen(image, faces[0].box)){
            qDebug()<<"张嘴通过，请摇头...";
            m_workState=MOUTH_PASSED;
            this->emitPrompt("请左右摇摇头");        // ★ 提示
        }
        break;
    case MOUTH_PASSED:
        if(faces.isEmpty()){
            this->emitPrompt("请将人脸放入框内");
            emit this->handleFrameFinished(false);
            return;
        }
        if(this->dlibUtil->detectHeadShake(image, faces[0].box)){
            qDebug()<<"摇头通过，活体检测全部完成...";
            m_workState=LIVENESS_DELECTED;
            SpeechUtils::getInstance().speak("活体检测已通过!",3000);
            this->emitPrompt("活体检测通过，正在提取特征...");  // ★ 提示
        }
        break;
    case LIVENESS_DELECTED:
        //活体检测通过，这一步要提取特征向量
        if(faces.isEmpty()){
            emit this->handleFrameFinished(false);   // ★ 补上，别让流水线冻结
            return;
        }
        {
            const std::vector<float> features=this->dlibUtil->getFaceFeatures(image,faces[0].box);
            if(!features.empty()){
                qDebug()<<"已经提取到人脸特征向量，可以准备提交数据了...";
                qDebug()<<features;
                SpeechUtils::getInstance().speak("采集成功，请填写基本信息后点击提交",4000);
                //把特征向量发送给主线程
                emit this->sendFaceFeaturce(features);
                m_workState=FEATURES_EXITRACTED;
                this->emitPrompt("录入完成，请填写人员信息");  // ★ 提示
                //特征已提取，暂时不要处理下一帧，等到管理员填写完业主信息后才放开
                isFinished=true;
            }
        }
        break;
    case FEATURES_EXITRACTED:
        //特征已提取，管理员填写完业主信息后才放开
        //提交完成后线程糊掉START，欢迎下一位
        break;
    default:
        break;
    }
    // //告诉采集线程：这一帧处理完了，可以送下一帧
    emit this->handleFrameFinished(isFinished);

    // //做  人脸识别  -> 活体检测  ->特征提取
    // //人脸识别 先跑起来
    // auto faces=this->faceDetector->detect(image);
    // qDebug()<<"本帧人脸数"<<faces.size();

    // //关键点和特征向量
    // if(!faces.isEmpty()){
    //     //把68个关键点画再图上，能直观看到关键点有没有落到五官上
    //     cv::Mat show=image.clone();
    //     this->dlibUtil->showLandmarks(show,faces[0].box);
    //     //特征向量：人脸对其-->ResNet-->128维
    //     std::vector<float> features=this->dlibUtil->getFaceFeatures(image,faces[0].box);
    //     qDebug()<<"关键点已经提取，特征向量长度="<<features.size()<<"(==128 就说明特征提取成功)";
    // }

    // //活体检测(眨眼)
    // const bool isBlinked=this->dlibUtil->dlibFaceLivenessDetect(image,faces[0].box);
    // if(isBlinked){
    //     qDebug()<<"活体检测已通过.............................................";
    // }
    // //告诉采集线程：这一帧处理完了，可以送下一帧
    // emit this->handleFrameFinished(false);
}