#include "xmlelement.h"

#include <QTest>

using namespace XSD;

class XmlElementTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void constructors();
    void assignment();
};

void XmlElementTest::constructors()
{
    XmlElement e("ns1");
    e.setName("elem");
    XmlElement copy(e);
    QCOMPARE(copy.name(), QString("elem"));
    XmlElement moved = std::move(e);
    QCOMPARE(moved.name(), QString("elem"));
}

void XmlElementTest::assignment()
{
    XmlElement e("ns1");
    e.setName("elem");
    XmlElement copy;
    copy = e;
    QCOMPARE(copy.name(), QString("elem"));
    XmlElement moved;
    moved = std::move(e);
    QCOMPARE(moved.name(), QString("elem"));
}

QTEST_MAIN(XmlElementTest)
#include "tst_xmlelement.moc"
