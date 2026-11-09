/*
 * @brief 人脸输入管理器// 负责管理人脸输入模块的运行// 包含采集线程、处理线程等功能。
 * 
*/
#include "faceinputmanager.h"
#include <QDebug>
#include <QVideoFrameFormat>
#include <QVideoFrame>
#include <cstring>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>
#include <qstringliteral.h>

static const QString API_BASE=QStringLiteral("http://localhost:80");

//模拟接口    布尔ture   "true"，字符串会返回false，需要处理兼容性
static bool isSuccess(const QJsonValue& v){
    return v.isBool()?v.toBool():(v.toString()==QStringLiteral("true"));
}


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
    this->manager=new QNetworkAccessManager(this);//http请求管理器创建对象
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
    //处理线程->主线程，收到特征向量存入features
    connect(this->faceHandleWorker,&FaceHandleWorker::sendFaceFeaturce,this,&FaceInputManager::receiveFaceFeatures);
        //处理线程->界面：把活体引导提示转发给QML
    //信号连信号，跨线程自动变队列连接，不用加锁
    connect(this->faceHandleWorker,&FaceHandleWorker::livenessPrompt,this,&FaceInputManager::livenessPrompt);
    //复位信号
    connect(this,&FaceInputManager::resetCapture,this->capWorker,&CaptureWorker::resetDetect);
    connect(this,&FaceInputManager::resetWorkState,this->faceHandleWorker,&FaceHandleWorker::resetWorkState);

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

//处理线程送来的128维特征，存起来等待提交
void FaceInputManager::receiveFaceFeatures(const std::vector<float>& features){
    qDebug()<<"录入收到人脸特征，维度"<<features.size();
    this->features=features;
}

//按住户编号查询信息(QML的“查询”按钮调用)
void FaceInputManager::queryUserInfo(const QString& sn){
    if(sn.isEmpty()){
        emit errorOccurred("获取用户信息失败，用户编号不能为空!");
        return;
    }
    //1.拼URL
    const QString url=API_BASE+QStringLiteral("/getuser.json?sn=%1").arg(sn);
    qDebug()<<"[Http] Get "<<url;
    //2.发起异步Get
    QNetworkReply* reply=this->manager->get(QNetworkRequest(QUrl(url)));
    connect(reply,&QNetworkReply::finished,this,[reply,this](){
        //响应内容
        const QByteArray data=reply->readAll();
        reply->deleteLater();
        const QJsonDocument doc=QJsonDocument::fromJson(data);
        qDebug()<<"[HTTP] 住户信息: "<<doc.object();
        emit this->showUserData(doc.object());
    });
}

//发送验证码(QML的“发送验证码”按钮调用)
void FaceInputManager::sendVerifyCode(const QString& phone){
    if(phone.isEmpty()){
        emit errorOccurred("发送验证码失败，手机号不能为空!");
        return;
    }
    //1.拼URL:
    const QString url=API_BASE+QStringLiteral("/getcode.json?phone=%1").arg(phone);
    qDebug()<<"[Http] Get "<<url;
    //2.发起异步Get
    QNetworkReply* reply=this->manager->get(QNetworkRequest(QUrl(url)));
    connect(reply,&QNetworkReply::finished,this,[reply,this](){
        //响应内容
        const QByteArray data=reply->readAll();
        //防止内存泄露
        reply->deleteLater();
        const QJsonObject obj=QJsonDocument::fromJson(data).object();
        qDebug()<<"[HTTP] 验证码接口返回: "<<obj;
        if(isSuccess(obj.value("success"))){
            emit errorOccurred("验证码发送成功，请注意查收！");
        }else{
            emit errorOccurred("验证码发送失败，请重新点击发送！");
        }
    });
}

//提交住户 + 人脸特征(QML的"提交"按钮调它)
/*
* @brief 提交住户 + 人脸特征(QML的"提交"按钮调它)
* @param obj 住户信息JSON对象
* @param verifyCode 验证码
*/
void FaceInputManager::saveData(const QJsonObject& obj,const QString& verifyCode){
    //1.从QML传递来的JSON对象里取值
    const QString sn=obj.value("sn").toString();
    const QString name=obj.value("name").toString();
    const QString phone=obj.value("phone").toString();
    const QString address=obj.value("address").toString();
    //2.五项都要非空才能提交
    if(sn.isEmpty() || name.isEmpty() || phone.isEmpty() || address.isEmpty()){
        emit errorOccurred("提交失败，信息不能为空！");
        return;
    }

    //3.人脸特征也不能为空，没采集就提交，不能过
    if(this->features.empty()){
        emit errorOccurred("请先采集人脸信息后再点击提交！");
        return;
    }

    //4.vector<float>转成Base64文本
    const QByteArray raw(reinterpret_cast<const char*>(this->features.data()),
                         static_cast<int>(this->features.size()*sizeof(float)));
    const QString featuresText=QString::fromLatin1(raw.toBase64());
    qDebug()<<"【录入】特征原始字节："<<raw.size()<<" Base64长度"<<featuresText.size();
    const QString url=API_BASE+QStringLiteral("/savefacefeature.json?sn=%1&name=%2&phone=%3&address=%4&verifyCode=%5&features=%6")
                        .arg(sn,name,phone,address,verifyCode,featuresText,featuresText);
    qDebug()<<"[HTTP] Get提交: "<<url;

    QNetworkReply* reply=this->manager->get(QNetworkRequest(QUrl(url)));
    connect(reply,&QNetworkReply::finished,this,[reply,this](){
        //响应内容
        const QByteArray data=reply->readAll();
        //防止内存泄露
        reply->deleteLater();
        const QJsonObject obj=QJsonDocument::fromJson(data).object();
        qDebug()<<"[HTTP] 提交保存人脸特征向量接口返回: "<<obj;
        /*
        注意顺序 （这条很关键）：
        1. `features.clear()` —— 先清掉这位住户的特征，否则下一个人会提交到上一个人的特征
        2. `emit saveDataSuccess()` —— 通知界面清空
        3. `emit resetWorkState()` —— 必须先于`resetCapture` ，让状态机先回`START` （上次讲过，顺序反了复位后头几帧会被丢）
        4. `emit resetCapture()` —— 最后才放开采集
        */
        if(isSuccess(obj.value("success"))){
            emit errorOccurred("提交成功！");
            //先通知界面清空，再复位两个子线程
            this->features.clear();
            emit this->saveDataSuccess();
            emit this->resetWorkState();
            emit this->resetCapture();
        }else{
            emit errorOccurred("提交失败，请重新提交！");
        }
    });
}




