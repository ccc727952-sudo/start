// #ifndef SPEECHUTILS_H
// #define SPEECHUTILS_H

// #include <QObject>
// #include <QTextToSpeech>
// #include <QString>

// class SpeechUtils : public QObject
// {
//     Q_OBJECT
// public:
//     explicit SpeechUtils(QObject *parent = nullptr);
//     //单例
//     SpeechUtils(SpeechUtils const &)=delete;
//     SpeechUtils& operator=(SpeechUtils const &)=delete;
//     static SpeechUtils* getInstance();
//     void speak(const QString& text);//播放语音yibu
//     void speak(const QString& text,int timeout);//播放语音
// private:
//     explicit SpeechUtils(QString *parent = nullptr);
//     QTextToSpeech* m_speak=nullptr;
// signals:
// };

// #endif // SPEECHUTILS_H

/*
 * @brief 语音播报工具类【单例】
 * 语音播报工具类【单例】，用于在程序中播报语音
*/
#ifndef SPEECHUTILS_H
#define SPEECHUTILS_H

#include <QObject>
#include <QString>
#include <QtTextToSpeech>

// 语言播报工具类【单例】
class SpeechUtils : public QObject
{
    Q_OBJECT
public:
    //禁止拷贝：单例只能存在一份
    SpeechUtils(const SpeechUtils&)=delete;
    SpeechUtils& operator=(const SpeechUtils&)=delete;
    static SpeechUtils& getInstance();
    //播报一句话(异步：说完之前的代码就继续往下走了)
    void speak(const QString& text);
    //播报一句话，并等待：用到事件循环等、界面不会卡死
    void speak(const QString& text,int timeout);
signals:
private:
    explicit SpeechUtils(QObject *parent = nullptr);
    QTextToSpeech* m_speak=nullptr;
};

#endif // SPEECHUTILS_H
