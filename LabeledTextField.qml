/*
 * @brief 带标签的输入框// 负责显示一个标签和一个输入框// 包含标签文字、输入框文字、输入框值等功能。
 * 
*/
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// =============================================================================
// 【LabeledTextField.qml】自定义组件：带标签的输入框
//
//   抽这个组件的原因：登录页和录入页都有"标签 + 输入框"这种一行一组的控件，
//   写两遍就是复制粘贴，改一处样式要改两处 —— 抽成组件只改一次。
//
//   【对外暴露什么？】
//     property string labelText  —— 左边标签的文字（外部传入）
//     property string placeholder —— 输入框的提示文字（外部传入）
//     property alias text         —— 把内部输入框的值暴露出去（外部可读写）
//     property alias echoMode     —— 密码框靠它设置（外部可写）
//     property alias inputItem   —— 需要直接操作输入框时用（外部可读）
// =============================================================================

RowLayout {
    id: root
    // 定义向外暴露的属性，接收外部传过来的值
    property string labelText: ""
    property string placeholder: ""

    // 定义向外暴露的属性，将内部的值暴露出去
    property alias text: input.text
    property alias echoMode: input.echoMode
    // 外面就可以直接通过 inputItem 这个属性名拿到整个输入框
    property alias inputItem: input

    property int labelWidth: 100

    Layout.fillWidth: true

    Text {
        text: root.labelText
        Layout.preferredWidth: root.labelWidth
        font {
            family: "宋体"
            pointSize: 14
        }
    }

    TextField {
        id: input
        placeholderText: root.placeholder
        Layout.fillWidth: true
    }
}