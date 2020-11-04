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

/**
 * Class representing the possible architectures of the releases
 */

#include <QList>
#include <QString>

enum Architecture {
    Architecture_ALL,
    Architecture_X86_64,
    Architecture_X86,
    Architecture_ARM,
    Architecture_AARCH64,
    Architecture_MIPSEL,
    Architecture_RISCV64,
    Architecture_E2K,
    Architecture_PPC64LE,
    Architecture_UNKNOWN,
    Architecture_COUNT,
};

QList<Architecture> architecture_all();

// Possible string representations of an architecture found in image
// filenames ("x86_64", "aarch64", etc)
QStringList architecture_strings(const Architecture architecture);

// An architecture name for display ("AMD 64bit", "AArch64", etc)
QString architecture_name(const Architecture architecture);

Architecture architecture_from_string(const QString &string);

Architecture architecture_from_filename(const QString &filename);

#endif // ARCHITECTURE_H
