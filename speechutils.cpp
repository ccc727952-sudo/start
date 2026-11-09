/*
 * @brief 语音播报工具类【单例】
 * 语音播报工具类【单例】，用于在程序中播报语音
*/
#include "speechutils.h"
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <qtexttospeech.h>


SpeechUtils::SpeechUtils(QObject *parent)
    : QObject{parent}
{
    //创建语音引擎，第一次getInstance再那个线程，这个引擎就属于那个线程
    //当前项目的状态机常驻“人脸处理线程”，所以引擎也归它，之后一直再同一线程里
    m_speak=new QTextToSpeech(this);
    qDebug()<<"[语言]可用引擎："<<QTextToSpeech::availableEngines()<<"当前状态："<<m_speak->state();
}


SpeechUtils& SpeechUtils::getInstance(){
    //函数内静态对象：C++11起保证只初始化一次，天然线程安全
    static SpeechUtils instace;
    return instace;
}

//播报一句话(异步：说完之前的代码就继续往下走了)
void SpeechUtils::speak(const QString& text){
    qDebug()<<"[语言]:"<<text;
    m_speak->say(text);
}

//播报一句话，并等待：用到事件循环等、界面不会卡死
void SpeechUtils::speak(const QString& text,int timeout){
    speak(text);
    //让“提示词”播完在进入下一步，但【不能】用Sleep---那会冻住整个线程
    //做法：开一个局部事件循环，定时器到点就 quit，期间Qt事件照常处理
    QEventLoop loop;
    QTimer timer;
    connect(&timer,&QTimer::timeout,&loop,&QEventLoop::quit);
    timer.start(timeout);
    loop.exec();
}