/*
    This file is part of KDE Schema Parser

    Copyright (c) 2005 Tobias Koenig <tokoe@kde.org>
    Copyright (c) 2006 Michaël Larouche <michael.larouche@kdemail.net>
                       based on wsdlpull parser by Vivek Krishna

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

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QtDebug>
#include <QtCore/QLatin1String>

#include <common/fileprovider.h>
#include <common/messagehandler.h>
#include <common/nsmanager.h>
#include <common/parsercontext.h>
#include "parser.h"

static const QString XMLSchemaURI(QLatin1String("http://www.w3.org/2001/XMLSchema"));
static const QString WSDLSchemaURI(QLatin1String("http://schemas.xmlsoap.org/wsdl/"));
static const QString soapEncNs = QLatin1String("http://schemas.xmlsoap.org/soap/encoding/");
static const QString soap12EncNs = QLatin1String("http://www.w3.org/2003/05/soap-encoding");

Q_LOGGING_CATEGORY(parser, "libkode.parser", QtWarningMsg)

namespace XSD {

static bool stringToBoolean(const QString &str)
{
    return str == QLatin1String("true") || str == QChar::fromLatin1('1');
}

class Parser::Private
{
public:
    QString mNameSpace;

    SimpleType::List mSimpleTypes;
    ComplexType::List mComplexTypes;
    Element::List mElements;
    Attribute::List mAttributes;
    Group::List mGroups;
    AttributeGroup::List mAttributeGroups;
    Annotation::List mAnnotations;

    QStringList mImportedSchemas;
    QStringList mIncludedSchemas;

    QMap<QUrl, QString> mLocalSchemas;

    bool mDefaultQualifiedElements = false;
    bool mDefaultQualifiedAttributes = false;
    bool mUseLocalFilesOnly = false;
    QStringList mImportPathList;
};

Parser::Parser(ParserContext *context, const QString &nameSpace, bool useLocalFilesOnly,
               const QStringList &importPathList)
    : d(new Private)
{
    d->mNameSpace = nameSpace;
    d->mUseLocalFilesOnly = useLocalFilesOnly;
    d->mImportPathList = importPathList;
    init(context);
}

Parser::Parser(const QString &nameSpace) : d(new Private)
{
    d->mNameSpace = nameSpace;
}

Parser::Parser(const Parser &other) : d(new Private)
{
    *d = *other.d;
}

Parser::Parser(Parser &&other) : d(std::move(other.d)) {}

Parser::~Parser()
{
    clear(); // why?
}

Parser &Parser::operator=(const Parser &other)
{
    if (this == &other) {
        return *this;
    }

    *d = *other.d;

    return *this;
}

Parser &Parser::operator=(Parser &&other) noexcept = default;

void Parser::setLocalSchemas(const QMap<QUrl, QString> &localSchemas)
{
    d->mLocalSchemas = localSchemas;
}

void Parser::clear()
{
    d->mImportedSchemas.clear();
    d->mComplexTypes.clear();
    d->mSimpleTypes.clear();
    d->mElements.clear();
    d->mGroups.clear();
    d->mAttributes.clear();
    d->mAttributeGroups.clear();
}

void Parser::init(ParserContext *context)
{
#if 0
    if (!parseFile(context, ":/schema/XMLSchema.xsd")) {
        qWarning("Error parsing builtin file XMLSchema.xsd");
    }
#else
    Q_UNUSED(context);
#endif

    // From the XML schema XSD
    {
        Element schema(XMLSchemaURI);
        schema.setName(QLatin1String("schema"));
        schema.setType(QName(XMLSchemaURI, QLatin1String("anyType")));
        d->mElements.append(schema);
    }
    d->mImportedSchemas.append(XMLSchemaURI);
    d->mImportedSchemas.append(NSManager::xmlNamespace());

    // Define xml:lang, since we don't parse xml.xsd
    {
        Attribute langAttr(NSManager::xmlNamespace());
        langAttr.setName(QLatin1String("lang"));
        langAttr.setType(QName(XMLSchemaURI, QLatin1String("string")));
        d->mAttributes.append(langAttr);
    }
}

bool Parser::parseSchemaTag(ParserContext *context, const QDomElement &root)
{
    QName name(root.tagName());
    if (name.localName() != QLatin1String("schema")) {
        qDebug() << "ERROR localName=" << name.localName();
        return false;
    }

    // Already done by caller when coming from type.cpp, but doesn't hurt to do twice
    context->namespaceManager()->enterChild(root);

    // This method can call itself recursively, so save/restore the member attribute.
    const QString oldNamespace = d->mNameSpace;
    const bool oldDefaultQualifiedElements = d->mDefaultQualifiedElements;
    const bool oldDefaultQualifiedAttributes = d->mDefaultQualifiedAttributes;

    if (root.hasAttribute(QLatin1String("targetNamespace"))) {
        d->mNameSpace = root.attribute(QLatin1String("targetNamespace"));
    }

    if (root.attribute(QLatin1String("elementFormDefault")) == QLatin1String("qualified")) {
        d->mDefaultQualifiedElements = true;
    }

    if (root.attribute(QLatin1String("attributeFormDefault")) == QLatin1String("qualified")) {
        d->mDefaultQualifiedAttributes = true;
    }

    // mTypesTable.setTargetNamespace( mNameSpace );

    QDomElement element = root.firstChildElement();
    while (!element.isNull()) {
        NSManager namespaceManager(context, element);
        const QName name(element.tagName());
        qCDebug(parser) << "Schema: parsing" << name.localName();

        if (name.localName() == QLatin1String("import")) {
            parseImport(context, element);
        } else if (name.localName() == QLatin1String("element")) {
            addGlobalElement(parseElement(context, element, d->mNameSpace, element));
        } else if (name.localName() == QLatin1String("complexType")) {
            ComplexType ct = parseComplexType(context, element);
            // add elements from parsed complexType into mElements too
            for (const auto &subelem : ct.elements()) {
                d->mElements.append(subelem);
            }
            d->mComplexTypes.append(ct);
        } else if (name.localName() == QLatin1String("simpleType")) {
            SimpleType st = parseSimpleType(context, element);
            d->mSimpleTypes.append(st);
        } else if (name.localName() == QLatin1String("attribute")) {
            addGlobalAttribute(parseAttribute(context, element, d->mNameSpace));
        } else if (name.localName() == QLatin1String("attributeGroup")) {
            d->mAttributeGroups.append(parseAttributeGroup(context, element, d->mNameSpace));
        } else if (name.localName() == QLatin1String("group")) {
            d->mGroups.append(parseGroup(context, element, d->mNameSpace));
        } else if (name.localName() == QLatin1String("annotation")) {
            d->mAnnotations = parseAnnotation(context, element);
        } else if (name.localName() == QLatin1String("include")) {
            parseInclude(context, element);
        } else {
            qWarning() << "Unsupported schema element" << name.localName();
        }

        element = element.nextSiblingElement();
    }

    resolveForwardDeclarations();

    d->mImportedSchemas.append(d->mNameSpace);
    d->mNameSpace = oldNamespace;
    d->mDefaultQualifiedElements = oldDefaultQualifiedElements;
    d->mDefaultQualifiedAttributes = oldDefaultQualifiedAttributes;

    return true;
}

void Parser::parseImport(ParserContext *context, const QDomElement &element)
{
    // https://www.w3.org/TR/2004/REC-xmlschema-1-20041028/structures.html#layer2
    // The actual value of its namespace [attribute] indicates that the containing schema document
    // may contain qualified references to schema components in that namespace (via one or more
    // prefixes declared with namespace declarations in the normal way).
    QString expectedNamespace = element.attribute(QLatin1String("namespace"));

    QString location = element.attribute(QLatin1String("schemaLocation"));

    if (location.isEmpty()) {
        // Testcase: <s:import namespace="http://microsoft.com/wsdl/types/" /> in the WSDL at
        // https://www.elogbook.org/logbookws/logbookifv3.asmx

        // When no schemaLocation [attribute] is present, the schema author is leaving the
        // identification of that schema to the instance, application or user, via the mechanisms
        // described below in Layer 3: Schema Document Access and Web-interoperability (§4.3). 4.3.2
        // is especially crazy in terms of "do whatever you can or want" Some implementations seem
        // to just use the namespace as a schema location, let's try that
        if (!expectedNamespace.isEmpty()) {
            location = expectedNamespace;
        } else {
            return; // <import/> means nothing to us
        }
    }

    // don't import a schema twice
    if (d->mImportedSchemas.contains(location)) {
        return;
    } else {
        d->mImportedSchemas.append(location);
    }

    importSchema(context, location);
}

void Parser::parseInclude(ParserContext *context, const QDomElement &element)
{
    QString location = element.attribute(QLatin1String("schemaLocation"));

    if (!location.isEmpty()) {
        // don't include a schema twice
        if (d->mIncludedSchemas.contains(location)) {
            return;
        } else {
            d->mIncludedSchemas.append(location);
        }

        includeSchema(context, location);
    } else {
        context->messageHandler()->warning(
                QString::fromLatin1("include tag found at (%1, %2) contains no schemaLocation tag.")
                        .arg(element.lineNumber(), element.columnNumber()));
    }
}

Annotation::List Parser::parseAnnotation(ParserContext *context, const QDomElement &element)
{
    Annotation::List result;

    QDomElement child;
    for (child = element.firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) {
        NSManager namespaceManager(context, child);
        const QName name(child.tagName());
        if (name.localName() == QLatin1String("documentation")) {
            result.append(Annotation(child));
        } else if (name.localName() == QLatin1String("appinfo")) {
            result.append(Annotation(child));
        }
    }

    return result;
}

ComplexType Parser::parseComplexType(ParserContext *context, const QDomElement &element)
{
    ComplexType newType(d->mNameSpace);

    newType.setName(element.attribute(QLatin1String("name")));

    qCDebug(parser) << "complexType:" << d->mNameSpace << newType.name();

    if (element.hasAttribute(QLatin1String("mixed"))) {
        newType.setContentModel(XSDType::MIXED);
    }

    QDomElement childElement = element.firstChildElement();

    AttributeGroup::List attributeGroups;
    Group::List groups;

    while (!childElement.isNull()) {
        NSManager namespaceManager(context, childElement);
        const QName name(childElement.tagName());
        if (name.localName() == QLatin1String("all")) {
            all(context, childElement, newType);
        } else if (name.localName() == QLatin1String("sequence")
                   || name.localName() == QLatin1String("choice")) {
            Element::List elems;
            parseCompositor(context, childElement, newType.nameSpace(), &elems, &groups);
            for (const Element &elem : std::as_const(elems)) {
                newType.addElement(elem);
            }
        } else if (name.localName() == QLatin1String("attribute")) {
            newType.addAttribute(parseAttribute(context, childElement, d->mNameSpace));
        } else if (name.localName() == QLatin1String("attributeGroup")) {
            attributeGroups.append(parseAttributeGroup(context, childElement, d->mNameSpace));
        } else if (name.localName() == QLatin1String("group")) {
            groups.append(parseGroup(context, childElement, newType.nameSpace()));
        } else if (name.localName() == QLatin1String("anyAttribute")) {
            addAnyAttribute(context, childElement, newType);
        } else if (name.localName() == QLatin1String("complexContent")) {
            parseComplexContent(context, childElement, newType);
        } else if (name.localName() == QLatin1String("simpleContent")) {
            parseSimpleContent(context, childElement, newType);
        } else if (name.localName() == QLatin1String("annotation")) {
            Annotation::List annotations = parseAnnotation(context, childElement);
            newType.setDocumentation(annotations.documentation());
            newType.setAnnotations(annotations);
        } else {
            qWarning() << "Unsupported complextype element" << name.localName();
        }

        childElement = childElement.nextSiblingElement();
    }

    newType.setAttributeGroups(attributeGroups);
    newType.setGroups(groups);

    return newType;
}

void Parser::all(ParserContext *context, const QDomElement &element, ComplexType &ct)
{
    QDomElement childElement = element.firstChildElement();

    while (!childElement.isNull()) {
        NSManager namespaceManager(context, childElement);
        const QName name(childElement.tagName());
        if (name.localName() == QLatin1String("element")) {
            ct.addElement(parseElement(context, childElement, ct.nameSpace(), childElement));
        } else if (name.localName() == QLatin1String("annotation")) {
            Annotation::List annotations = parseAnnotation(context, childElement);
            ct.setDocumentation(annotations.documentation());
            ct.setAnnotations(annotations);
        } else {
            qWarning() << "Unsupported all element" << name.localName();
        }

        childElement = childElement.nextSiblingElement();
    }
}

static int readMaxOccurs(const QDomElement &element)
{
    const QString value = element.attribute(QLatin1String("maxOccurs"), QLatin1String("1"));
    if (value == QLatin1String("unbounded")) {
        return Parser::UNBOUNDED;
    } else {
        return value.toInt();
    }
}

void Parser::parseCompositor(ParserContext *context, const QDomElement &element,
                             const QString &nameSpace, Element::List *elements, Group::List *groups)
{
    const QName name(element.tagName());
    bool isChoice = name.localName() == QLatin1String("choice");
    bool isSequence = name.localName() == QLatin1String("sequence");

    Compositor compositor;
    if (isChoice) {
        compositor.setType(Compositor::Choice);
    } else if (isSequence) {
        compositor.setType(Compositor::Sequence);
    }
    compositor.setMinOccurs(
            element.attribute(QLatin1String("minOccurs"), QLatin1String("1")).toInt());
    compositor.setMaxOccurs(readMaxOccurs(element));

    if (isChoice || isSequence) {
        QDomElement childElement = element.firstChildElement();

        while (!childElement.isNull()) {
            NSManager namespaceManager(context, childElement);
            const QName csName(childElement.tagName());
            const QString localName = csName.localName();
            if (localName == QLatin1String("element")) {
                Element newElement;
                if (isChoice) {
                    newElement = parseElement(context, childElement, nameSpace, element);
                } else {
                    newElement = parseElement(context, childElement, nameSpace, childElement);
                }
                newElement.setCompositor(compositor);
                elements->append(newElement);
                compositor.addChild(csName);
            } else if (localName == QLatin1String("any")) {
                elements->append(parseAny(context, childElement, nameSpace));
            } else if (localName == QLatin1String("choice")
                       || localName == QLatin1String("sequence")) {
                parseCompositor(context, childElement, nameSpace, elements, groups);
            } else if (localName == QLatin1String("group")) {
                groups->append(parseGroup(context, childElement, nameSpace));
            } else if (localName == QLatin1String("annotation")) {
                // Not implemented
                // Annotation::List annotations = parseAnnotation( context, childElement );
                // compositor.setDocumentation( annotations.documentation() );
                // compositor.setAnnotations( annotations );
            } else {
                qDebug() << "Unsupported element in" << name << ":" << csName;
            }

            childElement = childElement.nextSiblingElement();
        }
    }
}

Element Parser::parseElement(ParserContext *context, const QDomElement &element,
                             const QString &nameSpace, const QDomElement &occurrenceElement)
{
    Element newElement(nameSpace);

    newElement.setName(element.attribute(QLatin1String("name")));
    qCDebug(parser) << "newElement namespace=" << nameSpace << "name=" << newElement.name()
                    << "defaultQualified=" << d->mDefaultQualifiedElements;

    // https://www.w3.org/TR/xmlschema-0/#NS
    if (element.hasAttribute(QLatin1String("form"))) {
        newElement.setIsQualified(element.attribute(QLatin1String("form"))
                                  == QLatin1String("qualified"));
    } else {
        newElement.setIsQualified(d->mDefaultQualifiedElements);
    }

    if (element.hasAttribute(QLatin1String("ref"))) {
        QName reference(element.attribute(QLatin1String("ref")));
        reference.setNameSpace(context->namespaceManager()->uri(reference.prefix()));
        newElement.setReference(reference);
    }

    setOccurrenceAttributes(newElement, occurrenceElement);

    newElement.setDefaultValue(element.attribute(QLatin1String("default")));
    newElement.setFixedValue(element.attribute(QLatin1String("fixed")));
    newElement.setNillable(stringToBoolean(element.attribute(QLatin1String("nillable"))));

    if (element.hasAttribute(QLatin1String("type"))) {
        QName typeName(element.attribute(QLatin1String("type")).trimmed());
        typeName.setNameSpace(context->namespaceManager()->uri(typeName.prefix()));
        qCDebug(parser) << "typeName=" << typeName.qname()
                        << "namespace=" << context->namespaceManager()->uri(typeName.prefix());
        newElement.setType(typeName);

        if (element.hasAttribute(QLatin1String("substitutionGroup"))) {
            QName baseElementName(element.attribute(QLatin1String("substitutionGroup")));
            baseElementName.setNameSpace(
                    context->namespaceManager()->uri(baseElementName.prefix()));
            XSD::Element::List::iterator ctit_base = d->mElements.findElement(baseElementName);
            if (ctit_base != d->mElements.end()) {
                XSD::Element &baseElem = *ctit_base;
                // Record that the base element has substitutions
                baseElem.setHasSubstitutions(true);
                // Its type will need a virtual method _kd_substitutionElementName so fill in the
                // base type too. (OK, we do that for each derived type, but well)
                const QName baseType = baseElem.type();
                setSubstitutionElementName(baseType, baseElem.qualifiedName());
            } else {
                qWarning() << "Element" << newElement.qualifiedName()
                           << "uses undefined element as substitutionGroup" << baseElementName;
            }

            setSubstitutionElementName(typeName, newElement.qualifiedName());
        }
    } else {
        QDomElement childElement = element.firstChildElement();

        while (!childElement.isNull()) {
            NSManager namespaceManager(context, childElement);
            const QName childName(childElement.tagName());
            // qDebug() << "childName:" << childName.localName();
            if (childName.localName() == QLatin1String("complexType")) {
                ComplexType ct = parseComplexType(context, childElement);
                ct.setAnonymous(true);
                ct.setName(newElement.name());
                int i = 0;
                bool typeExists = false;
                ComplexType existingType =
                        d->mComplexTypes.complexType(QName(ct.nameSpace(), ct.name()));
                while (!existingType.isNull()) {
                    if (existingType == ct) {
                        qCDebug(parser) << "  Nested complexType of name" << ct.name()
                                        << "is structurally identical with existing complexType of "
                                           "the same name, skipping this instance...";
                        typeExists = true;
                        break;
                    } else {
                        ct.setName(newElement.name() + QString::number(++i));
                        existingType =
                                d->mComplexTypes.complexType(QName(ct.nameSpace(), ct.name()));
                    }
                }
                if (!typeExists) {
                    d->mComplexTypes.append(ct);
                    if (newElement.name() != ct.name())
                        qCDebug(parser)
                                << "  Detected type collision for nested complexType, updated name"
                                << newElement.name() << "to" << ct.name();
                    qCDebug(parser) << " found nested complexType element, type name is now "
                                    << "element name, i.e. " << ct.name() << "newElement.setType"
                                    << ct.qualifiedName();
                }
                newElement.setType(ct.qualifiedName());
            } else if (childName.localName() == QLatin1String("simpleType")) {
                SimpleType st = parseSimpleType(context, childElement);

                st.setName(newElement.name());
                d->mSimpleTypes.append(st);

                newElement.setType(st.qualifiedName());
            } else if (childName.localName() == QLatin1String("annotation")) {
                Annotation::List annotations = parseAnnotation(context, childElement);
                newElement.setDocumentation(annotations.documentation());
                newElement.setAnnotations(annotations);
            }

            childElement = childElement.nextSiblingElement();
        }
    }

    // Fixup elements without a type
    if (newElement.type().isEmpty() && newElement.reference().isEmpty()) {
        ComplexType ct;
        Q_ASSERT(!newElement.name().isEmpty());
        ct.setNameSpace(newElement.nameSpace());
        ct.setName(newElement.name());
        ct.setAnonymous(true);
        d->mComplexTypes.append(ct);
        newElement.setType(ct.qualifiedName());
    }

    return newElement;
}

void Parser::setSubstitutionElementName(const QName &typeName, const QName &elemName)
{
    XSD::ComplexType::List::iterator ctit = d->mComplexTypes.findComplexType(typeName);
    if (ctit != d->mComplexTypes.end()) {
        // If this type already has an element name associated, they are aliases, any one will do.
        (*ctit).setSubstitutionElementName(elemName);
    } else {
        XSD::SimpleType::List::iterator stit = d->mSimpleTypes.findSimpleType(typeName);
        if (stit != d->mSimpleTypes.end()) {
            (*stit).setSubstitutionElementName(elemName);
        } else {
            qWarning() << "Element" << elemName << "uses undefined type" << typeName;
        }
    }
}

// Testcase: salesforce-partner.wsdl has <any namespace="##targetNamespace" [...]/>
Element Parser::parseAny(ParserContext *, const QDomElement &element, const QString &nameSpace)
{
    Element newElement(nameSpace);
    newElement.setName(QLatin1String("any"));
    QName anyType(QLatin1String("http://www.w3.org/2001/XMLSchema"), QLatin1String("any"));
    newElement.setType(anyType);
    setOccurrenceAttributes(newElement, element);
    return newElement;
}

void Parser::setOccurrenceAttributes(Element &newElement, const QDomElement &element)
{
    newElement.setMinOccurs(
            element.attribute(QLatin1String("minOccurs"), QLatin1String("1")).toInt());
    newElement.setMaxOccurs(readMaxOccurs(element));
}

void Parser::addAnyAttribute(ParserContext *, const QDomElement &element, ComplexType &complexType)
{
    Attribute newAttribute;
    newAttribute.setName(QLatin1String("anyAttribute"));

    newAttribute.setNameSpace(element.attribute(QLatin1String("namespace")));

    // ### Hmm, technically, this should be a list of anys, I think.
    newAttribute.setType(QName(XMLSchemaURI, QString::fromLatin1("anyType")));

    complexType.addAttribute(newAttribute);
}

Attribute Parser::parseAttribute(ParserContext *context, const QDomElement &element,
                                 const QString &nameSpace)
{
    Attribute newAttribute;

    newAttribute.setName(element.attribute(QLatin1String("name")));
    newAttribute.setNameSpace(nameSpace);

    if (element.hasAttribute(QLatin1String("type"))) {
        // TODO pass nsmanager to QName so that it can resolve namespaces?
        QName typeName(element.attribute(QLatin1String("type")));
        typeName.setNameSpace(context->namespaceManager()->uri(typeName.prefix()));
        newAttribute.setType(typeName);
    }

    // https://www.w3.org/TR/xmlschema-0/#NS
    if (element.hasAttribute(QLatin1String("form"))) {
        newAttribute.setIsQualified(element.attribute(QLatin1String("form"))
                                    == QLatin1String("qualified"));
    } else {
        newAttribute.setIsQualified(d->mDefaultQualifiedAttributes);
    }

    if (element.hasAttribute(QLatin1String("ref"))) {
        QName reference(element.attribute(QLatin1String("ref")));
        reference.setNameSpace(context->namespaceManager()->uri(reference.prefix()));
        newAttribute.setReference(reference);
    }

    newAttribute.setDefaultValue(element.attribute(QLatin1String("default")));
    newAttribute.setFixedValue(element.attribute(QLatin1String("fixed")));

    if (element.hasAttribute(QLatin1String("use"))) {
        const QString use = element.attribute(QLatin1String("use"));
        if (use == QLatin1String("optional")) {
            newAttribute.setAttributeUse(Attribute::Optional);
        } else if (use == QLatin1String("required")) {
            newAttribute.setAttributeUse(Attribute::Required);
        } else if (use == QLatin1String("prohibited")) {
            qWarning("prohibited attributes are not supported"); // TODO skip parsing the attribute
                                                                 // altogether
            newAttribute.setAttributeUse(Attribute::Prohibited);
        }
    }

    QDomElement childElement = element.firstChildElement();

    while (!childElement.isNull()) {
        NSManager namespaceManager(context, childElement);
        const QName childName(childElement.tagName());
        if (childName.localName() == QLatin1String("simpleType")) {
            SimpleType st = parseSimpleType(context, childElement);
            st.setName(newAttribute.name());
            d->mSimpleTypes.append(st);

            newAttribute.setType(st.qualifiedName());
        } else if (childName.localName() == QLatin1String("annotation")) {
            Annotation::List annotations = parseAnnotation(context, childElement);
            newAttribute.setDocumentation(annotations.documentation());
            newAttribute.setAnnotations(annotations);
        }

        childElement = childElement.nextSiblingElement();
    }

    if (newAttribute.type().isEmpty() && !element.hasAttribute(QLatin1String("ref"))) {
        // https://www.w3.org/TR/2004/REC-xmlschema-1-20041028/structures.html#element-attribute
        // says "otherwise the simple ur-type definition", which is anySimpleType
        newAttribute.setType(QName(XMLSchemaURI, QLatin1String("anySimpleType")));
        qDebug() << "found attribute" << newAttribute.name()
                 << "without type and without ref, set to default" << newAttribute.type();
    }

    return newAttribute;
}

SimpleType Parser::parseSimpleType(ParserContext *context, const QDomElement &element)
{
    SimpleType st(d->mNameSpace);

    st.setName(element.attribute(QLatin1String("name")));

    qCDebug(parser) << "simpleType:" << d->mNameSpace << st.name();

    QDomElement childElement = element.firstChildElement();

    while (!childElement.isNull()) {
        NSManager namespaceManager(context, childElement);
        const QName name(childElement.tagName());
        if (name.localName() == QLatin1String("restriction")) {
            st.setSubType(SimpleType::TypeRestriction);

            QName typeName(childElement.attribute(QLatin1String("base")));
            typeName.setNameSpace(context->namespaceManager()->uri(typeName.prefix()));
            st.setBaseTypeName(typeName);

            parseRestriction(context, childElement, st);
        } else if (name.localName() == QLatin1String("union")) {
            st.setSubType(SimpleType::TypeUnion);
            // It means "the contents can be either one of my child elements, or one of the types
            // listed in memberTypes". For now we'll just use QVariant. For more compile-time
            // checking we would need to actually parse and store the references to the possible
            // types. And then generate methods for each; but we won't have a good name for these
            // methods, just some type name... setSizebyno / setSizebystring reads weird.
            st.setBaseTypeName(
                    QName(XMLSchemaURI, QString::fromLatin1("anyType"))); // to get QVariant

        } else if (name.localName() == QLatin1String("list")) {
            st.setSubType(SimpleType::TypeList);
            if (childElement.hasAttribute(QLatin1String("itemType"))) {
                QName typeName(childElement.attribute(QLatin1String("itemType")));
                if (!typeName.prefix().isEmpty()) {
                    typeName.setNameSpace(context->namespaceManager()->uri(typeName.prefix()));
                } else {
                    typeName.setNameSpace(st.nameSpace());
                }
                st.setListTypeName(typeName);
            } else {
                // Anonymous type
                QDomElement typeElement = childElement.firstChildElement();
                for (; !typeElement.isNull(); typeElement = typeElement.nextSiblingElement()) {
                    NSManager namespaceManager(context, typeElement);
                    const QName typeName(typeElement.tagName());
                    // qDebug() << "childName:" << childName.localName();
                    if (typeName.localName() == QLatin1String("complexType")) {
                        ComplexType ctItem = parseComplexType(context, typeElement);
                        ctItem.setName(
                                st.name()
                                + QLatin1String("ListItem")); // need to make something up, so that
                                                              // the classname looks good
                        st.setListTypeName(ctItem.qualifiedName());
                        d->mComplexTypes.append(ctItem);
                    } else if (typeName.localName() == QLatin1String("simpleType")) {
                        SimpleType stItem = parseSimpleType(context, typeElement);
                        stItem.setName(
                                st.name()
                                + QLatin1String("ListItem")); // need to make something up, so that
                                                              // the classname looks good
                        st.setListTypeName(stItem.qualifiedName());
                        d->mSimpleTypes.append(stItem);
                    } else {
                        qDebug() << "ERROR: parseSimpleType: unhandled: " << typeName.localName()
                                 << "in list" << d->mNameSpace << st.name();
                    }
                }
            }
        } else if (name.localName() == QLatin1String("annotation")) {
            Annotation::List annotations = parseAnnotation(context, childElement);
            st.setDocumentation(annotations.documentation());
            st.setAnnotations(annotations);
        }

        childElement = childElement.nextSiblingElement();
    }

    return st;
}

void Parser::parseRestriction(ParserContext *context, const QDomElement &element, SimpleType &st)
{
    if (st.baseTypeName().isEmpty()) {
        qDebug("<restriction>: unknown BaseType");
    }

    QDomElement childElement = element.firstChildElement();

    while (!childElement.isNull()) {
        NSManager namespaceManager(context, childElement);
        const QName tagName(childElement.tagName());
        if (tagName.localName() == QLatin1String("annotation")) {
            // Skip annotations here.
        } else {
            SimpleType::FacetType ft = st.parseFacetId(tagName.localName());
            if (ft == SimpleType::NONE) {
                qDebug("<restriction>: %s is not a valid facet for the simple type '%s'",
                       qPrintable(childElement.tagName()), qPrintable(st.name()));
            } else {
                st.setFacetValue(ft, childElement.attribute(QLatin1String("value")));
            }
        }
        childElement = childElement.nextSiblingElement();
    }
}

void Parser::parseComplexContent(ParserContext *context, const QDomElement &element,
                                 ComplexType &complexType)
{
    QName typeName;

    complexType.setContentModel(XSDType::COMPLEX);

    QDomElement childElement = element.firstChildElement();

    while (!childElement.isNull()) {
        NSManager namespaceManager(context, childElement);
        const QName name(childElement.tagName());

        if (name.localName() == QLatin1String("restriction")
            || name.localName() == QLatin1String("extension")) {
            typeName = childElement.attribute(QLatin1String("base"));
            typeName.setNameSpace(context->namespaceManager()->uri(typeName.prefix()));

            // if the base soapenc:Array, then read the arrayType attribute, and possibly the
            // desired name for the child elements
            // TODO check namespace is really soap-encoding
            if (typeName.localName() == QLatin1String("Array")) {

                QString typeStr;
                QString arrayElementStr;
                QDomElement arrayElement = childElement.firstChildElement();
                while (!arrayElement.isNull()) {
                    NSManager namespaceManager(context, arrayElement);
                    const QName tagName(arrayElement.tagName());
                    if (context->namespaceManager()->uri(tagName.prefix()) == XMLSchemaURI) {
                        const QString localName = tagName.localName();
                        if (localName == QLatin1String("attribute")) {
                            const QString prefix =
                                    context->namespaceManager()->prefix(WSDLSchemaURI);
                            const QString attributeName =
                                    (prefix.isEmpty() ? QString::fromLatin1("arrayType")
                                                      : prefix + QLatin1String(":arrayType"));

                            typeStr = arrayElement.attribute(attributeName);
                            if (typeStr.isEmpty()) {
                                qWarning("ERROR: arrayType attribute missing in Array element.");
                            }
                            if (typeStr.endsWith(QLatin1String("[]"))) {
                                typeStr.truncate(typeStr.length() - 2);
                            }
                        } else if (localName
                                   == QLatin1String("sequence")) { // detosagent-legacy.wsdl
                            arrayElement = arrayElement.firstChildElement(); // go down
                        }
                        if (QName(arrayElement.tagName()).localName() == QLatin1String("element")) {
                            arrayElementStr = arrayElement.attribute("name");
                            if (localName == QLatin1String("sequence")) { // go up again
                                arrayElement = arrayElement.parentNode().toElement();
                            }
                        }
                    }
                    arrayElement = arrayElement.nextSiblingElement();
                }
                if (typeStr.isEmpty()) {
                    qWarning("ERROR: <attribute> element not found");
                } else {
                    if (arrayElementStr.isEmpty()) {
                        arrayElementStr = QLatin1String("items"); // we have to call it something...
                    }

                    QName arrayType(typeStr);
                    arrayType.setNameSpace(context->namespaceManager()->uri(arrayType.prefix()));
                    complexType.setArrayType(arrayType);

                    Element items(complexType.nameSpace());
                    items.setName(arrayElementStr);
                    items.setType(arrayType);
                    // items.setArrayType( arrayType );
                    complexType.addElement(items);

                    // qDebug() << complexType.name() << "is array of" << arrayType;
                }

            } else {

                if (typeName
                    != QName(
                            XMLSchemaURI,
                            QString::fromLatin1("anyType"))) { // don't do this for anyType or Array
                    complexType.setBaseTypeName(typeName);
                }

                QDomElement ctElement = childElement.firstChildElement();
                while (!ctElement.isNull()) {
                    NSManager namespaceManager(context, ctElement);
                    const QName name(ctElement.tagName());

                    if (name.localName() == QLatin1String("all")) {
                        all(context, ctElement, complexType);
                    } else if (name.localName() == QLatin1String("sequence")
                               || name.localName() == QLatin1String("choice")) {
                        Element::List elems;
                        Group::List groups;
                        parseCompositor(context, ctElement, complexType.nameSpace(), &elems,
                                        &groups);
                        for (const Element &elem : std::as_const(elems)) {
                            complexType.addElement(elem);
                        }
                        for (const Group &group : std::as_const(groups)) {
                            complexType.addGroup(group);
                        }
                    } else if (name.localName() == QLatin1String("attribute")) {
                        complexType.addAttribute(
                                parseAttribute(context, ctElement, complexType.nameSpace()));
                    } else if (name.localName() == QLatin1String("anyAttribute")) {
                        addAnyAttribute(context, ctElement, complexType);
                    } else if (name.localName() == QLatin1String("attributeGroup")) {
                        complexType.addAttributeGroups(
                                parseAttributeGroup(context, ctElement, complexType.nameSpace()));
                    } else {
                        qWarning() << "Unsupported content element" << name.localName();
                    }

                    ctElement = ctElement.nextSiblingElement();
                }
            }
        }

        childElement = childElement.nextSiblingElement();
    }
    if (stringToBoolean(element.attribute(QLatin1String("mixed")))) {
        qDebug("<complexContent>: No support for mixed=true");
    }
}

void Parser::parseSimpleContent(ParserContext *context, const QDomElement &element,
                                ComplexType &complexType)
{
    complexType.setContentModel(XSDType::SIMPLE);

    QDomElement childElement = element.firstChildElement();

    while (!childElement.isNull()) {
        NSManager namespaceManager(context, childElement);
        const QName name(childElement.tagName());
        if (name.localName() == QLatin1String("restriction")) {
            SimpleType st(d->mNameSpace);

            if (childElement.hasAttribute(QLatin1String("base"))) {
                QName typeName(childElement.attribute(QLatin1String("base")));
                typeName.setNameSpace(context->namespaceManager()->uri(typeName.prefix()));
                st.setBaseTypeName(typeName);
            }

            parseRestriction(context, childElement, st);
        } else if (name.localName() == QLatin1String("extension")) {
            // This extension does not use the full model that can come in ComplexContent.
            // It uses the simple model. No particle allowed, only attributes

            if (childElement.hasAttribute(QLatin1String("base"))) {
                QName typeName(childElement.attribute(QLatin1String("base")));
                typeName.setNameSpace(context->namespaceManager()->uri(typeName.prefix()));
                complexType.setBaseTypeName(typeName);

                QDomElement ctElement = childElement.firstChildElement();
                while (!ctElement.isNull()) {
                    NSManager namespaceManager(context, ctElement);
                    const QName name(ctElement.tagName());
                    if (name.localName() == QLatin1String("attribute")) {
                        complexType.addAttribute(
                                parseAttribute(context, ctElement, complexType.nameSpace()));
                    }

                    ctElement = ctElement.nextSiblingElement();
                }
            }
        }

        childElement = childElement.nextSiblingElement();
    }
}

void Parser::addGlobalElement(const Element &newElement)
{
    // qDebug() << "Adding global element" << newElement.qualifiedName();

    // don't add elements twice
    bool found = false;
    for (int i = 0; i < d->mElements.count(); ++i) {
        if (d->mElements[i].qualifiedName() == newElement.qualifiedName()) {
            found = true;
            break;
        }
    }

    if (!found) {
        d->mElements.append(newElement);
    }
}

void Parser::addGlobalAttribute(const Attribute &newAttribute)
{
    // don't add attributes twice
    bool found = false;
    for (int i = 0; i < d->mAttributes.count(); ++i) {
        if (d->mAttributes[i].qualifiedName() == newAttribute.qualifiedName()) {
            found = true;
            break;
        }
    }

    if (!found) {
        d->mAttributes.append(newAttribute);
    }
}

AttributeGroup Parser::parseAttributeGroup(ParserContext *context, const QDomElement &element,
                                           const QString &nameSpace)
{
    Attribute::List attributes;

    AttributeGroup group;

    if (element.hasAttribute(QLatin1String("ref"))) {
        QName reference(element.attribute(QLatin1String("ref")));
        reference.setNameSpace(context->namespaceManager()->uri(reference.prefix()));
        group.setReference(reference);
        return group;
    }

    for (QDomElement e = element.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        QName childName = QName(e.tagName());
        if (childName.localName() == QLatin1String("attribute")) {
            Attribute a = parseAttribute(context, e, nameSpace);
            addGlobalAttribute(a);
            attributes.append(a);
        }
    }

    if (!element.hasAttribute(QLatin1String("name"))) {
        qWarning() << "Attribute Group without reference nor name, invalid XML schema";
    }

    group.setName(element.attribute(QLatin1String("name")));
    group.setNameSpace(nameSpace);
    group.setAttributes(attributes);

    return group;
}

// <group> https://www.w3.org/TR/xmlschema-0/#ref17
Group Parser::parseGroup(ParserContext *context, const QDomElement &element,
                         const QString &nameSpace)
{
    Element::List elements;
    Group group;

    if (element.hasAttribute(QLatin1String("ref"))) {
        QName reference(element.attribute(QLatin1String("ref")));
        reference.setNameSpace(context->namespaceManager()->uri(reference.prefix()));
        group.setReference(reference);
        return group;
    }

    for (QDomElement e = element.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        QName childName(e.tagName());
        const QString localName = childName.localName();
        // can contain all, choice or sequence
        if (localName == QLatin1String("sequence") || localName == QLatin1String("choice")) {
            parseCompositor(context, e, nameSpace, &elements, nullptr /*can't nest groups*/);
        } else if (localName == QLatin1String("all")) {
            qWarning() << "Unsupported element in group:" << localName; // TODO
        } else {
            qWarning() << "Unexpected element in group:" << localName;
        }
    }

    const QString name = element.attribute(QLatin1String("name"));
    Q_ASSERT(!name.isEmpty());
    group.setName(name);
    group.setNameSpace(nameSpace);
    group.setElements(elements);

    return group;
}

