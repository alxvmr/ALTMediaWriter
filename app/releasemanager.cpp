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

#define GETALT_IMAGES_LOCATION "http://getalt.org/_data/images/"
#define FRONTPAGE_ROW_COUNT 3

QString releaseImagesCacheDir() {
    QString appdataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    QDir dir(appdataPath);
    
    // Make path if it doesn't exist
    if (!dir.exists()) {
        dir.mkpath(appdataPath);
    }

    return appdataPath + "/";
}

QString fileToString(const QString &filename) {
    QFile file(filename);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qDebug() << "fileToString(): Failed to open file " << filename;
        return "";
    }
    QTextStream fileStream(&file);
    // NOTE: set codec manually, default codec is no good for cyrillic
    fileStream.setCodec("UTF8");
    QString str = fileStream.readAll();
    file.close();
    return str;
}

QList<QString> getReleaseImagesFiles() {
    const QDir dir(":/images");
    const QList<QString> releaseImagesFiles = dir.entryList();

    return releaseImagesFiles;
}

QString ymlToQString(const YAML::Node &yml_value) {
    const std::string value_std = yml_value.as<std::string>();
    QString out = QString::fromStdString(value_std);

    // Remove HTML character entities that don't render in Qt
    out.replace("&colon;", ":");
    out.replace("&nbsp;", " ");
    // Remove newlines because text will have wordwrap
    out.replace("\n", " ");

    return out;
}

ReleaseManager::ReleaseManager(QObject *parent)
: QSortFilterProxyModel(parent), m_sourceModel(new ReleaseListModel(this))
{
    qDebug() << this->metaObject()->className() << "construction";
    setSourceModel(m_sourceModel);

    qmlRegisterUncreatableType<Release>("MediaWriter", 1, 0, "Release", "");
    qmlRegisterUncreatableType<Variant>("MediaWriter", 1, 0, "Variant", "");
    qmlRegisterUncreatableType<Architecture>("MediaWriter", 1, 0, "Architecture", "");
    qmlRegisterUncreatableType<ImageType>("MediaWriter", 1, 0, "ImageType", "");
    qmlRegisterUncreatableType<Progress>("MediaWriter", 1, 0, "Progress", "");

    const QList<QString> releaseImagesList = getReleaseImagesFiles();

    // Try to load releases from cache
    bool loadedCachedReleases = true;
    for (auto release : releaseImagesList) {
        QString cachePath = releaseImagesCacheDir() + release;
        QFile cache(cachePath);
        if (!cache.open(QIODevice::ReadOnly)) {
            loadedCachedReleases = false;
            break;
        } else {
            cache.close();
        }
        loadReleaseFile(fileToString(cachePath));
    }

    if (!loadedCachedReleases) {
        // Load built-in release images if failed to load cache
        for (auto release : releaseImagesList) {
            const QString built_in_relese_images_path = ":/images/" + release;
            const QString release_images_string = fileToString(built_in_relese_images_path);
            loadReleaseFile(release_images_string);
        }
    }

    connect(
        this, &ReleaseManager::selectedChanged,
        this, &ReleaseManager::variantChangedFilter);

    // Download releases from getalt.org

    QTimer::singleShot(0, this, SLOT(fetchReleases()));
}

