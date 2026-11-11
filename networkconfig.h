/*
 * @brief 网络配置读写器 —— 负责把 WiFi/网络参数写入 Network.ini
 *
 * 为什么要封装成 QObject 子类：
 *   QML 侧没有直接操作 INI 的 API，必须通过 qmlRegisterType 注册后
 *   以 Q_INVOKABLE 方法的形式给 QML 调用。
 *
 * INI 文件位置：与 exe 同目录（QCoreApplication::applicationDirPath() + "/Network.ini"）
 * 这样部署时直接改 exe 旁边的 ini 文件就行，不用找系统路径。
 */
#ifndef NETWORKCONFIG_H
#define NETWORKCONFIG_H

#include <QObject>
#include <QVariantMap>

class NetworkConfig : public QObject
{
    Q_OBJECT
public:
    explicit NetworkConfig(QObject *parent = nullptr);

    // 保存网络配置到 INI 文件
    // 返回 true=写入成功，false=写入失败（比如权限问题）
    Q_INVOKABLE bool saveConfig(const QString &wifiName,
                                const QString &wifiPassword,
                                const QString &networkName,
                                const QString &networkPassword);

    // 从 INI 文件读取配置，返回 QVariantMap（QML 侧自动转成 JS 对象）
    // 结构：{ wifiName, wifiPassword, networkName, networkPassword }
    // 不管文件存不存在，都会返回完整结构（不存在的字段给空字符串默认值）
    Q_INVOKABLE QVariantMap loadConfig();

    // 获取 INI 文件的绝对路径（调试用，或者 QML 里显示给用户看）
    Q_INVOKABLE QString configFilePath() const;

signals:
    // 写入成功/失败的信号（QML 可以接了弹提示框）
    void saveSucceeded();
    void saveFailed(const QString &error);

private:
    QString m_filePath;   // INI 文件的绝对路径
};

#endif // NETWORKCONFIG_H