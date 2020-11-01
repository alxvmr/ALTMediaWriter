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

#include "utilities.h"

#include <QDebug>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QFile>

QNetworkAccessManager *network_access_manager = new QNetworkAccessManager();

static void myMessageOutput(QtMsgType type, const QMessageLogContext &, const QString &msg);
static QElapsedTimer timer;
static QFile *logFile;

static void myMessageOutput(QtMsgType type, const QMessageLogContext &, const QString &msg) {
    if (type == QtDebugMsg && !MessageHandler::debug && !MessageHandler::log) {
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

    const qint64 time = timer.elapsed();

    const QByteArray msg_bytes = msg.toLocal8Bit();
    const char *msg_cstr = msg_bytes.constData();

    const size_t buffer_size = 1000;
    static char buffer[buffer_size];
    snprintf(buffer, buffer_size, "%c@%lldms: %s\n", type_char, time, msg_cstr);

    // Print message to console
    printf("%s", buffer);

    // Write message to log file
    if (MessageHandler::log) {
        logFile->write(buffer);
        logFile->flush();
    }

    if (type == QtFatalMsg) {
        exit(1);
    }
}

void MessageHandler::install(const bool debug_arg, const bool log_arg) {
    debug = debug_arg;
    log = log_arg;

    const QString logFilename = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" ORIGIN_MEDIAWRITER_NAME ".log";
    logFile = new QFile(logFilename);
    logFile->open(QIODevice::WriteOnly);

    timer.start();
    qInstallMessageHandler(myMessageOutput);
}

bool MessageHandler::debug;
bool MessageHandler::log;

QString userAgent() {
    QString ret = QString("FedoraMediaWriter/%1 (").arg(MEDIAWRITER_VERSION);
#if QT_VERSION >= 0x050400
    ret.append(QString("%1").arg(QSysInfo::prettyProductName().replace(QRegExp("[()]"), "")));
    ret.append(QString("; %1").arg(QSysInfo::buildAbi()));
#else
    // TODO probably should follow the format of prettyProductName, however this will be a problem just on Debian it seems
# ifdef __linux__
    ret.append("linux");
# endif // __linux__
# ifdef __APPLE__
    ret.append("mac");
# endif // __APPLE__
# ifdef _WIN32
    ret.append("windows");
# endif // _WIN32
#endif
    ret.append(QString("; %1").arg(QLocale(QLocale().language()).name()));
#ifdef MEDIAWRITER_PLATFORM_DETAILS
    ret.append(QString("; %1").arg(MEDIAWRITER_PLATFORM_DETAILS));
#endif
    ret.append(")");

    return ret;
}

QNetworkReply *makeNetworkRequest(const QString &url, const int time_out_millis) {
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());

    QNetworkReply *reply = network_access_manager->get(request);

    // TODO: Qt 5.15 added QNetworkRequest::setTransferTimeout()
    // Abort download if it takes more than 5s
    if (time_out_millis > 0) {
        auto time_out_timer = new QTimer(reply);
        QObject::connect(
            time_out_timer, &QTimer::timeout,
            [reply]() {
                reply->abort();
            });
        time_out_timer->start(time_out_millis);
    }

    return reply;
}
