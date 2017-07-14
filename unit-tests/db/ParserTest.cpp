/***************************************************************************/ /**
 * \file       ParserTest.cc
 * OVERVIEW:   Provides the implementation for the ParserTest class, which
 *                tests the sslparser.y etc
 ******************************************************************************/
#include "ParserTest.h"

#include "boomerang/db/ssl/sslparser.h"
#include "boomerang/db/statements/statement.h"

#include "boomerang/util/Log.h"
#include "boomerang/util/Log.h"

#include <QtCore/QDir>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QDebug>


#define SPARC_SSL    (BOOMERANG_TEST_BASE "/frontend/machine/sparc/sparc.ssl")

static bool    logset = false;

void ParserTest::initTestCase()
{
	if (!logset) {

		logset = true;
		Boomerang::get()->setProgPath(BOOMERANG_TEST_BASE);
		Boomerang::get()->setPluginPath(BOOMERANG_TEST_BASE "/out");
		Boomerang::get()->setLogger(new NullLogger());
	}
}


/***************************************************************************/ /**
 * \fn        ParserTest::testRead
 * OVERVIEW:        Test reading the SSL file
 ******************************************************************************/
void ParserTest::testRead()
{
	RTLInstDict d;

	QVERIFY(d.readSSLFile(SPARC_SSL));
}


/***************************************************************************/ /**
 * \fn        ParserTest::testExp
 * OVERVIEW:        Test parsing an expression
 ******************************************************************************/
void ParserTest::testExp()
{
	QString     s("*i32* r0 := 5 + 6");
	Instruction *a = SSLParser::parseExp(qPrintable(s));

	QVERIFY(a);
	QString     res;
	QTextStream ost(&res);
	a->print(ost);
	QCOMPARE(res, "   0 " + s);
	QString s2 = "*i32* r[0] := 5 + 6";
	a = SSLParser::parseExp(qPrintable(s2));
	QVERIFY(a);
	res.clear();
	a->print(ost);
	// Still should print to string s, not s2
	QCOMPARE(res, "   0 " + s);
}


QTEST_MAIN(ParserTest)