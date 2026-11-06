#ifndef LOGINMANAGER_H
#define LOGINMANAGER_H

#include <QObject>

class LoginManager : public QObject
{
    Q_OBJECT
public:
    explicit LoginManager(QObject *parent = nullptr);
    // QML 调用：传入账号密码，返回是否成功
    Q_INVOKABLE bool login(const QString &account, const QString &password);

signals:
    void loginSuccess(const QString &account);   // 登录成功信号
    void loginFailed(const QString &msg);   // 登录失败信号，msg 是原因
private:
    // 演示用的"用户表"：账号 → 密码
    // 真实项目里这里会是查数据库、或者调后端接口
    QStringList m_validAccounts;      // 合法的账号
    QStringList m_validPasswords;      // 对应的密码
};

#endif // LOGINMANAGER_H
