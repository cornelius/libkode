#include "group.h"

#include <QTest>

using namespace XSD;

class GroupTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void constructors();
    void assignment();
};

void GroupTest::constructors()
{
    Group group;
    group.setName("group");
    group.setReference(QName("ns", "local"));
    Group copy(group);
    QCOMPARE(copy.name(), "group");
    QCOMPARE(copy.reference().nameSpace(), "ns");
    QCOMPARE(copy.reference().localName(), "local");
    Group moved = std::move(group);
    QCOMPARE(moved.name(), "group");
    QCOMPARE(moved.reference().nameSpace(), "ns");
    QCOMPARE(moved.reference().localName(), "local");
}

void GroupTest::assignment()
{
    Group group;
    group.setName("group");
    group.setReference(QName("ns", "local"));
    Group copy;
    copy = group;
    QCOMPARE(copy.name(), "group");
    QCOMPARE(copy.reference().nameSpace(), "ns");
    QCOMPARE(copy.reference().localName(), "local");
    Group moved;
    moved = std::move(group);
    QCOMPARE(moved.name(), "group");
    QCOMPARE(moved.reference().nameSpace(), "ns");
    QCOMPARE(moved.reference().localName(), "local");
}

QTEST_MAIN(GroupTest)
#include "tst_group.moc"
