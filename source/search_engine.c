/*
	SEARCH_ENGINE.C
	---------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "search_engine.h"
#include "file_memory.h"
#include "memory.h"
#include "search_engine_btree_node.h"
#include "search_engine_btree_leaf.h"
#include "search_engine_accumulator.h"
#include "search_engine_result.h"
#include "search_engine_stats.h"
#include "top_k_sort.h"
#include "ranking_function_bm25.h"
#include "stemmer.h"
#include "compress_variable_byte.h"

#ifndef FALSE
	#define FALSE 0
#endif
#ifndef TRUE
	#define TRUE (!FALSE)
#endif

/*
	ANT_SEARCH_ENGINE::ANT_SEARCH_ENGINE()
	--------------------------------------
*/
ANT_search_engine::ANT_search_engine(ANT_memory *memory, long memory_model, const char *filename)
{
int32_t four_byte;
int64_t eight_byte;
unsigned char *block;
long long end, term_header, this_header_block_size, sum, current_length;
long postings_buffer_length, decompressed_integer;
ANT_search_engine_btree_node *current, *end_of_node_list;
ANT_search_engine_btree_leaf collection_details;
ANT_compress_variable_byte variable_byte;

index_filename = filename;
trim_postings_k = LLONG_MAX;
stats = new ANT_search_engine_stats(memory);
stats_for_all_queries = new ANT_search_engine_stats(memory);
this->memory = memory;

if (memory_model)
	index = new ANT_file_memory(memory);
else
	index = new ANT_file(memory);
if (index->open((char *)index_filename, "rb") == 0)
	exit(printf("Cannot open index file:%s\n", index_filename));

/*
	At the end of the file is a "header" that provides various details:
	long long: the location of the b-tree header block
	long: the string length of the longest term
	long: the length of the longest compressed postings list
	long long: the maximum number of postings in a postings list (the highest DF)
*/
end = index->file_length();
index->seek(end - sizeof(eight_byte) - sizeof(four_byte) - sizeof(four_byte) - sizeof(eight_byte));

index->read(&eight_byte);
term_header = eight_byte;

index->read(&four_byte);
string_length_of_longest_term = four_byte;

index->read(&four_byte);
postings_buffer_length = four_byte;

index->read(&eight_byte);
highest_df = eight_byte;

/*
	Load the B-tree header
*/
//printf("B-tree header is %lld bytes on disk\n", end - term_header);
index->seek(term_header);
block = (unsigned char *)memory->malloc((long)(end - term_header));
index->read(block, (long)(end - term_header));

/*
	The first sizeof(int64_t) bytes of the header are the number of nodes in the root
*/
btree_nodes = (long)(get_long_long(block) + 1);		// +1 because were going to add a sentinal at the start
//printf("There are %ld nodes in the root of the btree\n", btree_nodes - 1);
block += sizeof(int64_t);
btree_root = (ANT_search_engine_btree_node *)memory->malloc((long)(sizeof(ANT_search_engine_btree_node) * (btree_nodes + 1)));	// +1 to null terminate (with the end of last block position)

/*
	Then we have a sequence of '\0' terminated string / offset pairs
	But first add the sentinal to the beginning (this is a one-off expense at startup)
*/
current = btree_root;
current->disk_pos = 0;
current->term = (char *)memory->malloc(2);
current->term[0] = '\0';
end_of_node_list = btree_root + btree_nodes;
for (current++; current < end_of_node_list; current++)
	{
	current->term = (char *)block;
	current->term_length = (long)strlen((char *)block);		// strings of no more than 4GB (to keep the structures small)
	while (*block != '\0')
		block++;
	block++;
	current->disk_pos = get_long_long(block);
	block += sizeof(current->disk_pos);
	}
current->term = NULL;
current->disk_pos = term_header;

/*
	Compute the size of the largest block and then allocate memory so that it will fit (and use that throughout the execution of this program)
*/
max_header_block_size = 0;
for (current = btree_root + 1; current < end_of_node_list; current++)
	{
	this_header_block_size = (current + 1)->disk_pos - current->disk_pos;
	if (this_header_block_size > max_header_block_size)
		max_header_block_size = this_header_block_size;
//	printf("%s : %lld (size:%lld bytes)\n", current->term, current->disk_pos, this_header_block_size);
	}
memory->realign();
btree_leaf_buffer = (unsigned char *)memory->malloc((long)max_header_block_size);

/*
	Allocate the accumulators array, the docid array, and the term_frequency array
*/
get_postings_details("~length", &collection_details);
documents = collection_details.document_frequency;

postings_buffer = (unsigned char *)memory->malloc(postings_buffer_length);
memory->realign();

/*
	Allocate space for decompression.
	NOTES:
		Add 512 because of the tf and 0 at each end of each impact ordered list.
		Further add ANT_COMPRESSION_FACTORY_END_PADDING so that compression schemes that don't know when to stop (such as Simple-9) can overflow without problems.
*/
decompress_buffer = (ANT_compressable_integer *)memory->malloc(sizeof(*decompress_buffer) * (512 + documents + ANT_COMPRESSION_FACTORY_END_PADDING));
memory->realign();

document_lengths = (ANT_compressable_integer *)memory->malloc(documents * sizeof(*document_lengths));

results_list = new (memory) ANT_search_engine_result(memory, documents);

/*
	decompress the document length vector
*/
get_postings(&collection_details, postings_buffer);
factory.decompress(decompress_buffer, postings_buffer, collection_details.document_frequency);

sum = 0;
for (current_length = 0; current_length < documents; current_length++)
	{
	decompressed_integer = decompress_buffer[current_length];
	sum += decompressed_integer;
	document_lengths[current_length] = decompressed_integer;
	}
mean_document_length = (double)sum / (double)documents;
collection_length_in_terms = sum;

memory->realign();
stem_buffer = (ANT_weighted_tf *)memory->malloc(stem_buffer_length_in_bytes = (sizeof(*stem_buffer) * documents));

stats_for_all_queries->add_disk_bytes_read_on_init(index->get_bytes_read());
}

