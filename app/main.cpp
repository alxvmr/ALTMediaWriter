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

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QCommandLineParser>
#include <QQmlContext>
#include <QLoggingCategory>
#include <QTranslator>
#include <QDebug>
#include <QScreen>
#include <QtPlugin>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QFile>

#ifdef __linux
#include <QX11Info>
#endif

#include "network.h"
#include "drivemanager.h"
#include "releasemanager.h"

#if QT_VERSION < 0x050300
# error "Minimum supported Qt version is 5.3.0"
#endif

#ifdef QT_STATIC
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);

Q_IMPORT_PLUGIN(QtQuick2Plugin);
Q_IMPORT_PLUGIN(QtQuick2WindowPlugin);
Q_IMPORT_PLUGIN(QtQuick2DialogsPlugin);
Q_IMPORT_PLUGIN(QtQuick2DialogsPrivatePlugin);
Q_IMPORT_PLUGIN(QtQuickControls1Plugin);
Q_IMPORT_PLUGIN(QtQuickLayoutsPlugin);
Q_IMPORT_PLUGIN(QmlFolderListModelPlugin);
Q_IMPORT_PLUGIN(QmlSettingsPlugin);
#endif

void myMessageOutput(QtMsgType type, const QMessageLogContext &, const QString &msg);
QElapsedTimer messageTimer;
QFile *logFile;
bool myMessageOutputDebug;
bool myMessageOutputLog;

int main(int argc, char **argv)
{
#ifdef __linux
    if (QX11Info::isPlatformX11()) {
        if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP"))
            qputenv("QSG_RENDER_LOOP", "threaded");
        qputenv("GDK_BACKEND", "x11");
    }
#endif

    QApplication::setOrganizationDomain("altlinux.org");
    QApplication::setOrganizationName("basealt.ru");
    QApplication::setApplicationName("MediaWriter");

#ifdef __linux
    // qt x11 scaling is broken
    if (QX11Info::isPlatformX11())
#endif
        QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationVersion(MEDIAWRITER_VERSION);

    // Parse args
    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::tr("A tool to write images of ALT media to portable drives like flash drives or memory cards."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption optionDebug({"d", "debug"}, QApplication::tr("Print debug messages"));
    parser.addOption(optionDebug);

    const QCommandLineOption optionLog({"l", "log"}, QApplication::tr("Log all messages to a log file located in Documents"));
    parser.addOption(optionLog);

    parser.process(app);

    // Setup message handler
    myMessageOutputDebug = parser.isSet(optionDebug);
    myMessageOutputLog = parser.isSet(optionLog);

    const QString logFilename = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" ORIGIN_MEDIAWRITER_NAME ".log";
    logFile = new QFile(logFilename);
    logFile->open(QIODevice::WriteOnly);

    messageTimer.start();

    qInstallMessageHandler(myMessageOutput);

    qDebug() << "Application constructed";

    QTranslator translator;
    translator.load(QLocale(QLocale().language(), QLocale().country()), QString(), QString(), ":/translations");
    app.installTranslator(&translator);

    qDebug() << "Injecting QML context properties";
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("drives", DriveManager::instance());
    engine.rootContext()->setContextProperty("releases", new ReleaseManager());
    engine.rootContext()->setContextProperty("mediawriterVersion", MEDIAWRITER_VERSION);
#if (defined(__linux) || defined(_WIN32))
    engine.rootContext()->setContextProperty("platformSupportsDelayedWriting", true);
#else
    engine.rootContext()->setContextProperty("platformSupportsDelayedWriting", false);
#endif
    qDebug() << "Loading the QML source code";
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    qDebug() << "Starting the application";
    int status = app.exec();
    qDebug() << "Quitting with status" << status;

    return status;
}

void myMessageOutput(QtMsgType type, const QMessageLogContext &, const QString &msg) {
    if (type == QtDebugMsg && !myMessageOutputDebug && !myMessageOutputLog) {
        return;
    }

    // Construct message
    const char type_char = 
    [type]() {
        switch (type) {
            case QtDebugMsg: return 'D';
            case QtInfoMsg: return 'I';
            case QtWarningMsg: return 'W';
            case QtCriticalMsg: return 'C';
            case QtFatalMsg: return 'F';
        }

        return '?';
    }();

    const qint64 time = messageTimer.elapsed();

    const QByteArray msg_bytes = msg.toLocal8Bit();
    const char *msg_cstr = msg_bytes.constData();

    const size_t buffer_size = 1000;
    static char buffer[buffer_size];
    snprintf(buffer, buffer_size, "%c@%lldms: %s\n", type_char, time, msg_cstr);

    // Print message to console
    printf("%s", buffer);

    // Write message to log file
    if (myMessageOutputDebug) {
        logFile->write(buffer);
        logFile->flush();
    }

    if (type == QtFatalMsg) {
        exit(1);
    }
}
