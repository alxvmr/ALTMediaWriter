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

#include "releasemanager.h"
#include "architecture.h"
#include "platform.h"
#include "file_type.h"
#include "network.h"
#include "release.h"
#include "release_model.h"
#include "variant.h"

#include <yaml-cpp/yaml.h>

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QtQml>

const QString METADATA_URLS_HOST = "http://getalt.org";
const QString METADATA_URLS_BACKUP_HOST = "http://kvel2d.github.io/posts";

QList<QString> load_list_from_file(const QString &filepath);
QString yml_get(const YAML::Node &node, const QString &key);
QList<QString> get_metadata_urls_list(const QString &host);

ReleaseManager::ReleaseManager(QObject *parent)
: QObject(parent) {
    m_downloadingMetadata = true;
    metadata_urls_reply_group = nullptr;
    metadata_urls_backup_reply_group = nullptr;
    metadata_reply_group = nullptr;
    md5sum_reply_group = nullptr;

    qDebug() << this->metaObject()->className() << "construction";

    sourceModel = new ReleaseModel(this);
    filterModel = new ReleaseFilterModel(sourceModel, this);

    // Add custom release to first position
    Release *customRelease = Release::custom(this);
    addReleaseToModel(0, customRelease);
    setSelectedIndex(0);

    QTimer::singleShot(0, this, &ReleaseManager::downloadMetadataUrls);
}

void ReleaseManager::downloadMetadataUrls() {
    qDebug() << "Downloading metadata urls";

    setDownloadingMetadata(true);

    const QList<QString> url_list = get_metadata_urls_list(METADATA_URLS_HOST);

    metadata_urls_reply_group = new NetworkReplyGroup(url_list, this);

    connect(
        metadata_urls_reply_group, &NetworkReplyGroup::finished,
        this, &ReleaseManager::onMetadataUrlsDownloaded);
}

// TODO: remove usage of backup when metadata urls
// start getting hosted on getalt
//
// NOTE: code duplication for downloading metadata urls
// is ok because this code is temporary and will be
// completely removed in the future
void ReleaseManager::onMetadataUrlsDownloaded() {
    const QHash<QString, QNetworkReply *> replies = metadata_urls_reply_group->get_reply_list();
    
    // Check that all replies suceeded
    // If not, retry
    for (const QNetworkReply *reply : replies.values()) {
        // NOTE: ignore ContentNotFoundError for
        // metadata since it can happen if one of
        // the files was moved or renamed. In that
        // case it's fine to process other
        // downloads and ignore this failed one.
        const QNetworkReply::NetworkError error = reply->error();
        const bool download_failed = (error != QNetworkReply::NoError);

        // Attempt to download from backup host if this
        // one fails
        // 
        // TODO: when usage of backup is removed, fail
        // and restart here
        if (download_failed) {
            qDebug() << "Failed to download metadata urls:" << reply->errorString() << reply->error() << "Downloading from backup";
            QTimer::singleShot(100, this, &ReleaseManager::downloadMetadataUrlsBackup);

            delete metadata_urls_reply_group;
            metadata_urls_reply_group = nullptr;

            return;
        }
    }

    qDebug() << "Downloaded metadata urls";
    qDebug() << "Saving metadata urls";

    // Collect results
    QHash<QString, QList<QString>> url_to_data;

    for (const QString &url : replies.keys()) {
        QNetworkReply *reply = replies[url];

        if (reply->error() == QNetworkReply::NoError) {
            url_to_data[url] = [&]() {
                const QByteArray bytes = reply->readAll();
                const QString string = QString(bytes);
                QList<QString> out = string.split("\n");
                // Remove last empty line, if there's one
                out.removeAll("");

                return out;
            }();
        } else {
            qDebug() << "Failed to download metadata from" << url;
            qDebug() << "Error:" << reply->error();
        }
    }

    const QList<QString> url_list = get_metadata_urls_list(METADATA_URLS_HOST);
    const QString section_metadata_url = url_list[0];
    const QString image_metadata_url = url_list[1];
    
    if (url_to_data.contains(section_metadata_url)) {
        section_urls = url_to_data[section_metadata_url];
    }
    if (url_to_data.contains(image_metadata_url)) {
        image_urls = url_to_data[image_metadata_url];
    }

    const bool success = (!section_urls.isEmpty() && !image_urls.isEmpty());

    delete metadata_urls_reply_group;
    metadata_urls_reply_group = nullptr;

    if (success) {
        qDebug() << "Processed metadata urls";
        qDebug() << "section_urls = " << section_urls;
        qDebug() << "image_urls = " << image_urls;

        downloadMetadata();
    } else {
        qDebug() << "Metadata urls are invalid or empty";
    }
}

