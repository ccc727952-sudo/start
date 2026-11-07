/*
 * @brief 主窗口// 负责显示应用的主界面// 包含登录页、人脸录入页等功能。
 * 
*/
// Main.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

ApplicationWindow {
    id: window
    width: 800
    height: 600
    minimumWidth: 200
    minimumHeight: 250
    visible: true
    title: qsTr("人脸录入系统")

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: Login {}      // ← 初始页（栈底）
    }
}