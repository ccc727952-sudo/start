#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <opencv2/opencv.hpp>
#include <QDebug>
#include "loginmanager.h"
#include "faceinputmanager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    // 登录管理器 —— 注册给 QML 用（登录案例要用）
    qmlRegisterType<LoginManager>("cn.rh", 1, 0, "LoginManager");
    qmlRegisterType<FaceInputManager>("cn.rh", 1, 0, "FaceInputManager");
    engine.loadFromModule("accessControlSystem_faceInput", "Main");
    qDebug()<<"OpenCV 版本："<<CV_VERSION;
    return QGuiApplication::exec();
}