void ReleaseManager::downloadMetadataUrlsBackup() {
    qDebug() << "Downloading metadata urls backup";

    const QList<QString> url_list = get_metadata_urls_list(METADATA_URLS_BACKUP_HOST);
    
    metadata_urls_backup_reply_group = new NetworkReplyGroup(url_list, this);

    connect(
        metadata_urls_backup_reply_group, &NetworkReplyGroup::finished,
        this, &ReleaseManager::onMetadataUrlsBackupDownloaded);
}

void ReleaseManager::onMetadataUrlsBackupDownloaded() {
    const QHash<QString, QNetworkReply *> replies = metadata_urls_backup_reply_group->get_reply_list();
    
    // Check that all replies suceeded
    // If not, retry
    for (const QNetworkReply *reply : replies.values()) {
        // NOTE: ignore ContentNotFoundError for
        // metadata since it can happen if one of
        // the files was moved or renamed. In that
        // case it's fine to process other
        // downloads and ignore this failed one.
        const QNetworkReply::NetworkError error = reply->error();
        const bool download_failed = (error != QNetworkReply::NoError);

        if (download_failed) {
            qDebug() << "Failed to download metadata urls:" << reply->errorString() << reply->error() << "Retrying in 10 seconds.";
            QTimer::singleShot(10000, this, &ReleaseManager::downloadMetadataUrls);

            delete metadata_urls_backup_reply_group;
            metadata_urls_backup_reply_group = nullptr;

            return;
        }
    }

    qDebug() << "Downloaded metadata urls";
    qDebug() << "Saving metadata urls";

    // Collect results
    QHash<QString, QList<QString>> url_to_data;

    for (const QString &url : replies.keys()) {
        QNetworkReply *reply = replies[url];

        if (reply->error() == QNetworkReply::NoError) {
            url_to_data[url] = [&]() {
                const QByteArray bytes = reply->readAll();
                const QString string = QString(bytes);
                QList<QString> out = string.split("\n");
                out.removeAll("");

                return out;
            }();
        } else {
            qDebug() << "Failed to download metadata from" << url;
            qDebug() << "Error:" << reply->error();
        }
    }

    const QList<QString> url_list = get_metadata_urls_list(METADATA_URLS_BACKUP_HOST);
    const QString section_metadata_url = url_list[0];
    const QString image_metadata_url = url_list[1];
    
    if (url_to_data.contains(section_metadata_url)) {
        section_urls = url_to_data[section_metadata_url];
    }
    if (url_to_data.contains(image_metadata_url)) {
        image_urls = url_to_data[image_metadata_url];
    }

    const bool success = (!section_urls.isEmpty() && !image_urls.isEmpty());

    delete metadata_urls_backup_reply_group;
    metadata_urls_backup_reply_group = nullptr;

    if (success) {
        qDebug() << "Processed metadata urls";
        qDebug() << "section_urls = " << section_urls;
        qDebug() << "image_urls = " << image_urls;

        downloadMetadata();
    } else {
        qDebug() << "Metadata urls are invalid or empty";
    }
}

void ReleaseManager::downloadMetadata() {
    qDebug() << "Downloading metadata";

    const QList<QString> all_urls = section_urls + image_urls;

    metadata_reply_group = new NetworkReplyGroup(all_urls, this);

    connect(
        metadata_reply_group, &NetworkReplyGroup::finished,
        this, &ReleaseManager::onMetadataDownloaded);
}

