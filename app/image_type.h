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
 * Class representing the possible image types of the releases
 *
 * @property abbreviation short names for the type, like iso
 * @property name a common name what the short stands for, like "ISO DVD"
 * @property canWrite whether this image type is supported for writing
 */
class ImageType final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool canWrite READ canWrite CONSTANT)
public:
    enum Id {
        ISO,
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
    bool canWrite() const;

private:
    ImageType(const ImageType::Id id_arg);

    ImageType::Id m_id;
};

#endif // IMAGE_TYPE_H
