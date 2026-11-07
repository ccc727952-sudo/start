/*
 * @brief 注册页// 负责新账号注册// 包含账号、密码、确认密码输入与提交校验。
 *
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: registerPage

    // 与 Login.qml 同样的写法：附加属性只在页面根 Item 上有效，
    // 所以在这里读一次，子对象通过 registerPage.stackView 访问
    property var stackView: StackView.view

    // 由 Login.qml 通过 push("Register.qml", { manager: loginManager }) 传入
    // 必须复用登录页那个实例，否则账号表是两份
    property var manager: null

    // 【错误提示弹窗】
    Popup {
        id: errorPopup
        anchors.centerIn: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        width: 300
        padding: 20

        property string errorMessage: ""

        background: Rectangle {
            color: "#ffffff"
            radius: 8
            border.color: "#dddddd"
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 15

            Text {
                text: errorPopup.errorMessage
                font { pointSize: 12 }
                color: "#333333"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            Button {
                text: "确定"
                Layout.alignment: Qt.AlignRight
                onClicked: errorPopup.close()
            }
        }
    }

    // 注册表单
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 380)
        spacing: 20

        //标题
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("注册新账号")
                font { pointSize: 20; bold: true }
            }
        }

        LabeledTextField {
            id: accountInput
            labelText: "账号:"
            placeholder: "请输入账号"
        }

        LabeledTextField {
            id: pwdInput
            labelText: "密码:"
            placeholder: "请输入密码"
            echoMode: TextInput.Password
        }

        LabeledTextField {
            id: pwdConfirmInput
            labelText: "确认密码:"
            placeholder: "请再次输入密码"
            echoMode: TextInput.Password
        }

        //按钮行
        RowLayout {
            Layout.fillWidth: true

            Button {
                text: "注册"
                font.pointSize: 14
                onClicked: registerPage.doRegister()
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "返回"
                font.pointSize: 14
                onClicked: registerPage.goBack()
            }
        }
    }

    // 接收 C++ 的注册结果
    Connections {
        target: registerPage.manager

        function onRegisterSuccess(account) {
            // 注册成功直接退回登录页，账号表已经写进同一个 LoginManager 实例
            registerPage.goBack()
        }

        function onRegisterFailed(msg) {
            errorPopup.errorMessage = msg
            errorPopup.open()
        }
    }

    // 提交前的本地校验（两次密码一致这类检查没必要跑一趟 C++）
    function doRegister() {
        if (pwdInput.text !== pwdConfirmInput.text) {
            errorPopup.errorMessage = "两次输入的密码不一致"
            errorPopup.open()
            return
        }

        if (!registerPage.manager) {
            errorPopup.errorMessage = "内部错误：未拿到登录管理器"
            errorPopup.open()
            return
        }

        registerPage.manager.registerAccount(accountInput.text, pwdInput.text)
    }

    function goBack() {
        if (registerPage.stackView)
            registerPage.stackView.pop()
    }
}