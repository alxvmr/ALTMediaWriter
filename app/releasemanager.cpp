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

#include "releasemanager.h"
#include "release.h"
#include "file_type.h"
#include "architecture.h"
#include "variant.h"
#include "network.h"

#include <yaml-cpp/yaml.h>

#include <QtQml>
#include <QApplication>
#include <QAbstractEventDispatcher>

#define FRONTPAGE_ROW_COUNT 3

QList<QString> load_list_from_file(const QString &filepath);
QList<QString> get_sections_urls();
QList<QString> get_images_urls();
QString yml_get(const YAML::Node &node, const QString &key);

ReleaseManager::ReleaseManager(QObject *parent)
: QObject(parent)
, m_downloadingMetadata(true)
{
    qDebug() << this->metaObject()->className() << "construction";

    sourceModel = new ReleaseModel(this);
    filterModel = new ReleaseFilterModel(sourceModel, this);

    // Add custom release to first position
    auto customRelease = Release::custom(this);
    addReleaseToModel(0, customRelease);
    setSelectedIndex(0);

    QTimer::singleShot(0, this, &ReleaseManager::downloadMetadata);
}

void ReleaseManager::downloadMetadata() {
    qDebug() << "Downloading metadata";
    
    setDownloadingMetadata(true);

    const QList<QString> section_urls = get_sections_urls();
    qDebug() << "section_urls = " << section_urls;

    const QList<QString> image_urls = get_images_urls();
    qDebug() << "image_urls = " << image_urls;

    // Create requests to download all release files and
    // collect the replies
    const QHash<QString, QNetworkReply *> replies =
    [section_urls, image_urls]() {
        QHash<QString, QNetworkReply *> out;
        const QList<QString> all_urls = section_urls + image_urls;

        for (const auto url : all_urls) {
            QNetworkReply *reply = makeNetworkRequest(url, 5000);

            out[url] = reply;
        }

        return out;
    }();

    // This will run when all the replies are finished (or
    // technically, the last one)
    const auto onReplyFinished =
    [this, replies, section_urls, image_urls]() {
        // Only proceed if this is the last reply
        for (auto reply : replies) {
            if (!reply->isFinished()) {
                return;
            }
        }

        // Check that all replies suceeded
        // If not, retry
        for (auto reply : replies.values()) {
            // NOTE: ignore ContentNotFoundError since it can happen if one of the files was moved or renamed
            const QNetworkReply::NetworkError error = reply->error();
            const bool download_failed = (error != QNetworkReply::NoError && error != QNetworkReply::ContentNotFoundError);

            if (download_failed) {
                qWarning() << "Failed to download metadata:" << reply->errorString() << reply->error() << "Retrying in 10 seconds.";
                QTimer::singleShot(10000, this, &ReleaseManager::downloadMetadata);

                return;
            }
        }

        qDebug() << "Downloaded metadata, loading it";

        // Collect results
        QHash<QString, QString> url_to_file;

        for (auto url : replies.keys()) {
            QNetworkReply *reply = replies[url];

            if (reply->error() == QNetworkReply::NoError) {
                const QByteArray bytes = reply->readAll();
                url_to_file[url] = QString(bytes);
            } else {
                qWarning() << "Failed to download metadata from" << url;
                qWarning() << "Error:" << reply->error();
            }
        }

        const QList<QString> sectionsFiles =
        [section_urls, url_to_file]() {
            QList<QString> out;
            for (const auto section_url : section_urls) {
                const QString section = url_to_file[section_url];
                out.append(section);
            }
            return out;
        }();

        loadReleases(sectionsFiles);

        const QList<QString> imagesFiles =
        [image_urls, url_to_file]() {
            QList<QString> out;
            for (const auto image_url : image_urls) {
                const QString image = url_to_file[image_url];
                out.append(image);
            }
            return out;
        }();

        for (auto imagesFile : imagesFiles) {
            loadVariants(imagesFile);
        }

        for (auto reply : replies.values()) {
            reply->deleteLater();
        }

        setDownloadingMetadata(false);
    };

    for (const auto reply : replies) {
        connect(
            reply, &QNetworkReply::finished,
            onReplyFinished);
    }
}

void ReleaseManager::setDownloadingMetadata(const bool value) {
    m_downloadingMetadata = value;
    emit downloadingMetadataChanged();
}

bool ReleaseManager::downloadingMetadata() const {
    return m_downloadingMetadata;
}

Release *ReleaseManager::selected() const {
    return sourceModel->get(m_selectedIndex);
}

int ReleaseManager::selectedIndex() const {
    return m_selectedIndex;
}

void ReleaseManager::setSelectedIndex(int o) {
    if (m_selectedIndex != o) {
        m_selectedIndex = o;
        emit selectedChanged();
    }
}

