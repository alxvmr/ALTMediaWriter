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

#ifndef DRIVEMANAGER_H
#define DRIVEMANAGER_H

#include <QAbstractListModel>
#include <QDebug>

class DriveManager;
class DriveProvider;
class Drive;
class UdisksDrive;
class Progress;
class Variant;

QString getHelperPath();

/**
 * @brief The DriveManager class
 *
 * The class providing the list of all available portable drives to the UI
 *
 * Platform-independent
 *
 * @property length count of the drives
 * @property selected the selected drive
 * @property selectedIndex the index of the selected drive
 * @property lastRestoreable the most recently connected restoreable drive
 */
class DriveManager : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int length READ length NOTIFY drivesChanged)
    Q_PROPERTY(Drive *selected READ selected NOTIFY selectedChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedChanged)
    Q_PROPERTY(bool isBroken READ isBackendBroken NOTIFY isBackendBrokenChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY isBackendBrokenChanged)

    Q_PROPERTY(Drive *lastRestoreable READ lastRestoreable NOTIFY restoreableDriveChanged)
public:
    static DriveManager *instance();

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Drive *selected() const;
    int selectedIndex() const;
    Q_INVOKABLE void setSelectedIndex(const int index);

    int length() const;

    Drive *lastRestoreable();

    bool isBackendBroken();
    QString errorString();

protected:
    void setLastRestoreable(Drive *drive);

private slots:
    void onDriveConnected(Drive *drive);
    void onDriveRemoved(Drive *drive);
    void onBackendBroken(const QString &message);

signals:
    void drivesChanged();
    void selectedChanged();
    void restoreableDriveChanged();
    void isBackendBrokenChanged();

private:
    explicit DriveManager(QObject *parent = 0);

    static DriveManager *_self;
    QList<Drive *> m_drives;
    int m_selectedIndex;
    Drive *m_lastRestoreable;
    DriveProvider *m_provider;
    QString m_errorString;
};

/**
 * @brief The DriveProvider class
 *
 * An "abstract" class providing a signal interface to the @ref DriveManager class.
 *
 * Should be reimplemented for every platform and the appropriate child class constructor should be added to the @ref create method.
 *
 * It reports coming and going drives through the @ref driveConnected and @ref driveRemoved signals.
 */
class DriveProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool initialized READ initialized NOTIFY initializedChanged)
public:
    static DriveProvider *create(DriveManager *parent);

    bool initialized() const;

signals:
    void driveConnected(Drive *drive);
    void driveRemoved(Drive *drive);
    void backendBroken(const QString &message);

    void initializedChanged();

protected:
    DriveProvider(DriveManager *parent);

    bool m_initialized;
};

/**
 * @brief The Drive class
 *
 * Contains a non-drive implementation. The methods can be reimplemented if necessary. Usually the child classes will just need to modify the values of the properties of the class.
 *
 * Should be reimplemented for every platform. The child instances should be created and handled by the @ref DriveProvider class.
 *
 * @property progress the @ref Progress object reporting the progress of writing the image
 * @property name name of the drive, should be human-readable, in ideal case the model of the drive and its size
 * @property size the size of the drive, in bytes
 * @property restoreStatus the status of restoring the drive
 */
class Drive : public QObject {
    Q_OBJECT
    Q_PROPERTY(Progress *progress READ progress CONSTANT)

    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString readableSize READ readableSize CONSTANT)
    Q_PROPERTY(qreal size READ size CONSTANT)
    Q_PROPERTY(RestoreStatus restoreStatus READ restoreStatus NOTIFY restoreStatusChanged)
public:
    enum RestoreStatus {
        CLEAN = 0,
        CONTAINS_LIVE,
        RESTORING,
        RESTORE_ERROR,
        RESTORED,
    };
    Q_ENUMS(RestoreStatus)

    Drive(DriveProvider *parent, const QString &name, const uint64_t size, const bool containsLive = false);

    Progress *progress() const;

    virtual void updateDrive(const QString &name, uint64_t size, bool containsLive = false);

    virtual QString name() const;
    virtual QString readableSize() const;
    virtual qreal size() const;
    virtual RestoreStatus restoreStatus();

    Q_INVOKABLE virtual bool write(Variant *variant);
    Q_INVOKABLE virtual void cancel();
    Q_INVOKABLE virtual void restore() = 0;

    bool operator==(const Drive &other) const;

public slots:
    void setRestoreStatus(const RestoreStatus status);

signals:
    void restoreStatusChanged();

protected:
    Variant *m_variant;
    Progress *m_progress;
    QString m_name;
    uint64_t m_size;
    RestoreStatus m_restoreStatus;
    QString m_error;
};

#endif // DRIVEMANAGER_H
