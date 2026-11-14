/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick
import QtQuick.Layouts
import QtQml.Models
import QtQuick.Window
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player
import VLC.PlayerControls
import VLC.Dialogs
import VLC.Util
import VLC.Terrikon
import VLC.Menus

FocusScope {
    id: rootPlayer

    // Properties

    //behave like a Page
    property var pagePrefix: []

    readonly property int positionSliderY: controlBar.y + controlBar.sliderY

    readonly property string coverSource: {
        if (Player.artwork &&
            Player.artwork.toString())
            return Player.artwork
        else if (Player.hasVideoOutput)
            return VLCStyle.noArtVideoCover
        else
            return VLCStyle.noArtAlbumCover
    }

    // Only applicable in video mode:
    property bool displayFadeRectangles: !_controlsUnderVideo && (height > (implicitFadeRectangleHeight * 3))
    property real implicitFadeRectangleHeight: VLCStyle.dp(206, VLCStyle.scale)

    // Private

    property bool _controlsUnderVideo: (MainCtx.pinVideoControls
                                        &&
                                        (MainCtx.intfMainWindow.visibility !== Window.FullScreen))

    property bool _keyPressed: false

    // Settings

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Player")
    AccessibleCompat.id: "playerView"

    // Events

    Component.onCompleted: {
        MainCtx.preferHotkeys = true
        _vlcVisualization = _vlcVisualizationIndex()
    }
    Component.onDestruction: MainCtx.preferHotkeys = false

    // Terrikon keys (clip, marks, picture in picture) are VLC hotkeys,
    // carried out by TerrikonActions. Escape closes an open clip panel,
    // except in full screen, where it keeps leaving full screen.
    Shortcut {
        sequence: StandardKey.Cancel
        enabled: ClipSelection.active && rootPlayer.visible && !CompactPlayer.active
                 && MainCtx.intfMainWindow.visibility !== Window.FullScreen
        onActivated: ClipSelection.close()
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => {
        if (event.accepted)
            return

        _keyPressed = true

        rootPlayer.Navigation.defaultKeyAction(event)

        //unhandled keys are forwarded as hotkeys
        if (!event.accepted || controlBar.state !== "visible")
            MainCtx.sendHotkey(event.key, event.modifiers, event.nativeScanCode);
    }

    Keys.onReleased: (event) => {
        if (event.accepted || _keyPressed === false)
            return

        _keyPressed = false

        rootPlayer.Navigation.defaultKeyReleaseAction(event)
    }

    on_ControlsUnderVideoChanged: {
        lockUnlockAutoHide(_controlsUnderVideo)
    }

    // Functions

    function lockUnlockAutoHide(lock) {
        if (lock) {
            playerToolbarVisibilityFSM.lock()
        } else {
            playerToolbarVisibilityFSM.unlock()
        }
    }

    // Private

    // Leaving the player stops the video, unless it goes on in the small
    // view: when asked for in the settings, or when it came from there
    function _leavePlayer() {
        const audioOnly = Player.videoTracks.count === 0
        const keepPlaying = audioOnly
                            || (MainCtx.canShowVideoPIP
                                && (MainCtx.backToPip || MainCtx.playerFromPip))
        MainCtx.playerFromPip = false
        if (MainCtx.hasEmbededVideo && !keepPlaying)
            MainPlaylistController.stop()
    }

    // Terrikon: the visualizations by name, the one shown marked
    Instantiator {
        id: visualizations
        model: Player.audioVisualization
        delegate: QtObject {
            required property string display
            required property var checked
        }
    }

    // Terrikon's own first (drawn in the player, around and under the
    // cover), then the backgrounds for them, then VLC's, which take the
    // whole view. Choosing one side turns the other off.
    StringListMenu {
        id: visualizationMenu
        ctx: MainCtx
        // What each line does, filled in with the names
        property var actions: []
        onSelected: (index, text) => {
            const action = actions[index]
            if (action)
                action()
        }
    }

    // Carries out the second half of a switch once the audio output has
    // settled after the first
    Timer {
        id: visualizationSwitch

        property var _next: null

        interval: 700
        function schedule(next) {
            _next = next
            restart()
        }
        onTriggered: {
            const next = _next
            _next = null
            if (next)
                next()
        }
    }

    // Read from the model itself: the delegates above do not follow changes
    function _vlcVisualizationIndex() {
        const model = Player.audioVisualization
        for (let i = 0; i < model.rowCount(); ++i) {
            const checked = model.data(model.index(i, 0), Qt.CheckStateRole)
            if (checked === true || checked === Qt.Checked)
                return i
        }
        return -1
    }

    // Which VLC visualization is shown, kept up to date
    property int _vlcVisualization: -1

    Connections {
        target: Player.audioVisualization
        function onDataChanged() { rootPlayer._vlcVisualization = rootPlayer._vlcVisualizationIndex() }
        function onModelReset() { rootPlayer._vlcVisualization = rootPlayer._vlcVisualizationIndex() }
    }

    function _chooseOwn(style) {
        if (Visualizer.style === style) {
            Visualizer.style = 0
        } else if (rootPlayer._vlcVisualizationIndex() > 0) {
            // Switching both at once stalls the audio output: first
            // VLC's goes, then ours comes
            visualizationSwitch.schedule(() => { Visualizer.style = style })
            Player.audioVisualization.toggleIndex(0)
        } else {
            Visualizer.style = style
        }
    }

    function _chooseVlc(vlcIndex) {
        if (Visualizer.style !== 0 && vlcIndex > 0) {
            visualizationSwitch.schedule(() => { Player.audioVisualization.toggleIndex(vlcIndex) })
            Visualizer.style = 0
        } else {
            Visualizer.style = 0
            Player.audioVisualization.toggleIndex(vlcIndex)
        }
    }

    // Terrikon's own first (drawn in the player, around and under the
    // cover), then their colour and background, then VLC's, which take the
    // whole view. Choosing one side turns the other off.
    function chooseVisualization(globalPos) {
        const p = rootPlayer.mapFromGlobal(globalPos.x, globalPos.y)
        visPanel.x = Math.max(VLCStyle.margin_small,
                              Math.min(rootPlayer.width - visPanel.width - VLCStyle.margin_small, p.x - visPanel.width + VLCStyle.dp(40, VLCStyle.scale)))
        visPanel.y = Math.min(rootPlayer.height - VLCStyle.dp(120, VLCStyle.scale), p.y + VLCStyle.margin_xxsmall)
        rootPlayer._vlcVisualization = rootPlayer._vlcVisualizationIndex()
        visPanel.open()
    }

    // Terrikon: the visualization's settings in one panel of chips rather
    // than a long menu: style, colour, background, layers, mini window, and
    // VLC's own visualizations
    T.Popup {
        id: visPanel

        width: VLCStyle.dp(360, VLCStyle.scale)
        height: implicitContentHeight + topPadding + bottomPadding
        padding: VLCStyle.margin_normal
        modal: false
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

        readonly property ColorContext colorContext: ColorContext {
            id: visTheme
            colorSet: ColorContext.Window
            palette: VLCStyle.darkPalette
        }

        background: Rectangle {
            radius: VLCStyle.dp(12, VLCStyle.scale)
            color: Qt.rgba(0.11, 0.11, 0.11, 0.96)
            border.color: Qt.rgba(1, 1, 1, 0.12)
            border.width: 1
        }

        function _cap(text) { return text.length ? text[0].toUpperCase() + text.slice(1) : text }

        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

        component Chip: Rectangle {
            id: chip
            property string label
            property bool on: false
            property color swatch: "transparent"
            signal picked()

            implicitWidth: chipRow.implicitWidth + VLCStyle.margin_small * 2
            implicitHeight: VLCStyle.dp(30, VLCStyle.scale)
            radius: height / 2
            color: on ? Qt.rgba(1, 1, 1, 0.92)
                      : (chipHover.hovered ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.08))
            Behavior on color { ColorAnimation { duration: 120 } }

            Row {
                id: chipRow
                anchors.centerIn: parent
                spacing: VLCStyle.margin_xxsmall
                Rectangle {
                    visible: chip.swatch.a > 0
                    anchors.verticalCenter: parent.verticalCenter
                    width: VLCStyle.dp(12, VLCStyle.scale)
                    height: width
                    radius: width / 2
                    color: chip.swatch
                    border.color: Qt.rgba(0, 0, 0, 0.3)
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: chip.label
                    color: chip.on ? "#111111" : "white"
                    font.pixelSize: VLCStyle.fontSize_normal
                }
            }
            HoverHandler { id: chipHover; cursorShape: VLCStyle.pointerCursor }
            TapHandler { onTapped: chip.picked() }
        }

        component Section: Text {
            color: Qt.rgba(1, 1, 1, 0.6)
            font.pixelSize: VLCStyle.fontSize_small
            font.weight: Font.DemiBold
            topPadding: VLCStyle.margin_xsmall
        }

        contentItem: Column {
            width: visPanel.availableWidth
            spacing: VLCStyle.margin_xsmall

            Section { text: qsTr("Style") }
            Flow {
                width: parent.width
                spacing: VLCStyle.margin_xxsmall
                Chip {
                    label: qsTr("No visualization")
                    on: Visualizer.style === 0 && rootPlayer._vlcVisualization <= 0
                    onPicked: { rootPlayer._chooseOwn(0); if (rootPlayer._vlcVisualization > 0) rootPlayer._chooseVlc(0) }
                }
                Repeater {
                    model: Visualizer.styleNames.length - 1
                    delegate: Chip {
                        required property int index
                        label: Visualizer.styleNames[index + 1]
                        on: Visualizer.style === index + 1
                        onPicked: rootPlayer._chooseOwn(index + 1)
                    }
                }
            }

            Section { text: qsTr("Colour"); visible: Visualizer.style !== 0 }
            Flow {
                visible: Visualizer.style !== 0
                width: parent.width
                spacing: VLCStyle.margin_xxsmall
                Repeater {
                    model: Visualizer.colorModeNames.length
                    delegate: Chip {
                        required property int index
                        label: visPanel._cap(Visualizer.colorModeNames[index].replace(/…$/, "")) + (index === 2 ? "…" : "")
                        on: Visualizer.colorMode === index
                        swatch: index === Visualizer.colorMode ? Visualizer.color : "transparent"
                        onPicked: {
                            if (index === 2)
                                Visualizer.pickCustomColor()
                            else
                                Visualizer.colorMode = index
                        }
                    }
                }
            }

            Section { text: qsTr("Background"); visible: Visualizer.style !== 0 }
            Flow {
                visible: Visualizer.style !== 0
                width: parent.width
                spacing: VLCStyle.margin_xxsmall
                Repeater {
                    model: Visualizer.backgroundNames.length
                    delegate: Chip {
                        required property int index
                        // the section says "Background" already
                        label: visPanel._cap(Visualizer.backgroundNames[index].replace(/^[^:]+:\s*/, ""))
                        on: Visualizer.background === index
                        onPicked: Visualizer.background = index
                    }
                }
            }

            Section { text: qsTr("More"); visible: Visualizer.style !== 0 }
            Flow {
                visible: Visualizer.style !== 0
                width: parent.width
                spacing: VLCStyle.margin_xxsmall
                Chip {
                    label: qsTr("Fainter layers behind")
                    on: Visualizer.echoes
                    onPicked: Visualizer.echoes = !Visualizer.echoes
                }
                Chip {
                    label: qsTr("Mini window over the others")
                    on: Visualizer.miniWindow
                    onPicked: Visualizer.miniWindow = !Visualizer.miniWindow
                }
            }

            Section { text: qsTr("VLC visualizations (the whole view)") }
            Flow {
                width: parent.width
                spacing: VLCStyle.margin_xxsmall
                Repeater {
                    model: visualizations.count
                    delegate: Chip {
                        required property int index
                        visible: index > 0
                        label: visualizations.objectAt(index) ? visualizations.objectAt(index).display : ""
                        on: Visualizer.style === 0 && rootPlayer._vlcVisualization === index
                        onPicked: rootPlayer._chooseVlc(index)
                    }
                }
            }
        }
    }

    function _onNavigationCancel() {
        _leavePlayer()
        History.previous()
    }

    //we draw both the view and the window here
    ColorContext {
        id: windowTheme

        // NOTE: We force the night theme when playing a video.
        palette: (MainCtx.hasEmbededVideo && MainCtx.pinVideoControls === false)
                 ? VLCStyle.darkPalette
                 : VLCStyle.palette

        colorSet: ColorContext.Window
    }

    PlayerToolbarVisibilityFSM {
        id: playerToolbarVisibilityFSM

        onForceUnlock:{
            controlBar.forceUnlock()

            topBar.forceUnlock()
        }
    }

    PlayerPlaylistVisibilityFSM {
        id: playlistVisibility

        onIsPlaylistVisibleChanged: lockUnlockAutoHide(isPlaylistVisible)
    }

    Connections {
        target: MainCtx

        //playlist
        function onHasEmbededVideoChanged() {
            playlistVisibility.updateVideoEmbed()
            playerToolbarVisibilityFSM.updateVideoEmbed()
        }
        function onAskShow() {
            playerToolbarVisibilityFSM.askShow()
        }
    }


    Connections {
        target: MainCtx.playqueuePanel

        //playlist
        function onDockedChanged() {
            playlistVisibility.updatePlaylistDocked()
        }
        function onVisibleChanged() {
            playlistVisibility.updatePlaylistVisible()
        }
    }


    Loader {
        id: playerSpecializationLoader

        objectName: "playerSpecializationLoader"

        anchors {
            left: parent.left
            right: parent.right
            top: (MainCtx.hasEmbededVideo && rootPlayer._controlsUnderVideo) ? topBar.bottom : parent.top
            bottom: (MainCtx.hasEmbededVideo && rootPlayer._controlsUnderVideo) ? controlBar.top : parent.bottom
        }

        sourceComponent: MainCtx.hasEmbededVideo ? videoComponent : audioComponent

        property int cursorShape

        // Have padding here, so that the content (unlike background) does not go behind the top bar or the control bar:
        property real topPadding: (anchors.top === parent.top) ? topBar.height : 0
        property real bottomPadding: (anchors.bottom === parent.bottom) ? controlBar.height : 0

        component TouchDragHandler : DragHandler {
            id: touchDragHandler

            acceptedDevices: PointerDevice.TouchScreen

            minimumPointCount: 1
            maximumPointCount: 1

            target: null

            dragThreshold: VLCStyle.margin_normal

            property vector2d lastActiveTranslation

            // TODO: Use `createComponent()` and load directly from module instead, since
            //       it is pointless to keep the `QQmlComponent` around for this object.
            readonly property Component wheelToVLCConverterComponent: Component { WheelToVLCConverter { } }

            property WheelToVLCConverter wheelToVLCConverter

            // We synthesize wheel event with pixel delta. It is a to-do to handle
            // touch properly with precision adjustment, like on Android:
            onActiveTranslationChanged: {
                if (active) {
                    console.assert(wheelToVLCConverter)

                    const delta = Qt.point(activeTranslation.x - lastActiveTranslation.x,
                                           activeTranslation.y - lastActiveTranslation.y)

                    // WheelToVLCConverter can not handle sole pixel delta now, so we also
                    // use angle delta. This is similar to wheel events on some platforms:
                    wheelToVLCConverter.customWheelEvent(delta,
                                                         delta,
                                                         Qt.NoButton,
                                                         Qt.NoModifier,
                                                         true)

                    lastActiveTranslation = activeTranslation
                }
            }

            onActiveChanged: {
                if (active) {
                    if (!wheelToVLCConverter) {
                        wheelToVLCConverter = wheelToVLCConverterComponent.createObject(touchDragHandler)
                        wheelToVLCConverter.vlcWheelKey.connect(MainCtx, MainCtx.sendVLCHotkey)
                    }
                } else {
                    lastActiveTranslation = Qt.vector2d(0, 0)
                }
            }
        }

        Component {
            id: videoComponent

            FocusScope {
                // Video

                focus: true

                VideoSurface {
                    id: videoSurface

                    anchors.fill: parent

                    // With regard to `ViewBlockingRectangle`, we
                    // do not need to prevent painting anything
                    // behind because there is already nothing
                    // behind (unlike pip player):
                    renderingEnabled: false

                    videoSurfaceProvider: MainCtx.videoSurfaceProvider

                    visible: MainCtx.hasEmbededVideo

                    focus: true

                    cursorShape: playerSpecializationLoader.cursorShape

                    function onMouseEvent() {
                        //short interval for mouse events
                        if (Player.isInteractive)
                            interactiveAutoHideTimer.restart()
                        else
                            playerToolbarVisibilityFSM.mouseMove();
                    }

                    // In device pixels of the surface
                    onMouseMoved: (x, y) => {
                        const dpr = MainCtx.effectiveDevicePixelRatio(Window.window)
                        rootPlayer._pointer = videoSurface.mapToItem(null, x / dpr, y / dpr)
                        pointerRestTimer.restart()
                    }

                    Component.onCompleted: {
                        mouseMoved.connect(videoSurface.onMouseEvent)
                        mousePressed.connect(videoSurface.onMouseEvent)
                        mouseReleased.connect(videoSurface.onMouseEvent)
                        mouseDblClicked.connect(videoSurface.onMouseEvent)
                    }

                    // Holding the left button plays at 2x until it is let go
                    onMousePressed: (button) => {
                        if (button === 0 && Player.playingState === Player.PLAYING_STATE_PLAYING)
                            holdTimer.restart()
                    }
                    onMouseReleased: (button) => {
                        if (button !== 0)
                            return
                        holdTimer.endHold()
                    }
                    // A double click is a full screen toggle, not a hold
                    onMouseDblClicked: holdTimer.stop()

                    Timer {
                        id: holdTimer
                        interval: 400

                        function endHold() {
                            stop()
                            if (rootPlayer._heldRate > 0) {
                                rootPlayer._holdEndedAt = Date.now()
                                Player.rate = rootPlayer._heldRate
                                rootPlayer._heldRate = 0
                            }
                        }

                        onTriggered: {
                            rootPlayer._heldRate = Player.rate
                            Player.rate = 2
                        }
                    }

                    // The release does not always come (the window left with
                    // the button down, the video ended meanwhile): the speed
                    // would stay at 2x
                    Connections {
                        target: Player
                        function onPlayingStateChanged() {
                            if (Player.playingState !== Player.PLAYING_STATE_PLAYING)
                                holdTimer.endHold()
                        }
                    }
                    Connections {
                        target: Qt.application
                        function onStateChanged() {
                            if (Qt.application.state !== Qt.ApplicationActive)
                                holdTimer.endHold()
                        }
                    }

                    Binding on cursorShape {
                        when: playerToolbarVisibilityFSM.started
                            && !playerToolbarVisibilityFSM.isVisible
                            && !interactiveAutoHideTimer.running
                        value: Qt.BlankCursor
                    }

                    TouchDragHandler {

                    }
                }

                component FadeRectangle : Rectangle {
                    implicitHeight: rootPlayer.implicitFadeRectangleHeight

                    SGManipulator {
                        // The rectangle is the bottom-most item in the interface,
                        // it does not need blending even though it is not opaque.
                        // Since `Rectangle` does not provide explicit control over
                        // blending, we are using `SGManipulator`:
                        blending: false
                    }
                }

                FadeRectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right

                    opacity: topBar.opacity
                    visible: rootPlayer.displayFadeRectangles && !topBarAcrylicBg.visible

                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.rgba(0, 0, 0, .8) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                }

                FadeRectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right

                    opacity: controlBar.opacity

                    gradient: Gradient {
                        GradientStop { position: 0; color: "transparent" }
                        GradientStop { position: .64; color: Qt.rgba(0, 0, 0, .8) }
                        GradientStop { position: 1; color: "black" }
                    }

                    visible: rootPlayer.displayFadeRectangles && !(controlBar.background && controlBar.background.visible)
                }
            }
        }

        Component {
            id: audioComponent

            FocusScope {
                id: audioFocusScope
                // Audio

                focus: true

                property real topPadding: playerSpecializationLoader.topPadding
                property real bottomPadding: playerSpecializationLoader.bottomPadding

                // Whether the full lyrics view is active
                readonly property bool lyricsMode: MainCtx.lyricsMode
                readonly property bool lyricsLayoutActive: lyricsMode
                                                        && Player.hasLyrics
                                                        && centerContent.width > VLCStyle.colWidth(6)

                // Reset sync when entering lyrics mode
                onLyricsModeChanged: {
                    if (lyricsMode && lyricsLoader.item)
                        lyricsLoader.item.lyricsSyncToPlayback = true
                }

                // background image
                Widgets.DualKawaseBlur {
                    id: blurredBackground

                    // Six pass is free here since we release the intermediate layers
                    // through unsetting `live` (see `liveTimer`):
                    mode: Widgets.DualKawaseBlur.Mode.SixPass
                    radius: 2

                    // We disable live when everything settles through `liveTimer`,
                    // which is important to release intermediate layers with static
                    // source:
                    live: true

                    //destination aspect ratio
                    readonly property real dar: parent.width / parent.height

                    anchors.centerIn: parent
                    width: (cover.sar < dar) ? parent.width :  parent.height * cover.sar
                    height: (cover.sar < dar) ? parent.width / cover.sar :  parent.height

                    source: textureProviderIndirection

                    postprocess: true
                    tint: bgtheme.palette.isDark ? "black" : "white"
                    tintStrength: 0.5
                    backgroundColor: bgtheme.bg.primary

                    readonly property ColorContext colorContext: ColorContext {
                        id: bgtheme
                        colorSet: ColorContext.View
                    }

                    // The window naturally clips the content, but having this saves some
                    // video memory, depending on the excess content in the last layer:
                    viewportRect: Qt.rect((width - parent.width) / 2, (height - parent.height) / 2, parent.width, parent.height)

                    sourceTextureProviderObserver.onTextureChanged: {
                        liveTimer.transientTurnOnLive()
                    }

                    TextureProviderIndirection {
                        id: textureProviderIndirection

                        // This should not be necessary anymore since `DualKawaseBlur`
                        // does not create layer for the source implicitly as `MultiEffect`
                        // or `FastBlur` does as they deem necessary (in this case, it
                        // is not necessary). But due to a Qt bug when `mipmap: true` is
                        // used where texture sampling becomes broken, we need this as
                        // `QSGTextureView` has a workaround for that bug. This is totally
                        // acceptable as there is virtually no overhead.
                        source: cover
                    }

                    Component.onCompleted: {
                        // Blur layers are effect-size dependent, so once the user starts resizing the window (hence the effect),
                        // we should either momentarily turn on live, or repeatedly call `scheduleUpdate()`. Due to the optimization,
                        // calling `scheduleUpdate()` would continuously create and release intermediate layers, which would be a
                        // really bad idea. So instead, we turn on live and after some time passes turn it off again.
                        widthChanged.connect(liveTimer, liveTimer.transientTurnOnLive)
                        heightChanged.connect(liveTimer, liveTimer.transientTurnOnLive)

                        // In case texture provider observer never signals texture change:
                        liveTimer.transientTurnOnLive()
                    }

                    Timer {
                        id: liveTimer

                        repeat: false
                        interval: VLCStyle.duration_humanMoment

                        function transientTurnOnLive() {
                            if (!blurredBackground.sourceTextureIsValid)
                                return
                            blurredBackground.live = true
                            liveTimer.restart()
                        }

                        onTriggered: {
                            blurredBackground.live = false
                        }
                    }
                }

                // Terrikon: the chosen background behind the visualizer
                Rectangle {
                    anchors.fill: parent
                    color: "black"
                    visible: Visualizer.style !== 0 && Visualizer.background === 1
                }

                Image {
                    anchors.fill: parent
                    visible: Visualizer.style !== 0 && Visualizer.background === 2
                    source: visible ? cover.source : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: false

                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(0, 0, 0, 0.5)
                    }
                }

                AudioVisualizer {
                    id: visualizer

                    anchors.fill: parent
                    anchors.topMargin: audioFocusScope.topPadding
                    anchors.bottomMargin: audioFocusScope.bottomPadding

                    style: Visualizer.style
                    visible: style !== 0 && !audioFocusScope.lyricsLayoutActive
                    color: Visualizer.color
                    color2: Visualizer.color2

                    cover: cover
                }

                MouseArea {
                    id: centerContent

                    readonly property ColorContext colorContext: ColorContext {
                        id: centerTheme
                        colorSet: ColorContext.View
                    }

                    anchors.fill: parent
                    anchors.topMargin: VLCStyle.margin_xsmall + audioFocusScope.topPadding
                    anchors.bottomMargin: VLCStyle.margin_xsmall + audioFocusScope.bottomPadding

                    // Terrikon: clicks on the empty space do nothing of their
                    // own (the visualization has its button in the top bar);
                    // a right click gives the player's menu, as elsewhere
                    acceptedButtons: Qt.NoButton

                    onWheel: (wheel) => {
                        wheel.accepted = true
                        wheelToVlc.qmlWheelEvent(wheel)
                    }

                    TouchDragHandler {
                        wheelToVLCConverter: wheelToVlc
                    }

                    WheelToVLCConverter {
                        id: wheelToVlc

                        onVlcWheelKey: (key) => MainCtx.sendVLCHotkey(key)
                    }

                    // ── Two-panel lyrics layout ────────────────────────
                    Item {
                        id: leftSideParent

                        visible: (opacity > 0.0) || (width > 0.0)

                        opacity: (lyricsLoader.item ? 1.0 : 0.0)

                        Behavior on opacity {
                            id: opacityBehavior

                            enabled: false

                            NumberAnimation {
                                duration: VLCStyle.duration_long

                                easing.type: Easing.InOutSine
                            }

                            Component.onCompleted: {
                                // The animation should not be used at initialization:
                                Qt.callLater(() => { opacityBehavior.enabled = true })
                            }
                        }

                        anchors.top: parent.top
                        anchors.topMargin: VLCStyle.margin_large
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left

                        width: lyricsLoader.item ? (parent.width * 0.6) : 0.0

                        Behavior on width {
                            id: widthBehavior

                            enabled: false

                            NumberAnimation {
                                duration: VLCStyle.duration_long

                                easing.type: Easing.InOutSine
                            }

                            Component.onCompleted: {
                                // The animation should not be used at initialization:
                                Qt.callLater(() => { widthBehavior.enabled = true })
                            }
                        }

                        Loader {
                            id: lyricsLoader

                            active: audioFocusScope.lyricsLayoutActive
                            sourceComponent: T.Pane {
                                // Swallow wheel events so scrolling the lyrics
                                // does not bubble up to the player and change
                                // the volume.
                                wheelEnabled: true

                                // Expose the inner Flickable's sync flag so the
                                // Loader's `item.lyricsSyncToPlayback` reaches
                                // through to LyricsFlickable in both directions.
                                property alias lyricsSyncToPlayback: lyricsFlickable.lyricsSyncToPlayback

                                implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                                        implicitContentWidth + leftPadding + rightPadding)
                                implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                                         implicitContentHeight + topPadding + bottomPadding)

                                contentItem: LyricsFlickable {
                                    id: lyricsFlickable

                                    onPlayerPositionChangeRequested: (time) => {
                                        Player.setTime(time)
                                        lyricsFlickable.lyricsSyncToPlayback = true
                                    }
                                }
                            }
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter

                            height: Math.min(implicitHeight, parent.height)
                        }
                    }

                    Item {
                        id: rightSideParent

                        visible: leftSideParent.visible

                        anchors.top: parent.top
                        anchors.topMargin: VLCStyle.margin_large
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.left: leftSideParent.right
                    }

                    ColumnLayout {
                        parent: leftSideParent.visible ? rightSideParent : centerContent

                        anchors.centerIn: parent
                        spacing: 0

                        Item {
                            id: coverItem
                            Layout.preferredHeight: rootPlayer.height / sizeConstant
                            Layout.preferredWidth: cover.paintedWidth
                            Layout.maximumHeight: centerContent.height
                            Layout.alignment: Qt.AlignHCenter

                            readonly property real sizeConstant: 2.7182

                            Image {
                                id: cover

                                //source aspect ratio
                                readonly property real sar: paintedWidth / paintedHeight
                                readonly property int maximumWidth: Helpers.alignUp((Screen.desktopAvailableWidth * eDPR / coverItem.sizeConstant), 32)
                                readonly property int maximumHeight: Helpers.alignUp((Screen.desktopAvailableHeight * eDPR / coverItem.sizeConstant), 32)

                                readonly property int maximumSize: Math.min(maximumWidth, maximumHeight)

                                readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

                                readonly property url targetSource: VLCAccessImage.uri(rootPlayer.coverSource)

                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                source: targetSource
                                fillMode: Image.PreserveAspectFit
                                mipmap: true
                                cache: false
                                asynchronous: true

                                onTargetSourceChanged: {
                                    cover.source = targetSource
                                }

                                onSourceChanged: {
                                    liveTimer.transientTurnOnLive()
                                }

                                onStatusChanged: {
                                    if (status === Image.Error) {
                                        cover.source = VLCStyle.noArtAlbumCover
                                    }
                                }

                                sourceSize: Qt.size(maximumSize, maximumSize)

                                // Terrikon: beats with the ring around it
                                scale: visualizer.visible && Visualizer.style === 1
                                       ? 1 + visualizer.level * 0.05 : 1

                                Accessible.role: Accessible.Graphic
                                Accessible.name: qsTr("Cover")

                                Component.onCompleted: {
                                    // After the update on source change, there can be another update when the mipmaps are generated.
                                    // We intentionally do not wait for this, initially using non-mipmapped source should be okay. As
                                    // the user should not be greeted with a black background until the mipmaps are ready, let alone
                                    // the possibility of knowing if the mipmaps are actually going to be ready as expected.
                                    // If the texture is not valid yet (which is signalled the latest), blur effect is going to queue
                                    // an update itself similar to the case when the source itself changes, so we do not check validity
                                    // of the texture here.
                                    blurredBackground.sourceTextureProviderObserver.hasMipmapsChanged.connect(blurredBackground,
                                                                                                              (hasMipmaps /*: bool */) => {
                                                                                                                  if (hasMipmaps) {
                                                                                                                      blurredBackground.scheduleUpdate()
                                                                                                                  }
                                                                                                              })
                                }

                                Widgets.RoundedRectangleShadow {
                                    color: Qt.rgba(0, 0, 0, .18)
                                    yOffset: VLCStyle.dp(24, VLCStyle.scale)
                                    blurRadius: VLCStyle.dp(54, VLCStyle.scale)
                                }

                                Widgets.RoundedRectangleShadow {
                                    color: Qt.rgba(0, 0, 0, .22)
                                    yOffset: VLCStyle.dp(5, VLCStyle.scale)
                                    blurRadius: VLCStyle.dp(14, VLCStyle.scale)
                                }
                            }
                        }

                        Widgets.SubtitleLabel {
                            id: albumLabel

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_xxlarge

                            Binding on visible {
                                delayed: true
                                when: albumLabel.componentCompleted
                                value: centerContent.height > (albumLabel.y + albumLabel.height)
                            }

                            text: Player.album
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            horizontalAlignment: Text.AlignHCenter
                            color: centerTheme.fg.primary
                            Accessible.description: qsTr("album")

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }
                        }

                        Widgets.MenuLabel {
                            id: artistLabel

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_small

                            Binding on visible {
                                delayed: true
                                when: artistLabel.componentCompleted
                                value: centerContent.height > (artistLabel.y + artistLabel.height)
                            }

                            text: Player.artist
                            font.weight: Font.Light
                            horizontalAlignment: Text.AlignHCenter
                            color: centerTheme.fg.primary
                            Accessible.description: qsTr("artist")

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }
                        }

                        Widgets.SubtitleLabel {
                            id: lyricsLabel

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_large
                            // Pin width and height so the surrounding centered
                            // column does not shift when the current lyric
                            // changes between empty / 1 line / 2 lines. Text
                            // exceeding two lines wraps then elides on line 2.
                            Layout.preferredWidth: parent.parent.width - VLCStyle.margin_xlarge * 2
                            Layout.preferredHeight: (lyricsLabelTextMetrics.height * 2) + VLCStyle.margin_xxxsmall

                            visible: false

                            Binding on visible {
                                delayed: true
                                when: lyricsLabel.componentCompleted

                                // Stay visible (reserving the 2-line slot) for the
                                // entire track when it has any lyrics, so the
                                // layout does not collapse between lyric lines.
                                value: Player.hasLyrics && !audioFocusScope.lyricsLayoutActive &&
                                       (centerContent.height > (lyricsLabel.y + lyricsLabel.height))
                            }

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }

                            elide: Text.ElideRight

                            text: Player.currentLyric.text
                            font.pixelSize: VLCStyle.fontSize_xlarge
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.WordWrap
                            color: centerTheme.fg.primary

                            // TODO: Do not use `Binding` when minimum Qt is 6.12:
                            Binding {
                                target: lyricsLabel
                                property: "mutabilityGroup"
                                when: (lyricsLabel.mutabilityGroup !== undefined)
                                value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.ModerateMutabilityGroup
                                                                                              : Item.StaticMutabilityGroup
                            }

                            TextMetrics {
                                id: lyricsLabelTextMetrics
                                font: lyricsLabel.font
                                text: "TEXT"
                            }

                            Accessible.role: Accessible.StaticText
                            Accessible.name: qsTr("Lyrics")
                            Accessible.description: text
                        }

                        Widgets.NavigableRow {
                            id: audioControls

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_large

                            Binding on visible {
                                delayed: true
                                when: audioControls.componentCompleted
                                value: Player.videoTracks.count === 0 && centerContent.height > (audioControls.y + audioControls.height)
                            }

                            focus: true
                            spacing: VLCStyle.margin_xxsmall
                            Navigation.parentItem: rootPlayer
                            Navigation.upItem: topBar
                            Navigation.downItem: syncToPlaybackCheckBox

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }

                            Widgets.IconToolButton {
                                text: VLCIcons.skip_back
                                // Terrikon: not on a live stream (a radio): no time to step in
                                visible: Player.seekable && Player.length.valid() && Player.length.toSeconds() > 0
                                font.pixelSize: VLCStyle.icon_audioPlayerButton
                                onClicked: Player.jumpBwd()
                                description: qsTr("Step back")

                                AccessibleCompat.id: "audioStepBackButton"
                            }

                            Widgets.ImageToolButton {
                                id: lyricsButton
                                // always there, dim when the song has no lyrics
                                opacity: Player.hasLyrics ? 1.0 : 0.4
                                sourceSize: Qt.size(VLCStyle.icon_audioPlayerButton, VLCStyle.icon_audioPlayerButton)
                                imageSource: SVGColorImage.colorize("qrc:///icons/lyrics.svg")
                                                 .color1(MainCtx.lyricsMode ? lyricsButton.colorContext.accent
                                                                            : lyricsButton.colorContext.fg.primary).uri()
                                checked: MainCtx.lyricsMode && Player.hasLyrics
                                onClicked: if (Player.hasLyrics) MainCtx.lyricsMode = !MainCtx.lyricsMode
                                description: Player.hasLyrics ? qsTr("Lyrics") : qsTr("No lyrics for this song")

                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Toggle lyrics view")
                                AccessibleCompat.id: "audioLyricsButton"
                            }

                            Widgets.IconToolButton{
                                text: VLCIcons.skip_for
                                // Terrikon: not on a live stream (a radio): no time to step in
                                visible: Player.seekable && Player.length.valid() && Player.length.toSeconds() > 0
                                font.pixelSize: VLCStyle.icon_audioPlayerButton
                                onClicked: Player.jumpFwd()
                                description: qsTr("Step forward")

                                AccessibleCompat.id: "audioStepForwardButton"
                            }
                        }

                        // "Sync to Playback" checkbox pinned to bottom of right panel
                        Widgets.CheckBoxExt {
                            id: syncToPlaybackCheckBox

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_small

                            visible: false

                            Binding on visible {
                                delayed: true
                                when: syncToPlaybackCheckBox.componentCompleted
                                value: audioFocusScope.lyricsLayoutActive &&
                                       centerContent.height > (syncToPlaybackCheckBox.y + syncToPlaybackCheckBox.height)
                            }

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }

                            text: qsTr("Sync lyrics to playback")
                            // Use a Binding element rather than an inline binding
                            // so manual clicks (which write `checked` directly)
                            // don't permanently break the source-of-truth link.
                            Binding on checked {
                                when: lyricsLoader.item
                                value: lyricsLoader.item ? lyricsLoader.item.lyricsSyncToPlayback : true
                                restoreMode: Binding.RestoreBindingOrValue
                            }
                            onClicked: {
                                if (lyricsLoader.item)
                                    lyricsLoader.item.lyricsSyncToPlayback = checked
                            }

                            Accessible.role: Accessible.CheckBox
                            Accessible.name: qsTr("Sync lyrics to playback position")
                            AccessibleCompat.id: "syncToPlaybackCheckBox"

                            Navigation.parentItem: rootPlayer
                            Navigation.upItem: audioControls
                            Navigation.downItem: Player.isInteractive ? toggleControlBarButton : controlBar
                        }
                    }

                    Widgets.SubtitleLabel {
                        id: labelVolume

                        anchors.right: parent.right
                        anchors.top: parent.top

                        anchors.rightMargin: VLCStyle.margin_normal
                        anchors.topMargin: VLCStyle.margin_xxsmall

                        visible: false

                        text: qsTr("Volume %1%").arg(Math.round(Player.volume * 100))

                        color: centerTheme.fg.primary

                        font.weight: Font.Normal

                        Connections {
                            target: Player

                            function onVolumeChanged() {
                                animationVolume.restart()
                            }
                        }

                        SequentialAnimation {
                            id: animationVolume

                            PropertyAction { target: labelVolume; property: "visible"; value: true }

                            PauseAnimation { duration: VLCStyle.duration_humanMoment }

                            PropertyAction { target: labelVolume; property: "visible"; value: false }
                        }
                    }

                }
            }
        }
    }

    TopBar {
        id: topBar

        // Terrikon: the one visualization button, for any music
        showVisualization: rootPlayer._vlcVisualization > 0 || Player.videoTracks.count === 0
        onVisualizationRequested: (globalPos) => rootPlayer.chooseVisualization(globalPos)

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        z: 1

        topMargin: VLCStyle.applicationVerticalMargin
        sideMargin: VLCStyle.applicationHorizontalMargin

        textWidth: playlistVisibility.isPlaylistVisible
                 ? rootPlayer.width - playlistpopup.width
                 : rootPlayer.width

        // NOTE: With pinned controls, the top controls are hidden when switching to
        //       fullScreen. Except when resume is visible
        visible: (MainCtx.pinVideoControls === false
                  ||
                  MainCtx.intfMainWindow.visibility !== Window.FullScreen
                  ||
                  resumeVisible)

        focus: true
        title: Player.title

        pinControls: MainCtx.pinVideoControls

        showCSD: MainCtx.clientSideDecoration && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)
        showToolbar: MainCtx.hasToolbarMenu && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)
        playlistVisible: playlistVisibility.isPlaylistVisible

        plListView: {
            if (playlistpopup.active)
                return playlistpopup.item
            else if (playlistWindowLoader?.status === Loader.Ready)
                return playlistWindowLoader.item.playlistView
            else
                return null
        }

        Navigation.parentItem: rootPlayer
        Navigation.downItem: {
            if (playlistVisibility.isPlaylistVisible)
                return playlistpopup
            if (MainCtx.hasEmbededVideo)
                return playerSpecializationLoader
            if (Player.isInteractive)
                return toggleControlBarButton
            return controlBar
        }

        //initial state value is "", using a binding avoid animation on startup
        Binding on state {
            when: playerToolbarVisibilityFSM.started
            value: (playerToolbarVisibilityFSM.isVisible || rootPlayer._controlsUnderVideo) ? "visible" : "hidden"
        }

        onTogglePlaylistVisibility: playlistVisibility.togglePlaylistVisibility()

        onRequestLockUnlockAutoHide: (lock) => {
            rootPlayer.lockUnlockAutoHide(lock)
        }

        onBackRequested: {
            rootPlayer._leavePlayer()
            MainCtx.playerView = false
        }

        FadeControllerStateGroup {
            target: topBar
        }

        // TODO: Make TopBar a Control and use background
        Widgets.AcrylicBackground {
            id: topBarAcrylicBg

            z: -1

            anchors.fill: parent

            opacity: {
                if (MainCtx.hasEmbededVideo && !MainCtx.pinVideoControls && !rootPlayer.displayFadeRectangles) {
                    return 0.6
                } else if (MainCtx.intfMainWindow.visibility === Window.FullScreen && MainCtx.hasEmbededVideo) {
                    return MainCtx.pinOpacity
                } else {
                    return 1.0
                }
            }

            tintColor: windowTheme.bg.primary

            visible: MainCtx.pinVideoControls || (MainCtx.hasEmbededVideo && !rootPlayer.displayFadeRectangles)
        }
    }

    Loader {
        id: playlistpopup

        anchors {
            // NOTE: When the controls are pinned we display the playqueue under the topBar.
            top: (rootPlayer._controlsUnderVideo) ? topBar.bottom
                                                  : parent.top

            right: parent.right
            bottom: parent.bottom

            bottomMargin: parent.height - rootPlayer.positionSliderY
        }

        focus: false

        active: MainCtx.playqueuePanel.docked

        //initial state value is "", using a binding avoid animation on startup
        Binding on state {
            when: playlistVisibility.started
            value: (status === Loader.Ready && playlistVisibility.isPlaylistVisible) ? "expanded" : "retracted"
        }

        Component.onCompleted: {
            Qt.callLater(() => { playlistTransition.enabled = true; })
        }

        states: [
            State {
                name: "expanded"
                PropertyChanges {
                    target: playlistpopup
                    width: playlistpopup.implicitWidth
                    visible: true
                }
           }, State {
                name: "retracted"
                PropertyChanges {
                    target: playlistpopup
                    width: 0
                    visible: false
                }
           }
        ]

        transitions: Transition {
            id: playlistTransition
            enabled: false

            from: "retracted"; to: "expanded";
            reversible: true

            SequentialAnimation {
                PropertyAction { property: "visible" }

                NumberAnimation {
                    property: "width"
                    duration: VLCStyle.duration_short
                    easing.type: Easing.InOutSine
                }
            }
        }

        sourceComponent: PlaylistPane {
            id: playlistView

            property real maximumWidth: (rootPlayer.width + playlistView.rightPadding) / 2

            implicitWidth: Helpers.clamp(MainCtx.playqueuePanel.width
                                 , playlistView.minimumWidth
                                 , playlistView.maximumWidth)
            height: playlistpopup.height

            useAcrylic: false
            focus: true

            Binding {
                target: playlistView.background
                property: "opacity"
                value: MainCtx.panelOpacity
            }

            wheelEnabled: true

            rightPadding: VLCStyle.applicationHorizontalMargin
            topPadding:  {
                if (rootPlayer._controlsUnderVideo)
                    return VLCStyle.margin_normal
                else
                    // NOTE: We increase the padding accordingly to avoid overlapping the TopBar.
                    return topBar.reservedHeight
            }
            bottomPadding: 0

            background: Rectangle {
                color: windowTheme.bg.primary.alpha(0.8)
            }

            view.fadingEdge.pixelAlignedForDPR: !controlBar.visible

            Navigation.parentItem: rootPlayer
            Navigation.upItem: topBar
            Navigation.downItem: Player.isInteractive ? toggleControlBarButton : controlBar
            Navigation.leftAction: closePlaylist
            Navigation.cancelAction: closePlaylist

            function closePlaylist() {
                playlistVisibility.togglePlaylistVisibility()
                if (audioControls.visible)
                    audioControls.forceActiveFocus()
                else
                    controlBar.forceActiveFocus()
            }


            Widgets.HorizontalResizeHandle {
                id: resizeHandle

                property bool _inhibitMainCtxUpdate: false

                parent: playlistView

                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    left: parent.left
                }

                atRight: false
                currentWidth: playlistpopup.width

                maximumWidth: playlistView.maximumWidth
                minimumWidth: playlistView.minimumWidth

                onRequestedWidthChanged: {
                    if (!_inhibitMainCtxUpdate)
                        MainCtx.playqueuePanel.width = requestedWidth
                }

                Component.onCompleted:  _updateFromMainCtx()

                function _updateFromMainCtx() {
                    if (requestedWidth === MainCtx.playqueuePanel.width)
                        return

                    _inhibitMainCtxUpdate = true
                    requestedWidth = MainCtx.playqueuePanel.width
                    _inhibitMainCtxUpdate = false
                }

                Connections {
                    target: MainCtx.playqueuePanel

                    function onWidthChanged() {
                        resizeHandle._updateFromMainCtx()
                    }
                }
            }
        }
    }

    Dialogs {
        z: 10
        bgContent: rootPlayer

        anchors {
            bottom: controlBar.visible ? controlBar.top : rootPlayer.bottom
            left: parent.left
            right: parent.right

            bottomMargin: (rootPlayer._controlsUnderVideo || !controlBar.visible)
                          ? 0 : - VLCStyle.margin_large
        }
    }

    Timer {
        // NavigationBox's visibility depends on this timer
        id: interactiveAutoHideTimer
        running: false
        repeat: false
        interval: 3000
    }

    NavigationBox {
        id: navBox
        visible: Player.isInteractive && navBox.show
                    && (interactiveAutoHideTimer.running
                    || navBox.hovered || !MainCtx.hasEmbededVideo)

        x: rootPlayer.x + VLCStyle.margin_normal + VLCStyle.applicationHorizontalMargin
        y: controlBar.y - navBox.height - VLCStyle.margin_normal

        dragXMin: 0
        dragXMax: rootPlayer.width - navBox.width
        dragYMin: 0
        dragYMax: rootPlayer.height - navBox.height

        Drag.onDragStarted: (controlId) => {
            navBox.x = drag.x
            navBox.y = drag.y
        }
    }

    Connections {
        target: MainCtx
        function onNavBoxToggled() { interactiveAutoHideTimer.restart() }
    }

    Connections {
        target: rootPlayer
        function onWidthChanged() {
            if (navBox.x > navBox.dragXMax)
                navBox.x = navBox.dragXMax
        }
        function onHeightChanged() {
            if (navBox.y > navBox.dragYMax)
                navBox.y = navBox.dragYMax
        }
    }

    Widgets.ButtonExt {
        id: toggleControlBarButton
        visible: Player.isInteractive
                 && MainCtx.hasEmbededVideo
                 && !(MainCtx.pinVideoControls && !Player.fullscreen)
                 && (interactiveAutoHideTimer.running === true
                     || controlBar.state !== "hidden" || toggleControlBarButton.hovered)
        focus: true
        anchors {
            bottom: controlBar.state === "hidden" ? parent.bottom : controlBar.top
            horizontalCenter: parent.horizontalCenter
        }
        iconSize: VLCStyle.icon_large
        iconTxt: controlBar.state === "hidden" ? VLCIcons.expand_inverted : VLCIcons.expand

        Navigation.parentItem: rootPlayer
        Navigation.upItem: playlistVisibility.isPlaylistVisible ? playlistpopup : (MainCtx.hasEmbededVideo ? playerSpecializationLoader : topBar)
        Navigation.downItem: controlBar

        onClicked:{
            playerToolbarVisibilityFSM.askShow();
        }
    }

    Widgets.FloatingNotification {
        id: notif

        anchors {
            bottom: controlBar.top
            left: parent.left
            right: parent.right
            margins: VLCStyle.margin_large
        }
    }

    ClipCropFrame {
        anchors.fill: parent
    }

    // Speed before a press and hold, 0 when not holding
    property real _heldRate: 0
    property double _holdEndedAt: 0

    // Short messages for what keys and wheels do: a jump, the volume, the
    // speed, a track, a mark. VLC's own text over the video is off.
    Rectangle {
        id: osdToast

        property string text: ""

        function show(message) {
            text = message
            opacity = 1
            osdHideTimer.restart()
        }

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: VLCStyle.margin_xlarge

        visible: opacity > 0 && rootPlayer._heldRate === 0
        opacity: 0
        Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }

        width: osdLabel.implicitWidth + VLCStyle.margin_normal * 2
        height: osdLabel.implicitHeight + VLCStyle.margin_xsmall * 2
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.73 * MainCtx.overlayOpacity)

        Text {
            id: osdLabel
            anchors.centerIn: parent
            text: osdToast.text
            color: "white"
            font.pixelSize: VLCStyle.fontSize_large
            font.weight: Font.DemiBold
        }

        Timer {
            id: osdHideTimer
            interval: 1200
            onTriggered: osdToast.opacity = 0
        }
    }

    Connections {
        target: TerrikonActions
        function onOsd(text) { osdToast.show(text) }
    }

    // The player sets volume and speed while starting: no message then
    Timer {
        id: osdArmTimer
        interval: 2000
        running: true
    }

    Connections {
        target: Player
        enabled: !osdArmTimer.running

        function onVolumeChanged() {
            osdToast.show(qsTr("Volume %1%").arg(Math.round(Player.volume * 100)))
        }
        function onMutedChanged() {
            osdToast.show(Player.muted ? qsTr("Muted")
                                       : qsTr("Volume %1%").arg(Math.round(Player.volume * 100)))
        }
        function onRateChanged() {
            // press and hold has its own badge
            if (rootPlayer._heldRate > 0 || Date.now() - rootPlayer._holdEndedAt < 600)
                return
            osdToast.show(qsTr("Speed %1×").arg(+Player.rate.toFixed(2)))
        }
        function onInputChanged() { osdArmTimer.restart() }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: VLCStyle.margin_xlarge

        visible: rootPlayer._heldRate > 0
        width: speedLabel.implicitWidth + VLCStyle.margin_normal * 2
        height: speedLabel.implicitHeight + VLCStyle.margin_xsmall * 2
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.73 * MainCtx.overlayOpacity)

        Text {
            id: speedLabel
            anchors.centerIn: parent
            text: "2×  ▶▶"
            color: "white"
            font.pixelSize: VLCStyle.fontSize_large
            font.weight: Font.DemiBold
        }
    }

    // Previous and next video: a tall band at mid-height along each side.
    // The arrow shows when the pointer comes near; the whole band clicks.
    component EdgeZone: Item {
        id: zone

        property bool forward: true
        property bool available: false
        // Pointer in scene coordinates, from the video surface (_pointer)
        property point pointer: Qt.point(-1, -1)

        readonly property bool _hot: {
            const p = zone.mapFromItem(null, pointer.x, pointer.y)
            return p.x >= 0 && p.x <= width && p.y >= 0 && p.y <= height
        }

        signal activated()

        visible: available && !ClipSelection.active
        width: VLCStyle.dp(96, VLCStyle.scale)
        height: parent.height / 3
        anchors.verticalCenter: parent.verticalCenter

        // Takes the click only; the hover comes from the player (_pointer)
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: zone.activated()
        }

        Rectangle {
            anchors.centerIn: parent
            width: VLCStyle.dp(56, VLCStyle.scale)
            height: width
            radius: width / 2
            color: Qt.rgba(0, 0, 0, 0.45)
            opacity: zone._hot ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }

            Text {
                anchors.centerIn: parent
                text: zone.forward ? VLCIcons.next : VLCIcons.previous
                color: "white"
                font.family: VLCIcons.fontFamily
                font.pixelSize: VLCStyle.icon_normal
            }
        }
    }

    // Where the pointer is over the video, in scene coordinates, for the
    // edge zones; forgotten when it stops moving, as the controls are
    property point _pointer: Qt.point(-1, -1)

    Timer {
        id: pointerRestTimer
        interval: 2500
        onTriggered: rootPlayer._pointer = Qt.point(-1, -1)
    }

    EdgeZone {
        anchors.left: parent.left
        pointer: rootPlayer._pointer
        forward: false
        available: MainPlaylistController.hasPrev
        onActivated: MainPlaylistController.prev()
    }

    EdgeZone {
        anchors.right: parent.right
        pointer: rootPlayer._pointer
        forward: true
        available: MainPlaylistController.hasNext
        onActivated: MainPlaylistController.next()
    }

    SkipChapterButton {
        controlsShown: controlBar.state === "visible"

        anchors {
            right: parent.right
            bottom: (controlBar.state === "visible") ? controlBar.top : parent.bottom
            rightMargin: VLCStyle.margin_xlarge + VLCStyle.applicationHorizontalMargin
            bottomMargin: VLCStyle.margin_xlarge
        }
    }

    ControlBar {
        id: controlBar

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        hoverEnabled: true

        focus: true

        rightPadding: VLCStyle.applicationHorizontalMargin
        leftPadding: VLCStyle.applicationHorizontalMargin
        bottomPadding: VLCStyle.applicationVerticalMargin + VLCStyle.margin_xsmall

        textPosition: (MainCtx.pinVideoControls)
                      ? ControlBar.TimeTextPosition.LeftRightSlider
                      : ControlBar.TimeTextPosition.AboveSlider

        // hide right text so that it won't overlap with play queue
        showRemainingTime: (textPosition !== ControlBar.TimeTextPosition.AboveSlider)
                           || !playlistVisibility.isPlaylistVisible

        onStateChanged: {
            if (state === "visible")
                showChapterMarks()
        }

        Navigation.parentItem: rootPlayer
        Navigation.upItem: {
            if (playlistVisibility.isPlaylistVisible)
                return playlistpopup
            if (Player.isInteractive)
                return toggleControlBarButton
            if (!MainCtx.hasEmbededVideo)
                return playerSpecializationLoader
            return topBar
        }

        //initial state value is "", using a binding avoid animation on startup
        Binding on state {
            when: playerToolbarVisibilityFSM.started
            value: (playerToolbarVisibilityFSM.isVisible || rootPlayer._controlsUnderVideo) ? "visible" : "hidden"
        }

        onRequestLockUnlockAutoHide: (lock) => rootPlayer.lockUnlockAutoHide(lock)

        identifier: (Player.hasVideoOutput) ? PlayerControlbarModel.Videoplayer
                                            : PlayerControlbarModel.Audioplayer

        onHoveredChanged: rootPlayer.lockUnlockAutoHide(hovered)

        background: Rectangle {
            id: controlBarBackground

            visible: !MainCtx.hasEmbededVideo || MainCtx.pinVideoControls || !rootPlayer.displayFadeRectangles

            opacity: {
                if (MainCtx.hasEmbededVideo && !MainCtx.pinVideoControls && !rootPlayer.displayFadeRectangles) {
                    return 0.6
                } else if ((Window.visibility === Window.FullScreen) && MainCtx.hasEmbededVideo) {
                    return MainCtx.pinOpacity
                } else if (AcrylicController.enabled || !MainCtx.hasEmbededVideo) {
                    return 0.7
                } else {
                    return 1.0
                }
            }

            color: windowTheme.bg.primary
        }

        FadeControllerStateGroup {
            target: controlBar
        }
    }

    QmlAudioContextMenu {
        id: audioContextMenu

        ctx: MainCtx
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        enabled: !MainCtx.hasEmbededVideo

        onTapped: (eventPoint, button) => {
            if (button & Qt.RightButton) {
                audioContextMenu.popup(eventPoint.globalPosition)
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.BackButton
        propagateComposedEvents: true
        cursorShape: undefined
        onClicked: {
            MainCtx.playerView = false
        }
    }

    //filter key events to keep toolbar
    //visible when user navigates within the control bar
    KeyEventFilter {
    id: filter
    target: MainCtx.intfMainWindow

        Keys.onPressed: (event) => {
            if (Player.isInteractive)
                interactiveAutoHideTimer.restart()
            else
                playerToolbarVisibilityFSM.keyboardMove()
        }
    }
}
