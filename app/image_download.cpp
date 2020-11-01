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

#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QDir>
#include <QTimer>
#include <QFile>

ImageDownload::ImageDownload(const QUrl &url_arg, Progress *progress_arg)
: QObject()
, url(url_arg)
, progress(progress_arg)
, hash(QCryptographicHash::Md5)
{
    mDebug() << this->metaObject()->className() << "created for" << url;
    
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    const QString tempFilePath = getFilePath() + ".part";
    file = new QFile(tempFilePath, this);
    file->open(QIODevice::WriteOnly);

    startImageDownload();
}

ImageDownload::Result ImageDownload::result() const {
    return m_result;
}

QString ImageDownload::errorString() const {
    return m_errorString;
}

void ImageDownload::cancel() {
    if (wasCancelled) {
        return;
    }

    mDebug() << this->metaObject()->className() << "Cancelling download";

    wasCancelled = true;
    emit cancelled();

    finish(ImageDownload::Cancelled);
}

void ImageDownload::onImageDownloadReadyRead() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    
    if (startingImageDownload) {
        mDebug() << "Request started successfully";
        startingImageDownload = false;

        const QVariant remainingSize = reply->header(QNetworkRequest::ContentLengthHeader);
        if (remainingSize.isValid()) {
            const qint64 totalSize = file->size() + remainingSize.toULongLong();
            progress->setTo(totalSize);
        }

        emit started();
    }

    const QByteArray data = reply->readAll();
    if (reply->error() == QNetworkReply::NoError && data.size() > 0) {
        if (reply->header(QNetworkRequest::ContentLengthHeader).isValid()) {
            ;
        }

        const qint64 writeSize = file->write(data);
        const bool writeSuccess = (writeSize != -1);
        if (writeSuccess) {
            progress->setValue(file->size());
        } else {
            QStorageInfo storage(file->fileName());
            const QString errorString =
            [storage]() {
                if (storage.bytesAvailable() < 5L * 1024L * 1024L) {
                    return tr("You ran out of space in your Downloads folder.");
                } else {
                    return tr("The download file is not writable.");
                }
            }();

            finish(ImageDownload::DiskError, errorString);
        }
    }
}

void ImageDownload::onImageDownloadFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();

    if (wasCancelled) {
        return;
    } else if (reply->error() == QNetworkReply::NoError) {
        mDebug() << this->metaObject()->className() << "Finished successfully";

        mDebug() << this->metaObject()->className() << "Downloading md5";

        const QString md5sumUrl = url.adjusted(QUrl::RemoveFilename).toString() + "/MD5SUM";
        QNetworkReply *md5Reply = makeNetworkRequest(md5sumUrl);
        
        connect(
            md5Reply, &QNetworkReply::finished,
            this, &ImageDownload::onMd5DownloadFinished);
        connect(
            this, &ImageDownload::cancelled,
            md5Reply, &QNetworkReply::abort);
    } else {
        mDebug() << "Download was interrupted by an error:" << reply->errorString();
        mDebug() << "Attempting to resume";

        emit interrupted();

        QTimer::singleShot(1000, this,
            [this]() {
                startImageDownload();
            });
    }
}

void ImageDownload::onMd5DownloadFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();

    if (wasCancelled) {
        return;
    } else {
        if (reply->error() == QNetworkReply::NoError) {
            mDebug() << this->metaObject()->className() << "Downloaded MD5SUM successfully";

            md5 =
            [this, reply]() {
                const QByteArray md5sumBytes = reply->readAll();
                const QString md5sumContents(md5sumBytes);

                // MD5SUM is of the form "sum image \n sum image \n ..."
                // Search for the sum by finding image matching url
                const QStringList elements = md5sumContents.split(QRegExp("\\s+"));
                QString prev = "";
                for (int i = 0; i < elements.size(); ++i) {
                    if (elements[i].size() > 0 && url.toString().contains(elements[i]) && prev.size() > 0) {
                        return prev;
                    }

                    prev = elements[i];
                }

                return QString();
            }();
        } else {
            mDebug() << this->metaObject()->className() << "Failed to download MD5SUM";

            md5 = QString();
        }

        if (md5.isEmpty()) {
            checkMd5(QString());
        } else { 
            file->close();
            const bool open_success = file->open(QIODevice::ReadOnly);
            if (open_success) {
                progress->setTo(0);
                emit startedMd5Check();
                QTimer::singleShot(0, this, &ImageDownload::computeMd5);
            } else {
                mDebug() << this->metaObject()->className() << "Failed to open file for md5 check";

                finish(ImageDownload::Md5CheckFail);
            }
        }
    }
}

