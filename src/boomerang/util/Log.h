#pragma once

#include <QString>
#include <memory>
#include <fstream>

#include "boomerang/core/Boomerang.h"

class Instruction;
class Exp;
class LocationSet;
class RTL;
class Type;
class Address;

class Printable;

using SharedType     = std::shared_ptr<Type>;
using SharedConstExp = std::shared_ptr<const Exp>;


class Log
{
public:
	Log() {}
	virtual Log& operator<<(const QString& s) = 0;
	virtual Log& operator<<(const Instruction *s);
	virtual Log& operator<<(const SharedConstExp& e);
	virtual Log& operator<<(const SharedType& ty);
	virtual Log& operator<<(const Printable& ty);
	virtual Log& operator<<(const RTL *r);
	virtual Log& operator<<(int i);
	virtual Log& operator<<(size_t i);
	virtual Log& operator<<(char c);
	virtual Log& operator<<(double d);
	virtual Log& operator<<(Address a);
	virtual Log& operator<<(const LocationSet *l);

	virtual ~Log() {}
};

/// Sets the outputfile to be the file "log" in the default output directory.
class FileLogger : public Log
{
protected:
	std::ofstream out;

public:
	FileLogger();
	virtual ~FileLogger() {}
	Log& operator<<(const QString& str)  override;
};

class SeparateLogger : public Log
{
protected:
	std::ofstream *out;

public:
	SeparateLogger(const QString& filePath);
	SeparateLogger(const SeparateLogger&) {}

	virtual ~SeparateLogger();

	Log& operator<<(const QString& str) override;
};

class NullLogger : public Log
{
public:
	virtual Log& operator<<(const QString& /*str*/) override
	{
		return *this;
	}
};