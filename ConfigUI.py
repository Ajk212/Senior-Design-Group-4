# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'Prototype_SettingsnnERmR.ui'
##
## Created by: Qt User Interface Compiler version 5.15.2
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PyQt5.QtCore import *  
from PyQt5.QtGui import *  
from PyQt5.QtWidgets import *  


class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        if not MainWindow.objectName():
            MainWindow.setObjectName(u"MainWindow")
        MainWindow.resize(681, 563)
        sizePolicy = QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(MainWindow.sizePolicy().hasHeightForWidth())
        MainWindow.setSizePolicy(sizePolicy)
        MainWindow.setTabShape(QTabWidget.Rounded)
        self.centralwidget = QWidget(MainWindow)
        self.centralwidget.setObjectName(u"centralwidget")
        self.verticalLayout_2 = QVBoxLayout(self.centralwidget)
        self.verticalLayout_2.setObjectName(u"verticalLayout_2")
        self.widget = QWidget(self.centralwidget)
        self.widget.setObjectName(u"widget")
        self.horizontalLayout_3 = QHBoxLayout(self.widget)
        self.horizontalLayout_3.setObjectName(u"horizontalLayout_3")
        self.horizontalLayout_2 = QHBoxLayout()
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.label_3 = QLabel(self.widget)
        self.label_3.setObjectName(u"label_3")

        self.horizontalLayout_2.addWidget(self.label_3)

        self.label = QLabel(self.widget)
        self.label.setObjectName(u"label")

        self.horizontalLayout_2.addWidget(self.label)

        self.horizontalSpacer = QSpacerItem(40, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)

        self.horizontalLayout_2.addItem(self.horizontalSpacer)

        self.pushButton = QPushButton(self.widget)
        self.pushButton.setObjectName(u"pushButton")

        self.horizontalLayout_2.addWidget(self.pushButton)


        self.horizontalLayout_3.addLayout(self.horizontalLayout_2)


        self.verticalLayout_2.addWidget(self.widget)

        self.label_7 = QLabel(self.centralwidget)
        self.label_7.setObjectName(u"label_7")

        self.verticalLayout_2.addWidget(self.label_7)

        self.line = QFrame(self.centralwidget)
        self.line.setObjectName(u"line")
        self.line.setFrameShadow(QFrame.Plain)
        self.line.setLineWidth(3)
        self.line.setFrameShape(QFrame.HLine)

        self.verticalLayout_2.addWidget(self.line)

        self.scrollArea = QScrollArea(self.centralwidget)
        self.scrollArea.setObjectName(u"scrollArea")
        sizePolicy1 = QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        sizePolicy1.setHorizontalStretch(0)
        sizePolicy1.setVerticalStretch(0)
        sizePolicy1.setHeightForWidth(self.scrollArea.sizePolicy().hasHeightForWidth())
        self.scrollArea.setSizePolicy(sizePolicy1)
        self.scrollArea.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOn)
        self.scrollArea.setSizeAdjustPolicy(QAbstractScrollArea.AdjustIgnored)
        self.scrollArea.setWidgetResizable(True)
        self.scrollArea.setAlignment(Qt.AlignLeading|Qt.AlignLeft|Qt.AlignTop)
        self.scrollAreaWidgetContents = QWidget()
        self.scrollAreaWidgetContents.setObjectName(u"scrollAreaWidgetContents")
        self.scrollAreaWidgetContents.setGeometry(QRect(0, 0, 644, 462))
        sizePolicy.setHeightForWidth(self.scrollAreaWidgetContents.sizePolicy().hasHeightForWidth())
        self.scrollAreaWidgetContents.setSizePolicy(sizePolicy)
        self.gridLayout_3 = QGridLayout(self.scrollAreaWidgetContents)
        self.gridLayout_3.setObjectName(u"gridLayout_3")
        self.gridLayout_3.setHorizontalSpacing(0)
        self.gridLayout_3.setVerticalSpacing(10)
        self.gridLayout_3.setContentsMargins(0, 0, 0, 0)
        self.openHand_swipeLeft_combo = QComboBox(self.scrollAreaWidgetContents)
        self.openHand_swipeLeft_combo.addItem("")
        self.openHand_swipeLeft_combo.setObjectName(u"openHand_swipeLeft_combo")

        self.gridLayout_3.addWidget(self.openHand_swipeLeft_combo, 11, 1, 1, 1)

        self.line_12 = QFrame(self.scrollAreaWidgetContents)
        self.line_12.setObjectName(u"line_12")
        self.line_12.setFrameShadow(QFrame.Plain)
        self.line_12.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_12, 12, 0, 1, 1)

        self.label_14 = QLabel(self.scrollAreaWidgetContents)
        self.label_14.setObjectName(u"label_14")
        sizePolicy2 = QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        sizePolicy2.setHorizontalStretch(0)
        sizePolicy2.setVerticalStretch(0)
        sizePolicy2.setHeightForWidth(self.label_14.sizePolicy().hasHeightForWidth())
        self.label_14.setSizePolicy(sizePolicy2)
        self.label_14.setFrameShape(QFrame.Box)
        self.label_14.setTextFormat(Qt.AutoText)
        self.label_14.setAlignment(Qt.AlignCenter)

        self.gridLayout_3.addWidget(self.label_14, 0, 0, 1, 1)

        self.openHand_swipeRight_combo = QComboBox(self.scrollAreaWidgetContents)
        self.openHand_swipeRight_combo.addItem("")
        self.openHand_swipeRight_combo.setObjectName(u"openHand_swipeRight_combo")

        self.gridLayout_3.addWidget(self.openHand_swipeRight_combo, 9, 1, 1, 1)

        self.openHand_swipeDown_combo = QComboBox(self.scrollAreaWidgetContents)
        self.openHand_swipeDown_combo.addItem("")
        self.openHand_swipeDown_combo.setObjectName(u"openHand_swipeDown_combo")

        self.gridLayout_3.addWidget(self.openHand_swipeDown_combo, 5, 1, 1, 1)

        self.line_4 = QFrame(self.scrollAreaWidgetContents)
        self.line_4.setObjectName(u"line_4")
        self.line_4.setFrameShadow(QFrame.Plain)
        self.line_4.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_4, 4, 0, 1, 1)

        self.doubleTap_thumb_combo = QComboBox(self.scrollAreaWidgetContents)
        self.doubleTap_thumb_combo.addItem("")
        self.doubleTap_thumb_combo.setObjectName(u"doubleTap_thumb_combo")

        self.gridLayout_3.addWidget(self.doubleTap_thumb_combo, 3, 1, 1, 1)

        self.thumbsDown_combo = QComboBox(self.scrollAreaWidgetContents)
        self.thumbsDown_combo.addItem("")
        self.thumbsDown_combo.setObjectName(u"thumbsDown_combo")

        self.gridLayout_3.addWidget(self.thumbsDown_combo, 19, 1, 1, 1)

        self.tap_thumb_combo = QComboBox(self.scrollAreaWidgetContents)
        self.tap_thumb_combo.addItem("")
        self.tap_thumb_combo.setObjectName(u"tap_thumb_combo")

        self.gridLayout_3.addWidget(self.tap_thumb_combo, 1, 1, 1, 1)

        self.line_3 = QFrame(self.scrollAreaWidgetContents)
        self.line_3.setObjectName(u"line_3")
        self.line_3.setFrameShadow(QFrame.Plain)
        self.line_3.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_3, 2, 1, 1, 1)

        self.line_9 = QFrame(self.scrollAreaWidgetContents)
        self.line_9.setObjectName(u"line_9")
        self.line_9.setFrameShadow(QFrame.Plain)
        self.line_9.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_9, 8, 1, 1, 1)

        self.line_16 = QFrame(self.scrollAreaWidgetContents)
        self.line_16.setObjectName(u"line_16")
        self.line_16.setFrameShadow(QFrame.Plain)
        self.line_16.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_16, 16, 0, 1, 1)

        self.line_14 = QFrame(self.scrollAreaWidgetContents)
        self.line_14.setObjectName(u"line_14")
        self.line_14.setFrameShadow(QFrame.Plain)
        self.line_14.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_14, 14, 0, 1, 1)

        self.label_5 = QLabel(self.scrollAreaWidgetContents)
        self.label_5.setObjectName(u"label_5")

        self.gridLayout_3.addWidget(self.label_5, 7, 0, 1, 1)

        self.label_9 = QLabel(self.scrollAreaWidgetContents)
        self.label_9.setObjectName(u"label_9")

        self.gridLayout_3.addWidget(self.label_9, 11, 0, 1, 1)

        self.line_5 = QFrame(self.scrollAreaWidgetContents)
        self.line_5.setObjectName(u"line_5")
        self.line_5.setFrameShadow(QFrame.Plain)
        self.line_5.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_5, 4, 1, 1, 1)

        self.line_6 = QFrame(self.scrollAreaWidgetContents)
        self.line_6.setObjectName(u"line_6")
        self.line_6.setFrameShadow(QFrame.Plain)
        self.line_6.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_6, 6, 0, 1, 1)

        self.label_12 = QLabel(self.scrollAreaWidgetContents)
        self.label_12.setObjectName(u"label_12")

        self.gridLayout_3.addWidget(self.label_12, 17, 0, 1, 1)

        self.label_6 = QLabel(self.scrollAreaWidgetContents)
        self.label_6.setObjectName(u"label_6")

        self.gridLayout_3.addWidget(self.label_6, 5, 0, 1, 1)

        self.line_13 = QFrame(self.scrollAreaWidgetContents)
        self.line_13.setObjectName(u"line_13")
        self.line_13.setFrameShadow(QFrame.Plain)
        self.line_13.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_13, 12, 1, 1, 1)

        self.CW_index_combo = QComboBox(self.scrollAreaWidgetContents)
        self.CW_index_combo.addItem("")
        self.CW_index_combo.setObjectName(u"CW_index_combo")

        self.gridLayout_3.addWidget(self.CW_index_combo, 15, 1, 1, 1)

        self.line_18 = QFrame(self.scrollAreaWidgetContents)
        self.line_18.setObjectName(u"line_18")
        self.line_18.setFrameShadow(QFrame.Plain)
        self.line_18.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_18, 18, 0, 1, 1)

        self.label_2 = QLabel(self.scrollAreaWidgetContents)
        self.label_2.setObjectName(u"label_2")
        self.label_2.setFrameShape(QFrame.NoFrame)

        self.gridLayout_3.addWidget(self.label_2, 1, 0, 1, 1)

        self.line_10 = QFrame(self.scrollAreaWidgetContents)
        self.line_10.setObjectName(u"line_10")
        self.line_10.setFrameShadow(QFrame.Plain)
        self.line_10.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_10, 10, 0, 1, 1)

        self.line_8 = QFrame(self.scrollAreaWidgetContents)
        self.line_8.setObjectName(u"line_8")
        self.line_8.setFrameShadow(QFrame.Plain)
        self.line_8.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_8, 8, 0, 1, 1)

        self.thumbsUp_combo = QComboBox(self.scrollAreaWidgetContents)
        self.thumbsUp_combo.addItem("")
        self.thumbsUp_combo.setObjectName(u"thumbsUp_combo")

        self.gridLayout_3.addWidget(self.thumbsUp_combo, 17, 1, 1, 1)

        self.line_17 = QFrame(self.scrollAreaWidgetContents)
        self.line_17.setObjectName(u"line_17")
        self.line_17.setFrameShadow(QFrame.Plain)
        self.line_17.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_17, 16, 1, 1, 1)

        self.line_11 = QFrame(self.scrollAreaWidgetContents)
        self.line_11.setObjectName(u"line_11")
        self.line_11.setFrameShadow(QFrame.Plain)
        self.line_11.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_11, 10, 1, 1, 1)

        self.line_7 = QFrame(self.scrollAreaWidgetContents)
        self.line_7.setObjectName(u"line_7")
        self.line_7.setFrameShadow(QFrame.Plain)
        self.line_7.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_7, 6, 1, 1, 1)

        self.label_4 = QLabel(self.scrollAreaWidgetContents)
        self.label_4.setObjectName(u"label_4")

        self.gridLayout_3.addWidget(self.label_4, 3, 0, 1, 1)

        self.label_8 = QLabel(self.scrollAreaWidgetContents)
        self.label_8.setObjectName(u"label_8")

        self.gridLayout_3.addWidget(self.label_8, 9, 0, 1, 1)

        self.line_19 = QFrame(self.scrollAreaWidgetContents)
        self.line_19.setObjectName(u"line_19")
        self.line_19.setFrameShadow(QFrame.Plain)
        self.line_19.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_19, 18, 1, 1, 1)

        self.label_11 = QLabel(self.scrollAreaWidgetContents)
        self.label_11.setObjectName(u"label_11")

        self.gridLayout_3.addWidget(self.label_11, 15, 0, 1, 1)

        self.grip_hand_combo = QComboBox(self.scrollAreaWidgetContents)
        self.grip_hand_combo.addItem("")
        self.grip_hand_combo.setObjectName(u"grip_hand_combo")

        self.gridLayout_3.addWidget(self.grip_hand_combo, 7, 1, 1, 1)

        self.line_15 = QFrame(self.scrollAreaWidgetContents)
        self.line_15.setObjectName(u"line_15")
        self.line_15.setFrameShadow(QFrame.Plain)
        self.line_15.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_15, 14, 1, 1, 1)

        self.line_2 = QFrame(self.scrollAreaWidgetContents)
        self.line_2.setObjectName(u"line_2")
        self.line_2.setFrameShadow(QFrame.Plain)
        self.line_2.setFrameShape(QFrame.HLine)

        self.gridLayout_3.addWidget(self.line_2, 2, 0, 1, 1)

        self.label_10 = QLabel(self.scrollAreaWidgetContents)
        self.label_10.setObjectName(u"label_10")

        self.gridLayout_3.addWidget(self.label_10, 13, 0, 1, 1)

        self.CCW_index_combo = QComboBox(self.scrollAreaWidgetContents)
        self.CCW_index_combo.addItem("")
        self.CCW_index_combo.setObjectName(u"CCW_index_combo")

        self.gridLayout_3.addWidget(self.CCW_index_combo, 13, 1, 1, 1)

        self.label_15 = QLabel(self.scrollAreaWidgetContents)
        self.label_15.setObjectName(u"label_15")
        sizePolicy2.setHeightForWidth(self.label_15.sizePolicy().hasHeightForWidth())
        self.label_15.setSizePolicy(sizePolicy2)
        self.label_15.setFrameShape(QFrame.Box)
        self.label_15.setAlignment(Qt.AlignCenter)

        self.gridLayout_3.addWidget(self.label_15, 0, 1, 1, 1)

        self.label_13 = QLabel(self.scrollAreaWidgetContents)
        self.label_13.setObjectName(u"label_13")

        self.gridLayout_3.addWidget(self.label_13, 19, 0, 1, 1)

        self.verticalSpacer = QSpacerItem(20, 20, QSizePolicy.Minimum, QSizePolicy.Fixed)

        self.gridLayout_3.addItem(self.verticalSpacer, 20, 0, 1, 1)

        self.verticalSpacer_2 = QSpacerItem(20, 20, QSizePolicy.Minimum, QSizePolicy.Fixed)

        self.gridLayout_3.addItem(self.verticalSpacer_2, 20, 1, 1, 1)

        self.scrollArea.setWidget(self.scrollAreaWidgetContents)

        self.verticalLayout_2.addWidget(self.scrollArea)

        self.verticalLayout_2.setStretch(0, 1)
        self.verticalLayout_2.setStretch(2, 1)
        self.verticalLayout_2.setStretch(3, 10)
        MainWindow.setCentralWidget(self.centralwidget)
        self.statusbar = QStatusBar(MainWindow)
        self.statusbar.setObjectName(u"statusbar")
        MainWindow.setStatusBar(self.statusbar)

        self.retranslateUi(MainWindow)

        QMetaObject.connectSlotsByName(MainWindow)
    # setupUi

    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle(QCoreApplication.translate("MainWindow", u"HandSense Configuration", None))
        self.label_3.setText(QCoreApplication.translate("MainWindow", u"Connection Status    - ", None))
        self.label.setText(QCoreApplication.translate("MainWindow", u"No connection found", None))
        self.pushButton.setText(QCoreApplication.translate("MainWindow", u"Connect to glove", None))
        self.label_7.setText(QCoreApplication.translate("MainWindow", u"Navigation Settings", None))
        self.openHand_swipeLeft_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Previous Media", None))

        self.label_14.setText(QCoreApplication.translate("MainWindow", u"Gesture Type", None))
        self.openHand_swipeRight_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Next Media", None))

        self.openHand_swipeDown_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Minimize Window", None))

        self.doubleTap_thumb_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Right-Click", None))

        self.thumbsDown_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Volume Down", None))

        self.tap_thumb_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Click", None))

        self.label_5.setText(QCoreApplication.translate("MainWindow", u"Grip hand", None))
        self.label_9.setText(QCoreApplication.translate("MainWindow", u"Open hand - Swipe left", None))
        self.label_12.setText(QCoreApplication.translate("MainWindow", u"Thumb up", None))
        self.label_6.setText(QCoreApplication.translate("MainWindow", u"Open hand - Swipe down", None))
        self.CW_index_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Scroll Down", None))

        self.label_2.setText(QCoreApplication.translate("MainWindow", u"Tap thumb sensor", None))
        self.thumbsUp_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Volume Up", None))

        self.label_4.setText(QCoreApplication.translate("MainWindow", u"Double tap thumb sensor", None))
        self.label_8.setText(QCoreApplication.translate("MainWindow", u"Open hand - Swipe right", None))
        self.label_11.setText(QCoreApplication.translate("MainWindow", u"Clockwise index finger circles", None))
        self.grip_hand_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Play/Pause Media", None))

        self.label_10.setText(QCoreApplication.translate("MainWindow", u"Counter-clockwise index finger circles", None))
        self.CCW_index_combo.setItemText(0, QCoreApplication.translate("MainWindow", u"Scroll up", None))

        self.label_15.setText(QCoreApplication.translate("MainWindow", u"Activation Method", None))
        self.label_13.setText(QCoreApplication.translate("MainWindow", u"Thumb down", None))
    # retranslateUi


