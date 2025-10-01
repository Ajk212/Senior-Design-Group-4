from PyQt5.QtCore import QObject, pyqtSignal



class GloveSettingsBackend(QObject):
    def __init__(self):
        super().__init__()

        ## Initialize defualt profile 
        # TODO make dynamic profile options
        #self.active_profile = DEFAULT

        self.isConnected = False


    def backendUpdate(self):
        # TODO implement backend update logic
        # TODO update connection status text, connection button, connection routine etc
        pass

    def connectGlove(self):
        # TODO implement connection logic
        pass