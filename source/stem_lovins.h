/*
	STEM_LOVINS.H
	-------------
*/
#ifndef STEM_LOVINS_H_
#define STEM_LOVINS_H_

#include "stem.h"

/*
	class ANT_STEM_LOVINS
	---------------------
*/
class ANT_stem_lovins : public ANT_stem
{
public:
	ANT_stem_lovins() {}
	virtual ~ANT_stem_lovins() {}
	virtual size_t stem(char *term, char *destination);
} ;

#endif /* STEM_LOVINS_H_ */