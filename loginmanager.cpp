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

// 【注册新账号】
//   注意：账号表是成员变量，所以注册页必须复用【登录页那一个】LoginManager 实例，
//   否则注册进的是另一张表，登录时照样查不到。
bool LoginManager::registerAccount(const QString &account, const QString &password)
{
    qDebug() << "[C++] 收到注册请求: 账号 =" << account;

    //① 空值校验
    if (account.isEmpty() || password.isEmpty()) {
        const QString msg = "账号和密码不能为空";
        emit registerFailed(msg);
        return false;
    }

    //② 账号重复校验
    if (m_validAccounts.contains(account)) {
        const QString msg = "账号已存在，请换一个";
        emit registerFailed(msg);
        return false;
    }

    //③ 写入账号表
    m_validAccounts << account;
    m_validPasswords << password;
    qDebug() << "[C++] 注册成功:" << account;
    emit registerSuccess(account);
    return true;
}