QString Parser::targetNamespace() const
{
    return d->mNameSpace;
}

static QUrl urlForLocation(ParserContext *context, const QString &location)
{
    QUrl url(location);
    if (url.isRelative() || url.scheme() == QLatin1String("file")) {
        QDir dir(location);
        url = context->documentBaseUrl();
        return QUrl::fromLocalFile(QDir(url.path()).filePath(location));
    }
    return url;
}

// Note: https://www.w3.org/TR/xmlschema-0/#schemaLocation paragraph 3 (for <import>) says
// "schemaLocation is only a hint"
void Parser::importSchema(ParserContext *context, const QString &location)
{
    // Ignore this one, we don't need it
    if (location == QLatin1String("http://schemas.xmlsoap.org/wsdl/")) {
        return;
    }

    if (location.startsWith(QLatin1String("urn:"))) { // Can't download that :-)
        return;
    }

    FileProvider provider(d->mUseLocalFilesOnly, d->mImportPathList, d->mLocalSchemas);
    QString fileName;
    const QUrl schemaLocation = urlForLocation(context, location);
    qDebug("importing schema at %s", schemaLocation.toEncoded().constData());
    if (provider.get(schemaLocation, fileName)) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug("Unable to open file %s", qPrintable(file.fileName()));
            return;
        }

        QDomDocument doc(QLatin1String("kwsdl"));
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
        QString errorMsg;
        int errorLine, errorColumn;
        bool ok = doc.setContent(&file, false, &errorMsg, &errorLine, &errorColumn);
        if (!ok) {
            qDebug("Error[%d:%d] %s", errorLine, errorColumn, qPrintable(errorMsg));
            return;
        }