void ReleaseManager::onMetadataDownloaded() {
    const QHash<QString, QNetworkReply *> replies = metadata_reply_group->get_reply_list();
    
    // Check that all replies suceeded
    // If not, retry
    for (const QNetworkReply *reply : replies.values()) {
        // NOTE: ignore ContentNotFoundError for
        // metadata since it can happen if one of
        // the files was moved or renamed. In that
        // case it's fine to process other
        // downloads and ignore this failed one.
        const QNetworkReply::NetworkError error = reply->error();
        const bool download_failed = (error != QNetworkReply::NoError && error != QNetworkReply::ContentNotFoundError);

        if (download_failed) {
            qDebug() << "Failed to download metadata:" << reply->errorString() << reply->error() << "Retrying in 10 seconds.";
            QTimer::singleShot(10000, this, &ReleaseManager::downloadMetadata);

            delete metadata_reply_group;
            metadata_reply_group = nullptr;

            return;
        }
    }

    qDebug() << "Downloaded metadata, loading it";

    // Collect results
    QHash<QString, QString> url_to_file;

    for (const QString &url : replies.keys()) {
        QNetworkReply *reply = replies[url];

        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray bytes = reply->readAll();
            url_to_file[url] = QString(bytes);
        } else {
            qDebug() << "Failed to download metadata from" << url;
            qDebug() << "Error:" << reply->error();
        }
    }

    const QList<QString> sectionsFiles = [&]() {
        QList<QString> out;

        for (const QString &section_url : section_urls) {
            const QString section = url_to_file[section_url];
            out.append(section);
        }
        return out;
    }();

    qDebug() << "Loading releases";

    loadReleases(sectionsFiles);

    imagesFiles = [&]() {
        QList<QString> out;

        for (const QString &image_url : image_urls) {
            const QString image = url_to_file[image_url];
            out.append(image);
        }
        return out;
    }();

    // NOTE: images/variants are loaded later after
    // md5sum is downloaded

    delete metadata_reply_group;
    metadata_reply_group = nullptr;

    const QList<QString> md5sum_url_list = [&]() {
        const QList<QString> image_url_list = [&]() {
            QList<QString> out;

            for (const QString &imagesFile : imagesFiles) {
                YAML::Node variants = YAML::Load(imagesFile.toStdString());

                if (!variants["entries"]) {
                    continue;
                }

                for (const YAML::Node &variantData : variants["entries"]) {
                    const QString url = yml_get(variantData, "link");
                    out.append(url);
                }
            }

            return out;
        }();

        const QList<QString> out = [&]() {
            // NOTE: using set because there will be
            // duplicates due to there being multiple
            // images per folder
            QSet<QString> out_set;

            for (const QString &image_url : image_url_list) {
                // TODO: duplicating code in
                // image_download.cpp
                const QString md5sum_url = QUrl(image_url).adjusted(QUrl::RemoveFilename).toString() + "/MD5SUM";

                out_set.insert(md5sum_url);
            }

            const QList<QString> out = out_set.toList();

            return out;
        }();

        return out;
    }();

    downloadMD5SUM(md5sum_url_list);
}

void ReleaseManager::downloadMD5SUM(const QList<QString> &md5sum_url_list) {
    qDebug() << "Downloading MD5SUM's";

    md5sum_reply_group = new NetworkReplyGroup(md5sum_url_list, this);

    connect(
        md5sum_reply_group, &NetworkReplyGroup::finished,
        this, &ReleaseManager::onMD5SUMDownloaded);
}

