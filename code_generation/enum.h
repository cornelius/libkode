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
#ifndef KODE_ENUM_H
#define KODE_ENUM_H

#include <QtCore/QStringList>

#include <kode_export.h>

namespace KODE {

class Code;

class KODE_EXPORT Enum
{
public:
    typedef QList<Enum> List;

    /**
     * Creates a new enum.
     */
    Enum();

    /**
     * Creates a new enum from @param other.
     */
    Enum(const Enum &other);

    /**
     * Creates a new enum with the given name and enum values.
     *
     * @param name The name of the enum.
     * @param enums The values of the enum.
     * @param combinable If true the integer associations will be a power of two,
     *                   so the enum flags will be combinable.
     */
    Enum(const QString &name, const QStringList &enums, bool combinable = false);

    /**
     * Destroys the enum.
     */
    ~Enum();

    /**
     * Assignment operator.
     */
    Enum &operator=(const Enum &other);

    /**
     * Return name of enum.
     */
    QString name() const;

    /**
     * Prints the declaration of the enum to the code
     */
    void printDeclaration(Code &code) const;

    /**
     * @brief setIsQENUM
     * This method can be used to mark enums to have a Q_ENUM(enumname) macro generated after the
     * declaration. If you add an enum with isQENUM set to a class make sure that the isQGadget
     * or the isQObject property of the class is set before code generation.
     * (The latter got set automatically if a signal or slot declaration to the class.)
     * @param qenum
     */
    void setIsQENUM(bool qenum = true);

    /**
     * @brief setTypedef
     * This method can be used to generate typedef for the given enum.
     * Using typedefs is mostly useful for C code and not recommended in C++ code.
     * @param typeDef
     */
    void setTypedef(bool typeDef = true);

private:
    class Private;
    Private *d;
};
}

#endif