ReleaseFilterModel *ReleaseManager::getFilterModel() const {
    return filterModel;
}

void ReleaseManager::loadVariants(const QString &variantsFile) {
    YAML::Node variants = YAML::Load(variantsFile.toStdString());

    if (!variants["entries"]) {
        return;
    }

    for (auto variantData : variants["entries"]) {
        const QString url = yml_get(variantData, "link");
        if (url.isEmpty()) {
            qDebug() << "Invalid url for" << url;
            continue;
        }

        const QString name = yml_get(variantData, "solution");
        if (name.isEmpty()) {
            qDebug() << "Invalid name for" << url;
            continue;
        }

        const QString arch_string = yml_get(variantData, "arch");
        const Architecture arch =
        [arch_string, url]() -> Architecture  {
            if (!arch_string.isEmpty()) {
                return architecture_from_string(arch_string);
            } else {
                return architecture_from_filename(url);
            }
        }();
        if (arch == Architecture_UNKNOWN) {
            qDebug() << "Variant has unknown architecture" << arch_string;
            continue;
        }

        // NOTE: yml file doesn't define "board" for pc32/pc64, so default to "PC"
        const QString board =
        [variantData]() -> QString {
            const QString out = yml_get(variantData, "board");
            if (!out.isEmpty()) {
                return out;
            } else {
                return "PC";
            }
        }();

        FileType *fileType = FileType::fromFilename(url);
        if (!fileType->isValid()) {
            qDebug() << "Invalid image type for" << url;
            continue;
        }

        const bool live =
        [variantData]() {
            const QString live_string = yml_get(variantData, "live");
            if (!live_string.isEmpty()) {
                return (live_string == "1");
            } else {
                return false;
            }
        }();

        qDebug() << "Loading variant:" << name << architecture_name(arch) << board << fileType->name() << QUrl(url).fileName();

        // Find a release that has the same name as this variant
        Release *release =
        [this, name]() -> Release *{
            for (int i = 0; i < sourceModel->rowCount(); i++) {
                Release *release = sourceModel->get(i);

                if (release->name() == name) {
                    return release;
                }
            }
            return nullptr;
        }();

        if (release != nullptr) {
            Variant *variant = new Variant(url, arch, fileType, board, live, this);
            release->addVariant(variant);
        } else {
            qWarning() << "Failed to find a release for this variant!";
        }
    }
}

QStringList ReleaseManager::architectures() const {
    QStringList out;
    for (const Architecture architecture : architecture_all()) {
        if (architecture != Architecture_UNKNOWN) {
            const QString name = architecture_name(architecture);
            out.append(name);
        }
    }
    return out;
}

QStringList ReleaseManager::fileTypeFilters() const {
    const QList<FileType *> fileTypes = FileType::all();

    QStringList filters;
    for (const auto type : fileTypes) {
        const QString extensions =
        [type]() {
            const QStringList extension = type->extension();
            if (extension.isEmpty()) {
                return QString();
            }

            QString out;
            out += "(";

            for (const auto e : extension) {
                if (extension.indexOf(e) > 0) {
                    out += " ";
                }

                out += "*." + e;
            }

            out += ")";

            return out;
        }();

        if (extensions.isEmpty()) {
            continue;
        }

        const QString name = type->name();
        
        const QString filter = name + " " + extensions;
        filters.append(filter);
    }

    filters.append(tr("All files") + " (*)");

    return filters;
}

void ReleaseManager::loadReleases(const QList<QString> &sectionsFiles) {
    qDebug() << "Loading releases";

    for (auto sectionFile : sectionsFiles) {
        const YAML::Node section = YAML::Load(sectionFile.toStdString());

        if (!section["members"]) {
            continue;
        }
        
        for (unsigned int i = 0; i < section["members"].size(); i++) {
            const YAML::Node releaseData = section["members"][i];

            const QString name = yml_get(releaseData, "code");

            qDebug() << "Loading release: " << name;

            const QString language =
            []() {
                if (QLocale().language() == QLocale::Russian) {
                    return "_ru";
                } else {
                    return "_en";
                }
            }();
            
            const QString display_name = yml_get(releaseData, "name" + language);
            const QString summary = yml_get(releaseData, "descr" + language);
            const QString description = yml_get(releaseData, "descr_full" + language);

            // NOTE: currently no screenshots
            const QStringList screenshots;
            
            // Check that icon file exists
            const QString icon_name = yml_get(releaseData, "img");
            const QString icon_path_test = ":/logo/" + icon_name;
            const QFile icon_file(icon_path_test);
            if (!icon_file.exists()) {
                qWarning() << "Failed to find icon file at " << icon_path_test << " needed for release " << name;
            }

            // NOTE: icon_path is consumed by QML, so it needs to begin with "qrc:/" not ":/"
            const QString icon_path = "qrc" + icon_path_test;

            const auto release = new Release(name, display_name, summary, description, icon_path, screenshots, this);

            // Reorder releases because default order in
            // sections files is not good. Try to put
            // workstation first after custom release and
            // server second, so that they are both on the
            // frontpage.
            const int index =
            [this, release]() {
                const QString release_name = release->name();
                const bool is_workstation = (release_name == "alt-workstation");
                const bool is_server = (release_name == "alt-server");

                if (is_workstation) {
                    return 1;
                } else if (is_server) {
                    return 2;
                } else {
                    return sourceModel->rowCount();
                }
            }();
            
            addReleaseToModel(index, release);
        }
    }

    filterModel->invalidateCustom();
}

