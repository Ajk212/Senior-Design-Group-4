
from PyQt5.QtWidgets import QMainWindow
from ConfigUI import Ui_MainWindow as configUI #May need to change name based on window name
from PyQt5.QtCore import pyqtSignal

class GloveSettingsFrontend(QMainWindow):
    connect_requested = pyqtSignal()
    def __init__(self):
        super().__init__()
        self.ui = configUI()
        self.ui.setupUi(self)

        #list of all navigation combo boxes in UI
        self.NAV_COMBOS = [self.ui.tap_thumb_combo, 
                        self.ui.doubleTap_thumb_combo, 
                        self.ui.hold_thumb_combo,
                        self.ui.openHand_swipeDown_combo, 
                        self.ui.grip_hand_combo, 
                        self.ui.openHand_swipeRight_combo, 
                        self.ui.openHand_swipeLeft_combo, 
                        self.ui.CCW_index_combo, 
                        self.ui.CW_index_combo, 
                        self.ui.thumbsUp_combo, 
                        self.ui.thumbsDown_combo]
        
        self.NAV_OPTIONS = ["None", "Click", "Right Click", "Toggle Pause Mode", "Minimize Window", "Play/Pause Media", "Next Media", "Previous Media", "Scroll Up", "Scroll Down", "Volume Up", "Volume Down"]
        
        self.GESTURE_MAP = {
            "tap_thumb": self.ui.tap_thumb_combo,
            "doubleTap_thumb": self.ui.doubleTap_thumb_combo,
            "hold_thumb": self.ui.hold_thumb_combo,
            "openHand_swipeDown": self.ui.openHand_swipeDown_combo,
            "grip_hand": self.ui.grip_hand_combo,
            "openHand_swipeRight": self.ui.openHand_swipeRight_combo,
            "openHand_swipeLeft": self.ui.openHand_swipeLeft_combo,
            "CCW_index": self.ui.CCW_index_combo,
            "CW_index": self.ui.CW_index_combo,
            "thumbs_up": self.ui.thumbsUp_combo,
            "thumbs_down": self.ui.thumbsDown_combo,
        }

        self.profile = "Default"

        # Initialize UI labels & buttons
        self.comboBoxInit()
        # Load combo boxes with available NAV_OPTIONS
        # Connect buttons to their respective functions

        self.ui.pushButton.clicked.connect(self.attemptConnection)

    def attemptConnection(self):
        print("Connect button clicked")
        self.connect_requested.emit()
        

    def comboBoxInit(self):
        for combo in self.NAV_COMBOS:
            combo.clear()

        for combo in self.NAV_COMBOS:
            combo.addItems(self.NAV_OPTIONS)

        for combo in self.NAV_COMBOS:
            combo.currentIndexChanged.connect(self.updateComboBoxes)

        self.load_combo_profile()

    def updateComboBoxes(self):
        selected_commands = set()

        for combo in self.NAV_COMBOS:
            text = combo.currentText()
            if text and text != "None":
                selected_commands.add(text)

        for combo in self.NAV_COMBOS:
            curr_text = combo.currentText()
            combo.blockSignals(True)
            combo.clear()

            for option in self.NAV_OPTIONS:
                if option not in selected_commands or option == curr_text or option == "None":
                    combo.addItem(option)

            if curr_text:
                index = combo.findText(curr_text)
                if index >= 0:
                    combo.setCurrentIndex(index)
            combo.blockSignals(False)

    def load_combo_profile(self):
        #TODO implement dynamic profile loading

        if self.profile == "Default":
            default_settings = ["Click", "Right Click", "Toggle Pause Mode", "Minimize Window", "Play/Pause Media", "Next Media", "Previous Media", "Scroll Up", "Scroll Down", "Volume Up", "Volume Down"]
            for combo, setting in zip(self.NAV_COMBOS, default_settings):
                index = combo.findText(setting)
                if index >= 0:
                    combo.setCurrentIndex(index)