/*
	ANT_SEARCH_ENGINE::~ANT_SEARCH_ENGINE()
	---------------------------------------
*/
ANT_search_engine::~ANT_search_engine()
{
index->close();
delete index;
delete stats;
delete stats_for_all_queries;
}

/*
	ANT_SEARCH_ENGINE::STATS_TEXT_RENDER()
	--------------------------------------
*/
void ANT_search_engine::stats_text_render(void)
{
stats->text_render();
}

/*
	ANT_SEARCH_ENGINE::STATS_ALL_TEXT_RENDER()
	------------------------------------------
*/
void ANT_search_engine::stats_all_text_render(void)
{
stats_for_all_queries->text_render();
}

/*
	ANT_SEARCH_ENGINE::STATS_INITIALISE()
	-------------------------------------
*/
void ANT_search_engine::stats_initialise(void)
{
stats->initialise();
}

/*
	ANT_SEARCH_ENGINE::STATS_ADD()
	------------------------------
*/
void ANT_search_engine::stats_add(void)
{
stats_for_all_queries->add(stats);
}

/*
	ANT_SEARCH_ENGINE::INIT_ACCUMULATORS()
	--------------------------------------
*/
#ifdef TOP_K_SEARCH
void ANT_search_engine::init_accumulators(long long top_k)
#else
void ANT_search_engine::init_accumulators(void)
#endif
{
long long now;

now = stats->start_timer();
#ifdef TOP_K_SEARCH
results_list->init_accumulators(top_k);
#else
results_list->init_accumulators();
#endif
stats->add_accumulator_init_time(stats->stop_timer(now));
}

/*
	ANT_SEARCH_ENGINE::GET_BTREE_LEAF_POSITION()
	--------------------------------------------
*/
long long ANT_search_engine::get_btree_leaf_position(char *term, long long *length, long *exact_match, long *btree_root_node)
{
long low, high, mid;

/*
	Binary search to find the block containing the term
*/
low = 0;
high = btree_nodes;
while (low < high)
	{
	mid = (low + high) / 2;
	if (strcmp(btree_root[mid].term, term) < 0)
		low = mid + 1;
	else
		high = mid;
	}
if ((low < btree_nodes) && (strcmp(btree_root[low].term, term) == 0))
	{
	/*
		Found, so we're either a short string or we're the name of a header
	*/
	*btree_root_node = low;
	*exact_match = TRUE;
	*length = btree_root[low + 1].disk_pos - btree_root[low].disk_pos;
	return btree_root[low].disk_pos;
	}
else
	{
	/*
		Not Found, so we're one past the header node
	*/
	*btree_root_node = low - 1;
	*exact_match = FALSE;
	*length = btree_root[low].disk_pos - btree_root[low - 1].disk_pos;
	return btree_root[low - 1].disk_pos;
	}
}

