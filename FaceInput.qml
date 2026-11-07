/*
 * @brief 人脸录入页// 负责显示人脸录入界面// 包含采集线程、处理线程等功能。
 * 
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia  //QML多媒体
import QtQuick.Dialogs
import cn.rh

//人脸录入界面
Item {
    id:faceInputPage
    //定义一个属性，接收当前摄像头是否开始
    property bool isOpen: false

    //摄像头编号：0=第一个摄像头，打不开就试试 1 2 (多摄像头)
    property int cameraIndex: 0

    //创建人脸采集对象
    FaceInputManager{
        id:faceInputManager
    }
    //模态框
    MessageDialog{
        id:errorDialog
        title:"错误"
        buttons: MessageDialog.Ok
    }
        //接收C++发来的活体引导提示
    Connections{
        target: faceInputManager
        function onLivenessPrompt(text){
            hintText.text = text
        }
    }
    //垂直布局
    ColumnLayout{
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        //上半部分:左边是矩形  右边是表单
        RowLayout{
            //撑满ColumnLayout 分配的全部宽高
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20
            //左边：展示摄像头实现捕获到的信息
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width*0.5
                color: "#000000"
                radius: 8
                border.color: "#cccccc"
                border.width: 1
                VideoOutput{
                    id:previewOutput
                    anchors.fill: parent
                    anchors.margins: 2
                    // 保持宽高比，完全填充区域，超出部分被裁剪。不会留下空白区域。
                    fillMode: VideoOutput.PreserveAspectCrop
                    //画面修正，水平镜像(摄像头画面是反的)
                    transform: Scale{xScale: -1;origin.x:previewOutput.width/2}
                }
                // 人脸引导框：固定在画面中央
                Rectangle {
                    id: faceGuide
                    width: parent.width * 0.4
                    height: width * 1.3  // 人脸大致比例
                    anchors.centerIn: parent
                    color: "transparent"
                    border.color: "#00FF00"
                    border.width: 2
                    radius: 8
                }
                                //活体检测引导提示条
                Rectangle {
                    id: hintBar
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 12
                    width: hintText.implicitWidth + 40
                    height: 42
                    radius: 21
                    color: "#B3000000"        // 半透明黑底
                    border.color: "#00FF00"
                    border.width: 1
                    visible: hintText.text.length > 0

                    Text {
                        id: hintText
                        anchors.centerIn: parent
                        text: ""
                        color: "#00FF00"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
                Text{
                    anchors.centerIn: parent
                    text:"点击【开始采集】预览画面"
                    color: "#888888"
                    font.pixelSize: 14
                    visible: !isOpen
                }
                //模块加载载完毕后，就要默认操作
                Component.onCompleted: {
                    //把显示目标交给C++  c++采集到帧后才知道往哪里贴
                    faceInputManager.displaySink=previewOutput.videoSink
                    //调用C++类的函数打开摄像头
                    faceInputManager.openCamera(faceInputPage.cameraIndex)
                    //摄像头打开
                    isOpen=true
                }
            }
            //右边 表单：填写住户基本信息
            Rectangle{
                Layout.fillHeight: true
                Layout.fillWidth: true
                color: "#f7f7f7"
                radius: 8
                border.color: "#cccccc"
                border.width: 1
                // 表单是一个垂直布局
                ColumnLayout {
                   anchors.fill: parent
                   anchors.margins: 20
                   spacing: 16
                   Text{
                       text:qsTr("人员信息")
                       font.pointSize: 18
                       font.bold: true
                       Layout.alignment: Qt.AlignHCenter
                   }
                   LabeledTextField {
                       id: idRow
                       labelText: "住户编号:"
                   }
                   LabeledTextField {
                       id: nameRow
                       labelText: "住户姓名:"
                   }
                   LabeledTextField {
                       id: phoneRow
                       labelText: "住户电话:"
                   }
                   LabeledTextField {
                       id: addressRow
                       labelText: "住户地址:"
                   }
                }
            }
        }
        //下半部分 三个按钮
        RowLayout{
            Layout.fillWidth: true
            spacing: 20
            Layout.alignment: Qt.AlignCenter
            Button{
                id: startBtn
                text: isOpen?"暂停采集":"开始采集"
                Layout.preferredWidth: 140
                Layout.preferredHeight: 44
                font.pointSize: 15
                //点击事件
                onClicked:{
                    if(isOpen){
                        //当前摄像头是打开的，
                        faceInputManager.closeCamera(0)  //0暂停  1关闭
                        isOpen=false
                    }else{
                        faceInputManager.openCamera(faceInputPage.cameraIndex);
                        isOpen=true
                    }
                }
            }
            Button{
                id: submitBtn
                text: "提交"
                Layout.preferredWidth: 140
                Layout.preferredHeight: 44
                font.pointSize: 15
                //点击事件
                onClicked:{
                    console.log("提交",idRow.text,nameRow.text)
                }
            }
            Button{
                id: backBtn
                text: "返回"
                Layout.preferredWidth: 140
                Layout.preferredHeight: 44
                font.pointSize: 15
                //点击事件
                onClicked:{
                    // 从 StackView 中弹出当前页，回到登录页
                    // ★ 用 StackView.view（挂在页面根节点上的附加属性），
                    //   因为栈的 id 在 Main.qml 里，本文件访问不到
                    faceInputPage.StackView.view.pop()
                    faceInputManager.closeCamera(1)  //0暂停  1关闭
                    isOpen=false
                }
            }
        }
    }
    Connections{
        target: faceInputManager
        function onErrorOccurred(message){
            errorDialog.text=message
            //打开模态框，模态默认关闭
            errorDialog.open()
        }
    }
}
