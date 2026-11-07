/*
 * @brief 注册页// 负责新账号注册// 包含账号、密码、确认密码输入与提交校验。
 *
*/
import QtQuick// 导入 QtQuick 模块用来显示注册页面
import QtQuick.Controls// 导入 QtQuick.Controls 模块用来显示注册页面的控件
import QtQuick.Layouts// 导入 QtQuick.Layouts 模块用来显示注册页面的布局

Item {
    id: registerPage

    // 与 Login.qml 同样的写法：附加属性只在页面根 Item 上有效，
    // 所以在这里读一次，子对象通过 registerPage.stackView 访问
    property var stackView: StackView.view// 注册页的 StackView 实例，用来切换页面

    // 由 Login.qml 通过 push("Register.qml", { manager: loginManager }) 传入
    // 必须复用登录页那个实例，否则账号表是两份
    property var manager: null// 注册页的 LoginManager 实例，用来调用注册方法

    // 【错误提示弹窗】
    Popup {
        id: errorPopup
        anchors.centerIn: Overlay.overlay// 相对【整个窗口】居中
        modal: true// 模态，挡住背后操作// 点击弹窗外部不能操作
        focus: true// 弹窗默认获得焦点
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside// 点击背景关闭弹窗

        width: 300
        padding: 20

        property string errorMessage: ""// 错误提示文字

        background: Rectangle {// 背景
            color: "#ffffff"// 颜色：白色
            radius: 8// 圆角半径：8px
            border.color: "#dddddd"
            border.width: 1// 边框宽度：1px
        }

        contentItem: ColumnLayout {// 内容布局
            spacing: 15// 间距：15px

            Text {// 错误提示文字
                text: errorPopup.errorMessage// 错误提示文字
                font { pointSize: 12 }
                color: "#333333"
                Layout.fillWidth: true// 填充宽度
                wrapMode: Text.WordWrap// 换行模式：按单词换行
            }

            Button {// 确定按钮
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
        if (pwdInput.text !== pwdConfirmInput.text) {// 检查两次密码是否一致
            errorPopup.errorMessage = "两次输入的密码不一致"
            errorPopup.open()
            return
        }

        if (!registerPage.manager) {// 检查是否拿到登录管理器
            errorPopup.errorMessage = "内部错误：未拿到登录管理器"
            errorPopup.open()
            return
        }

        registerPage.manager.registerAccount(accountInput.text, pwdInput.text)
    }

    function goBack() {// 返回登录页
        if (registerPage.stackView)
            registerPage.stackView.pop()
    }
}