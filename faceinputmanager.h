#ifndef FACEINPUTMANAGER_H
#define FACEINPUTMANAGER_H

#include <QObject>
#include <QImage>
#include <QThread>
#include <QVideoSink>
#include "captureworker.h"
#include "facehandleworker.h"

/* FaceInputManager人脸录入管理器  C++与QML之间的门面
 * 它做三件事：
 *  1.管理采集子线程(创建、启动、停止)
 *  2.接住QML的VideoOutput提供的sink（画面要显示到哪儿）
 *  3.把CaptureWorker采到的帧转成VideoFrame交给QML显示
 *
 *  QML不直接指挥CaptureWorker(它在另一个线程里)，CaptureWorker也不知道QML的存在
 *      QML --调用-->  FaceInputManager  --信号--> CaptureWorker(子线程)
 *      QML <--信号--  FaceInputManager  <--信号-- CaptureWorker(子线程)
 *      三方各司其责，改任何一方都不影响另外两方
 */
class FaceInputManager : public QObject
{
    Q_OBJECT
    //把QML的显示目标交给C++
    // QML那边的VideoOutput提供一个videoSink 是一个QVideoSink类型的C++对象，本质上是一个VideoOutput内部用来接收视频帧的“入口”或“插槽”
    //并且赋值后  QML侧的绑定能够自动刷新
    Q_PROPERTY(QVideoSink* displaySink READ getDisplaySink WRITE setDisplaySink NOTIFY displaySinkChanged)

public:
    explicit FaceInputManager(QObject *parent = nullptr);

    //析构函数
    ~FaceInputManager() override;

    //displaySink的getter/setter
    QVideoSink* getDisplaySink() const;
    //FaceInputManager只管推帧，显示层爱怎么用就怎么用-->依赖倒置/解耦
    void setDisplaySink(QVideoSink* sink);//QML播放

    //供QML页面调用：打开/关闭摄像头
    Q_INVOKABLE void openCamera(int index);
    Q_INVOKABLE void closeCamera(int type);
signals:
    //通知子线程里面的CaptureWorker打开/关闭摄像头
    void notifyOpenCamera(int index);
    void notifyCloseCamera(int type);
    //displaySinkChanged 属性变化信号
    void displaySinkChanged();
    //出错了，通知QML谈提示框
    void errorOccurred(const QString &message);
public slots:
    //worker采集到帧后，通过信号通知，在这里操作QML可以显示的对象
    void onFrameReady(const QImage &image);
private:
    CaptureWorker* capWorker=nullptr;//采集线程的worker
    QThread* capWorkerThread=nullptr;//承载采集线程的worker的线程
    QVideoSink *m_displaySink=nullptr;//QML的VideoOutput提供的sink
    //人脸处理线程的faceHandleWorker
    FaceHandleWorker *faceHandleWorker=nullptr;
    //开启人脸处理线程
    QThread* faceHandleWorkerThread=nullptr;
};

#endif // FACEINPUTMANAGER_H
