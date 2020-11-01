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

#ifndef RELEASEMANAGER_H
#define RELEASEMANAGER_H

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QQmlListProperty>

class ReleaseManager;
class ReleaseListModel;
class Release;
class ReleaseVersion;
class Variant;
class Architecture;
class ImageType;

/*
 * Architecture - singleton (x86, x86_64, etc)
 *
 * Release -> Version -> Variant
 *
 * Server  -> 24      -> Full    -> x86_64
 *                               -> i686
 *                    -> Netinst -> x86_64
 *                               -> i686
 *         -> 23      -> Full    -> x86_64
 *                               -> i686
 *                    -> Netinst -> x86_64
 *                               -> i686
 *
 * Variant can be downloaded.
 * Variant can be written to a drive - that's handled by the target drive object itself.
 *
 * There should be no platform-dependent code in this file nor in potential child classes.
 */


/**
 * @brief The ReleaseManager class
 *
 * The main entry point to access all the available releases.
 *
 * It is a QSortFilterProxyModel - that means the actual release data has to be provided first by the @ref ReleaseListModel .
 *
 * @property frontPage is true if the application is on the front page
 * @property beingUpdated is true when the background data update is still running (waiting for data)
 * @property filterArchitecture index of the currently selected architecture
 * @property filterText user-entered text filter
 * @property selected the currently selected release
 * @property selectedIndex the index of the currently selected release
 * @property architectures the list of the available architectures
 * @property fileNameFilters image type filters for file dialog
 */
class ReleaseManager : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(bool frontPage READ frontPage WRITE setFrontPage NOTIFY frontPageChanged)
    Q_PROPERTY(bool beingUpdated READ beingUpdated NOTIFY beingUpdatedChanged)

    Q_PROPERTY(int filterArchitecture READ filterArchitecture WRITE setFilterArchitecture NOTIFY filterArchitectureChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

    Q_PROPERTY(Release* selected READ selected NOTIFY selectedChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedChanged)

    Q_PROPERTY(Variant* variant READ variant NOTIFY variantChanged)

    Q_PROPERTY(QStringList architectures READ architectures CONSTANT)
    Q_PROPERTY(QStringList fileNameFilters READ fileNameFilters CONSTANT)
public:
    explicit ReleaseManager(QObject *parent = 0);
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

    Q_INVOKABLE Release *get(int index) const;

    bool beingUpdated() const;

    bool frontPage() const;
    void setFrontPage(bool o);

    QString filterText() const;
    void setFilterText(const QString &o);

    QStringList architectures() const;
    QStringList fileNameFilters() const;
    int filterArchitecture() const;
    void setFilterArchitecture(int o);

    Release *selected() const;
    int selectedIndex() const;
    void setSelectedIndex(int o);

    Variant *variant();

public slots:
    void fetchReleases();
    void variantChangedFilter();

signals:
    void beingUpdatedChanged();
    void frontPageChanged();
    void filterTextChanged();
    void filterArchitectureChanged();
    void selectedChanged();
    void variantChanged();

private:
    ReleaseListModel *m_sourceModel { nullptr };
    bool m_frontPage { true };
    QString m_filterText {};
    int m_filterArchitecture { 0 };
    int m_selectedIndex { 0 };
    bool m_beingUpdated = true;

    void loadReleaseFile(const QString &fileContents);
    void setBeingUpdated(const bool value);
};


/**
 * @brief The ReleaseListModel class
 *
 * The list model containing all available releases without filtering.
 */
class ReleaseListModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit ReleaseListModel(ReleaseManager *parent = 0);
    ReleaseManager *manager();

    Q_INVOKABLE Release *get(int index);

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
private:
    bool loadRelease(const QString &name, const QString &sectionFileName);

    QList<Release*> m_releases {};
};


/**
 * @brief The Release class
 *
 * The class representing a fedora flavor, like for example Workstation or KDE Plasma Desktop spin.
 *
 * It can have multiple versions.
 *
 * @property index the index in the list
 * @property name the name of the release, like "Fedora Workstation"
 * @property summary the summary describing the release - displayed on the main screen
 * @property description the extensive description of the release - displayed on the detail screen
 * @property isLocal true if name is "custom"
 * @property icon path of the icon of this release
 * @property screenshots a list of paths to screenshots (typically HTTP URLs)
 * @property prerelease true if the release contains a prerelease version of a future version
 * @property versions a list of available versions of the @ref ReleaseVersion class
 * @property versionNames a list of the names of the available versions
 * @property version the currently selected @ref ReleaseVersion
 * @property versionIndex the index of the currently selected @ref ReleaseVersion
 */
