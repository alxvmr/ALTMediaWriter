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

#ifndef WRITEJOB_H
#define WRITEJOB_H

#include <QObject>
#include <QTextStream>
#include <QProcess>

#ifndef MEDIAWRITER_LZMA_LIMIT
// 256MB memory limit for the decompressor
# define MEDIAWRITER_LZMA_LIMIT (1024*1024*256)
#endif

class WriteJob : public QObject
{
    Q_OBJECT
public:
    explicit WriteJob(const QString &what, const QString &where);

private slots:
    void work();

    bool writePlain();
    bool writeCompressed();
private:
    QString what;
    QString where;

    QTextStream out { stdout };
    QTextStream err { stderr };

    const int BLOCK_SIZE { 512 * 1024 };
};

#endif // WRITEJOB_H
