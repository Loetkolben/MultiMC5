#pragma once

#include <QFile>
#include <QCoreApplication>
#include <QTest>
#include <QDir>
#include <QUrl>

#include "test_config.h"

class TestsInternal
{
public:
	static QByteArray readFile(const QString &fileName)
	{
		const QUrl url(fileName);
		QFile f(url.isLocalFile() ? url.toLocalFile() : fileName);
		f.open(QFile::ReadOnly);
		return f.readAll();
	}
	static QString readFileUtf8(const QString &fileName)
	{
		return QString::fromUtf8(readFile(fileName));
	}
};

#define MULTIMC_GET_TEST_FILE(file) TestsInternal::readFile(QFINDTESTDATA(file))
#define MULTIMC_GET_TEST_FILE_UTF8(file) TestsInternal::readFileUtf8(QFINDTESTDATA(file))