#else
        if (auto result = doc.setContent(&file); !result) {
            qDebug("Error[%lld:%lld] %s", result.errorLine, result.errorColumn,
                   qPrintable(result.errorMessage));
            return;
        }
#endif

        QDomElement node = doc.documentElement();

        NSManager namespaceManager(context, node);

        const QName tagName(node.tagName());
        if (tagName.localName() == QLatin1String("schema")) {
            importOrIncludeSchema(context, node, schemaLocation);
        } else {
            qDebug("No schema tag found in schema file %s", schemaLocation.toEncoded().constData());
        }

        file.close();
    }
}

// TODO: Try to merge import and include schema
// The main difference is that <include> can only
//     "pull in definitions and declarations from a schema whose
//      target namespace is the same as the including schema's target namespace"
void Parser::includeSchema(ParserContext *context, const QString &location)
{
    FileProvider provider(d->mUseLocalFilesOnly, d->mImportPathList, d->mLocalSchemas);
    QString fileName;
    const QUrl schemaLocation = urlForLocation(context, location);
    qDebug("including schema at %s", schemaLocation.toEncoded().constData());
    if (provider.get(schemaLocation, fileName)) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug("Unable to open file %s", qPrintable(file.fileName()));
            return;
        }

        QDomDocument doc(QLatin1String("kwsdl"));
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
        QString errorMsg;
        int errorLine, errorColumn;
        bool ok = doc.setContent(&file, false, &errorMsg, &errorLine, &errorColumn);
        if (!ok) {
            qDebug("Error[%d:%d] %s", errorLine, errorColumn, qPrintable(errorMsg));
            return;
        }
