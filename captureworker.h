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



#ifndef CAPTUREWORKER_H
#define CAPTUREWORKER_H

#include <QObject>
#include <QImage>//用于显示采集到的图像
#include <QTimer>
#include <opencv2/opencv.hpp>//用于处理图像

//摄像头采集线程，从摄像头取一帧图像，然后发信号通知别人，不关注图像显示到哪里(FaceInputManager的事情)，界面长什么样子(完全不碰QML)
/*为什么必须放在子线程里？
 *  打开摄像头，从摄像头读一帧都是【耗时操作】，摄像头没有准备好cap>>frame会阻塞，
 *  放在主线程里做，整个界面就卡死(鼠标点不动、窗口白屏)
 *  Qt的规矩：一个对象的槽函数都在它所属线程里指向，所以把CaptureWorker搬到子线程，它的槽函数就自动在子线程跑

*/
/*新增三件事：
 *  1.跳帧：不是每一帧都处理，每5帧处理一次(否则处理不过来，画面会卡)
 *  2.剪裁：只取画面中间【 引导框对应的那一块】送去处理
 *  3.交接：把裁好的区域用信号发给"人脸处理线程"(FaceHanleWorker)
*/
class CaptureWorker : public QObject
{
    Q_OBJECT
public:
    explicit CaptureWorker(QObject *parent = nullptr);

signals:
    //取到一帧，通知上层FaceInputManager拿去显示
    void onFrameReady(const QImage &image);
    //出错了(打不开摄像头、读帧失败)，通知上层弹提示FaceInputManager
    void errorOccurred(const QString &message);
    //把裁剪好的引导框区域交给  人脸处理线程
    void handleFrame(const cv::Mat& image);
public slots:
    //打开摄像头，index是摄像头的编号(0=第一个)
    void open(int index);
    //关闭摄像头，type=0暂停采集、type=1彻底释放设备
    void close(int type);
    //抓取一帧(由定时器自动调动，一般不用手动掉)
    void grabFrame();

    //人脸处理线程回报：这一帧处理完了
    void handleFrameFinished(bool isFinished);
    //停掉采集定时器，因为定时器在CaptureWorker，发信号让CaptureWorker自己停止采集线程
    void stopTimer();
    //提交完成后，重新运行送帧
    void resetDetect();
private:
    //定时器
    QTimer *timer=nullptr;
    //OpenCV 的摄像头句柄
    cv::VideoCapture cap;
    //是否正在采集
    bool m_isrunning=false;
    //帧数计数器：用来做跳帧(每5帧才处理一次)
    QAtomicInt m_frameCounter=0;
    //当前是否有一帧正在被处理  true 上一帧还没处理完，这一帧就先不送，false 处理线程空闲，送帧
    bool is_processing=false;
    //当前这个用户的特征是否已经提取完
    // true=特征已提取，在等用户信息、提交、这期间不在处理新帧
    // false 可以正常处理
    bool is_detect=false;
};

#endif // CAPTUREWORKER_H
