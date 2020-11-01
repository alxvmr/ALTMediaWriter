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

#ifndef ARCHITECTURE_H
#define ARCHITECTURE_H

#include <QObject>
#include <QList>
#include <QString>

/**
 * @brief The Architecture class
 *
 * Class representing the possible architectures of the releases
 *
 * @property abbreviation short names for the architecture, like x86_64
 * @property description a better description what the short stands for, like Intel 64bit
 */
class Architecture : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList abbreviation READ abbreviation CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
public:
    enum Id {
        X86_64 = 0,
        X86,
        ARM,
        AARCH64,
        // NOTE: can't use just 'MIPSEL' because it's a predefined macro on mipsel
        MIPSEL_arch,
        RISCV64,
        E2K,
        PPC64LE,
        UNKNOWN,
        _ARCHCOUNT,
    };
    Q_ENUMS(Id);
    static Architecture *fromId(Id id);
    static Architecture *fromAbbreviation(const QString &abbr);
    static Architecture *fromFilename(const QString &filename);
    static QList<Architecture *> listAll();
    static QStringList listAllDescriptions();

    QStringList abbreviation() const;
    QString description() const;
    int index() const;

private:
    Architecture(const QStringList &abbreviation, const char *description);

    static Architecture m_all[];

    const QStringList m_abbreviation {};
    const char *m_description {};
    const char *m_details {};
};

#endif // ARCHITECTURE_H
