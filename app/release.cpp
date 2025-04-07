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

#include "release.h"
#include "architecture.h"
#include "releasemanager.h"
#include "variant.h"

#include <QDebug>

Release::Release(const QString &name, const QString &display_name, const QString &summary, const QString &description, const QString &icon, const QStringList &screenshots, QObject *parent)
: QObject(parent) {
    m_name = name;
    m_displayName = display_name;
    m_summary = summary;
    m_description = description;
    m_icon = icon;
    m_screenshots = screenshots;
    m_isCustom = false;
    m_selectedVariant = 0;
}

Release *Release::custom(QObject *parent) {
    auto customRelease = new Release(QString(), tr("Custom image"), QT_TRANSLATE_NOOP("Release", "Pick a file from your drive(s)"), {QT_TRANSLATE_NOOP("Release", "<p>Here you can choose a OS image from your hard drive to be written to your flash disk</p><p>Currently it is only supported to write raw disk images (.iso or .bin)</p>")}, "qrc:/logo/custom", {}, parent);
    customRelease->m_isCustom = true;
    customRelease->setLocalFile(QString());

    return customRelease;
}

void Release::addVariant(Variant *variant) {
    // Sorting is done first by platform, then by architecture
    // This is to ensure that the storage of variants corresponds to the display order
    const int insert_index = [this, variant]() {
        int out = 0;
        for (const Variant *current : m_variants) {
            if (current->platform() > variant->platform()) {
                return out;
            } else if (current->platform() == variant->platform()) {
                if (current->arch() > variant->arch()) {
                    return out;
                }
            }
            out++;
        }
        return out;
    }();

    m_variants.insert(insert_index, variant);
    emit variantsChanged();

    platform_members_count[platform_name(variant->platform())]++;

    // Select first variant by default
    if (m_variants.count() == 1) {
        m_selectedVariant = 0;
        emit selectedVariantChanged();
    }
}

void Release::setLocalFile(const QUrl &fileUrl) {
    QString filePath = fileUrl.path();

// NOTE: on windows QUrl::path() leaves a leading
// slash like this: "/C:foo/bar", very fun!
#ifdef _WIN32
    if (filePath.startsWith("/")) {
        filePath.remove(0, 1);
    }
#endif // _WIN32

    qDebug() << "Setting local file to: " << filePath;

    // Delete old custom variant (there's really only one, but iterate anyway)
    for (Variant *variant : m_variants) {
        variant->deleteLater();
    }
    m_variants.clear();

    // Add new variant
    auto customVariant = new Variant(filePath, this);
    m_variants.append(customVariant);

    emit variantsChanged();
    emit selectedVariantChanged();
}

QString Release::name() const {
    return m_name;
}

QString Release::displayName() const {
    return m_displayName;
}

QString Release::summary() const {
    return tr(m_summary.toUtf8());
}

QString Release::description() const {
    return m_description;
}

bool Release::isCustom() const {
    return m_isCustom;
}

QString Release::icon() const {
    return m_icon;
}

QStringList Release::screenshots() const {
    return m_screenshots;
}

Variant *Release::selectedVariant() const {
    if (m_selectedVariant >= 0 && m_selectedVariant < m_variants.count()) {
        return m_variants[m_selectedVariant];
    }
    return nullptr;
}

int Release::selectedVariantIndex() const {
    return m_selectedVariant;
}

QStringList Release::platformsList() const {
    QStringList keys;
    for (const auto& pair : platform_members_count) {
        keys.append(pair.first);
    }

    return keys;
}

QVariantList Release::filteredVariantsPlatform() const {
    QVariantList filtered;
    for (Variant* v : m_variants) {
        if (platform_name(v->platform()) == platform_name(m_selectedPlatform)) {
            filtered.append(QVariant::fromValue(v));
        }
    }
    return filtered;
}

Platform Release::selectedPlatform () const {
    return m_selectedPlatform;
}

void Release::setSelectedPlatform (const QString &platform_name) {
    m_selectedPlatform = platform_from_string (platform_name);
    emit selectedPlatformChanged();
}

int Release::countAddIndex() const {
    int add_index = 0;
    if (m_selectedPlatform != Platform_UNKNOWN && m_selectedPlatform != Platform_ALL) {
        QString current_platform_name = platform_name (m_selectedPlatform);
        for (auto it = platform_members_count.begin(); it!=platform_members_count.end(); ++it) {
            if (it->first == current_platform_name) {
                break;
            }
            add_index += it->second;
        }
    }

    return add_index;
}

void Release::setSelectedVariantIndex(const int index) {

    if (m_selectedVariant == index) {
        return;
    }

    int index_p = index + countAddIndex();

    if (m_selectedVariant >= 0 && m_selectedVariant < m_variants.count()) {
        m_selectedVariant = index_p;
        emit selectedVariantChanged();
    }
}

QQmlListProperty<Variant> Release::variants() {
    return QQmlListProperty<Variant>(this, m_variants);
}

QList<Variant *> Release::variantList() const {
    return m_variants;
}