bool ReleaseManager::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    Q_UNUSED(source_parent)
    auto release = get(source_row);
    
    if (m_frontPage) {
        // Don't filter when on front page, just show 3 releases
        const bool on_front_page = (source_row < FRONTPAGE_ROW_COUNT);
        return on_front_page;
    } else if (release->isLocal()) {
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

void ReleaseManager::fetchReleases() {
    setBeingUpdated(true);

    // Create requests to download all release files and
    // collect the replies
    QHash<QString, QNetworkReply *> replies;
    const QList<QString> releaseFiles = getReleaseImagesFiles();
    for (const auto file : releaseFiles) {
        const QString url = GETALT_IMAGES_LOCATION + file;

        qDebug() << "Release url:" << url;

        QNetworkReply *reply = makeNetworkRequest(url, 5000);

        replies[file] = reply;
    }

    // This will run when all the replies are finished (or
    // technically, the last one)
    const auto onReplyFinished =
    [this, replies]() {
        // Only proceed if this is the last reply
        for (auto reply : replies) {
            if (!reply->isFinished()) {
                return;
            }
        }

        // Check that all replies suceeded
        // If not, retry
        for (auto reply : replies.values()) {
            const bool success = (reply->error() == QNetworkReply::NoError && reply->bytesAvailable() > 0);
            if (!success) {
                qWarning() << "Was not able to fetch new releases:" << reply->errorString() << "Retrying in 10 seconds.";
                QTimer::singleShot(10000, this, SLOT(fetchReleases()));

                return;
            }
        }

        qDebug() << this->metaObject()->className() << "Downloaded all release files";

        // Finally, save release files if all is good
        for (auto file : replies.keys()) {
            QNetworkReply *reply = replies[file];

            const QByteArray contents_bytes = reply->readAll();
            const QString contents(contents_bytes);

            // Save to cache
            const QString cachePath = releaseImagesCacheDir() + file;
            std::ofstream cacheFile(cachePath.toStdString());
            cacheFile << contents.toStdString();

            loadReleaseFile(contents);

            reply->deleteLater();
        }

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

void ReleaseManager::variantChangedFilter() {
    // TODO here we could add some filters to help signal/slot performance
    // TODO otherwise this can just go away and connections can be directly to the signal
    emit variantChanged();
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
    if (m_selectedIndex >= 0 && m_selectedIndex < m_sourceModel->rowCount())
        return m_sourceModel->get(m_selectedIndex);
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

Variant *ReleaseManager::variant() {
    Release *release = selected();

    if (release != nullptr) {
        return release->selectedVariant();
    } else {
        return nullptr;
    }
}

void ReleaseManager::loadReleaseFile(const QString &fileContents) {
    YAML::Node file = YAML::Load(fileContents.toStdString());

    for (auto e : file["entries"]) {
        const QString url = ymlToQString(e["link"]);
        if (url.isEmpty()) {
            qDebug() << "Invalid url for" << url;
            continue;
        }

        const QString name = ymlToQString(e["solution"]);
        if (name.isEmpty()) {
            qDebug() << "Invalid name for" << url;
            continue;
        }

        Architecture *arch =
        [e, url]() -> Architecture * {
            if (e["arch"]) {
                const QString arch_abbreviation = ymlToQString(e["arch"]);
                return Architecture::fromAbbreviation(arch_abbreviation);
            } else {
                return Architecture::fromFilename(url);
            }
        }();
        if (arch == nullptr) {
            qDebug() << "Invalid arch for" << url;
            continue;
        }

        // NOTE: yml file doesn't define "board" for pc32/pc64
        const QString board =
        [e]() -> QString {
            if (e["board"]) {
                return ymlToQString(e["board"]);
            } else {
                return "PC";
            }
        }();
        if (board.isEmpty()) {
            qDebug() << "Invalid board for" << url;
            continue;
        }

        ImageType *imageType = ImageType::fromFilename(url);
        if (!imageType->isValid()) {
            qDebug() << "Invalid image type for" << url;
            continue;
        }

        qDebug() << this->metaObject()->className() << "Adding" << name << arch->abbreviation().first() << board << imageType->abbreviation().first() << QUrl(url).fileName();

        for (int i = 0; i < m_sourceModel->rowCount(); i++) {
            Release *release = get(i);

            if (release->name().toLower().contains(name)) {
                release->updateUrl(url, arch, imageType, board);
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

ReleaseListModel::ReleaseListModel(ReleaseManager *parent)
: QAbstractListModel(parent) {
    // Load releases from sections files
    const QDir sections_dir(":/sections");
    const QList<QString> sectionsFiles = sections_dir.entryList();

    for (auto sectionFile : sectionsFiles) {
        const QString sectionFileContents = fileToString(":/sections/" + sectionFile);
        const YAML::Node sectionsFile = YAML::Load(sectionFileContents.toStdString());

        for (unsigned int i = 0; i < sectionsFile["members"].size(); i++) {
            const YAML::Node release_yml = sectionsFile["members"][i];
            const QString name = ymlToQString(release_yml["code"]);

            std::string lang = "_en";
            if (QLocale().language() == QLocale::Russian) {
                lang = "_ru";
            }
            
            const QString display_name = ymlToQString(release_yml["name" + lang]);
            const QString summary = ymlToQString(release_yml["descr" + lang]);

            QString description = ymlToQString(release_yml["descr_full" + lang]);

            // NOTE: currently no screenshots
            const QStringList screenshots;
            
            // Check that icon file exists
            const QString icon_name = ymlToQString(release_yml["img"]);
            const QString icon_path_test = ":/logo/" + icon_name;
            const QFile icon_file(icon_path_test);
            if (!icon_file.exists()) {
                qWarning() << "Failed to find icon file at " << icon_path_test << " needed for release " << name;
            }

            // NOTE: icon_path is consumed by QML, so it needs to begin with "qrc:/" not ":/"
            const QString icon_path = "qrc" + icon_path_test;

            const auto release = new Release(manager(), name, display_name, summary, description, icon_path, screenshots);

            // Reorder releases because default order in
            // sections files is not good. Try to put
            // workstation first and server second, so that they
            // are both on the frontpage.
            // NOTE: this will break if names change in sections files, in that case the order will be the default one
            const int index =
            [this, release]() {
                const QString release_name = release->name();
                const bool is_workstation = (release_name == "alt-workstation");
                const bool is_server = (release_name == "alt-server");

                if (is_workstation) {
                    return 0;
                } else if (is_server) {
                    return 1;
                } else {
                    return m_releases.size();
                }
            }();
            m_releases.insert(index, release);
        }
    }

    // Create custom release and variant
    // Insert custom release at the end of the front page
    const auto customRelease = new Release(manager(), "custom", tr("Custom image"), QT_TRANSLATE_NOOP("Release", "Pick a file from your drive(s)"), { QT_TRANSLATE_NOOP("Release", "<p>Here you can choose a OS image from your hard drive to be written to your flash disk</p><p>Currently it is only supported to write raw disk images (.iso or .bin)</p>") }, "qrc:/logo/custom", {});
    customRelease->updateUrl(QString(), Architecture::fromId(Architecture::UNKNOWN), ImageType::all()[ImageType::ISO], QString("UNKNOWN BOARD"));
    m_releases.insert(FRONTPAGE_ROW_COUNT - 1, customRelease);
}

ReleaseManager *ReleaseListModel::manager() {
    return qobject_cast<ReleaseManager*>(parent());
}

Release *ReleaseListModel::get(int index) {
    if (index >= 0 && index < m_releases.count())
        return m_releases[index];
    return nullptr;
}


Release::Release(ReleaseManager *parent, const QString &name, const QString &display_name, const QString &summary, const QString &description, const QString &icon, const QStringList &screenshots)
: QObject(parent)
, m_name(name)
, m_displayName(display_name)
, m_summary(summary)
, m_description(description)
, m_icon(icon)
, m_screenshots(screenshots)
{
    // TODO: connect to release's signal in parent, not the other way around, won't need to have parent be releasemanager then
    connect(
        this, &Release::selectedVariantChanged,
        parent, &ReleaseManager::variantChangedFilter);
}

void Release::setLocalFile(const QString &path) {
    if (QFile::exists(path)) {
        qWarning() << path << "doesn't exist";
        return;
    }

    // TODO: don't need to delete, can change path in variant. Though have to consider the case where path doesn't exist.

    // Delete old variant
    for (auto variant : m_variants) {
        variant->deleteLater();
    }
    m_variants.clear();

    // Add new variant
    auto local_variant = new Variant(path, this);
    m_variants.append(local_variant);
    emit variantsChanged();
    emit selectedVariantChanged();
}

void Release::updateUrl(const QString &url, Architecture *architecture, ImageType *imageType, const QString &board) {
    // If variant already exists, update it
    Variant *variant_in_list =
    [=]() -> Variant * {
        for (auto variant : m_variants) {
            // TODO: equals?
            if (variant->arch() == architecture && variant->board() == board) {
                return variant;
            }
        }
        return nullptr;
    }();
    if (variant_in_list != nullptr) {
        variant_in_list->updateUrl(url);
        return;
    }

    // Otherwise make a new variant

    // NOTE: preserve the order from the Architecture::Id enum (to not have ARM first, etc.)
    const int insert_index =
    [this, architecture]() {
        int out = 0;
        for (auto variant : m_variants) {
            // NOTE: doing pointer comparison because architectures are a singleton pointers
            if (variant->arch() > architecture) {
                return out;
            }

            out++;
        }
        return out;
    }();
    auto new_variant = new Variant(url, architecture, imageType, board, this);

    m_variants.insert(insert_index, new_variant);
    emit variantsChanged();
    
    // Select first variant by default
    // TODO: use setSelectedVariantIndex()? Need to avoid checking for (if changed) condition in there then
    if (m_variants.count() == 1) {
        emit selectedVariantChanged();
    }
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

bool Release::isLocal() const {
    return m_name == "custom";
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
