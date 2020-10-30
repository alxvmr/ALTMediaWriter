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


#ifndef IMAGE_DOWNLOAD_H
#define IMAGE_DOWNLOAD_H

// TODO: remove
#include "utilities.h"

#include <QObject>
#include <QUrl>

/**
 * Downloads an image using QNetwork and writes downloaded
 * image to disk in parallel. The default download
 * directory is used. While the image file is partially
 * downloaded, it is suffixed with ".part". This suffix is
 * removed when the image download completes. If there's a
 * partially downloaded image present, the download is
 * resumed.
 */

class QNetworkReply;
class QTimer;
class QFile;

class ImageDownload final : public QObject {
    Q_OBJECT

public:
    ImageDownload(const QUrl &url_arg, Progress *progress_arg);

signals:
    // Emitted when the download finishes succesfully
    void finished();
    /**
     * Encountered a disk error. For example, this will be
     * emitted if ran out of disk space.
     */
    void diskError(const QString &message);
    /**
     * Encountered a network error. Unlike disk errors, it
     * is possible to recover from such errors, so it's ok
     * to attempt to restart a download by starting a new
     * download. For example, if internet goes down
     * temporarily and then shortly goes back up, it would
     * be possible to resume a download.
     */
    void networkError();

    // Emitted when new data is downloaded. Useful to
    // confirm that download is progressing fine.
    void readyRead();

private slots:
    void onTimeout();
    void onReadyRead();
    void onFinished();

private:
    QUrl url;
    QTimer *timeout_timer;
    // Size of the previously downloaded file, if continuing
    // a previous download
    qint64 previousSize = 0;
    // Size of the portion download in this session only
    qint64 bytesDownloaded = 0;
    QFile *file = nullptr;
    QNetworkReply *reply = nullptr;
    Progress *progress = nullptr;
};

#endif // IMAGE_DOWNLOAD_H
