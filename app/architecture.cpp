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

#include "architecture.h"

Architecture Architecture::m_all[] = {
    {{"All"}, QT_TR_NOOP("All")},
    {{"x86-64"}, QT_TR_NOOP("AMD 64bit")},
    {{"x86", "i386", "i586", "i686"}, QT_TR_NOOP("Intel 32bit")},
    {{"armv7hl", "armhfp", "armh"}, QT_TR_NOOP("ARM v7")},
    {{"aarch64"}, QT_TR_NOOP("AArch64")},
    {{"mipsel"}, QT_TR_NOOP("MIPS")},
    {{"riscv", "riscv64"}, QT_TR_NOOP("RiscV64")},
    {{"e2k"}, QT_TR_NOOP("Elbrus")},
    {{"ppc64le"}, QT_TR_NOOP("PowerPC")},
    {{"", "unknown"}, QT_TR_NOOP("Unknown")},
};

Architecture::Architecture(const QStringList &abbreviation, const char *description)
: m_abbreviation(abbreviation), m_description(description)
{

}

Architecture *Architecture::fromId(Architecture::Id id) {
    if (id >= 0 && id < _ARCHCOUNT)
        return &m_all[id];
    return &m_all[Architecture::UNKNOWN];
}

Architecture *Architecture::fromAbbreviation(const QString &abbr) {
    for (int i = 0; i < _ARCHCOUNT; i++) {
        if (m_all[i].abbreviation().contains(abbr, Qt::CaseInsensitive))
            return &m_all[i];
    }
    return &m_all[Architecture::UNKNOWN];
}

Architecture *Architecture::fromFilename(const QString &filename) {
    for (int i = 0; i < _ARCHCOUNT; i++) {
        Architecture *arch = &m_all[i];
        for (int j = 0; j < arch->m_abbreviation.size(); j++) {
            if (filename.contains(arch->m_abbreviation[j], Qt::CaseInsensitive))
                return &m_all[i];
        }
    }
    return &m_all[Architecture::UNKNOWN];
}

QStringList Architecture::listAllDescriptions() {
    QStringList ret;
    for (int i = 0; i < _ARCHCOUNT; i++) {
        if (i == UNKNOWN) {
            continue;
        }
        ret.append(m_all[i].description());
    }
    return ret;
}

QStringList Architecture::abbreviation() const {
    return m_abbreviation;
}

QString Architecture::description() const {
    return tr(m_description);
}

int Architecture::index() const {
    return this - m_all;
}
