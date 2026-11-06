import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cn.rh //import 的是上面注册的模块 URI
import QtQuick.Dialogs // MessageDialog 在这个模块里

Item {
    id: loginPage
    LoginManager {
        id: loginManager // id 就相当于这个实例化对象的对象名
      }
      // 【把 StackView 存到根部的 property 上】
      //为什么不能直接写 StackView.view.push(...)？
      // StackView.view 这个【附加属性】只在【页面根 Item】上有值，
      // 在它的子对象（比如下面的 Connections）里读到的是 null ——
      // 运行时还会报一句 "StackView must be attached to an Item"。
      // 正确姿势：在根上取一次，子对象通过 id 访问
      property var stackView: StackView.view
      // 显示 C++ 返回的结果
      property string resultText: ""
    // 【错误提示弹窗】
      //   用 Popup 而不是直接改界面，交互更规范
      Popup {
          id: errorPopup
          anchors.centerIn: Overlay.overlay   // 相对【整个窗口】居中
          modal: true    // 模态，挡住背后操作
          focus: true
          //ESC或者点击背景退出
          closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

          width: 300
          padding: 20

          // 给 Popup 加一个自定义属性，用来显示"到底是哪错了"
          //   QML 允许给内置类型加属性
          property string errorMessage: "账号或密码错误，请重新输入！"

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
      //MessageDialog 的用法（和 Popup 两种提示方式）
      MessageDialog {
          id: infoDialog
          title: "提示"
          text: "注册功能还在开发中"
          buttons: MessageDialog.Ok
      }
    // 登录表单
    // 垂直布局：管理整个登录表单的纵向排列
    ColumnLayout {
        anchors.centerIn: parent
        // 宽度自适应：两边留白 40，最大宽度不超过 380
        width: Math.min(parent.width - 40, 380)
        //间距为 20
        spacing: 20

        //标题
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("欢迎登录xxxx管理系统")
                // 设置字体大小为 20，并加粗
                font { pointSize: 20; bold: true }
            }
        }

        // //账号输入行
        // RowLayout {
        //     Layout.fillWidth: true
        //     Text { text: "账号:"; font { pointSize: 14 } }
        //     // 输入框为空时的提示文字
        //     TextField { id: accountInput; placeholderText: "请输入账号" }
        // }

        // //密码输入行
        // RowLayout {
        //     Layout.fillWidth: true
        //     Text { text: "密码:"; font { pointSize: 14 } }
        //     TextField {
        //         id: pwdInput
        //         placeholderText: "请输入密码"
        //         echoMode: TextInput.Password //密码要打码
        //     }
        // }
        LabeledTextField{
            id: accountInput
            objectName: "accountInput"     // ★ C++ 侧靠 objectName 找控件
            labelText: "账号:"
            placeholder: "请输入账号"
			text: "admin"
        }

        LabeledTextField{
            id: pwdInput
            objectName: "pwdInput"         // ★ C++ 侧靠 objectName 找控件
            labelText: "密码:"
            placeholder: "请输入密码"
            echoMode: TextInput.Password
			text: "123"
        }

        //按钮行
        RowLayout {
            Layout.fillWidth: true
            Button {
              id: btnLogin
              text: "登录"
              font.pointSize: 14
              onClicked:{
                  //账号密码交给 C++ 去校验
                  loginManager.login(accountInput.text, pwdInput.text)
              }
            }
			// 这是一个不可见的 Item，作为弹簧（Spacer）
            // 作用：把所有按钮推向左侧，如果放在两个按钮中间，可以把按钮推向两边
            Item { Layout.fillWidth: true }  //占位撑杆
            Button {
              id: btnRegister
              text: "注册"
              font.pointSize: 14
              //注册模态框
              onClicked: infoDialog.open()
            }
            
        }
        // 显示 C++ 发回来的结果
        Text {
            Layout.fillWidth: true
            text: loginPage.resultText
            color: "#c0392b"
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }
    }
    // 接收 C++ 的登录结果 → 弹框 / 跳转
        Connections {
            target: loginManager

            function onLoginSuccess(account) {
                // ★ 压入首页，并把账号一起传过去
                //   注意用的是 loginPage.stackView，不是 StackView.view
                loginPage.stackView.push("FaceInput.qml")
            }

            function onLoginFailed(msg) {
                errorPopup.errorMessage = msg
                errorPopup.open()
            }
        }
}