void ReleaseManager::addReleaseToModel(const int index, Release *release) {
    QVariant variant;
    variant.setValue(release);

    QStandardItem *item = new QStandardItem();
    item->setData(variant);

    sourceModel->insertRow(index, item);
}

QList<QString> load_list_from_file(const QString &filepath) {
    QFile file(filepath);

    const bool open_success = file.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!open_success) {
        qWarning() << "Failed to open" << filepath;
        return QList<QString>();
    }

    const QString contents = file.readAll();
    QList<QString> list = contents.split("\n");
    list.removeAll(QString());

    return list;
}

QList<QString> get_sections_urls() {
    static QList<QString> sections_urls = load_list_from_file(":/sections_urls.txt");

    return sections_urls;
}

QList<QString> get_images_urls() {
    static QList<QString> images_urls = load_list_from_file(":/images_urls.txt");

    return images_urls;
}

QString yml_get(const YAML::Node &node, const QString &key) {
    const std::string key_std = key.toStdString();
    const bool node_has_key = node[key_std];

    if (node_has_key) {
        const YAML::Node value_yml = node[key_std];
        const std::string value_std = value_yml.as<std::string>();
        QString value = QString::fromStdString(value_std);

        // Remove HTML character entities that don't render in Qt
        value.replace("&colon;", ":");
        value.replace("&nbsp;", " ");
        // Remove newlines because text will have wordwrap
        value.replace("\n", " ");

        return value;
    } else {
        return QString();
    }
}

Release *ReleaseModel::get(const int index) const {
    QStandardItem *the_item = item(index);
    
    if (the_item != nullptr) {
        QVariant variant = the_item->data();
        Release *release = variant.value<Release *>();

        return release;
    } else {
        return nullptr;
    }
}

// NOTE: this is a very roundabout way of making the Release
// pointer stored inside the item available in the qml
// delegate as "release". (Qt::UserRole + 1 is the default
// data role)
QHash<int, QByteArray> ReleaseModel::roleNames() const {
    static const QHash<int, QByteArray> names = {
        {Qt::UserRole + 1, "release"}
    };

    return names;
}

ReleaseFilterModel::ReleaseFilterModel(ReleaseModel *model_arg, QObject *parent)
: QSortFilterProxyModel(parent)
{
    model = model_arg;
    frontPage = true;
    filterArch = Architecture_ALL;

    setSourceModel(model_arg);
}

bool ReleaseFilterModel::filterAcceptsRow(int source_row, const QModelIndex &) const {

    Release *release = model->get(source_row);
    if (release == nullptr) {
        return false;
    }
    
    if (frontPage) {
        // Don't filter when on front page, just show 3 releases
        const bool on_front_page = (source_row < FRONTPAGE_ROW_COUNT);

        return on_front_page;
    } else if (release->isCustom()) {
        // Always show local release
        return true;
    } else {
        const bool releaseMatchesName = release->displayName().contains(filterText, Qt::CaseInsensitive);

        // Exit early if don't match name to skip checking
        // for arch because that takes a long time
        // TODO: cache that somehow?
        if (!releaseMatchesName) {
            return false;
        }

        // Otherwise filter by arch
        const bool releaseHasVariantWithArch =
        [this, release]() {
            // If filtering for all, accept all architectures
            if (filterArch == Architecture_ALL) {
                return true;
            }

            for (auto variant : release->variantList()) {
                if (variant->arch() == filterArch) {
                    return true;
                }
            }
            return false;
        }();

        if (!releaseHasVariantWithArch) {
            return false;
        }

        return true;
    }
}

bool ReleaseFilterModel::getFrontPage() const {
    return frontPage;
}

void ReleaseFilterModel::leaveFrontPage() {
    frontPage = false;

    invalidateFilter();

    emit frontPageChanged();
}

void ReleaseFilterModel::setFilterText(const QString &text) {
    filterText = text;

    invalidateFilter();
}

void ReleaseFilterModel::setFilterArch(const int index) {
    filterArch = (Architecture) index;
    invalidateFilter();
}

void ReleaseFilterModel::invalidateCustom() {
    invalidateFilter();
}
