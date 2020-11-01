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

#ifndef IMAGE_TYPE_H
#define IMAGE_TYPE_H

#include <QObject>
#include <QList>
#include <QString>

/**
 * @brief The ImageType class
 *
 * Class representing the possible image types of the releases
 *
 * @property abbreviation short names for the type, like iso
 * @property name a common name what the short stands for, like "ISO DVD"
 * @property supportedForWriting whether this image type can be written to media
 * @property canWriteWithRootfs whether this image type can be written with rootfs
 */
class ImageType : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList abbreviation READ abbreviation CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool supportedForWriting READ supportedForWriting CONSTANT)
    Q_PROPERTY(bool canWriteWithRootfs READ canWriteWithRootfs CONSTANT)
public:
    enum Id {
        ISO,
        TAR,
        TAR_GZ,
        TAR_XZ,
        IMG,
        IMG_GZ,
        IMG_XZ,
        RECOVERY_TAR,
        UNKNOWN,
        COUNT,
    };
    Q_ENUMS(Id);

    static QList<ImageType *> all();
    static ImageType *fromFilename(const QString &filename);

    bool isValid() const;
    QStringList abbreviation() const;
    QString name() const;
    QString description() const;
    bool supportedForWriting() const;
    bool canWriteWithRootfs() const;

private:
    ImageType(const ImageType::Id id_arg);

    ImageType::Id m_id;
};

#endif // IMAGE_TYPE_H