class Release : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(QString summary READ summary CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)

    Q_PROPERTY(bool isLocal READ isLocal CONSTANT)

    Q_PROPERTY(QString icon READ icon CONSTANT)
    Q_PROPERTY(QStringList screenshots READ screenshots CONSTANT)

    Q_PROPERTY(QString prerelease READ prerelease NOTIFY prereleaseChanged)

    Q_PROPERTY(QQmlListProperty<ReleaseVersion> versions READ versions NOTIFY versionsChanged)
    Q_PROPERTY(QStringList versionNames READ versionNames NOTIFY versionsChanged)
    Q_PROPERTY(ReleaseVersion* version READ selectedVersion NOTIFY selectedVersionChanged)
    Q_PROPERTY(int versionIndex READ selectedVersionIndex WRITE setSelectedVersionIndex NOTIFY selectedVersionChanged)
public:
    Release(ReleaseManager *parent, const QString &name, const QString &displayName, const QString &summary, const QString &description, const QString &icon, const QStringList &screenshots);
    Q_INVOKABLE void setLocalFile(const QString &path);
    bool updateUrl(const QString &version, const QString &status, Architecture *architecture, ImageType *imageType, const QString &board, const QString &url);
    ReleaseManager *manager();

    QString name() const;
    QString displayName() const;
    QString summary() const;
    QString description() const;
    bool isLocal() const;
    QString icon() const;
    QStringList screenshots() const;
    QString prerelease() const;

    void addVersion(ReleaseVersion *version);
    void removeVersion(ReleaseVersion *version);
    QQmlListProperty<ReleaseVersion> versions();
    QList<ReleaseVersion*> versionList() const;
    QStringList versionNames() const;
    ReleaseVersion *selectedVersion() const;
    int selectedVersionIndex() const;
    void setSelectedVersionIndex(int o);

signals:
    void versionsChanged();
    void selectedVersionChanged();
    void prereleaseChanged();
private:
    QString m_name {};
    QString m_displayName {};
    QString m_summary {};
    QString m_description {};
    QString m_icon {};
    QStringList m_screenshots {};
    QList<ReleaseVersion *> m_versions {};
    int m_selectedVersion { 0 };
};


/**
 * @brief The ReleaseVersion class
 *
 * Represents the version of the release. It can have multiple variants (like a different architecture or netinst/live)
 *
 * @property number the version number (as string)
 * @property name the name of the release (version + alpha/beta/etc)
 * @property status the release status of the version (alpha - beta - release candidate - final)
 * @property variants list of the version's variants, like architectures
 * @property variant the currently selected variant
 * @property variantIndex the index of the currently selected variant
 */
class ReleaseVersion : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString number READ number CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)

    Q_PROPERTY(ReleaseVersion::Status status READ status NOTIFY statusChanged)

    Q_PROPERTY(QQmlListProperty<Variant> variants READ variants NOTIFY variantsChanged)
    Q_PROPERTY(Variant* variant READ selectedVariant NOTIFY selectedVariantChanged)
    Q_PROPERTY(int variantIndex READ selectedVariantIndex WRITE setSelectedVariantIndex NOTIFY selectedVariantChanged)

public:
    enum Status {
        FINAL,
        RELEASE_CANDIDATE,
        BETA,
        ALPHA
    };

    Q_ENUMS(Status)

    ReleaseVersion(Release *parent, const QString &number, ReleaseVersion::Status status);
    ReleaseVersion(Release *parent, const QString &file);
    Release *release();
    const Release *release() const;

    bool updateUrl(const QString &status, Architecture *architecture, ImageType *imageType, const QString &board, const QString &url);

    QString number() const;
    QString name() const;
    ReleaseVersion::Status status() const;

    void addVariant(Variant *v);
    QQmlListProperty<Variant> variants();
    QList<Variant*> variantList() const;
    Variant *selectedVariant() const;
    int selectedVariantIndex() const;
    void setSelectedVariantIndex(int o);

signals:
    void variantsChanged();
    void selectedVariantChanged();
    void statusChanged();

private:
    QString m_number { "0" };
    ReleaseVersion::Status m_status { FINAL };
    QList<Variant*> m_variants {};
    int m_selectedVariant { 0 };
};

#endif // RELEASEMANAGER_H
