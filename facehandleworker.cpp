#include "facehandleworker.h"
#include <QDebug>
FaceHandleWorker::FaceHandleWorker(QObject *parent)
    : QObject{parent}
{
    //在这里创建 faceDetector和dlibUtil
    this->faceDetector=new FaceDetector;
    this->dlibUtil=new DlibUtil;

}

//收到一帧图像，做  人脸识别  -> 活体检测  ->特征提取
//状态机实现
void FaceHandleWorker::handleFrame(const cv::Mat& image){
    bool isFinished=false;
    //人脸录入流程： 人脸检测->活体检测->特征向量提取->之后保存
    const auto faces=this->faceDetector->detect(image);
    qDebug()<<"(检测)本帧人脸数："<<faces.size()<<
        "当前状态(0=等检测  1=已检测  2=活体通过  3=特征已提取)"<<int(m_workState);

    switch (m_workState) {
    case START:
        //先来做人脸加内存
        if(!faces.isEmpty()){
            qDebug()<<"人脸检测已通过...";
            m_workState=FACE_DELECTED;
        }
        break;
    case FACE_DELECTED:
        //活体检测，眨眼
        if(faces.isEmpty()){
            return;//这一帧丢了
        }
        {
            const bool isBlinked=this->dlibUtil->dlibFaceLivenessDetect(image,faces[0].box);
            if(isBlinked){
                qDebug()<<"活体检测已通过.............................................";
                m_workState=LIVENESS_DELECTED;
            }
        }
        break;
    case LIVENESS_DELECTED:
        //活体检测通过，这一步要提取特征向量
        if(faces.isEmpty()){
            return;//这一帧丢了
        }
        {
            const std::vector<float> features=this->dlibUtil->getFaceFeatures(image,faces[0].box);
            if(!features.empty()){
                qDebug()<<"已经提取到人脸特征向量，可以准备提交数据了...";
                qDebug()<<features;
                m_workState=FEATURES_EXITRACTED;
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