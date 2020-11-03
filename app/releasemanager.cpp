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
#include "image_type.h"
#include "architecture.h"
#include "variant.h"
#include "network.h"
#include "progress.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

#include <QtQml>
#include <QApplication>
#include <QAbstractEventDispatcher>

#define GETALT_IMAGES_URL "http://getalt.org/_data/images/"
#define GETALT_SECTIONS_URL "http://getalt.org/_data/sections/"
#define FRONTPAGE_ROW_COUNT 3

QString readFile(const QString &filename) {
    QFile file(filename);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qDebug() << "readFile(): Failed to open file " << filename;
        return "";
    }
    QTextStream fileStream(&file);
    // NOTE: set codec manually, default codec is no good for cyrillic
    fileStream.setCodec("UTF8");
    QString str = fileStream.readAll();
    file.close();
    return str;
}

QList<QString> imagesFilenames() {
    const QDir dir(":/images");
    const QList<QString> files = dir.entryList();
    return files;
}

QList<QString> sectionsFilenames() {
    const QDir dir(":/sections");
    const QList<QString> files = dir.entryList();
    return files;
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

ReleaseManager::ReleaseManager(QObject *parent)
: QSortFilterProxyModel(parent)
, m_sourceModel(new ReleaseListModel(this))
{
    setSourceModel(m_sourceModel);

    setSelectedIndex(0);

    qDebug() << this->metaObject()->className() << "construction";

    qmlRegisterUncreatableType<Release>("MediaWriter", 1, 0, "Release", "");
    qmlRegisterUncreatableType<Variant>("MediaWriter", 1, 0, "Variant", "");
    qmlRegisterUncreatableType<Architecture>("MediaWriter", 1, 0, "Architecture", "");
    qmlRegisterUncreatableType<ImageType>("MediaWriter", 1, 0, "ImageType", "");
    qmlRegisterUncreatableType<Progress>("MediaWriter", 1, 0, "Progress", "");

    // Download releases from getalt.org
    QTimer::singleShot(0, this, &ReleaseManager::downloadMetadata);
}

bool ReleaseManager::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    Q_UNUSED(source_parent)
    auto release = get(source_row);
    
    if (m_frontPage) {
        // Don't filter when on front page, just show 3 releases
        const bool on_front_page = (source_row < FRONTPAGE_ROW_COUNT);
        return on_front_page;
    } else if (release->isCustom()) {
        // Always show local release
        return true;
    } else {
        // Otherwise filter by arch
        const bool releaseHasVariantWithArch =
        [this, release]() {
            for (auto variant : release->variantList()) {
                if (variant->arch()->index() == m_filterArchitecture) {
                    return true;
                }
            }
            return false;
        }();

        return releaseHasVariantWithArch;
    }
}

Release *ReleaseManager::get(int index) const {
    return m_sourceModel->get(index);
}

void ReleaseManager::downloadMetadata() {
    qDebug() << "Downloading metadata";
    
    setBeingUpdated(true);

    const QList<QString> section_urls =
    []() {
        QList<QString> out;
        for (const auto sectionFile : sectionsFilenames()) {
            const QString url = GETALT_SECTIONS_URL + sectionFile;
            out.append(url);
            qDebug() << "Section url:" << url;
        }
        return out;
    }();

    const QList<QString> image_urls =
    []() {
        QList<QString> out;

        for (const auto imageFile : imagesFilenames()) {
            const QString url = GETALT_IMAGES_URL + imageFile;
            out.append(url);
            qDebug() << "Image url:" << url;
        }

        return out;
    }();

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

            const QString contents =
            [reply, url, image_urls, section_urls]() {
                if (reply->bytesAvailable() > 0) {
                    const QByteArray bytes = reply->readAll();

                    return QString(bytes);
                } else {
                    // If file failed to download for whatever reason, load built-in metadata
                    const QString file_name = QUrl(url).fileName();
                    const QString type_string =
                    [url, image_urls, section_urls]() {
                        if (image_urls.contains(url)) {
                            return "images";
                        } else {
                            return "sections";
                        }
                    }();

                    qDebug() << "Failed to download metadata from" << url;
                    qDebug() << "Using built-in version instead";

                    const QString builtin_file_path = QString(":/%1/%2").arg(type_string, file_name);
                    return readFile(builtin_file_path);
                }
            }();

            url_to_file[url] = contents;
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

        m_sourceModel->loadReleases(sectionsFiles);

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

        invalidateFilter();
        setBeingUpdated(false);
    };

    for (const auto reply : replies) {
        connect(
            reply, &QNetworkReply::finished,
            onReplyFinished);
    }
}

