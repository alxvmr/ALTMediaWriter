/*
 * Fedora Media Writer
 * Copyright (C) 2017 Martin Bříza <mbriza@redhat.com>
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

#include "downloadmanager.h"

#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QNetworkProxyFactory>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <QDir>

DownloadManager *DownloadManager::instance() {
    static DownloadManager *instance = = new DownloadManager();
    return instance;
}

QString DownloadManager::dir() {
    QString d = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

    return d;
}

QString DownloadManager::userAgent() {
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

/*
 * TODO explain how this works
 */
QString DownloadManager::downloadFile(DownloadReceiver *receiver, const QUrl &url, const QString &folder, Progress *progress) {
    mDebug() << this->metaObject()->className() << "Going to download" << url;
    QString bareFileName = QString("%1/%2").arg(folder).arg(url.fileName());

    QDir dir;
    dir.mkpath(folder);

    if (QFile::exists(bareFileName)) {
        mDebug() << this->metaObject()->className() << "The file already exists on" << bareFileName;
        return bareFileName;
    }

    current_image_url = url.toString();

    if (m_current)
        m_current->deleteLater();

    m_current = new Download(this, receiver, bareFileName, progress);
    connect(m_current, &QObject::destroyed, [&](){ m_current = nullptr; });

    if (m_current->hasCatchedUp()) {
        QNetworkReply *reply = tryAnotherMirror();
        m_current->handleNewReply(reply);
    }

    return bareFileName + ".part";
}

QNetworkReply *DownloadManager::tryAnotherMirror() {
    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setUrl(current_image_url);
    request.setRawHeader("Range", QString("bytes=%1-").arg(m_current->bytesDownloaded()).toLocal8Bit());
    if (!options.noUserAgent)
        request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());

    return m_manager.get(request);
}

void DownloadManager::cancel() {
    if (m_current) {
        m_current->deleteLater();
        m_current = nullptr;
        mDebug() << this->metaObject()->className() << "Cancelling";

        m_resumingDownload = false;
    }
}

bool DownloadManager::resumingDownload() const {
    return m_resumingDownload;
}

void DownloadManager::setResumingDownload(const bool value) {
    m_resumingDownload = value;
    emit resumingDownloadChanged();
}

DownloadManager::DownloadManager() {
    mDebug() << this->metaObject()->className() << "User-Agent:" << userAgent();
    QNetworkProxyFactory::setUseSystemConfiguration(true);
}


Download::Download(DownloadManager *parent, DownloadReceiver *receiver, const QString &filePath, Progress *progress)
: QObject(parent)
, m_receiver(receiver)
, m_progress(progress)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimedOut()));

    m_file = new QFile(filePath + ".part", this);

    if (m_file->exists()) {
        m_bytesDownloaded = m_file->size();
        m_catchingUp = true;
    }

    QTimer::singleShot(0, this, SLOT(start()));
}

DownloadManager *Download::manager() {
    return qobject_cast<DownloadManager*>(parent());
}

void Download::handleNewReply(QNetworkReply *reply) {
    mDebug() << this->metaObject()->className() << "Trying to download from :" << reply->url();

    if (m_reply) {
        m_reply->deleteLater();
    }

    m_reply = reply;
    // have only a 64MB buffer in case the user is on a very fast network
    m_reply->setReadBufferSize(64*1024*1024);
    m_reply->setParent(this);

    connect(m_reply, &QNetworkReply::readyRead, this, &Download::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &Download::onFinished);

    m_timer.start(15000);

    if (m_reply->bytesAvailable() > 0) {
        onReadyRead();
    }
}

qint64 Download::bytesDownloaded() {
    return m_bytesDownloaded;
}

bool Download::hasCatchedUp() {
    return !m_catchingUp;
}

void Download::start() {
    if (m_catchingUp) {
        mDebug() << this->metaObject()->className() << "Will need to precompute the hash of the previously downloaded part";
        // first precompute the SHA hash of the already downloaded part
        m_file->open(QIODevice::ReadOnly);
        m_previousSize = 0;

        QTimer::singleShot(0, this, SLOT(catchUp()));
    }
    else {
        mDebug() << this->metaObject()->className() << "Creating a new empty file";
        m_file->open(QIODevice::WriteOnly);
    }
}

void Download::catchUp() {
    QByteArray buffer = m_file->read(1024*1024);
    m_previousSize += buffer.size();
    m_hash.addData(buffer);
    if (m_progress && m_previousSize < m_progress->to())
        m_progress->setValue(m_previousSize);
    m_bytesDownloaded = m_previousSize;

    if (!m_file->atEnd()) {
        QTimer::singleShot(0, this, SLOT(catchUp()));
    }
    else {
        mDebug() << this->metaObject()->className() << "Finished computing the hash of the downloaded part";
        m_file->close();
        m_file->open(QIODevice::Append);
        m_catchingUp = false;

        QNetworkReply *reply = manager()->tryAnotherMirror();
        handleNewReply(reply);
    }
}

void Download::onReadyRead() {
    if (!m_reply)
        return;
    QByteArray buf = m_reply->read(1024*64);
    if (m_reply->error() == QNetworkReply::NoError && buf.size() > 0) {

        m_bytesDownloaded += buf.size();

        if (m_progress && m_reply->header(QNetworkRequest::ContentLengthHeader).isValid())
            m_progress->setTo(m_reply->header(QNetworkRequest::ContentLengthHeader).toULongLong() + m_previousSize);

        if (m_progress)
            m_progress->setValue(m_bytesDownloaded);

        if (m_timer.isActive())
            m_timer.start(15000);

        if (m_file) {
            if (m_file->exists() && m_file->isOpen() && m_file->isWritable() && m_file->write(buf) == buf.size()) {
                m_hash.addData(buf);
            }
            else {
                QStorageInfo storage(m_file->fileName());
                if (storage.bytesAvailable() < 5L * 1024L * 1024L) // HACK(?): 5MB
                    m_receiver->onDownloadError(tr("You ran out of space in your Downloads folder."));
                else
                    m_receiver->onDownloadError(tr("The download file is not writable."));
                deleteLater();
            }
        }
        else {
            m_buf.append(buf);
        }
    }
    if (m_reply->bytesAvailable() > 0) {
        QTimer::singleShot(0, this, SLOT(onReadyRead()));
    }
}

void Download::onFinished() {
    m_timer.stop();

    if (m_reply->error() != 0) {
        mDebug() << this->metaObject()->className() << "A reply finished with error:" << m_reply->errorString();
        mWarning() << "reply: " << m_reply;

        mWarning() << "Resuming download";
        DownloadManager::instance()->setResumingDownload(true);

        QTimer::singleShot(2000, this,
            [this]() {
                DownloadManager::instance()->setResumingDownload(false);

                QNetworkReply *reply = manager()->tryAnotherMirror();
                handleNewReply(reply);
            });
    } else {
        while (m_reply->bytesAvailable() > 0) {
            onReadyRead();
            qApp->eventDispatcher()->processEvents(QEventLoop::ExcludeSocketNotifiers);
        }
        mDebug() << this->metaObject()->className() << "Finished successfully";

        m_file->close();
        m_receiver->onFileDownloaded(m_file->fileName(), m_hash.result().toHex());
        m_reply->deleteLater();
        m_reply = nullptr;
        deleteLater();
    }
}

void Download::onTimedOut() {
    mWarning() << m_reply->url() << "timed out.";
    m_reply->abort();
}
