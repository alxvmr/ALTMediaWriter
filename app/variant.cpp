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

#include "variant.h"
#include "release.h"
#include "image_download.h"
#include "image_type.h"
#include "architecture.h"
#include "network.h"
#include "progress.h"
#include "releasemanager.h"
#include "drivemanager.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>

Variant::Variant(QString url, Architecture *arch, ImageType *imageType, QString board, const bool live, Release *parent)
: QObject(parent)
, m_arch(arch)
, m_image_type(imageType)
, m_board(board)
, m_live(live)
, m_url(url)
, m_progress(new Progress(this))
{

}

bool Variant::updateUrl(const QString &url) {
    bool changed = false;
    if (!url.isEmpty() && m_url.toUtf8().trimmed() != url.toUtf8().trimmed()) {
        // qWarning() << "Url" << m_url << "changed to" << url;
        m_url = url;
        emit urlChanged();
        changed = true;
    }
    return changed;
}

Release *Variant::release() {
    return qobject_cast<Release*>(parent());
}

const Release *Variant::release() const {
    return qobject_cast<const Release*>(parent());
}

Architecture *Variant::arch() const {
    return m_arch;
}

ImageType *Variant::imageType() const {
    return (ImageType *)m_image_type;
}

QString Variant::board() const {
    return m_board;
}

bool Variant::live() const {
    return m_live;
}

QString Variant::name() const {
    QString out = m_arch->description() + " | " + m_board;

    if (m_live) {
        out += " LIVE";
    }

    return out;
}

QString Variant::fullName() {
    if (release()->isCustom())
        return QFileInfo(image()).fileName();
    else
        return QString("%1 %2").arg(release()->displayName(), name());
}

QString Variant::url() const {
    return m_url;
}

QString Variant::image() const {
    return m_image;
}

qreal Variant::size() const {
    return m_size;
}

Progress *Variant::progress() {
    return m_progress;
}

void Variant::setDelayedWrite(const bool value) {
    delayedWrite = value;
}

Variant::Status Variant::status() const {
    if (m_status == READY && DriveManager::instance()->isBackendBroken())
        return WRITING_NOT_POSSIBLE;
    return m_status;
}

QString Variant::statusString() const {
    return m_statusStrings[status()];
}

void Variant::onImageDownloadFinished() {
    ImageDownload *download = qobject_cast<ImageDownload *>(sender());
    const ImageDownload::Result result = download->result();

    switch (result) {
        case ImageDownload::Success: {
            const QString download_dir_path = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
            const QDir download_dir(download_dir_path);
            m_image = download_dir.filePath(QUrl(m_url).fileName());;

            emit imageChanged();

            qDebug() << this->metaObject()->className() << "Image is ready";
            setStatus(READY);

            setSize(QFile(m_image).size());

            if (delayedWrite) {
                Drive *drive = DriveManager::instance()->selected();

                if (drive != nullptr) {
                    drive->write(this);
                }
            }

            break;
        }
        case ImageDownload::DiskError: {
            setErrorString(download->errorString());
            setStatus(FAILED_DOWNLOAD);

            break;
        }
        case ImageDownload::Md5CheckFail: {
            qWarning() << "MD5 check of" << m_url << "failed";
            setErrorString(tr("The downloaded image is corrupted"));
            setStatus(FAILED_DOWNLOAD);

            break;
        }
        case ImageDownload::Cancelled: {
            break;
        }
    }
}

void Variant::download() {
    if (url().isEmpty() && !image().isEmpty()) {
        setStatus(READY);

        return;
    }

    delayedWrite = false;

    resetStatus();

    // Check if already downloaded
    const QString download_dir_path = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QDir download_dir(download_dir_path);
    const QString filePath = download_dir.filePath(QUrl(m_url).fileName());
    const bool already_downloaded = QFile::exists(filePath);

    if (already_downloaded) {
        // Already downloaded so skip download step
        m_image = filePath;
        emit imageChanged();

        qDebug() << this->metaObject()->className() << m_image << "is already downloaded";
        setStatus(READY);

        setSize(QFile(m_image).size());
    } else {
        // Download image
        auto download = new ImageDownload(QUrl(m_url));

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
    if (!m_image.isEmpty()) {
        setStatus(READY);
    } else {
        setStatus(PREPARING);
        m_progress->setMax(0.0);
        m_progress->setCurrent(0.0);
    }
    setErrorString(QString());
    emit statusChanged();
}

bool Variant::erase() {
    if (QFile(m_image).remove()) {
        qDebug() << this->metaObject()->className() << "Deleted" << m_image;
        m_image = QString();
        emit imageChanged();
        return true;
    }
    else {
        qWarning() << this->metaObject()->className() << "An attempt to delete" << m_image << "failed!";
        return false;
    }
}

void Variant::setStatus(Status s) {
    if (m_status != s) {
        m_status = s;
        emit statusChanged();
    }
}

QString Variant::errorString() const {
    return m_error;
}

void Variant::setErrorString(const QString &o) {
    if (m_error != o) {
        m_error = o;
        emit errorStringChanged();
    }
}

void Variant::setSize(const qreal value) {
    if (m_size != value) {
        m_size = value;
        emit sizeChanged();
    }
}