void ImageDownload::computeMd5() {
    if (wasCancelled) {
        return;
    }

    const QByteArray bytes = file->read(64L * 1024L);
    const bool read_success = (bytes.size() > 0);
    
    if (read_success) {
        hash.addData(bytes);
        progress->setValue(file->pos());

        if (file->atEnd()) {
            const QByteArray sum_bytes = hash.result().toHex();
            const QString computedMd5 = QString(sum_bytes);
            checkMd5(computedMd5);
        } else {
            QTimer::singleShot(0, this, &ImageDownload::computeMd5);
        }
    } else {
        finish(ImageDownload::Md5CheckFail, tr("Failed to read from file while verifying"));
    }
}

void ImageDownload::startImageDownload() {
    mDebug() << this->metaObject()->className() << "startImageDownload()";

    startingImageDownload = true;

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setUrl(url);
    request.setRawHeader("Range", QString("bytes=%1-").arg(file->size()).toLocal8Bit());
    if (!options.noUserAgent) {
        request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    }

    QNetworkReply *reply = network_access_manager->get(request);
    // NOTE: 64MB buffer in case the user is on a very fast network
    reply->setReadBufferSize(64L * 1024L * 1024L);

    connect(
        reply, &QNetworkReply::readyRead,
        this, &ImageDownload::onImageDownloadReadyRead);
    connect(
        reply, &QNetworkReply::finished,
        this, &ImageDownload::onImageDownloadFinished);
    connect(
        this, &ImageDownload::cancelled,
        reply, &QNetworkReply::abort);
}

void ImageDownload::checkMd5(const QString &computedMd5) {
    const bool checkPassed =
    [this, computedMd5]() {
        if (md5.isEmpty()) {
            // Can fail to download md5 sum if:
            // 1) Failed to download MD5SUM file
            // 2) MD5SUM file is not present
            // 3) MD5SUM file does not contain needed sum
            // In all cases, DON'T treat this as a fail.
            // Instead, skip the check.
            mDebug() << this->metaObject()->className() << "Failed to download md5 sum, so skipping md5 check";

            return true;
        } else {
            return (computedMd5 == md5);
        }
    }();

    if (checkPassed) {
        mDebug() << this->metaObject()->className() << "Renaming to final filename";

        const QString filePath = getFilePath();
        const bool rename_success = file->rename(filePath);

        if (rename_success) {
            finish(ImageDownload::Success);
        } else {
            finish(ImageDownload::DiskError, tr("Unable to rename the temporary file."));
        }
    } else {
        mDebug() << "MD5 mismatch";
        mDebug() << "sum should be =" << md5;
        mDebug() << "computed sum  =" << computedMd5;

        finish(ImageDownload::Md5CheckFail);
    }
}

void ImageDownload::finish(const Result result_arg, const QString &errorString_arg) {
    m_result = result_arg;
    m_errorString = errorString_arg;

    mDebug() << "Image download finished";
    if (!m_errorString.isEmpty()) {
        mDebug() << "Error string:" << m_errorString;
    }

    if (m_result == ImageDownload::Success) {
        file->close();
    } else {
        file->remove();
    }

    emit finished();

    deleteLater();
}

// Get filename from url and append it to download path
QString ImageDownload::getFilePath() const {
    const QString fileName = url.fileName();
    const QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QDir downloadDir(downloadPath);
    const QString filePath = downloadDir.filePath(fileName);

    return filePath;
}
