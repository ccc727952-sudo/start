#ifndef CAPTUREWORKER_H
#define CAPTUREWORKER_H

#include <QObject>
#include <QImage>
#include <QTimer>
#include <opencv2/opencv.hpp>

//摄像头采集线程，从摄像头取一帧图像，然后发信号通知别人，不关注图像显示到哪里(FaceInputManager的事情)，界面长什么样子(完全不碰QML)
/*为什么必须放在子线程里？
 *  打开摄像头，从摄像头读一帧都是【耗时操作】，摄像头没有准备好cap>>frame会阻塞，
 *  放在主线程里做，整个界面就卡死(鼠标点不动、窗口白屏)
 *  Qt的规矩：一个对象的槽函数都在它所属线程里指向，所以把CaptureWorker搬到子线程，它的槽函数就自动在子线程跑

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
public slots:
    //打开摄像头，index是摄像头的编号(0=第一个)
    void open(int index);
    //关闭摄像头，type=0暂停采集、type=1彻底释放设备
    void close(int type);
    //抓取一帧(由定时器自动调动，一般不用手动掉)
    void grabFrame();
private:
    //定时器
    QTimer *timer=nullptr;
    //OpenCV 的摄像头句柄
    cv::VideoCapture cap;
    //是否正在采集
    bool m_isrunning=false;
};

#endif // CAPTUREWORKER_H
