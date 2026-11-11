#include "networkconfig.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

// 编译期兜底路径：由 CMakeLists.txt 的 target_compile_definitions 传进来
#ifndef NETWORK_CONFIG_DIR
#define NETWORK_CONFIG_DIR ""
#endif

NetworkConfig::NetworkConfig(QObject *parent)
    : QObject{parent}
{
    // INI 路径：默认放在源码目录（networkconfig.cpp 同级）
    //   ① 环境变量 NETWORK_CONFIG_DIR —— 运行前临时改，最灵活
    //   ② 编译期宏 NETWORK_CONFIG_DIR —— CMakeLists.txt 指向源码目录（默认）
    //   ③ 兜底：exe 同级目录
    const QString env = qEnvironmentVariable("NETWORK_CONFIG_DIR");
    if (!env.isEmpty()) {
        m_filePath = QDir(env).absoluteFilePath(QStringLiteral("Network.ini"));
    } else {
        const QString compiled = QStringLiteral(NETWORK_CONFIG_DIR);
        if (!compiled.isEmpty()) {
            m_filePath = QDir(compiled).absoluteFilePath(QStringLiteral("Network.ini"));
        } else {
            m_filePath = QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral("Network.ini"));
        }
    }
    qDebug() << "[NetworkConfig] 配置文件路径：" << m_filePath;
}

QString NetworkConfig::configFilePath() const
{
    return m_filePath;
}

bool NetworkConfig::saveConfig(const QString &wifiName,
                               const QString &wifiPassword,
                               const QString &networkName,
                               const QString &networkPassword)
{
    // QSettings 自动处理 INI 的 section 和 key
    QSettings settings(m_filePath, QSettings::IniFormat);

    // [WiFi] 段：路由器的 SSID 和密码
    settings.beginGroup("WiFi");
    settings.setValue("ssid",     wifiName);
    settings.setValue("password", wifiPassword);
    settings.endGroup();

    // [Network] 段：设备本身的网络名/密码（比如 AP 模式的热点名）
    settings.beginGroup("Network");
    settings.setValue("name",     networkName);
    settings.setValue("password", networkPassword);
    settings.endGroup();

    // 强制刷盘，避免程序崩溃时丢数据
    settings.sync();

    // 检查写入是否成功（QSettings::NoError 表示正常）
    const QSettings::Status status = settings.status();
    if (status != QSettings::NoError) {
        const QString errMsg = QStringLiteral("写入 Network.ini 失败，错误码：%1").arg(status);
        qWarning() << "[NetworkConfig]" << errMsg;
        emit saveFailed(errMsg);
        return false;
    }

    qDebug() << "[NetworkConfig] 配置保存成功";
    emit saveSucceeded();
    return true;
}

QVariantMap NetworkConfig::loadConfig()
{
    QVariantMap result;

    // 先给全量默认值，保证 QML 侧拿到的一定有 4 个 key
    result["wifiName"]         = QString();
    result["wifiPassword"]     = QString();
    result["networkName"]      = QString();
    result["networkPassword"]  = QString();

    QSettings settings(m_filePath, QSettings::IniFormat);

    // 如果 INI 文件不存在，QSettings::status() 会返回 FileError 但不会崩溃
    // 这里我们静默处理——没有配置就给空值
    if (settings.status() == QSettings::NoError) {
        settings.beginGroup("WiFi");
        result["wifiName"]     = settings.value("ssid",     QString()).toString();
        result["wifiPassword"] = settings.value("password", QString()).toString();
        settings.endGroup();

        settings.beginGroup("Network");
        result["networkName"]     = settings.value("name",     QString()).toString();
        result["networkPassword"] = settings.value("password", QString()).toString();
        settings.endGroup();
    } else {
        qDebug() << "[NetworkConfig] 没有找到 Network.ini，返回空配置";
    }

    return result;
}