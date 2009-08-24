/*
	ENCODING_FACTORY.H
	------------------
	Created on: Jun 22, 2009
	Author: monfee
*/
#ifndef __ENCODING_FACTORY_H__
#define __ENCODING_FACTORY_H__

#include "encoding.h"

/*
	class UNISEG_ENCODING_FACTORY
	--------------------------
*/
class UNISEG_encoding_factory
{
public:
	enum encoding { ASCII, UTF8 };

	static UNISEG_encoding *enc;

public:
	static UNISEG_encoding *gen_encoding_scheme(encoding what_encoding);

	static UNISEG_encoding *get_encoding();
} ;

#endif __ENCODING_FACTORY_H__


