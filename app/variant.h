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


#ifndef VARIANT_H
#define VARIANT_H

#include <QObject>
#include <QList>
#include <QString>
#include <QHash>

class Progress;
class FileType;
class Architecture;

/**
 * @brief The Variant class
 *
 * The variant of the release version. Usually it represents different architectures. It's possible to differentiate netinst and dvd images here too.
 *
 * @property name the name of the release, generated from @ref arch and @ref board
 * @property board the name of supported hardware of the image
 * @property url the URL pointing to the image
 * @property image the path to the image on the drive
 * @property fileType the type of the image on the drive
 * @property progress the progress object of the image - reports the progress of download
 * @property status status of the variant - if it's downloading, being written, etc.
 * @property statusString string representation of the @ref status
 * @property errorString a string better describing the current error @ref status of the variant
 */
class Variant final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(FileType *fileType READ fileType CONSTANT)

    Q_PROPERTY(QString filePath READ filePath CONSTANT)
    Q_PROPERTY(QString fileName READ fileName CONSTANT)
    Q_PROPERTY(Progress* progress READ progress CONSTANT)

    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusString READ statusString NOTIFY statusChanged)
    Q_PROPERTY(QString errorString READ errorString WRITE setErrorString NOTIFY errorStringChanged)
public:
    Q_ENUMS(Type)
    enum Status {
        PREPARING = 0,
        DOWNLOADING,
        DOWNLOAD_RESUMING,
        DOWNLOAD_VERIFYING,
        READY,
        WRITING_NOT_POSSIBLE,
        WRITING,
        FINISHED,
        FAILED_DOWNLOAD,
        FAILED
    };
    Q_ENUMS(Status)
    const QHash<Status, QString> m_statusStrings = {
        {PREPARING, tr("Preparing")},
        {DOWNLOADING, tr("Downloading")},
        {DOWNLOAD_RESUMING, tr("Resuming download")},
        {DOWNLOAD_VERIFYING, tr("Checking the download")},
        {READY, tr("Ready to write")},
        {WRITING_NOT_POSSIBLE, tr("Image file was saved to your downloads folder. Writing is not possible")},
        {WRITING, tr("Writing")},
        {FINISHED, tr("Finished!")},
        {FAILED_DOWNLOAD, tr("Download failed")},
        {FAILED, tr("Error")},
    };

    Variant(QString url, Architecture *arch, FileType *fileType, QString board, const bool live, QObject *parent);

    // Constructor for local file
    Variant(const QString &path, QObject *parent);

    Q_INVOKABLE void setDelayedWrite(const bool value);

    Architecture *arch() const;
    FileType *fileType() const;
    QString name() const;
    QString board() const;
    bool live() const;

    QString url() const;
    QString filePath() const;
    QString fileName() const;
    Progress *progress();

    Status status() const;
    QString statusString() const;
    void setStatus(Status s);
    QString errorString() const;
    void setErrorString(const QString &o);

    Q_INVOKABLE bool erase();

signals:
    void fileChanged();
    void statusChanged();
    void errorStringChanged();
    void cancelledDownload();

public slots:
    void download();
    void cancelDownload();
    void resetStatus();
    void onImageDownloadFinished();

private:
    const QString m_url;
    const QString m_fileName;
    const QString m_filePath;
    const QString m_board;
    const bool m_live;
    Architecture *m_arch;
    FileType *m_fileType;
    Status m_status;
    QString m_error;
    bool delayedWrite;

    Progress *m_progress;

    void setSize(const qreal value);
};

#endif // VARIANT_H
