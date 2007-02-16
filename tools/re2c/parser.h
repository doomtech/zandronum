/* $Id: parser.h,v 1.10 2006/01/21 15:51:02 helly Exp $ */
#ifndef _parser_h
#define _parser_h

#include "scanner.h"
#include "re.h"
#include <iosfwd>
#include <map>

namespace re2c
{

class Symbol
{
public:

	RegExp*   re;

	static Symbol *find(const SubStr&);
	static void ClearTable();

	~Symbol()
	{
		/** \todo should we delete 're'? */
	}

	typedef std::map<std::string, Symbol*> SymbolTable;

protected:

	Symbol(const SubStr& str)
		: re(NULL)
		, name(str)
	{
	}

private:

	static SymbolTable symbol_table;

	Str	name;

#if PEDANTIC
	Symbol(const Symbol& oth)
		: re(oth.re)
		, name(oth.name)
	{
	}
	Symbol& operator = (const Symbol& oth)
	{
		new(this) Symbol(oth);
		return *this;
	}
#endif
};

void parse(Scanner&, std::ostream&);

} // end namespace re2c

#endif
