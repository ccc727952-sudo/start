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