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
INCLUDEPATH += $$top_srcdir/lib/
isEmpty(MEDIAWRITER_VERSION) {
    MEDIAWRITER_VERSION=$$quote($$system(git -C \""$$_PRO_FILE_PWD_"\" describe --tags || echo N/A))
}
MEDIAWRITER_VERSION_SHORT=$$section(MEDIAWRITER_VERSION,-,0,0)
DEFINES += MEDIAWRITER_VERSION="\\\"$$MEDIAWRITER_VERSION\\\""
DEFINES += ORIGIN_MEDIAWRITER_NAME="\\\"$$ORIGIN_MEDIAWRITER_NAME\\\""
DEFINES += MEDIAWRITER_NAME="\\\"$$MEDIAWRITER_NAME\\\""
