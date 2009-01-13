/*
	SEARCH_ENGINE_STATS.C
	---------------------
*/
#include <stdio.h>
#include "search_engine_stats.h"

/*
	ANT_SEARCH_ENGINE_STATS::ANT_SEARCH_ENGINE_STATS()
	--------------------------------------------------
*/
ANT_search_engine_stats::ANT_search_engine_stats(ANT_memory *memory) : ANT_time_stats(memory)
{
initialise();
}

/*
	ANT_SEARCH_ENGINE_STATS::INITIALISE()
	-------------------------------------
*/
void ANT_search_engine_stats::initialise(void)
{
dictionary_time = count_relevant_time = sort_time = accumulator_init_time = posting_read_time = decompress_time = rank_time = 0;
}

/*
	ANT_SEARCH_ENGINE_STATS::TEXT_RENDER()
	--------------------------------------
*/
void ANT_search_engine_stats::text_render(void)
{
long long min, sum;

print_time("Accumulator Init Time :", accumulator_init_time);
print_time("Dictionary Read Time  :", dictionary_time);
print_time("Posting Disk Read Time:", posting_read_time);
print_time("Decompress Time       :", decompress_time);
print_time("Rank Time             :", rank_time);
print_time("Sort Time             :", sort_time);
print_time("Count Relevant Time   :", count_relevant_time);

sum = 0;
sum += accumulator_init_time;
sum += dictionary_time;
sum += posting_read_time;
sum += decompress_time;
sum += rank_time;
sum += sort_time;
sum += count_relevant_time;

print_time("Total Time to Search  :", sum);

min = posting_read_time;
min = min < decompress_time ? min : decompress_time;
min = min < rank_time ? min : rank_time;

printf("Ratio (read/decompress/rank):%2.2f %2.2f %2.2f\n", posting_read_time / (double)min, decompress_time / (double)min, rank_time / (double)min);
}