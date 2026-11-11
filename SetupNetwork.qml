import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: setupNetworkPage
    property var stackView: StackView.view

    // 结果提示弹窗
    MessageDialog {
        id: resultDialog
        title: "提示"
        buttons: MessageDialog.Ok
        onAccepted: setupNetworkPage.goBack()
    }

    // 表单主体
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 420)
        spacing: 20

        Text {
            text: qsTr("网络配置")
            font { pointSize: 20; bold: true }
            Layout.alignment: Qt.AlignHCenter
        }

        LabeledTextField {
            id: wifiNameRow
            labelText: "WiFi名称(SSID):"
            placeholder: "请输入要连接的WiFi热点名称"
        }
        LabeledTextField {
            id: wifiPasswordRow
            labelText: "WiFi密码:"
            placeholder: "请输入WiFi密码"
            echoMode: TextInput.Password
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#dddddd"
        }

        LabeledTextField {
            id: netNameRow
            labelText: "设备网络名称:"
            placeholder: "可选，留空则不开启AP模式"
        }
        LabeledTextField {
            id: netPasswordRow
            labelText: "设备网络密码:"
            placeholder: "可选，留空则不开启AP模式"
            echoMode: TextInput.Password
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: "确定"
                font.pointSize: 14
                onClicked: setupNetworkPage.doSave()
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "取消"
                font.pointSize: 14
                onClicked: setupNetworkPage.goBack()
            }
        }
    }

    function doSave() {
        if (wifiNameRow.text.trim().length === 0) {
            resultDialog.title = "错误"
            resultDialog.text = "WiFi名称不能为空"
            resultDialog.open()
            return
        }
        if (wifiPasswordRow.text.length === 0) {
            resultDialog.title = "错误"
            resultDialog.text = "WiFi密码不能为空"
            resultDialog.open()
            return
        }

        var ok = networkConfig.saveConfig(
            wifiNameRow.text.trim(),
            wifiPasswordRow.text,
            netNameRow.text.trim(),
            netPasswordRow.text
        )

        if (ok) {
            resultDialog.title = "成功"
            resultDialog.text = "网络配置已保存到：\n" + networkConfig.configFilePath()
            resultDialog.open()
        } else {
            resultDialog.title = "失败"
            resultDialog.text = "保存失败，请检查设备权限或磁盘空间"
            resultDialog.open()
        }
    }

    function goBack() {
        if (setupNetworkPage.stackView)
            setupNetworkPage.stackView.pop()
    }

    Component.onCompleted: {
        var cfg = networkConfig.loadConfig()
        wifiNameRow.text        = cfg.wifiName       || ""
        wifiPasswordRow.text    = cfg.wifiPassword   || ""
        netNameRow.text         = cfg.networkName    || ""
        netPasswordRow.text     = cfg.networkPassword || ""
    }
}