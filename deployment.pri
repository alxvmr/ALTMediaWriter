# this file is shared for all targets
# and it is actually located in the root folder of the project
isEmpty(ORIGIN_MEDIAWRITER_NAME) {
    ORIGIN_MEDIAWRITER_NAME = ALTMediaWriter
}
isEmpty(MEDIAWRITER_NAME) {
    MEDIAWRITER_NAME = mediawriter
}
isEmpty(PREFIX) {
    PREFIX = /usr/local
}
BINDIR = $$PREFIX/bin
DATADIR = $$PREFIX/share
isEmpty(LIBEXECDIR) {
    LIBEXECDIR = $$PREFIX/libexec/$$MEDIAWRITER_NAME
}
linux {
    DEFINES += BINDIR=\\\"$$BINDIR\\\"
    DEFINES += DATADIR=\\\"$$DATADIR\\\"
    DEFINES += LIBEXECDIR=\\\"$$LIBEXECDIR\\\"
}

QMAKE_LIBDIR += $$top_builddir/lib
windows {
    # NOTE: this is equivalent to "C:/msys64/mingw32/lib". Couldn't find a better path variable to use, but at least it's relative and doesn't depend on disk name or msys64 install location. It does depend on qt installation so if for whatever reason msys qt bin location changes this will break (that is, only if it becomes deeper from msys root, or less deep). I think static qmake is supposed to add this libdir path but for whatever reason that doesn't work. 
    QMAKE_LIBDIR += $$[QT_INSTALL_BINS]/../../lib
}

INCLUDEPATH += $$top_srcdir/lib/
isEmpty(MEDIAWRITER_VERSION) {
    MEDIAWRITER_VERSION=$$quote($$system(git -C \""$$_PRO_FILE_PWD_"\" describe --tags || echo N/A))
}
MEDIAWRITER_VERSION_SHORT=$$section(MEDIAWRITER_VERSION,-,0,0)
DEFINES += MEDIAWRITER_VERSION="\\\"$$MEDIAWRITER_VERSION\\\""
DEFINES += ORIGIN_MEDIAWRITER_NAME="\\\"$$ORIGIN_MEDIAWRITER_NAME\\\""
DEFINES += MEDIAWRITER_NAME="\\\"$$MEDIAWRITER_NAME\\\""
