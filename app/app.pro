TEMPLATE = app

include($$top_srcdir/deployment.pri)

TARGET = $$MEDIAWRITER_NAME

QT += qml quick widgets network

linux {
    LIBS += -lyaml-cpp
}
windows {
    # NOTE: "-lyaml-cpp" ignores static linking option and links to dynamic "yaml.cpp.dll.a", so have to manually link to static file
    LIBS += -l:libyaml-cpp.a
}

CONFIG += c++11

HEADERS += \
    drivemanager.h \
    releasemanager.h \
    utilities.h \
    fakedrivemanager.h \
    notifications.h \
    crashhandler.h \
    image_download.h

SOURCES += main.cpp \
    drivemanager.cpp \
    releasemanager.cpp \
    utilities.cpp \
    fakedrivemanager.cpp \
    notifications.cpp \
    crashhandler.cpp \
    image_download.cpp

RESOURCES += qml.qrc \
    assets.qrc \
    ../translations/translations.qrc

lupdate_only {
    SOURCES += $$PWD/*.qml \
        $$PWD/complex/*.qml \
        $$PWD/dialogs/*.qml \
        $$PWD/simple/*.qml \
        $$PWD/views/*.qml \
        linuxdrivemanager.cpp \
        macdrivemanager.cpp \
        windrivemanager.cpp
    HEADERS += linuxdrivemanager.h \
        macdrivemanager.h \
        windrivemanager.h
}

target.path = $$BINDIR
INSTALLS += target

linux {
    QT += dbus x11extras

    HEADERS += linuxdrivemanager.h
    SOURCES += linuxdrivemanager.cpp

    icon.path = "$$DATADIR/icons/hicolor"
    icon.files = assets/icon/16x16 \
                 assets/icon/22x22 \
                 assets/icon/24x24 \
                 assets/icon/32x32 \
                 assets/icon/48x48 \
                 assets/icon/256x256 \
                 assets/icon/512x512

    desktopfile.path = "$$DATADIR/applications/"
    desktopfile.files = "$$top_srcdir/dist/linux/mediawriter.desktop"

    appdatafile.path = "$$DATADIR/appdata/"
    appdatafile.files = "$$top_srcdir/dist/linux/mediawriter.appdata.xml"

    INSTALLS += icon desktopfile appdatafile
}
macx {
    TARGET = "ALT Media Writer"
    HEADERS += macdrivemanager.h \
                macdrivearbiter.h
    SOURCES += macdrivemanager.cpp
    OBJECTIVE_SOURCES += macdrivearbiter.mm

    QMAKE_LFLAGS += -F/System/Library/Frameworks
    LIBS += -framework Foundation
    LIBS += -framework DiskArbitration
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9

    QMAKE_INFO_PLIST = Info.plist
    ICON = assets/icon/mediawriter.icns

    QMAKE_POST_LINK += sed -i -e "s/@MEDIAWRITER_VERSION_SHORT@/$$MEDIAWRITER_VERSION_SHORT/g" \"./$${TARGET}.app/Contents/Info.plist\";
    QMAKE_POST_LINK += sed -i -e "s/@MEDIAWRITER_VERSION@/$$MEDIAWRITER_VERSION/g" \"./$${TARGET}.app/Contents/Info.plist\";
}
win32 {
    HEADERS += windrivemanager.h
    SOURCES += windrivemanager.cpp
    RESOURCES += windowsicon.qrc

    LIBS += -ldbghelp

    # Until I find out how (or if it's even possible at all) to run a privileged process from an unprivileged one, the main binary will be privileged too
    DISTFILES += windows.manifest
    QMAKE_MANIFEST = $${PWD}/windows.manifest

    RC_ICONS = assets/icon/mediawriter.ico
    static {
        QTPLUGIN.platforms = qwindows
    }
}