void ReleaseManager::setBeingUpdated(const bool value) {
    m_beingUpdated = value;
    emit beingUpdatedChanged();
}

bool ReleaseManager::beingUpdated() const {
    return m_beingUpdated;
}

bool ReleaseManager::frontPage() const {
    return m_frontPage;
}

void ReleaseManager::setFrontPage(bool o) {
    if (m_frontPage != o) {
        m_frontPage = o;
        emit frontPageChanged();
        invalidateFilter();
    }
}

QString ReleaseManager::filterText() const {
    return m_filterText;
}

void ReleaseManager::setFilterText(const QString &o) {
    if (m_filterText != o) {
        m_filterText = o;
        emit filterTextChanged();
        invalidateFilter();
    }
}

int ReleaseManager::filterArchitecture() const {
    return m_filterArchitecture;
}

void ReleaseManager::setFilterArchitecture(int o) {
    if (m_filterArchitecture != o && m_filterArchitecture >= 0 && m_filterArchitecture < Architecture::_ARCHCOUNT) {
        m_filterArchitecture = o;
        emit filterArchitectureChanged();

        // Select first variant with this arch
        // TODO: needed? probably something goes wrong in qml if don't do this
        for (int i = 0; i < m_sourceModel->rowCount(); i++) {
            Release *release = get(i);

            for (auto variant : release->variantList()) {
                if (variant->arch()->index() == o) {
                    const int index = release->variantList().indexOf(variant);
                    release->setSelectedVariantIndex(index);

                    break;
                }
            }
        }

        invalidateFilter();
    }
}

Release *ReleaseManager::selected() const {
    if (m_selectedIndex >= 0 && m_selectedIndex < m_sourceModel->rowCount()) {
        return m_sourceModel->get(m_selectedIndex);
    }
    return nullptr;
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

        Architecture *arch =
        [variantData, url]() -> Architecture * {
            const QString arch_abbreviation = yml_get(variantData, "arch");
            if (!arch_abbreviation.isEmpty()) {
                return Architecture::fromAbbreviation(arch_abbreviation);
            } else {
                return Architecture::fromFilename(url);
            }
        }();
        if (arch == nullptr) {
            qDebug() << "Invalid arch for" << url;
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

        ImageType *imageType = ImageType::fromFilename(url);
        if (!imageType->isValid()) {
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

        qDebug() << "Loading variant:" << name << arch->abbreviation().first() << board << imageType->abbreviation().first() << QUrl(url).fileName();

        for (int i = 0; i < m_sourceModel->rowCount(); i++) {
            Release *release = get(i);

            if (release->name() == name) {
                release->updateUrl(url, arch, imageType, board, live);
            }
        }
    }
}

QStringList ReleaseManager::architectures() const {
    return Architecture::listAllDescriptions();
}

QStringList ReleaseManager::fileNameFilters() const {
    const QList<ImageType *> imageTypes = ImageType::all();

    QStringList filters;
    for (const auto type : imageTypes) {
        const QString extensions =
        [type]() {
            const QStringList abbreviation = type->abbreviation();
            if (abbreviation.isEmpty()) {
                return QString();
            }

            QString out;
            out += "(";

            for (const auto e : abbreviation) {
                if (abbreviation.indexOf(e) > 0) {
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

QVariant ReleaseListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    Q_UNUSED(section); Q_UNUSED(orientation);

    if (role == Qt::UserRole + 1)
        return "release";

    return QVariant();
}

QHash<int, QByteArray> ReleaseListModel::roleNames() const {
    QHash<int, QByteArray> ret;
    ret.insert(Qt::UserRole + 1, "release");
    return ret;
}

int ReleaseListModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return m_releases.count();
}

QVariant ReleaseListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    if (role == Qt::UserRole + 1)
        return QVariant::fromValue(m_releases[index.row()]);

    return QVariant();
}

ReleaseListModel::ReleaseListModel(QObject *parent)
: QAbstractListModel(parent)
{
    qDebug() << "Creating ReleaseListModel";

    // Add custom release to first position
    auto customRelease = Release::custom(this);
    m_releases.append(customRelease);
}

void ReleaseListModel::loadReleases(const QList<QString> &sectionsFiles) {
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
                    return m_releases.size();
                }
            }();
            
            // NOTE: do model calls to notify parent model about changes
            beginInsertRows(QModelIndex(), index, index);
            m_releases.insert(index, release);
            endInsertRows();
        }
    }

    qDebug() << "Loaded" << (m_releases.count() - 1 ) << "releases";
}

Release *ReleaseListModel::get(int index) {
    if (index >= 0 && index < m_releases.count())
        return m_releases[index];
    return nullptr;
}
