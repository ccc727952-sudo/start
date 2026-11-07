/*
 * @brief 人脸输入管理器// 负责管理人脸输入模块的运行// 包含采集线程、处理线程等功能。
 * 
*/
#include "faceinputmanager.h"
#include <QDebug>
#include <QVideoFrameFormat>
#include <QVideoFrame>
#include <cstring>

FaceInputManager::FaceInputManager(QObject *parent)
    : QObject{parent}
{
    this->capWorker=new CaptureWorker;
    //移动到子线程中去执行
    this->capWorkerThread=new QThread;
    this->capWorker->moveToThread(this->capWorkerThread);

    this->faceHandleWorker=new FaceHandleWorker;
    //移动到子线程中去执行
    this->faceHandleWorkerThread=new QThread;
    this->faceHandleWorker->moveToThread(this->faceHandleWorkerThread);

    //绑定信号和槽
    //跨线程的信号与槽变成队列连接，槽函数是在槽对象所属的线程中指向
    //通知子线程的woker打开/关闭摄像头
    connect(this,&FaceInputManager::notifyOpenCamera,this->capWorker,&CaptureWorker::open);
    connect(this,&FaceInputManager::notifyCloseCamera,this->capWorker,&CaptureWorker::close);
    //CaptureWorker捕获的帧，交给FaceInputManager
    connect(this->capWorker,&CaptureWorker::onFrameReady,this,&FaceInputManager::onFrameReady);
    //摄像头有错误，通知
    connect(this->capWorker,&CaptureWorker::errorOccurred,this,&FaceInputManager::errorOccurred);

    //采集线程->处理线程：把裁剪好的引导框区域送过去
    connect(this->capWorker,&CaptureWorker::handleFrame,this->faceHandleWorker,&FaceHandleWorker::handleFrame);
    //处理线程->采集线程：报告这一帧处理完了
    connect(this->faceHandleWorker,&FaceHandleWorker::handleFrameFinished,this->capWorker,&CaptureWorker::handleFrameFinished);

    //开启子线程
    this->capWorkerThread->start();
    this->faceHandleWorkerThread->start();
}

//析构函数
FaceInputManager::~FaceInputManager(){

    //先让采集线程自己停止定时器
    QMetaObject::invokeMethod(this->capWorker,"stopTimer",Qt::BlockingQueuedConnection);
    this->faceHandleWorkerThread->quit();//让线程的事件循环退出
    this->faceHandleWorkerThread->wait();//等它真的退出
    delete this->faceHandleWorkerThread; //必须在wait之后删除

    this->capWorkerThread->quit();//让线程的事件循环退出
    this->capWorkerThread->wait();//等它真的退出
    delete this->capWorker; //必须在wait之后删除
}
//displaySink的getter/setter
QVideoSink* FaceInputManager::getDisplaySink() const{
    return this->m_displaySink;
}

//FaceInputManager只管推帧，显示层爱怎么用就怎么用-->依赖倒置/解耦
//QML播放
void FaceInputManager::setDisplaySink(QVideoSink* sink){
    this->m_displaySink=sink;
    emit displaySinkChanged();
}

//供QML页面调用：打开/关闭摄像头
void FaceInputManager::openCamera(int index){
    emit notifyOpenCamera(index);
}
void FaceInputManager:: closeCamera(int type){
    emit notifyCloseCamera(type);
}

//worker采集到帧后，通过信号通知，在这里操作QML可以显示的对象
void FaceInputManager::onFrameReady(const QImage &image){
    if(!m_displaySink || image.isNull()){
        qDebug()<<"m_displaySink 为空，先不显示";
        return;
    }

    QVideoFrameFormat fmt(image.size(),QVideoFrameFormat::Format_RGBX8888);
    QVideoFrame frame(fmt);
    if(!frame.map(QVideoFrame::WriteOnly)){
        return;
    }
    QImage converted=image.convertToFormat(QImage::Format_RGBX8888);
    //关键：单平面格式，一次性拷贝整个缓冲区
    memcpy(frame.bits(0),converted.constBits(),converted.sizeInBytes());
    /*
    QVideoFrame::unmap() 是 Qt 多媒体框架中用于 解除视频帧内存映射 的一个关键函数。
    在处理视频帧数据时，您通常需要直接访问帧的原始像素数据，以便进行读取、修改或将其上传到 GPU。内存映射（Map） 就是一个操作，它让视频帧的底层数据（通常存储在硬件或特殊缓冲区中）可供 CPU 上的程序像访问普通内存块一样进行访问。
    QVideoFrame::map()
    当您调用这个函数时，系统会确保视频帧的像素数据对您的程序来说是可读写的，并返回指向这些数据的指针（bits()）。
    当您完成对视频帧数据的处理后，就应该调用 QVideoFrame::unmap() 来解除映射
    */
    frame.unmap();
    //拿到  frame后交给 m_displaySink，由QML做展示
    this->m_displaySink->setVideoFrame(frame);
}







