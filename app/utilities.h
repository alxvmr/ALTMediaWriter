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

#ifndef UTILITIES_H
#define UTILITIES_H

#include <QLoggingCategory>

class QNetworkAccessManager;
class QNetworkReply;

extern QNetworkAccessManager *network_access_manager;

class MessageHandler {
public:
    static void install(const bool debug_arg, const bool log_arg);
    static QLoggingCategory category;

    static bool debug;
    static bool log;
};

#define mInfo() qCInfo(MessageHandler::category)
#define mDebug() qCDebug(MessageHandler::category)
#define mWarning() qCWarning(MessageHandler::category)
#define mCritical() qCCritical(MessageHandler::category)
#define mFatal() qCFatal(MessageHandler::category)

QString userAgent();
QNetworkReply *makeNetworkRequest(const QString &url, const int time_out_millis = 0);

#endif // UTILITIES_H
