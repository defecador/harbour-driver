import QtQuick 2.6
import "../theme"

Text {
    property string variant: "body"   // title | subtitle | body | caption | button

    color: M.ink
    font.family: M.fontFamily
    font.pixelSize: variant === "title" ? M.sizeTitle
                  : variant === "subtitle" ? M.sizeSubtitle
                  : variant === "caption" ? M.sizeCaption
                  : variant === "button" ? M.sizeButton
                  : M.sizeBody
    font.weight: variant === "title" || variant === "button" ? M.weightSemiBold
               : variant === "subtitle" ? M.weightMedium
               : M.weightRegular
    renderType: Text.NativeRendering
    elide: Text.ElideRight
}
