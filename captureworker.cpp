/*
 * @brief 摄像头采集线程
 * 
没有函数

有信号
1.onFrameReady(const QImage &image)：取到一帧，通知上层FaceInputManager拿去显示
2.errorOccurred(const QString &message)：出错了(打不开摄像头、读帧失败)，通知上层弹提示FaceInputManager
3.handleFrame(const cv::Mat& image)：把裁剪好的引导框区域交给  人脸处理线程

有槽函数
1.void open(int index);//打开摄像头，index是摄像头的编号(0=第一个)  
2.void close(int type);//关闭摄像头，type=0暂停采集、type=1彻底释放设备
3.void grabFrame();//抓取一帧视频，由定时器自动调动，一般不用手动掉
4.void handleFrameFinished(bool isFinished);//处理一帧结束，通知上层，可以处理下一帧了
5.void stopTimer();//停止定时器，用于暂停采集
 * 它包含了一个定时器，每33毫秒抓取一帧视频，然后发送给qml用户。
*/




#include "captureworker.h"
#include <QDebug>
#include <QThread>

CaptureWorker::CaptureWorker(QObject *parent)
    : QObject{parent}
{
    //定时器
    this->timer=new QTimer(this);//以this为父对象，让定时器跟着CaptureWorker一起搬到子线程

    connect(this->timer,&QTimer::timeout,this,&CaptureWorker::grabFrame);//定时器每33毫秒抓取一帧视频

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
    if(!this->m_isrunning){//如果采集线程没有运行，直接返回
        return;
    }
    cv::Mat frame;//存储采集到的帧
    this->cap>>frame;//这一步可能会阻塞，所以必须在子线程中//读取一帧视频
    if(frame.empty()){//如果读取到的帧为空，说明摄像头有问题，也要给用户提示
        //读取帧失败，说明摄像头有问题，也要给用户提示
        emit errorOccurred("读取失败");
        this->m_isrunning=false;//采集线程结束
        return;
    }
    //为什么要跳帧？人脸检测+dlib推理一次要几十毫秒，摄像头每秒30帧，每一帧都出来，帧会越积越多(越处理越卡)，跳帧减少压力，又流畅又省力
    const int seq=this->m_frameCounter.fetchAndAddOrdered(1);//获取当前帧序号
    //每5帧推一次，并且上一帧已经处理完了，并且用户特征已经提取
        if(seq%2==0 && !this->is_processing && !this->is_detect){//改为了每2帧推一次，因为每2帧处理一次，每2帧提取一次特征，所以每2帧推一次
        //裁剪出【页面引导框】对应的区域，C++和QML必须抱持一致
        int cropW = frame.cols * 0.4;
        int cropH = cropW * 1.3;
        int cropX = (frame.cols - cropW) / 2;
        int cropY = (frame.rows - cropH) / 2;
        cv::Rect roi(cropX, cropY, cropW, cropH);
        //frame和roi引用(同一块内存)，拷贝出来
        const cv::Mat faceRegion=frame(roi).clone();
        //测试 裁剪后的图
        // const QString path = "./face_test_%1.jpg";
        // cv::imwrite(path.arg(seq).toStdString(),faceRegion);

        //将裁剪好的区域，通过信号发给  人脸处理线程，跨线程的信号会自动变成队列连接，不用自己加锁
        emit this->handleFrame(faceRegion);
        //标记：有一帧正在处理，处理线程回报之前不在送新帧
        this->is_processing=true;
    }




    //BGR->RGB
    cv::cvtColor(frame,frame,cv::COLOR_BGR2RGB);

    //将Mat转为QImage,方便页面展示
    QImage image(frame.data,frame.cols,frame.rows,static_cast<int>(frame.step),QImage::Format_RGB888);
    //发信号通知做处理,发信号传递拷贝后的对象，不要传递原对象，因为Mat的内存下次会被覆盖
    emit this->onFrameReady(image.copy());
}

//人脸处理线程回报：这一帧处理完了
void CaptureWorker::handleFrameFinished(bool isFinished){
    //上一帧处理完了，可以再送新帧进来
    this->is_processing=false;
    if(isFinished){
        //用户特征已经提取，等待管理员填写 信息、提交、提交完成之前不再处理新帧
        this->is_detect=true;
    }
}
//停掉采集定时器，因为定时器在CaptureWorker，发信号让CaptureWorker自己停止采集线程
void CaptureWorker::stopTimer(){
    //停掉定时器，停掉后定时器就不在了，避免析构时也不再报错
    this->timer->stop();
}
//提交完成后，重新运行送帧
void CaptureWorker::resetDetect(){
    this->is_detect=false;
    this->is_processing=false;
    qDebug()<<"【采集】已经复位，可以继续送帧了";
}