/*
	ANT_SEARCH_ENGINE::GET_LEAF()
	-----------------------------
*/
ANT_search_engine_btree_leaf *ANT_search_engine::get_leaf(unsigned char *leaf, long term_in_leaf, ANT_search_engine_btree_leaf *term_details)
{
long leaf_size;
unsigned char *base;

leaf_size = 28;		// length of a leaf node (sum of cf, df, etc. sizes)
base = leaf + leaf_size * term_in_leaf + sizeof(int32_t);		// sizeof(int32_t) is for the number of terms in the node
term_details->collection_frequency = get_long(base);
term_details->document_frequency = get_long(base + 4);
term_details->postings_position_on_disk = get_long_long(base + 8);
term_details->impacted_length = get_long(base + 16);
term_details->postings_length = get_long(base + 20);

return term_details;
}

/*
	ANT_SEARCH_ENGINE::GET_POSTINGS_DETAILS()
	-----------------------------------------
*/
ANT_search_engine_btree_leaf *ANT_search_engine::get_postings_details(char *term, ANT_search_engine_btree_leaf *term_details)
{
long long node_position, node_length;
long low, high, mid, nodes;
long leaf_size, exact_match, root_node_element;
size_t length_of_term;

if ((node_position = get_btree_leaf_position(term, &node_length, &exact_match, &root_node_element)) == 0)
	return NULL;		// before the first term in the term list

length_of_term = strlen(term);
if (length_of_term < B_TREE_PREFIX_SIZE)
	if (!exact_match)
		return NULL;		// we have a short string (less then the length of the head node) and did not find it as a node
	else
		term += length_of_term;
else
	if (strncmp(btree_root[root_node_element].term, term, B_TREE_PREFIX_SIZE) != 0)
		return NULL;		// there is no node in the list that starts with the head of the string.
	else
		term += B_TREE_PREFIX_SIZE;

index->seek(node_position);
index->read(btree_leaf_buffer, (long)node_length);
/*
	First 4 bytes are the number of terms in the node
	then there are N loads of:
		CF (4), DF (4), Offset_in_postings (8), DocIDs_Len (4), Postings_len (4), String_pos_in_node (4)
*/
low = 0;
high = nodes = (long)get_long(btree_leaf_buffer);
leaf_size = 28;		// length of a leaf node (sum of cf, df, etc. sizes)

while (low < high)
	{
	mid = (low + high) / 2;
	if (strcmp((char *)(btree_leaf_buffer + get_long(btree_leaf_buffer + (leaf_size * (mid + 1)))), term) < 0)
		low = mid + 1;
	else
		high = mid;
	}
if ((low < nodes) && (strcmp((char *)(btree_leaf_buffer + get_long(btree_leaf_buffer + (leaf_size * (low + 1)))), term) == 0))
	return get_leaf(btree_leaf_buffer, low, term_details);
else
	return NULL;
}

/*
	ANT_SEARCH_ENGINE::GET_POSTINGS()
	---------------------------------
*/
unsigned char *ANT_search_engine::get_postings(ANT_search_engine_btree_leaf *term_details, unsigned char *destination)
{
#ifdef SPECIAL_COMPRESSION
	ANT_compressable_integer *into;
	if (term_details->document_frequency <= 2)
		{
		/*
			We're about to generate the impact-ordering here and so we interlace TF, DOC-ID and 0s
		*/
		*destination = 0;		// no compression
		into = (ANT_compressable_integer *)(destination + 1);
		*into++ = term_details->postings_position_on_disk & 0xFFFFFFFF;
		*into++ = term_details->postings_position_on_disk >> 32;
		*into++ = 0;
		*into++ = term_details->postings_length;
		*into++ = term_details->impacted_length;
		*into++ = 0;
		term_details->impacted_length = term_details->document_frequency == 1 ? 3 : 6;
		}
	else
		{
		index->seek(term_details->postings_position_on_disk);
		index->read(destination, term_details->postings_length);
		}
#else
	index->seek(term_details->postings_position_on_disk);
	index->read(destination, term_details->postings_length);
#endif

return destination;
}

