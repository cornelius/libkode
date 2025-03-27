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
#pragma once

#include <kode_export.h>
#include "variable.h"
#include "code.h"

#include <QString>

namespace KODE {

class KODE_EXPORT Struct
{
public:
    typedef QList<Struct> List;

    Struct();

    void setTypedef(bool typeDef = true);
    /**
     * Sets the @param name of the class object.
     */
    void setName(const QString &name);

    /**
     * Returns the name of the class object.
     */
    QString name() const;

    /**
     * Adds a member @param variable to the class object.
     */
    void addMemberVariable(const KODE::Variable &variable);

    /**
     * Returns the list of all member variables.
     */
    KODE::Variable::List memberVariables() const;

    /**
     * Prints the declaration of the enum to the code
     */
    void printDeclaration(KODE::Code &code) const;

private:
    class Private;
    Private *d;
};

}
