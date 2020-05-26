/*
    This file is part of libkode.

    Copyright (c) 2020 Miklos Marton <martonmiklosqdev@gmail.com>

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
#ifndef INCLUDE_H
#define INCLUDE_H

#include <QList>
#include <QString>

#include <kode_export.h>

namespace KODE {

class KODE_EXPORT Include
{
public:
    enum IncludeType { Global, Relative };
    Include() = default;
    Include(const QString &fileName, IncludeType type_ = Global);
    bool operator==(const Include &other) const;
    QString includeFileName;
    IncludeType type = Global;

    typedef QVector<Include> List;
};

} // end namespace KODE

#endif // INCLUDE_H
