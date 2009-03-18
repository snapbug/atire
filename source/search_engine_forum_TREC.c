/*
	SEARCH_ENGINE_FORUM_TREC.C
	--------------------------
*/
#include <string.h>
#include "search_engine_forum_TREC.h"

/*
	ANT_SEARCH_ENGINE_FORUM_TREC::ANT_SEARCH_ENGINE_FORUM_TREC()
	------------------------------------------------------------
*/
ANT_search_engine_forum_TREC::ANT_search_engine_forum_TREC(char *filename, char *participant_id, char *run_id, char *task) : ANT_search_engine_forum(filename)
{
/*
	These lines remove the compiler warnings about unused parameters
*/
task = NULL;
participant_id = NULL;

/*
	Now get on with it
*/
strncpy(this->run_id, run_id, sizeof(this->run_id));
this->run_id[sizeof(this->run_id) - 1] = '\0';
}

/*
	ANT_SEARCH_ENGINE_FORUM_TREC::WRITE()
	-------------------------------------
*/
void ANT_search_engine_forum_TREC::write(long topic_id, char **docids, long hits)
{
long which;

for (which = 0; which < hits; which++)
	fprintf(file, "%ld Q0 %s %ld %ld %s\n", topic_id, docids[which], which + 1, (hits - which), run_id);
}