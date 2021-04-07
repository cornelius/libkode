#include "element.h"

#include <QTest>

using namespace XSD;

class ElementTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void constructors();
    void assignment();
};

void ElementTest::constructors()
{
    Element e("ns1");
    e.setName("elem");
    e.setMinOccurs(3);
    Element copy(e);
    QCOMPARE(copy.name(), "elem");
    QCOMPARE(copy.minOccurs(), 3);
    Element moved = std::move(e);
    QCOMPARE(moved.name(), "elem");
    QCOMPARE(moved.minOccurs(), 3);
}

void ElementTest::assignment()
{
    Element e("ns1");
    e.setName("elem");
    e.setNillable(true);
    Element copy;
    copy = e;
    QCOMPARE(copy.name(), "elem");
    QVERIFY(copy.nillable());
    Element moved;
    moved = std::move(e);
    QCOMPARE(moved.name(), "elem");
    QVERIFY(moved.nillable());
}

QTEST_MAIN(ElementTest)
#include "tst_element.moc"
