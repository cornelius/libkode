/*
    This file is part of KDE.

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

#include <QtCore/QStringList>

#include "enum.h"
#include "code.h"

using namespace KODE;

class Enum::Private
{
public:
    QString mName;
    QStringList mEnums;
    bool mCombinable = false;
    bool mTypedef = false;
    bool mIsQENUM = false;
};

Enum::Enum() : d(new Private) {}

Enum::Enum(const Enum &other) : d(new Private)
{
    *d = *other.d;
}

Enum::Enum(const QString &name, const QStringList &enums, bool combinable) : d(new Private)
{
    d->mName = name;
    d->mEnums = enums;
    d->mCombinable = combinable;
}

Enum::~Enum()
{
    delete d;
}

Enum &Enum::operator=(const Enum &other)
{
    if (this == &other)
        return *this;

    *d = *other.d;

    return *this;
}

QString Enum::name() const
{
    return d->mName;
}

void Enum::printDeclaration(Code &code) const
{
    if (d->mTypedef)
        code.addLine(QStringLiteral("typedef enum {"));
    else
        code.addLine(QStringLiteral("enum %1 {").arg(d->mName));
    code.indent();
    auto last = (d->mEnums.count() - 1);
    for (int value = 0; value <= last; value++) {
        if (d->mCombinable) {
            if (value != last)
                code.addLine(QString::fromLatin1("%1 = %2,").arg(d->mEnums[value]).arg(1 << value));
            else
                code.addLine(QString::fromLatin1("%1 = %2").arg(d->mEnums[value]).arg(1 << value));
        } else {
            if (value != last)
                code.addLine(d->mEnums[value] + QLatin1String(", "));
            else
                code.addLine(d->mEnums[value]);
        }
    }
    code.unindent();
    if (d->mTypedef)
        code.addLine(QString("} %1;").arg(d->mName));
    else
        code.addLine("};");

    if (d->mIsQENUM)
        code.addLine(QStringLiteral("Q_ENUM(%1)").arg(d->mName));
    code.newLine();
}

void Enum::setIsQENUM(bool qenum)
{
    d->mIsQENUM = qenum;
}

void Enum::setTypedef(bool typeDef)
{
    d->mTypedef = typeDef;
}