/*
	ANT_SEARCH_ENGINE::PROCESS_ONE_TERM()
	-------------------------------------
*/
ANT_search_engine_btree_leaf *ANT_search_engine::process_one_term(char *term, ANT_search_engine_btree_leaf *term_details)
{
ANT_search_engine_btree_leaf *verify;
long long now, bytes_already_read;

bytes_already_read = index->get_bytes_read();

now = stats->start_timer();
verify = get_postings_details(term, term_details);
stats->add_dictionary_lookup_time(stats->stop_timer(now));
if (verify == NULL)
	{
	/*
		The term was not found so set the collection frequency and document frequency to 0
	*/
	term_details->collection_frequency = 0;
	term_details->document_frequency = 0;
	}

stats->add_disk_bytes_read_on_search(index->get_bytes_read() - bytes_already_read);

return verify;
}

/*
	ANT_SEARCH_ENGINE::PROCESS_ONE_TERM_DETAIL()
	--------------------------------------------
*/
void ANT_search_engine::process_one_term_detail(ANT_search_engine_btree_leaf *term_details, ANT_ranking_function *ranking_function)
{
void *verify;
long long now, bytes_already_read;

if (term_details != NULL && term_details->document_frequency > 0)
	{
	bytes_already_read = index->get_bytes_read();
	now = stats->start_timer();
	verify = get_postings(term_details, postings_buffer);
	stats->add_posting_read_time(stats->stop_timer(now));
	if (verify != NULL)
		{
		now = stats->start_timer();
		factory.decompress(decompress_buffer, postings_buffer, term_details->impacted_length);
		stats->add_decompress_time(stats->stop_timer(now));

		now = stats->start_timer();
		ranking_function->relevance_rank_top_k(results_list, term_details, decompress_buffer, trim_postings_k);
		stats->add_rank_time(stats->stop_timer(now));
		}

	stats->add_disk_bytes_read_on_search(index->get_bytes_read() - bytes_already_read);
	}
}

/*
	ANT_SEARCH_ENGINE::PROCESS_ONE_SEARCH_TERM()
	--------------------------------------------
*/
void ANT_search_engine::process_one_search_term(char *term, ANT_ranking_function *ranking_function)
{
ANT_search_engine_btree_leaf term_details;

process_one_term_detail(process_one_term(term, &term_details), ranking_function);
}

/*
	ANT_SEARCH_ENGINE::PROCESS_ONE_STEMMED_SEARCH_TERM()
	----------------------------------------------------
*/
void ANT_search_engine::process_one_stemmed_search_term(ANT_stemmer *stemmer, char *base_term, ANT_ranking_function *ranking_function)
{
void *verify;
long long bytes_already_read;
ANT_search_engine_btree_leaf term_details, stemmed_term_details;
long long now, collection_frequency;
ANT_compressable_integer *current_document, *end;
long document, weight_terms;
char *term;
ANT_compressable_integer term_frequency;
ANT_weighted_tf tf_weight;
/*
	The way we stem is to load the terms that match the stem one at a time and to
	accumulate the term frequences into the stem_buffer accumulator list.  This then
	gets converted into a postings list and processed (ranked) as if a single search
	term within the search engine.
*/
verify = NULL;
bytes_already_read = index->get_bytes_read();

/*
	Initialise the term_frequency accumulators to all zero.
*/
now = stats->start_timer();
memset(stem_buffer, 0, (size_t)stem_buffer_length_in_bytes);
collection_frequency = 0;
stats->add_stemming_time(stats->stop_timer(now));

/*
	Find the first term that matches the stem
*/
now = stats->start_timer();
term = stemmer->first(base_term);
stats->add_dictionary_lookup_time(stats->stop_timer(now));

while (term != NULL)
	{
	/*
		get the location of the postings on disk
	*/
	now = stats->start_timer();
	stemmer->get_postings_details(&term_details); 
	stats->add_dictionary_lookup_time(stats->stop_timer(now));

	/*
		load the postings from disk
	*/
	now = stats->start_timer();
	verify = get_postings(&term_details, postings_buffer);
	stats->add_posting_read_time(stats->stop_timer(now));
	if (verify == NULL)
		break;

	/*
		Decompress the postings
	*/
	now = stats->start_timer();
	factory.decompress(decompress_buffer, postings_buffer, term_details.impacted_length);
	stats->add_decompress_time(stats->stop_timer(now));

	/*
		Add to the collecton frequency, the process the postings list
	*/
	now = stats->start_timer();
	collection_frequency += term_details.collection_frequency;

	current_document = decompress_buffer;
	end = decompress_buffer + term_details.impacted_length;
	weight_terms = stemmer->weight_terms(&tf_weight, term);
	while (current_document < end)
		{
		term_frequency = *current_document++;
		document = -1;
		while (*current_document != 0)
			{
			document += *current_document++;
			/*
				Only weight TFs if we're using floats, this makes no sense for integer tfs. 
			*/
			#ifdef USE_FLOATED_TF 
				if (weight_terms)
					stem_buffer[document] += term_frequency * tf_weight;
				else
					stem_buffer[document] += term_frequency;
			#else
				stem_buffer[document] += term_frequency;
			#endif
			}
		current_document++;
		}

	stats->add_stemming_time(stats->stop_timer(now));

	/*
		Now move on to the next term that matches the stem
	*/
	now = stats->start_timer();
	term = stemmer->next();
	stats->add_dictionary_lookup_time(stats->stop_timer(now));
	}

/*
	Finally, if we had no problem loading the search terms then 
	do the relevance ranking as if it was a single search term
*/
if (verify != NULL)
	{
	now = stats->start_timer();
	stemmed_term_details.collection_frequency = collection_frequency;
	ranking_function->relevance_rank_tf(results_list, &stemmed_term_details, stem_buffer, trim_postings_k);
	stats->add_rank_time(stats->stop_timer(now));
	}

stats->add_disk_bytes_read_on_search(index->get_bytes_read() - bytes_already_read);
}

