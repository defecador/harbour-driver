import QtQuick 2.0
import Sailfish.Silica 1.0

ApplicationWindow {
    id: appWindow
    allowedOrientations: Orientation.Portrait
    cover: Qt.resolvedUrl("cover/CoverPage.qml")

    initialPage: Component {
        Page {
            id: page
            backNavigation: false
            allowedOrientations: Orientation.Portrait

            AppRoot {
                anchors.fill: parent
            }
        }
    }
}
