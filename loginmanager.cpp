/*
 * @brief 登录管理器// 负责管理登录模块的运行// 包含登录校验等功能。
 * 
*/
#include "loginmanager.h"
#include <QDebug>

LoginManager::LoginManager(QObject *parent) : QObject(parent)
{
    // 初始化"用户表",演示用的假数据。
    m_validAccounts << "admin" << "zhangsan";
    m_validPasswords << "123"    << "123456";
}


// 【登录校验】
//   返回值 + 信号 两种通知方式同时用，各自解决不同的问题：
//   ① 返回值 bool
//      适合"立刻就知道结果"的场景。
//      QML 里可以写：if (loginManager.login(a, p)) { ... }
//   ② 信号 loginSuccess / loginFailed
//      适合"要等网络/数据库"的场景 —— 函数早就返回了，
//      结果过一会儿才到，只能靠信号通知。
//      真实项目应该以信号为主。
bool LoginManager::login(const QString &account, const QString &password)
{
    qDebug() << "[C++] 收到登录请求: 账号 =" << account << " 密码 =" << password;

    //① 账号为空 / 密码为空 → 直接判失败
    if (account.isEmpty() || password.isEmpty()) {
        const QString msg = "账号和密码不能为空";
        qDebug() << "[C++] 登录失败:" << msg;
        emit loginFailed(msg);      // ★ 发信号告诉 QML
        return false;
    }

    //② 查"用户表"
    int index = m_validAccounts.indexOf(account);
    if (index < 0) {
        const QString msg = "账号不存在";
        qDebug() << "[C++] 登录失败:" << msg;
        emit loginFailed(msg);
        return false;
    }

    //③ 比对密码
    if (m_validPasswords.at(index) != password) {
        const QString msg = "密码错误";
        qDebug() << "[C++] 登录失败:" << msg;
        emit loginFailed(msg);
        return false;
    }

    //④ 全部通过
    qDebug() << "[C++] 登录成功:" << account;
    emit loginSuccess(account); //发信号告诉 QML
    return true;
}
