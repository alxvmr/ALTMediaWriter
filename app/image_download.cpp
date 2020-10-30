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

#include "image_download.h"
#include "utilities.h"

#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <QDir>
#include <QTimer>
#include <QFile>

ImageDownload::ImageDownload(const QUrl &url_arg, Progress *progress_arg)
: QObject()
{
    url = url_arg;
    progress = progress_arg;
   
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    timeout_timer = new QTimer(this);
    timeout_timer->setSingleShot(true);
    timeout_timer->setInterval(5000);

    connect(
        timeout_timer, &QTimer::timeout,
        this, &ImageDownload::onTimeout);

    mDebug() << this->metaObject()->className() << "Going to download" << url;

    const QString download_dir_path = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QDir download_dir(download_dir_path);

    const QString filePath = download_dir.filePath(url.fileName()) + ".part";
    file = new QFile(filePath, this);

    previousSize = file->size();
    bytesDownloaded = previousSize;
    progress->setValue(previousSize);

    if (file->exists()) {
        mDebug() << this->metaObject()->className() << "Continuing previous download";
        file->open(QIODevice::Append);
    } else {
        file->open(QIODevice::WriteOnly);
    }

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setUrl(url);
    request.setRawHeader("Range", QString("bytes=%1-").arg(bytesDownloaded).toLocal8Bit());
    if (!options.noUserAgent) {
        request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    }

    reply = network_access_manager->get(request);
    // NOTE: 64MB buffer in case the user is on a very fast network
    reply->setReadBufferSize(64L * 1024L * 1024L);
    reply->setParent(this);

    connect(reply, &QNetworkReply::readyRead, this, &ImageDownload::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &ImageDownload::onFinished);

    timeout_timer->start();

    if (reply->bytesAvailable() > 0) {
        onReadyRead();
    }
}

void ImageDownload::onReadyRead() {
    emit readyRead();

    // Restart timer
    timeout_timer->start();

    QByteArray buf = reply->read(1024*64);
    if (reply->error() == QNetworkReply::NoError && buf.size() > 0) {

        bytesDownloaded += buf.size();

        if (progress && reply->header(QNetworkRequest::ContentLengthHeader).isValid())
            progress->setTo(reply->header(QNetworkRequest::ContentLengthHeader).toULongLong() + previousSize);

        if (progress)
            progress->setValue(bytesDownloaded);

        if (file->exists() && file->isOpen() && file->isWritable() && file->write(buf) == buf.size()) {
            // m_hash.addData(buf);
        }
        else {
            QStorageInfo storage(file->fileName());
            const QString error_message =
            [storage]() {
                if (storage.bytesAvailable() < 5L * 1024L * 1024L) {
                    return tr("You ran out of space in your Downloads folder.");
                } else {
                    return tr("The download file is not writable.");
                }
            }();

            emit diskError(error_message);

            deleteLater();
        }
    }
    if (reply->bytesAvailable() > 0) {
        QTimer::singleShot(0, this, SLOT(onReadyRead()));
    }
}

void ImageDownload::onFinished() {
    timeout_timer->stop();

    if (reply->error() != 0) {
        mDebug() << this->metaObject()->className() << "A reply finished with error:" << reply->errorString();
        mWarning() << "reply: " << reply;


        emit networkError();
    } else {
        while (reply->bytesAvailable() > 0) {
            onReadyRead();
            qApp->eventDispatcher()->processEvents(QEventLoop::ExcludeSocketNotifiers);
        }
        mDebug() << this->metaObject()->className() << "Finished successfully";

        file->close();
        emit finished();
    }
}

void ImageDownload::onTimeout() {
    mWarning() << reply->url() << "timed out.";
    reply->abort();
}
