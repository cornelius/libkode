/*
    This file is part of KDE Schema Parser

    Copyright (c) 2005 Tobias Koenig <tokoe@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#include "fileprovider.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QUrl>
#include <QDebug>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTemporaryFile>

#ifndef Q_OS_WIN
#    include <unistd.h>
#endif

static QHash<QUrl, QByteArray> fileProviderCache;

FileProvider::FileProvider(bool useLocalFilesOnly, const QStringList &importPathList,
                           const QMap<QUrl, QString> &localSchemas)
    : mUseLocalFilesOnly(useLocalFilesOnly),
      mImportPathList(importPathList),
      mLocalSchemas(localSchemas)
{
}

FileProvider::~FileProvider()
{
    cleanUp();
}

void FileProvider::cleanUp()
{
    if (!mFileName.isEmpty()) {
        QFile::remove(mFileName);
        mFileName = QString();
    }
}

bool FileProvider::get(const QUrl &url, QString &target)
{
    if (!mFileName.isEmpty()) {
        cleanUp();
    }

    if (url.isLocalFile()) {
        target = url.toLocalFile();
        return true;
    }
    if (url.scheme() == QLatin1String("qrc")) {
        target = QLatin1String(":") + url.path();
        return true;
    }

    const auto localSchemaIt = mLocalSchemas.constFind(url);
    if (localSchemaIt != mLocalSchemas.constEnd()) {
        target = localSchemaIt.value();
        return true;
    }

    Q_FOREACH (const QString &importPath, mImportPathList) {
        QDir importDir(importPath);
        QString path = importDir.absoluteFilePath(url.host() + QDir::separator() + url.path());
        if (QFile::exists(path)) {
            qDebug("Using import path '%s'", qPrintable(path));
            target = path;
            return true;
        }
    }

    if (mUseLocalFilesOnly) {
        qCritical("ERROR: Could not find the local file for '%s'", qPrintable(url.toEncoded()));
        qCritical("ERROR: Try to download the file using:");
        qCritical("ERROR:  $ cd %s", qPrintable(mImportPathList.first()));
        qCritical("ERROR:  $ wget -r %s", qPrintable(url.toEncoded()));
        qCritical("ERROR: Or use the -import-path argument to set the correct search path");
        QCoreApplication::exit(12);
        return false;
    }

    if (target.isEmpty()) {
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        tmpFile.open();
        target = tmpFile.fileName();
        mFileName = target;
    }

    QByteArray data;
    const QHash<QUrl, QByteArray>::const_iterator it = fileProviderCache.constFind(url);
    if (it != fileProviderCache.constEnd()) {
        data = it.value();
    } else {
        qDebug("Downloading '%s'", url.toEncoded().constData());

        QNetworkAccessManager manager;
        manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkRequest request(url);
        QNetworkReply *job = manager.get(request);

        QEventLoop loop;
        QObject::connect(job, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (job->error()) {
            qWarning("Error downloading '%s': %s", url.toEncoded().constData(),
                     qPrintable(job->errorString()));
            return false;
        }

        qDebug("Download successful");
        data = job->readAll();
        fileProviderCache[url] = data;
    }

    QFile file(mFileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to create" << mFileName << ":" << file.errorString();
        return false;
    }

    file.write(data);
    file.close();

    return true;
}
