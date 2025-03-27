/*
    This file is part of libkode

 Copyright (c) 2025 Miklos Marton <martonmiklosqdev@gmail.com>

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
#include "struct.h"

#include "code.h"

using namespace KODE;

class Struct::Private
{
public:
    QString mName;
    bool mTypedef = false;
    Variable::List mMembers;
};

Struct::Struct() : d(new Private) {}

void Struct::setTypedef(bool typeDef)
{
    d->mTypedef = typeDef;
}

void Struct::setName(const QString &name)
{
    d->mName = name;
}

QString Struct::name() const
{
    return d->mName;
}

void Struct::addMemberVariable(const Variable &variable)
{
    d->mMembers.append(variable);
}

Variable::List Struct::memberVariables() const
{
    return d->mMembers;
}

void Struct::printDeclaration(KODE::Code &code) const
{
    if (d->mTypedef)
        code.addLine(QStringLiteral("typedef struct {"));
    else
        code.addLine(QStringLiteral("struct %1 {").arg(d->mName));
    code.indent();

    for (const auto &member : d->mMembers) {
        code.addLine(QString("%1 %2 %3;")
                             .arg(member.isStatic() ? "static" : "", member.type(), member.name()));
    }
    code.unindent();
    if (d->mTypedef)
        code.addLine(QString("} %1;").arg(d->mName));
    else
        code.addLine("};");

    code.newLine();
}