#else
        if (auto result = doc.setContent(&file); !result) {
            qDebug("Error[%lld:%lld] %s", result.errorLine, result.errorColumn,
                   qPrintable(result.errorMessage));
            return;
        }
#endif

        QDomElement node = doc.documentElement();
        NSManager namespaceManager(context, node);
        const QName tagName(node.tagName());
        if (tagName.localName() == QLatin1String("schema")) {
            // For include, targetNamespace must be the same as the current document.
            if (node.hasAttribute(QLatin1String("targetNamespace"))) {
                if (node.attribute(QLatin1String("targetNamespace")) != d->mNameSpace) {
                    context->messageHandler()->error(
                            QLatin1String("Included schema must be in the same namespace of the "
                                          "resulting schema."));
                    return;
                }
            }
            importOrIncludeSchema(context, node, schemaLocation);
        } else {
            qDebug("No schema tag found in schema file %s", schemaLocation.toEncoded().constData());
        }

        file.close();
    }
}

bool Parser::importOrIncludeSchema(ParserContext *context, const QDomElement &element,
                                   const QUrl &schemaLocation)
{
    const QUrl oldBaseUrl = context->documentBaseUrl();
    context->setDocumentBaseUrlFromFileUrl(schemaLocation);

    const bool ret = parseSchemaTag(context, element);

    context->setDocumentBaseUrl(oldBaseUrl);

    return ret;
}

