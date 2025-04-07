/*
 * ALT Media Writer
 * Copyright (C) 2016-2019 Martin Bříza <mbriza@redhat.com>
 * Copyright (C) 2020-2022 Dmitry Degtyarev <kevl@basealt.ru>
 *
 * ALT Media Writer is a fork of Fedora Media Writer
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

#include "variant.h"
#include "architecture.h"
#include "drivemanager.h"
#include "image_download.h"
#include "network.h"
#include "progress.h"
#include "release.h"
#include "releasemanager.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

Variant::Variant(const QString &url, const Architecture arch, const Platform platform, const FileType fileType, const QString &board, const bool live, const QString &md5sum, QObject *parent)
: QObject(parent) {
    m_url = url;
    m_fileName = QUrl(url).fileName();
    m_filePath = QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(fileName());
    m_board = board;
    m_live = live;
    m_md5sum = md5sum;
    m_arch = arch;
    m_platform = platform;
    m_fileType = fileType;
    m_status = Variant::PREPARING;
    m_progress = new Progress(this);
}

Variant::Variant(const QString &path, QObject *parent)
: QObject(parent) {
    m_url = QString();
    m_fileName = QFileInfo(path).fileName();
    m_filePath = path;
    m_board = QString();
    m_live = false;
    m_md5sum = QString();
    m_arch = Architecture_UNKNOWN;
    m_platform = Platform_UNKNOWN;
    m_fileType = file_type_from_filename(path);
    m_status = Variant::READY_FOR_WRITING;
    m_progress = new Progress(this);
}

Architecture Variant::arch() const {
    return m_arch;
}

QString Variant::fileName() const {
    return m_fileName;
}

QString Variant::fileTypeName() const {
    return file_type_name(m_fileType);
}

QString Variant::md5sum() const {
    return m_md5sum;
}

QString Variant::name() const {
    QString out = architecture_name(m_arch) + " | " + m_board;

    if (m_live) {
        out += " LIVE";
    }

    return out;
}

Platform Variant::platform() const {
    return m_platform;
}

QString Variant::platformName() const {
    return platform_name (m_platform);
}

QString Variant::url() const {
    return m_url;
}

QString Variant::filePath() const {
    return m_filePath;
}

bool Variant::canWrite() const {
    return file_type_can_write(m_fileType);
}

bool Variant::noMd5sum() const {
    return md5sum().isEmpty();
}

bool Variant::isCompressed() const {
    return (m_fileType == FileType_TAR_XZ || m_fileType == FileType_IMG_XZ);
}

Progress *Variant::progress() {
    return m_progress;
}

void Variant::setDelayedWrite(const bool value) {
    delayedWrite = value;

    Drive *drive = DriveManager::instance()->selected();
    if (drive != nullptr) {
        if (value) {
            drive->write(this);
        } else {
            drive->cancel();
        }
    }
}

Variant::Status Variant::status() const {
    if (m_status == READY_FOR_WRITING && DriveManager::instance()->isBackendBroken()) {
        return WRITING_NOT_POSSIBLE;
    }
    return m_status;
}

QString Variant::statusString() const {
    if (m_statusStrings.contains(status())) {
        return m_statusStrings[status()];
    } else {
        return tr("Unknown Status");
    }
}

void Variant::onImageDownloadFinished() {
    ImageDownload *download = qobject_cast<ImageDownload *>(sender());
    const ImageDownload::Result result = download->result();

    switch (result) {
        case ImageDownload::Success: {
            qDebug() << this->metaObject()->className() << "Image is ready";
            setStatus(READY_FOR_WRITING);

            break;
        }
        case ImageDownload::DiskError: {
            setErrorString(download->errorString());
            setStatus(DOWNLOAD_FAILED);

            break;
        }
        case ImageDownload::Md5CheckFail: {
            qDebug() << "MD5 check of" << m_url << "failed";
            setErrorString(tr("The downloaded image is corrupted"));
            setStatus(DOWNLOAD_FAILED);

            break;
        }
        case ImageDownload::Cancelled: {
            break;
        }
    }
}

void Variant::download() {
    delayedWrite = false;

    resetStatus();

    const bool already_downloaded = QFile::exists(filePath());

    if (already_downloaded) {
        // Already downloaded so skip download step
        qDebug() << this->metaObject()->className() << fileName() << "is already downloaded";
        setStatus(READY_FOR_WRITING);
    } else {
        // Download image
        auto download = new ImageDownload(QUrl(url()), filePath(), md5sum());

        connect(
            download, &ImageDownload::started,
            [this]() {
                setErrorString(QString());
                setStatus(DOWNLOADING);
            });
        connect(
            download, &ImageDownload::interrupted,
            [this]() {
                setErrorString(tr("Connection was interrupted, attempting to resume"));
                setStatus(DOWNLOAD_RESUMING);
            });
        connect(
            download, &ImageDownload::startedMd5Check,
            [this]() {
                setErrorString(QString());
                setStatus(DOWNLOAD_VERIFYING);
            });
        connect(
            download, &ImageDownload::finished,
            this, &Variant::onImageDownloadFinished);
        connect(
            download, &ImageDownload::progress,
            [this](const qint64 value) {
                m_progress->setCurrent(value);
            });
        connect(
            download, &ImageDownload::progressMaxChanged,
            [this](const qint64 value) {
                m_progress->setMax(value);
            });

        connect(
            this, &Variant::cancelledDownload,
            download, &ImageDownload::cancel);
    }
}

void Variant::cancelDownload() {
    emit cancelledDownload();
}

void Variant::resetStatus() {
    if (QFile::exists(filePath())) {
        setStatus(READY_FOR_WRITING);
    } else {
        setStatus(PREPARING);
        m_progress->setMax(0.0);
        m_progress->setCurrent(0.0);
    }
    setErrorString(QString());
    emit statusChanged();
}

bool Variant::erase() {
    if (QFile(filePath()).remove()) {
        qDebug() << this->metaObject()->className() << "Deleted" << filePath();
        return true;
    } else {
        qDebug() << "An attempt to delete" << filePath() << "failed!";
        return false;
    }
}

void Variant::setStatus(const Status status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

QString Variant::errorString() const {
    return m_error;
}

void Variant::setErrorString(const QString &error) {
    if (m_error != error) {
        m_error = error;
        emit errorStringChanged();
    }
}
