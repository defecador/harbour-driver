import QtQuick 2.6
import "../theme"
import "../components"

Item {
    id: root
    signal closed()
    signal signDataRequested()

    property string keyStatus: ""
    property bool keyOk: false
    property string voiceError: ""

    Connections {
        target: SearchController
        onKeyTested: {
            root.keyStatus = message
            root.keyOk = ok
        }
    }

    Connections {
        target: VoiceClips
        onError: root.voiceError = message
        onRecordingChanged: if (VoiceClips.recording.length > 0) root.voiceError = ""
    }

    Component.onDestruction: VoiceClips.stopRecording()

    Rectangle { anchors.fill: parent; color: M.bg }

    Column {
        anchors.fill: parent

        Item { width: 1; height: M.topInset }

        Item {
            width: parent.width
            height: 56 * M.dp
            RoundIconButton {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: M.spacingM
                iconName: "arrow_back"
                elevation: 0
                onClicked: root.closed()
            }
            MText {
                anchors.centerIn: parent
                variant: "subtitle"
                text: qsTr("Settings")
            }
        }
        Divider { width: parent.width }

        Flickable {
            id: flick
            width: parent.width
            height: root.height - y
            clip: true
            contentWidth: width
            contentHeight: content.height + M.spacingM * 2
            boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            width: flick.width - M.spacingM * 2
            x: M.spacingM
            y: M.spacingM
            spacing: M.spacingM

            TextFieldM {
                width: parent.width
                label: qsTr("HERE API key")
                placeholder: qsTr("Paste your key")
                text: AppSettings.hereApiKey
                onTextChanged: AppSettings.hereApiKey = text
            }

            Row {
                spacing: M.spacingS
                PrimaryButton {
                    text: qsTr("Test key")
                    iconName: "vpn_key"
                    onClicked: SearchController.testKey()
                }
            }

            MText {
                visible: root.keyStatus.length > 0
                width: parent.width
                wrapMode: Text.WordWrap
                color: root.keyOk ? M.good : M.bad
                text: root.keyStatus
            }

            Divider { width: parent.width }

            Item {
                width: parent.width
                height: 56 * M.dp
                MText {
                    anchors.left: parent.left
                    anchors.right: view3dSwitch.left
                    anchors.rightMargin: M.spacingS
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("3D view while driving (tilted map with 3D buildings)")
                }
                SwitchM {
                    id: view3dSwitch
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: AppSettings.view3d
                    onToggled: AppSettings.view3d = checked
                }
            }

            Item {
                width: parent.width
                height: 56 * M.dp
                MText {
                    anchors.left: parent.left
                    anchors.right: vibrationSwitch.left
                    anchors.rightMargin: M.spacingS
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Vibrate on navigation prompts")
                }
                SwitchM {
                    id: vibrationSwitch
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: AppSettings.vibration
                    onToggled: AppSettings.vibration = checked
                }
            }

            Divider { width: parent.width }

            MText { variant: "caption"; color: M.inkMuted; text: qsTr("Warnings while driving") }

            Item {
                width: parent.width
                height: 56 * M.dp
                MText {
                    anchors.left: parent.left
                    anchors.right: limitSwitch.left
                    anchors.rightMargin: M.spacingS
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Show the speed limit of the road")
                }
                SwitchM {
                    id: limitSwitch
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: AppSettings.speedLimitSign
                    onToggled: AppSettings.speedLimitSign = checked
                }
            }

            Item {
                width: parent.width
                height: 56 * M.dp
                MText {
                    anchors.left: parent.left
                    anchors.right: signsSwitch.left
                    anchors.rightMargin: M.spacingS
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Warn about stop and give-way signs ahead")
                }
                SwitchM {
                    id: signsSwitch
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: AppSettings.warnSigns
                    onToggled: AppSettings.warnSigns = checked
                }
            }

            Item {
                width: parent.width
                height: 56 * M.dp
                MText {
                    anchors.left: parent.left
                    anchors.right: camerasSwitch.left
                    anchors.rightMargin: M.spacingS
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Speed camera alerts")
                }
                SwitchM {
                    id: camerasSwitch
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: AppSettings.speedCameras
                    onToggled: AppSettings.speedCameras = checked
                }
            }

            MText {
                width: parent.width
                variant: "caption"
                color: M.inkMuted
                wrapMode: Text.WordWrap
                text: qsTr("Warning about speed cameras while driving is banned in some countries, for example Germany and Switzerland. You are responsible for using this lawfully. Warnings come from OpenStreetMap data and can be incomplete.")
            }

            ListRow {
                iconName: "download"
                title: qsTr("Sign data by country")
                subtitle: SignPacks.installedCount > 0
                          ? qsTr("%n country downloaded", "", SignPacks.installedCount)
                          : qsTr("Download the countries you drive in")
                showChevron: true
                showDivider: false
                onClicked: root.signDataRequested()
            }

            Column {
                width: parent.width
                visible: VoiceClips.available
                spacing: M.spacingS

                MText {
                    width: parent.width
                    variant: "caption"
                    color: M.inkMuted
                    wrapMode: Text.WordWrap
                    text: qsTr("Your voice: record a short phrase for each warning. It plays when the sign is ahead, and music is turned down meanwhile.")
                }

                Repeater {
                    model: [
                        {"kind": "stop", "label": qsTr("Stop sign")},
                        {"kind": "giveWay", "label": qsTr("Give way")},
                        {"kind": "camera", "label": qsTr("Speed camera")}
                    ]
                    delegate: Item {
                        id: voiceRow
                        width: content.width
                        height: 56 * M.dp
                        readonly property bool has: VoiceClips.revision >= 0 && VoiceClips.has(modelData.kind)
                        readonly property bool recording: VoiceClips.recording === modelData.kind

                        Column {
                            anchors.left: parent.left
                            anchors.right: buttons.left
                            anchors.verticalCenter: parent.verticalCenter
                            MText { text: modelData.label; variant: "subtitle" }
                            MText {
                                variant: "caption"
                                color: voiceRow.recording ? M.bad : M.inkMuted
                                text: voiceRow.recording ? qsTr("Recording... tap to stop")
                                      : voiceRow.has ? qsTr("Recorded") : qsTr("Not recorded")
                            }
                        }
                        Row {
                            id: buttons
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: M.spacingS
                            RoundIconButton {
                                elevation: 0
                                iconName: voiceRow.recording ? "stop" : "mic"
                                iconColor: voiceRow.recording ? M.bad : M.ink
                                onClicked: {
                                    if (voiceRow.recording)
                                        VoiceClips.stopRecording()
                                    else
                                        VoiceClips.startRecording(modelData.kind)
                                }
                            }
                            RoundIconButton {
                                elevation: 0
                                enabled: voiceRow.has && !voiceRow.recording
                                opacity: enabled ? 1 : 0.35
                                iconName: "play_arrow"
                                onClicked: VoiceClips.play(modelData.kind)
                            }
                            RoundIconButton {
                                elevation: 0
                                enabled: voiceRow.has && !voiceRow.recording
                                opacity: enabled ? 1 : 0.35
                                iconName: "delete"
                                onClicked: VoiceClips.remove(modelData.kind)
                            }
                        }
                    }
                }

                MText {
                    visible: root.voiceError.length > 0
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: M.bad
                    text: root.voiceError
                }
            }

            Divider { width: parent.width }

            MText { variant: "caption"; color: M.inkMuted; text: qsTr("Look") }
            Row {
                spacing: M.spacingS
                Chip {
                    label: qsTr("New")
                    filled: AppSettings.uiStyle !== "classic"
                    onClicked: AppSettings.uiStyle = "new"
                }
                Chip {
                    label: qsTr("Classic")
                    filled: AppSettings.uiStyle === "classic"
                    onClicked: AppSettings.uiStyle = "classic"
                }
            }

            MText { variant: "caption"; color: M.inkMuted; text: qsTr("Day or night") }
            Row {
                spacing: M.spacingS
                Chip {
                    iconName: "brightness_auto"
                    label: qsTr("System")
                    filled: AppSettings.theme === "auto"
                    onClicked: AppSettings.theme = "auto"
                }
                Chip {
                    iconName: "light_mode"
                    label: qsTr("Day")
                    filled: AppSettings.theme === "light"
                    onClicked: AppSettings.theme = "light"
                }
                Chip {
                    iconName: "dark_mode"
                    label: qsTr("Night")
                    filled: AppSettings.theme === "dark"
                    onClicked: AppSettings.theme = "dark"
                }
            }

            MText { variant: "caption"; color: M.inkMuted; text: qsTr("Units") }
            Row {
                spacing: M.spacingS
                Chip {
                    label: qsTr("Metric")
                    filled: AppSettings.units !== "imperial"
                    onClicked: AppSettings.units = "metric"
                }
                Chip {
                    label: qsTr("Imperial")
                    filled: AppSettings.units === "imperial"
                    onClicked: AppSettings.units = "imperial"
                }
            }
        }
        }
    }
}