void ReleaseManager::onMD5SUMDownloaded() {
    const QHash<QString, QNetworkReply *> replies = md5sum_reply_group->get_reply_list();

    // Check that all replies suceeded
    // If not, retry
    // TODO: duplicating code
    for (const QNetworkReply *reply : replies.values()) {
        // NOTE: ignore ContentNotFoundError for
        // metadata since it can happen if one of
        // the files was moved or renamed. In that
        // case it's fine to process other
        // downloads and ignore this failed one.
        const QNetworkReply::NetworkError error = reply->error();
        const bool download_failed = (error != QNetworkReply::NoError && error != QNetworkReply::ContentNotFoundError);

        if (download_failed) {
            qDebug() << "Failed to download md5sum:" << reply->errorString() << reply->error() << "Retrying in 10 seconds.";
            QTimer::singleShot(10000, this, &ReleaseManager::downloadMetadata);

            delete md5sum_reply_group;
            md5sum_reply_group = nullptr;

            return;
        }
    }

    qDebug() << "Downloaded md5sum, loading it";

    const QList<QString> md5sum_file_list = [&]() {
        QList<QString> out;

        for (const QString &url : replies.keys()) {
            QNetworkReply *reply = replies[url];

            if (reply->error() == QNetworkReply::NoError) {
                const QByteArray bytes = reply->readAll();
                const QString string = QString(bytes);
                out.append(string);
            } else {
                qDebug() << "Failed to download metadata from" << url;
                qDebug() << "Error:" << reply->error();
            }
        }

        return out;
    }();

    const QHash<QString, QString> md5sum_map = [&]() {
        QHash<QString, QString> out;

        for (const QString &file : md5sum_file_list) {
            const QList<QString> line_list = file.split("\n");

            // MD5SUM is of the form "sum image \n sum
            // image \n ..."
            for (const QString &line : line_list) {
                const QList<QString> elements = line.split(QRegExp("\\s+"));

                if (elements.size() != 2) {
                    continue;
                }

                const QString md5sum = elements[0];
                const QString filename = elements[1];

                out[filename] = md5sum;
            }
        }

        return out;
    }();

    qDebug() << "Loading variants";

    for (const QString &imagesFile : imagesFiles) {
        loadVariants(imagesFile, md5sum_map);
    }

    delete md5sum_reply_group;
    md5sum_reply_group = nullptr;

    setDownloadingMetadata(false);
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

// NOTE: index arg refers to proxy, so have to convert
// it to source
void ReleaseManager::setSelectedIndex(const int index) {
    const QModelIndex index_proxy = filterModel->index(index, 0);
    const QModelIndex index_source = filterModel->mapToSource(index_proxy);

    const int row_source = index_source.row();

    if (m_selectedIndex != row_source) {
        m_selectedIndex = row_source;
        emit selectedChanged();
    }
}

ReleaseFilterModel *ReleaseManager::getFilterModel() const {
    return filterModel;
}

void ReleaseManager::loadVariants(const QString &variantsFile, const QHash<QString, QString> &md5sum_map) {
    YAML::Node variants = YAML::Load(variantsFile.toStdString());

    if (!variants["entries"]) {
        return;
    }

    for (const YAML::Node &variantData : variants["entries"]) {
        const QString url = yml_get(variantData, "link");
        if (url.isEmpty()) {
            qDebug() << "Variant has no url";
            continue;
        }

        const QString releaseName = yml_get(variantData, "solution");
        if (releaseName.isEmpty()) {
            qDebug() << "Variant has no releaseName" << url;
            continue;
        }

        const QString arch_string = yml_get(variantData, "arch");
        const Architecture arch = [arch_string, url]() -> Architecture {
            if (!arch_string.isEmpty()) {
                return architecture_from_string(arch_string);
            } else {
                return architecture_from_filename(url);
            }
        }();
        if (arch == Architecture_UNKNOWN) {
            qDebug() << "Variant has unknown architecture" << arch_string << url;
            continue;
        }

        const QString platform_string = yml_get(variantData, "platform");
        const Platform platform = [platform_string, url]() -> Platform {
            if (!platform_string.isEmpty()) {
                return platform_from_string(platform_string);
            } else {
                return Platform_UNKNOWN;
            }
        }();
        if (platform == Platform_UNKNOWN) {
            qDebug() << "Variant has unknown platform" << platform << url;
            continue;
        }

        // NOTE: yml file doesn't define "board" for pc32/pc64, so default to "PC"
        const QString board = [variantData]() -> QString {
            const QString out = yml_get(variantData, "board");
            if (!out.isEmpty()) {
                return out;
            } else {
                return "PC";
            }
        }();

        const FileType fileType = file_type_from_filename(url);
        if (fileType == FileType_UNKNOWN) {
            qDebug() << "Variant has unknown file type" << url;
            continue;
        }

        const bool live = [variantData]() {
            const QString live_string = yml_get(variantData, "live");
            if (!live_string.isEmpty()) {
                return (live_string == "1");
            } else {
                return false;
            }
        }();

        const QString md5sum = [&]() {
            const QString filename = QUrl(url).fileName();
            const QString out = md5sum_map[filename];

            return out;
        }();

        // qDebug() << QUrl(url).fileName() << releaseName << architecture_name(arch) << board << file_type_name(fileType) << (live ? "LIVE" : "");

        // Find a release that has the same name as this variant
        Release *release = [this, releaseName]() -> Release * {
            for (int i = 0; i < sourceModel->rowCount(); i++) {
                Release *release = sourceModel->get(i);

                if (release->name() == releaseName) {
                    return release;
                }
            }
            return nullptr;
        }();

        if (release != nullptr) {
            Variant *variant = new Variant(url, arch, platform, fileType, board, live, md5sum, this);
            release->addVariant(variant);
        } else {
            qDebug() << "Failed to find a release for this variant!" << url;
        }
    }
}

QStringList ReleaseManager::architectures() const {
    QStringList out;
    for (const Architecture &architecture : architecture_all) {
        if (architecture != Architecture_UNKNOWN) {
            const QString name = architecture_name(architecture);
            out.append(name);
        }
    }
    return out;
}

QStringList ReleaseManager::fileTypeFilters() const {
    const QList<FileType> fileTypes = file_type_all;

    QStringList filters;
    for (const FileType &type : fileTypes) {
        const QString extensions = [type]() {
            const QStringList strings = file_type_strings(type);
            if (strings.isEmpty()) {
                return QString();
            }

            QString out;
            out += "(";

            for (const QString &e : strings) {
                if (strings.indexOf(e) > 0) {
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

        const QString name = file_type_name(type);

        const QString filter = name + " " + extensions;
        filters.append(filter);
    }

    filters.append(tr("All files") + " (*)");

    return filters;
}

void ReleaseManager::loadReleases(const QList<QString> &sectionsFiles) {
    for (const QString &sectionFile : sectionsFiles) {
        const YAML::Node section = YAML::Load(sectionFile.toStdString());

        if (!section["members"]) {
            continue;
        }

        for (unsigned int i = 0; i < section["members"].size(); i++) {
            const YAML::Node releaseData = section["members"][i];

            const QString name = yml_get(releaseData, "code");
            if (name.isEmpty()) {
                qDebug() << "Release has no name";
                continue;
            }

            const QString language = []() {
                if (QLocale().language() == QLocale::Russian) {
                    return "_ru";
                } else {
                    return "_en";
                }
            }();

            const QString display_name = yml_get(releaseData, "name" + language);
            if (display_name.isEmpty()) {
                qDebug() << "Release has no display name";
                continue;
            }

            const QString summary = yml_get(releaseData, "descr" + language);
            if (summary.isEmpty()) {
                qDebug() << "Release has no summary";
                continue;
            }

            const QString description = yml_get(releaseData, "descr_full" + language);
            if (description.isEmpty()) {
                qDebug() << "Release has no description";
                continue;
            }

            // NOTE: currently no screenshots
            const QStringList screenshots;

            // Check that icon file exists
            const QString icon_name = yml_get(releaseData, "img");
            if (icon_name.isEmpty()) {
                qDebug() << "Release has no icon";
                continue;
            }

            const QString icon_path_test = ":/logo/" + icon_name;
            const QFile icon_file(icon_path_test);
            if (!icon_file.exists()) {
                qDebug() << "Failed to find icon file at " << icon_path_test << " needed for release " << name;
                continue;
            }

            // NOTE: icon_path is consumed by QML, so it needs to begin with "qrc:/" not ":/"
            const QString icon_path = "qrc" + icon_path_test;

            const auto release = new Release(name, display_name, summary, description, icon_path, screenshots, this);

            // Reorder releases because default order in
            // sections files is not good. Try to put
            // workstation first after custom release and
            // server second, so that they are both on the
            // frontpage.
            const int index = [this, release]() {
                const QString release_name = release->name();
                const bool is_workstation = (release_name == "alt-workstation");
                const bool is_kworkstation = (release_name == "alt-kworkstation");

                if (is_workstation) {
                    return 1;
                } else if (is_kworkstation) {
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
        qDebug() << "Failed to open" << filepath;
        return QList<QString>();
    }

    const QString contents = file.readAll();
    QList<QString> list = contents.split("\n");
    list.removeAll(QString());

    return list;
}

QString yml_get(const YAML::Node &node, const QString &key) {
    const std::string key_std = key.toStdString();
    const YAML::Node value_yml = node[key_std];
    const std::string fallback = std::string();
    const std::string value_std = value_yml.as<std::string>(fallback);
    QString value = QString::fromStdString(value_std);

    // Remove HTML character entities that don't render in Qt
    value.replace("&colon;", ":");
    value.replace("&nbsp;", " ");
    // Remove newlines because text will have wordwrap
    value.replace("\n", " ");

    return value;
}

// TODO: this f-n might become unneeded when usage of
// backup host is removed
QList<QString> get_metadata_urls_list(const QString &host) {
    const QString SECTION_URL_LIST_FILENAME = "altmediawriter_section_url_list.txt";
    const QString IMAGE_URL_LIST_FILENAME = "altmediawriter_image_url_list.txt";
        
    const QList<QString> out = {
        QString("%1/%2").arg(host, SECTION_URL_LIST_FILENAME),
        QString("%1/%2").arg(host, IMAGE_URL_LIST_FILENAME),
    };

    return out;
}
