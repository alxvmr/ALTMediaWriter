/*
 * Fedora Media Writer
 * Copyright (C) 2016 Martin Bříza <mbriza@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.1
import QtQuick.Window 2.0

import MediaWriter 1.0

import "../simple"
import "../complex"

Dialog {
    id: dialog
    title: qsTr("Write %1").arg(releases.selected.displayName)

    height: layout.height + 36
    standardButtons: StandardButton.NoButton

    width: 640

    function reset() {
        writeArrow.color = palette.text
    }

    onVisibleChanged: {
        if (!visible) {
            if (drives.selected)
                drives.selected.cancel()
            releases.variant.cancelDownload()
            releases.variant.resetStatus()
        }
        reset()
    }

    contentItem: Rectangle {
        id: dialogContainer
        anchors.fill: parent
        color: palette.window
        focus: true

        states: [
            State {
                name: "preparing"
                when: releases.variant.status === Variant.PREPARING
                PropertyChanges {
                    target: progressBar;
                    value: 0.0/0.0
                }
            },
            State {
                name: "downloading"
                when: releases.variant.status === Variant.DOWNLOADING
                PropertyChanges {
                    target: messageDownload
                    visible: true
                }
                PropertyChanges {
                    target: progressBar;
                    value: releases.variant.progress.ratio
                }
            },
            State {
                name: "resuming"
                when: releases.variant.status === Variant.DOWNLOAD_RESUMING
                PropertyChanges {
                    target: progressBar;
                    value: 0.0/0.0
                }
            },
            State {
                name: "download_verifying"
                when: releases.variant.status === Variant.DOWNLOAD_VERIFYING
                PropertyChanges {
                    target: messageDownload
                    visible: true
                }
                PropertyChanges {
                    target: progressBar;
                    value: releases.variant.progress.ratio;
                    progressColor: Qt.lighter("green")
                }
            },
            State {
                name: "ready_no_drives"
                when: releases.variant.status === Variant.READY && drives.length <= 0
            },
            State {
                name: "ready"
                when: releases.variant.status === Variant.READY && drives.length > 0
                PropertyChanges {
                    target: messageLoseData;
                    visible: true
                }
                PropertyChanges {
                    target: rightButton;
                    enabled: releases.variant.imageType.supportedForWriting;
                    color: "red";
                    onClicked: drives.selected.write(releases.variant)
                }
            },
            State {
                name: "writing_not_possible"
                when: releases.variant.status === Variant.WRITING_NOT_POSSIBLE
                PropertyChanges {
                    target: driveCombo;
                    enabled: false;
                    placeholderText: qsTr("Writing is not possible")
                }
            },
            State {
                name: "writing"
                when: releases.variant.status === Variant.WRITING
                PropertyChanges {
                    target: messageDriveSize
                    enabled: false
                }
                PropertyChanges {
                    target: messageRestore;
                    visible: true
                }
                PropertyChanges {
                    target: driveCombo;
                    enabled: false
                }
                PropertyChanges {
                    target: progressBar;
                    value: drives.selected.progress.ratio;
                    progressColor: "red"
                }
            },
            State {
                name: "write_verifying"
                when: releases.variant.status === Variant.WRITE_VERIFYING
                PropertyChanges {
                    target: messageDriveSize
                    enabled: false
                }
                PropertyChanges {
                    target: messageRestore;
                    visible: true
                }
                PropertyChanges {
                    target: driveCombo;
                    enabled: false
                }
                PropertyChanges {
                    target: progressBar;
                    value: drives.selected.progress.ratio;
                    progressColor: Qt.lighter("green")
                }
            },
            State {
                name: "finished"
                when: releases.variant.status === Variant.FINISHED
                PropertyChanges {
                    target: messageDriveSize
                    enabled: false
                }
                PropertyChanges {
                    target: messageRestore;
                    visible: true
                }
                PropertyChanges {
                    target: leftButton;
                    text: qsTr("Close");
                    color: "#628fcf";
                    textColor: "white"
                    onClicked: {
                        dialog.close()
                    }
                }
            },
            State {
                name: "failed_download"
                when: releases.variant.status === Variant.FAILED_DOWNLOAD
                PropertyChanges {
                    target: driveCombo;
                    enabled: false
                }
                PropertyChanges {
                    target: rightButton;
                    text: qsTr("Retry");
                    enabled: true;
                    color: "#628fcf";
                    onClicked: releases.variant.download()
                }
            },
            State {
                name: "failed_no_drives"
                when: releases.variant.status === Variant.FAILED && drives.length <= 0
                PropertyChanges {
                    target: rightButton;
                    text: qsTr("Retry");
                    enabled: false;
                    color: "red";
                    onClicked: drives.selected.write(releases.variant)
                }
            },
            State {
                name: "failed"
                when: releases.variant.status === Variant.FAILED && drives.length > 0
                PropertyChanges {
                    target: messageLoseData;
                    visible: true
                }
                PropertyChanges {
                    target: rightButton;
                    text: qsTr("Retry");
                    enabled: true;
                    color: "red";
                    onClicked: drives.selected.write(releases.variant)
                }
            }
        ]

        Keys.onEscapePressed: {
            if ([Variant.WRITING, Variant.WRITE_VERIFYING].indexOf(releases.variant.status) < 0)
                dialog.visible = false
        }

        ScrollView {
            id: contentScrollView
            anchors.fill: parent
            horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff
            flickableItem.flickableDirection: Flickable.VerticalFlick
            activeFocusOnTab: false

            contentItem: Item {
                width: contentScrollView.width - 18
                height: layout.height + 18
                ColumnLayout {
                    id: layout
                    spacing: 18
                    anchors {
                        top: parent.top
                        left: parent.left
                        right: parent.right
                        topMargin: 18
                        leftMargin: 18
                    }
                    ColumnLayout {
                        id: infoColumn
                        spacing: 4
                        Layout.fillWidth: true

                        InfoMessage {
                            id: messageDownload
                            visible: false
                            width: infoColumn.width
                            text: qsTr("The file will be saved to your Downloads folder.")
                        }

                        InfoMessage {
                            id: messageLoseData
                            visible: false
                            width: infoColumn.width
                            text: qsTr("By writing, you will lose all of the data on %1.").arg(driveCombo.currentText)
                        }

                        InfoMessage {
                            id: messageRestore
                            visible: false
                            width: infoColumn.width
                            text: qsTr("Your drive will be resized to a smaller capacity. You may resize it back to normal by using ALT Media Writer; this will remove installation media from your drive.")
                        }

                        InfoMessage {
                            id: messageSelectedImage
                            width: infoColumn.width
                            visible: releases.selected.isLocal
                            text: "<font color=\"gray\">" + qsTr("Selected:") + "</font> " + (releases.variant.image ? (((String)(releases.variant.image)).split("/").slice(-1)[0]) : ("<font color=\"gray\">" + qsTr("None") + "</font>"))
                        }

                        InfoMessage {
                            id: messageDriveSize
                            width: infoColumn.width
                            enabled: true
                            visible: enabled && drives.selected && drives.selected.size > 160 * 1024 * 1024 * 1024 // warn when it's more than 160GB
                            text: qsTr("The selected drive's size is %1. It's possible you have selected an external drive by accident!").arg(drives.selected ? drives.selected.readableSize : "N/A")
                        }

                        InfoMessage {
                            error: true
                            width: infoColumn.width
                            visible: releases.variant && releases.variant.errorString.length > 0
                            text: releases.variant ? releases.variant.errorString : ""
                        }
                    }

                    ColumnLayout {
                        width: parent.width
                        spacing: 5

                        Behavior on y {
                            NumberAnimation {
                                duration: 1000
                            }
                        }
                        
                        Text {
                            visible: true
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            horizontalAlignment: Text.AlignHCenter
                            font.pointSize: 9
                            property double leftSize: releases.variant.progress.leftSize
                            property string leftStr:  leftSize <= 0                    ? "" :
                                                     (leftSize < 1024)                 ? qsTr("(%1 B left)").arg(leftSize) :
                                                     (leftSize < (1024 * 1024))        ? qsTr("(%1 KB left)").arg((leftSize / 1024).toFixed(1)) :
                                                     (leftSize < (1024 * 1024 * 1024)) ? qsTr("(%1 MB left)").arg((leftSize / 1024 / 1024).toFixed(1)) :
                                                                                         qsTr("(%1 GB left)").arg((leftSize / 1024 / 1024 / 1024).toFixed(1))
                            text: releases.variant.statusString + (releases.variant.status == Variant.DOWNLOADING ? (" " + leftStr) : "")
                            color: palette.windowText
                        }
                        Item {
                            Layout.fillWidth: true
                            height: childrenRect.height
                            AdwaitaProgressBar {
                                id: progressBar
                                width: parent.width
                                progressColor: "#54aada"
                                value: 0.0
                            }
                        }
                        AdwaitaCheckBox {
                            text: qsTr("Write the image after downloading")
                            enabled: drives.selected && ((releases.variant.status == Variant.DOWNLOADING) || (releases.variant.status == Variant.DOWNLOAD_RESUMING))
                            visible: enabled

                            onCheckedChanged: {
                                releases.variant.delayedWrite = checked
                            }
                        }
                    }

                    RowLayout {
                        Layout.alignment : Qt.AlignHCenter
                        spacing: 32
                        Image {
                            source: releases.selected.icon
                            Layout.preferredWidth: 64
                            Layout.preferredHeight: 64
                            sourceSize.width: 64
                            sourceSize.height: 64
                            fillMode: Image.PreserveAspectFit
                        }
                        Arrow {
                            id: writeArrow
                            Layout.alignment : Qt.AlignVCenter
                            scale: 1.4
                            SequentialAnimation {
                                running: releases.variant.status == Variant.WRITING
                                loops: -1
                                onStopped: {
                                    if (releases.variant.status == Variant.FINISHED)
                                        writeArrow.color = "#00dd00"
                                    else
                                        writeArrow.color = palette.text
                                }
                                ColorAnimation {
                                    duration: 3500
                                    target: writeArrow
                                    property: "color"
                                    to: "red"
                                }
                                PauseAnimation {
                                    duration: 500
                                }
                                ColorAnimation {
                                    duration: 3500
                                    target: writeArrow
                                    property: "color"
                                    to: palette.text
                                }
                                PauseAnimation {
                                    duration: 500
                                }
                            }
                        }
                        AdwaitaComboBox {
                            id: driveCombo
                            Layout.preferredWidth: implicitWidth * 2.5
                            z: pressed ? 1 : 0
                            model: drives
                            textRole: "display"
                            Binding {
                                target: drives
                                property: "selectedIndex"
                                value: driveCombo.currentIndex
                            }
                            onActivated: {
                                if ([Variant.FINISHED, Variant.FAILED].indexOf(releases.variant.status) >= 0)
                                    releases.variant.resetStatus()
                            }
                            placeholderText: qsTr("There are no portable drives connected")
                        }
                    }

                    ColumnLayout {
                        z: -1
                        Layout.maximumWidth: parent.width
                        spacing: 3
                        Item {
                            height: 3
                            width: 1
                        }
                        Text {
                            // Show this if image type is not supported and can't be rootfs'ed
                            visible: releases.variant && !releases.variant.imageType.supportedForWriting && !releases.variant.imageType.canWriteWithRootfs
                            font.pointSize: 10
                            Layout.fillWidth: true
                            width: Layout.width
                            wrapMode: Text.WordWrap
                            text: qsTr("Writing this image type is not supported.")
                            color: "red"
                        }
                        Text {
                            // Show this if image type is not supported BUT can be rootfs'ed
                            visible: releases.variant && !releases.variant.imageType.supportedForWriting && releases.variant.imageType.canWriteWithRootfs
                            font.pointSize: 10
                            Layout.fillWidth: true
                            width: Layout.width
                            wrapMode: Text.WordWrap
                            text: drives.selected ? qsTr("Writing this image type is not supported here but you can write it using rootfs:\nsudo alt-rootfs-installer --rootfs=%1 --media=%2 --target=%3").arg(releases.variant.image).arg(drives.selected.devicePath).arg(releases.variant.board) : qsTr("Writing this image type is not supported here but you can write it using rootfs. Insert a drive to get a formatted command.")
                            color: "black"
                        }
                        RowLayout {
                            height: rightButton.height
                            Layout.minimumWidth: parent.width
                            Layout.maximumWidth: parent.width
                            spacing: 6

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                            }

                            AdwaitaButton {
                                id: leftButton
                                Layout.alignment: Qt.AlignRight
                                Behavior on implicitWidth { NumberAnimation { duration: 80 } }
                                text: qsTr("Cancel")
                                enabled: true
                                onClicked: {
                                    dialog.close()
                                }
                            }
                            AdwaitaButton {
                                id: rightButton
                                Layout.alignment: Qt.AlignRight
                                Behavior on implicitWidth { NumberAnimation { duration: 80 } }
                                textColor: enabled ? "white" : palette.text
                                text: qsTr("Write to Disk")
                                enabled: false
                            }
                        }
                    }
                }
            }
        }
    }
}