QString Parser::schemaUri()
{
    return XMLSchemaURI;
}

Element Parser::findElement(const QName &name) const
{
    for (const Element &e : std::as_const(d->mElements)) {
        if (e.nameSpace() == name.nameSpace() && e.name() == name.localName()) {
            return e;
        }
    }
    qDebug() << "Element not found:" << name.nameSpace() << name.localName();
    return Element();
}

Group Parser::findGroup(const QName &name) const
{
    for (const Group &g : std::as_const(d->mGroups)) {
        if (g.nameSpace() == name.nameSpace() && g.name() == name.localName()) {
            return g;
        }
    }
    qDebug() << "Group not found:" << name.nameSpace() << name.localName();
    return Group();
}

Attribute Parser::findAttribute(const QName &name) const
{
    for (const Attribute &attr : std::as_const(d->mAttributes)) {
        if (attr.nameSpace() == name.nameSpace() && attr.name() == name.localName()) {
            return attr;
        }
    }
    qDebug() << "Attribute not found:" << name.nameSpace() << name.localName();
    return Attribute();
}

AttributeGroup Parser::findAttributeGroup(const QName &name) const
{
    for (const AttributeGroup &g : std::as_const(d->mAttributeGroups)) {
        if (g.nameSpace() == name.nameSpace() && g.name() == name.localName()) {
            return g;
        }
    }
    qDebug() << "Attribute Group not found:" << name.nameSpace() << name.localName();
    return AttributeGroup();
}