/*
	ANT_SEARCH_ENGINE::SORT_RESULTS_LIST()
	--------------------------------------
*/
ANT_search_engine_accumulator **ANT_search_engine::sort_results_list(long long accurate_rank_point, long long *hits)
{
long long now;

now = stats->start_timer();
*hits = results_list->init_pointers();
stats->add_count_relevant_documents(stats->stop_timer(now));

/*
	Sort the postings into decreasing order, but only guarantee the first accurate_rank_point postings are accurately ordered (top-k sorting)
*/
now = stats->start_timer();

//qsort(accumulator_pointers, documents, sizeof(*accumulator_pointers), ANT_search_engine_accumulator::compare_pointer);
//top_k_sort(accumulator_pointers, documents, sizeof(*accumulator_pointers), ANT_search_engine_accumulator::compare_pointer);
ANT_search_engine_accumulator::top_k_sort(results_list->accumulator_pointers, *hits, accurate_rank_point);

stats->add_sort_time(stats->stop_timer(now));

return NULL;
}

/*
	ANT_SEARCH_ENGINE::GENERATE_RESULTS_LIST()
	------------------------------------------
*/
char **ANT_search_engine::generate_results_list(char **document_id_list, char **sorted_id_list, long long top_k)
{
long long found;
ANT_search_engine_accumulator **current, **end;

found = 0;
#ifdef TOP_K_SEARCH
end = results_list->accumulator_pointers + top_k;
#else
end = results_list->accumulator_pointers + documents;
#endif
for (current = results_list->accumulator_pointers; current < end; current++)
	if (!(*current)->is_zero_rsv())
		{
		if (found < top_k)		// first page
			sorted_id_list[found] = document_id_list[*current - results_list->accumulator];
		else
			break;
		found++;
		}

return sorted_id_list;
}

/*
	ANT_SEARCH_ENGINE::GET_DECOMPRESSED_POSTINGS()
	----------------------------------------------
*/
ANT_compressable_integer *ANT_search_engine::get_decompressed_postings(char *term, ANT_search_engine_btree_leaf *term_details)
{
void *verify;
long long now;

/*
	Find the term in the term vocab
*/
now = stats->start_timer();
verify = get_postings_details(term, term_details);
stats->add_dictionary_lookup_time(stats->stop_timer(now));
if (verify == NULL)
	return NULL;
/*
	Load the postings from disk
*/
now = stats->start_timer();
verify = get_postings(term_details, postings_buffer);
stats->add_posting_read_time(stats->stop_timer(now));
if (verify == NULL)
	return NULL;

/*
	And now decompress
*/
now = stats->start_timer();
factory.decompress(decompress_buffer, postings_buffer, term_details->impacted_length);
stats->add_decompress_time(stats->stop_timer(now));

return decompress_buffer;
} 
