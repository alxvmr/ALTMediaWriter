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

#include "release.h"
#include "releasemanager.h"
#include "architecture.h"
#include "variant.h"

#include <QDebug>

Release::Release(const QString &name, const QString &display_name, const QString &summary, const QString &description, const QString &icon, const QStringList &screenshots, QObject *parent)
: QObject(parent)
, m_name(name)
, m_displayName(display_name)
, m_summary(summary)
, m_description(description)
, m_icon(icon)
, m_screenshots(screenshots)
, m_isCustom(false)
{

}

Release *Release::custom(QObject *parent) {
    auto customRelease = new Release(QString(), tr("Custom image"), QT_TRANSLATE_NOOP("Release", "Pick a file from your drive(s)"), { QT_TRANSLATE_NOOP("Release", "<p>Here you can choose a OS image from your hard drive to be written to your flash disk</p><p>Currently it is only supported to write raw disk images (.iso or .bin)</p>") }, "qrc:/logo/custom", {}, parent);
    customRelease->m_isCustom = true;
    customRelease->setLocalFile(QString());

    return customRelease;
}

void Release::addVariant(Variant *variant) {
    // Check if variant already exists for debugging purposes
    Variant *variant_in_list =
    [=]() -> Variant * {
        for (auto current : m_variants) {
            const bool arch_equals = (current->arch() == variant->arch());
            const bool fileType_equals = (current->fileType() == variant->fileType());
            const bool board_equals = (current->board() == variant->board());
            const bool live_equals = (current->live() == variant->live());
            if (arch_equals && fileType_equals && board_equals && live_equals) {
                return current;
            }
        }
        return nullptr;
    }();
    if (variant_in_list != nullptr) {
        qWarning() << "Duplicate variant for release" << m_name;
        qWarning() << "\tcurrent=" << variant_in_list->url();
        qWarning() << "\tnew=" << variant->url();

        return;
    }

    // Sort variants by architecture
    const int insert_index =
    [this, variant]() {
        int out = 0;
        for (auto current : m_variants) {
            if (current->arch() > variant->arch()) {
                return out;
            }

            out++;
        }
        return out;
    }();

    m_variants.insert(insert_index, variant);
    emit variantsChanged();

    // Select first variant by default
    if (m_variants.count() == 1) {
        m_selectedVariant = 0;
        emit selectedVariantChanged();
    }
}

void Release::setLocalFile(const QString &fileUrl) {
    // NOTE: fileUrl has this suffix, which needs to be removed
    // for files to open properly
    QString filePath = fileUrl;
    filePath.remove("file:///");

    // Delete old custom variant (there's really only one, but iterate anyway)
    for (auto variant : m_variants) {
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
    if (m_selectedVariant >= 0 && m_selectedVariant < m_variants.count())
        return m_variants[m_selectedVariant];
    return nullptr;
}

int Release::selectedVariantIndex() const {
    return m_selectedVariant;
}

void Release::setSelectedVariantIndex(int o) {
    if (m_selectedVariant != o && m_selectedVariant >= 0 && m_selectedVariant < m_variants.count()) {
        m_selectedVariant = o;
        emit selectedVariantChanged();
    }
}

QQmlListProperty<Variant> Release::variants() {
    return QQmlListProperty<Variant>(this, m_variants);
}

QList<Variant *> Release::variantList() const {
    return m_variants;
}