bool Parser::resolveForwardDeclarations()
{
    const QName any(QLatin1String("http://www.w3.org/2001/XMLSchema"), QLatin1String("any"));
    // const QName anyType( "http://www.w3.org/2001/XMLSchema", "anyType" );
    for (int i = 0; i < d->mComplexTypes.count(); ++i) {

        ComplexType &complexType = d->mComplexTypes[i];

        Element::List elements = complexType.elements();
        // qDebug() << i << "looking at" << complexType << " " << elements.count() << "elements";
        Element::List finalElementList;
        for (int j = 0; j < elements.count(); ++j) {
            Element element = elements.at(j);
            // qDebug() << "  " << element;
            if (!element.isResolved()) {
                Element resolvedElement = findElement(element.reference());
                if (resolvedElement.qualifiedName().isEmpty()) {
                    qWarning("ERROR in %s: resolving element ref to '%s': not found!",
                             qPrintable(complexType.qualifiedName().qname()),
                             qPrintable(element.reference().qname()));
                    if (qEnvironmentVariableIsSet("LIBKODE_VERBOSE_ERRORS")) {
                        d->mElements.dump();
                    } else {
                        qWarning() << "Set the environment variable LIBKODE_VERBOSE_ERRORS to see "
                                      "the list of all available elements";
                    }
                    return false;
                } else {
                    resolvedElement.setMinOccurs(element.minOccurs());
                    resolvedElement.setMaxOccurs(element.maxOccurs());
                    resolvedElement.setCompositor(element.compositor());
                    element = resolvedElement;
                }
            }
            if (j > 0 && finalElementList.last().type() == any) {
                if (element.type() == any) {
                    // Keep only one any. The alternative would be to implement namespace
                    // "filtering"...
                    // qWarning("ERROR: two 'any' values in the same type %s",
                    // qPrintable(d->mComplexTypes[i].name())); return false;
                    continue;
                }
                // Hack for deserialization: keep "any" last.
                Element lastElem = finalElementList.takeLast();
                finalElementList.append(element);
                finalElementList.append(lastElem);
            } else {
                finalElementList.append(element);
            }
        }

        const auto groups = complexType.groups();
        for (const Group &group : groups) {
            if (!group.isResolved()) {
                const Group refGroup = findGroup(group.reference());
                if (!refGroup.isNull()) {
                    // qDebug() << "  resolved group" << group.reference() << "got these elements"
                    // << refGroup.elements();
                    const auto elements = refGroup.elements();
                    for (const Element &elem : elements) {
                        Q_ASSERT(!elem.type().isEmpty());
                        finalElementList.append(elem);
                    }
                    // qDebug() << "  finalElementList" << finalElementList;
                }
            }
        }
        // groups were resolved, don't do it again if resolveForwardDeclarations() is called again
        complexType.setGroups(Group::List());
        complexType.setElements(finalElementList);

        Attribute::List attributes = complexType.attributes();

        for (int j = 0; j < attributes.count(); ++j) {
            const Attribute &attribute = attributes.at(j);
            if (!attribute.isResolved()) {
                Attribute refAttribute = findAttribute(attribute.reference());
                if (refAttribute.qualifiedName().isEmpty()) {
                    qWarning("ERROR in %s: resolving attribute ref to '%s': not found!",
                             qPrintable(d->mComplexTypes[i].qualifiedName().qname()),
                             qPrintable(attribute.reference().qname()));
                    if (qEnvironmentVariableIsSet("LIBKODE_VERBOSE_ERRORS")) {
                        d->mAttributes.dump();
                    } else {
                        qWarning() << "Set the environment variable LIBKODE_VERBOSE_ERRORS to see "
                                      "the list of all available attributes";
                    }
                    return false;
                } else {
                    attributes[j] = refAttribute;
                }
            }
        }

        const auto attributeGroups = complexType.attributeGroups();
        for (const AttributeGroup &group : attributeGroups) {
            Q_ASSERT(!group.reference().isEmpty());
            AttributeGroup refAttributeGroup = findAttributeGroup(group.reference());
            Attribute::List groupAttributes = refAttributeGroup.attributes();
            for (const Attribute &ga : std::as_const(groupAttributes)) {
                attributes.append(ga);
            }
        }

        // groups were resolved, don't do it again if resolveForwardDeclarations() is called again
        complexType.setAttributeGroups(AttributeGroup::List());
        complexType.setAttributes(attributes);
    }
    return true;
}

