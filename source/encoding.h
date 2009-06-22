/*
 * encoding.h
 *
 *  Created on: Jun 20, 2009
 *      Author: monfee
 */

#ifndef __ENCODING_H__
#define __ENCODING_H__

class ANT_encoding {
public:
	enum language { ENGLISH, CHINESE }; // supported languages
	enum encoding { ASCII, UTF8 };

	virtual bool is_valid_char(unsigned char* c) = 0;
};

class ANT_encoding_ascii : public ANT_encoding {
public:
	virtual bool is_valid_char(unsigned char* c); // for English, a valid char will be an alphabet character

	virtual void tolower(unsigned char* c);
	virtual void tohigher(unsigned char* c);
};

inline bool ANT_encoding_ascii::is_valid_char(unsigned char* c) { return ANT_isalpha(*c); }

inline void ANT_encoding_ascii::tolower(unsigned char* c) { if ((*c) >= 'A' && (*c) <= 'Z') *c = ANT_tolower(*c); }
inline void ANT_encoding_ascii::tohigher(unsigned char* c) { if ((*c) >= 'a' && (*c) <= 'z') *c = ANT_tohigher(c); }

class ANT_encoding_utf8 : public ANT_encoding_ascii {
public:
	int test_utf8char(unsigned char* c);			// return number of bytes occupied by the character
													// to test if the input character is a valid utf8 character? could be anything including symbols and punctuation

	virtual bool is_valid_char(unsigned char* c); 	// a valid char will be actual language dependent character
													// for Chinese, a valid char will be an Chinese character rather than symbols or punctuation

	unsigned int to_codepoint(unsigned char* utf8_char);

	bool is_chinese_codepoint(unsigned int cp);
};

inline bool ANT_encoding_utf8::is_chinese_codepoint(unsigned int cp)
{
	return ((code >= 0x4e00 && code <= 0x9fff)
		|| (code >= 0x3400 && code <= 0x4dbf)
		|| (code >=0x20000 && code <= 0x2a6df)
		|| (code >=0xf900 && code <= 0xfaff)
		|| (code >=0x2f800 && code <= 0x2fa1f) );
}

#endif /* __ENCODING_H__ */
