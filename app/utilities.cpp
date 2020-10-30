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

QNetworkAccessManager *network_access_manager = new QNetworkAccessManager();

Options options;

static void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
static QElapsedTimer timer;
static FILE *debugFile;

// TODO: everything Q_UNUSED

Progress::Progress(QObject *parent, qreal from, qreal to)
    : QObject(parent), m_from(from), m_to(to), m_value(from) {
    connect(this, &Progress::toChanged, this, &Progress::valueChanged);
}

qreal Progress::from() const {
    return m_from;
}

qreal Progress::to() const {
    return m_to;
}

qreal Progress::value() const {
    return m_value;
}

qreal Progress::ratio() const {
    return (value() - from()) / (to() - from());
}

void Progress::setTo(qreal v) {
    if (m_to != v) {
        m_to = v;
        emit toChanged();
    }
}

void Progress::setValue(qreal v) {
    if (m_value != v) {
        m_value = v;
        emit valueChanged();
    }
}

void Progress::setValue(qreal v, qreal to) {
    qreal computedValue = v / to * (m_to - m_from) + m_from;
    if (computedValue != m_value) {
        m_value = computedValue;
        emit valueChanged();
    }
}

void Progress::update(qreal value) {
    if (m_value != value) {
        m_value = value;
        emit valueChanged();
    }
}

void Progress::reset() {
    update(from());
}


// this is slowly getting out of hand
// when adding an another option, please consider using a real argv parser

void Options::parse(QStringList argv) {
    int index;
    if (argv.contains("--testing"))
        testing = true;
    if (argv.contains("--verbose") || argv.contains("-v")) {
        verbose = true;
        logging = false;
    }
    if (argv.contains("--logging") || argv.contains("-l"))
        logging = true;
    if (argv.contains("--no-user-agent")) {
        noUserAgent = true;
    }
    if (argv.contains("--help")) {
        printHelp();
    }

    if (options.logging) {
        QString debugFileName = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" ORIGIN_MEDIAWRITER_NAME ".log";
        debugFile = fopen(debugFileName.toStdString().c_str(), "w");
        if (!debugFile) {
            debugFile = stderr;
        }
    }
}

void Options::printHelp() {
    QTextStream out(stdout);
    out << MEDIAWRITER_NAME " [--testing] [--no-user-agent] [--releasesUrl <url>]\n";
}



static void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        if (options.verbose || options.logging)
            fprintf(debugFile, "D");
        break;
#if QT_VERSION >= 0x050500
    case QtInfoMsg:
        fprintf(debugFile, "I");
        break;
#endif
    case QtWarningMsg:
        fprintf(debugFile, "W");
        break;
    case QtCriticalMsg:
        fprintf(debugFile, "C");
        break;
    case QtFatalMsg:
        fprintf(debugFile, "F");
    }
    if ((type == QtDebugMsg && (options.verbose || options.logging)) || type != QtDebugMsg) {
        if (context.line > 0)
            fprintf(debugFile, "@%lldms: %s (%s:%u)\n", timer.elapsed(), localMsg.constData(), context.file, context.line);
        else
            fprintf(debugFile, "@%lldms: %s\n", timer.elapsed(), localMsg.constData());
        fflush(debugFile);
    }
    if (type == QtFatalMsg)
        exit(1);
}

void MessageHandler::install() {
    timer.start();
    debugFile = stderr;
    qInstallMessageHandler(myMessageOutput); // Install the handler
}

QLoggingCategory MessageHandler::category { "org.fedoraproject.MediaWriter" };

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