Types Parser::types() const
{
    Types types;

    types.setSimpleTypes(d->mSimpleTypes);
    types.setComplexTypes(d->mComplexTypes);
    // qDebug() << "Parser::types: elements:";
    // d->mElements.dump();
    types.setElements(d->mElements);
    // types.setGroups( d->mGroups );
    types.setAttributes(d->mAttributes);
    // types.setAttributeGroups( d->mAttributeGroups );

    return types;
}

Annotation::List Parser::annotations() const
{
    return d->mAnnotations;
}

bool Parser::parse(ParserContext *context, QIODevice *sourceDevice)
{
    QDomDocument document(QLatin1String("KWSDL"));

    QDomDocument doc;
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    QString errorMsg;
    int errorLine, errorCol;
    if (!doc.setContent(sourceDevice, false, &errorMsg, &errorLine, &errorCol)) {
        qDebug("%s at (%d,%d)", qPrintable(errorMsg), errorLine, errorCol);
        return false;
    }
#else
    if (auto result = doc.setContent(sourceDevice); !result) {
        qDebug("%s at (%lld,%lld)", qPrintable(result.errorMessage), result.errorLine,
               result.errorColumn);
        return false;
    }
#endif

    QDomElement element = doc.documentElement();
    const QName name = element.tagName();
    if (name.localName() != QLatin1String("schema")) {
        qDebug("document element is '%s'", qPrintable(element.tagName()));
        return false;
    }

    return parseSchemaTag(context, element);
}

bool Parser::parseFile(ParserContext *context, QFile &file)
{
    return parse(context, &file);
}

bool Parser::parseString(ParserContext *context, const QByteArray &data)
{
    QBuffer buffer;
    buffer.setData(data);
    return parse(context, &buffer);
}

} // end namespace XSD
