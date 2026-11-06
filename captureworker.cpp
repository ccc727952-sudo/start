#include "captureworker.h"
#include <QDebug>
#include <QThread>

CaptureWorker::CaptureWorker(QObject *parent)
    : QObject{parent}
{
    //定时器
    this->timer=new QTimer(this);//以this为父对象，让定时器跟着CaptureWorker一起搬到子线程

    connect(this->timer,&QTimer::timeout,this,&CaptureWorker::grabFrame);

    //1s大概30帧
    this->timer->start(33);//33毫秒
}

//打开摄像头，index是摄像头的编号(0=第一个)
void CaptureWorker::open(int index){
    qDebug()<<"CaptureWorker 所在线程："<<QThread::currentThread();
    //开始采集
    this->m_isrunning=true;
    //打开摄像头
    if(!this->cap.isOpened()){
        //Windows 上使用DSHOW都端更稳定
        this->cap.open(index,cv::CAP_DSHOW);
        //打开后为false，说明打开失败
        if(!this->cap.isOpened()){
            //给qml用户做提示
            emit errorOccurred("打开摄像头失败");
            this->m_isrunning=false;
        }
    }
}
//关闭摄像头，type=0暂停采集、type=1彻底释放设备
void CaptureWorker::close(int type){
    this->m_isrunning=false;
    if(type==1){
        //要彻底关闭摄像头，把设备释放掉
        this->cap.release();
    }
    //type==0只是暂时不采集，设备还开着，下次open接着使用
}
//抓取一帧(由定时器自动调动，一般不用手动掉)
void CaptureWorker::grabFrame(){
    if(!this->m_isrunning){
        return;
    }
    cv::Mat frame;
    this->cap>>frame;//这一步可能会阻塞，所以必须在子线程中
    if(frame.empty()){
        //读取帧失败，说明摄像头有问题，也要给用户提示
        emit errorOccurred("读取失败");
        this->m_isrunning=false;
        return;
    }

    //BGR->RGB
    cv::cvtColor(frame,frame,cv::COLOR_BGR2RGB);

    //将Mat转为QImage,方便页面展示
    QImage image(frame.data,frame.cols,frame.rows,static_cast<int>(frame.step),QImage::Format_RGB888);
    //发信号通知做处理,发信号传递拷贝后的对象，不要传递原对象，因为Mat的内存下次会被覆盖
    emit this->onFrameReady(image.copy());
}
