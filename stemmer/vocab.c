#include <string.h>
#include <stdlib.h>
#include "vocab.h"
#include "btree_iterator.h"
#include "ga_individual.h"

const int MAX_TRIE_DEPTH = 7; 	/* Trie holds MAX_TRIE_DEPTH chars */
const int ALPHABET_SIZE = 26;
const int ARBITRARY_NUMBER = 300;

class trie_stats {
 private:
	int freq[MAX_TRIE_DEPTH][ARBITRARY_NUMBER];

 public:
	trie_stats() {
		int i, j;
		for (i = 0; i < MAX_TRIE_DEPTH; i++) 
			for (j = 0; j < ARBITRARY_NUMBER; j++) 
				freq[i][j] = 0;
	}

	inline void add(int depth, int cum_freq) {
		if (cum_freq >= ARBITRARY_NUMBER)
			cum_freq = ARBITRARY_NUMBER - 1;
		if (depth >= MAX_TRIE_DEPTH) {
			fprintf(stderr, "Trie stats are doing things they shouldn't; depth = %d\n", depth);
			return;
		}
		freq[depth][cum_freq]++;
	}
	void print() {
		int i,j;
		for (i = 0; i < MAX_TRIE_DEPTH; i++) {
			printf("-- DEPTH %d --\n", i);
			for (j = 0; j < ARBITRARY_NUMBER; j++)
				printf("%d : %d\n", j, freq[i][j]);
		}
	}
};

/* 
 * I store a trie, starting from the endings of words.
 * up to a maximum depth. Each node contains the cumulative
 * frequency of each occurrance. 
 */
class trie_node {
    public:
        trie_node *child[ALPHABET_SIZE];
        int cum_freq;
        
        trie_node() {
            int i;
            for (i = 0; i < ALPHABET_SIZE; i++)
                this->child[i] = NULL;
            this->cum_freq = 0;
        } 

		~trie_node() {
			int i;
			for (i = 0; i < ALPHABET_SIZE; i++)
				if (child[i])
					delete child[i];
		}

        void add(char *word) {
			internal_add(word, strlen(word), 0);
        }

		void print() {
			char buffer[MAX_TRIE_DEPTH + 1];
			buffer[MAX_TRIE_DEPTH] = '\0';
			internal_print(buffer + MAX_TRIE_DEPTH, 0);
		}

		void print_stats() {
			trie_stats *stats = new trie_stats();
			internal_stats(stats, 0);
			stats->print();
			delete stats;
		}

		/* Removes leaves from the trie with frequency under a certain amount */
		void trim(int level) {
			internal_trim(level);
		}

 private:
        void internal_add(char *word, int pos, int depth) {
            this->cum_freq++; 
            
            if (pos == 0)
                return;
            if (depth >= MAX_TRIE_DEPTH)
                return;
            if (this->child[word[pos-1] - 'a'] == NULL)
                this->child[word[pos-1] - 'a'] = new trie_node();
            this->child[word[pos-1] - 'a']->internal_add(word, pos - 1, depth + 1);
        }

        void internal_print(char *me, int depth) {
            int i;
			if (depth > MAX_TRIE_DEPTH)
				return;
            printf("%d * '%s'\n", cum_freq, me);
            for (i = 0; i < ALPHABET_SIZE; i++) {
                if (child[i]) {
					me[-1] = i + 'a';
                    child[i]->internal_print(me - 1, depth + 1);
				}
            }
        }

        void internal_stats(trie_stats *stats, int depth) {
			int i;
			if (depth > 0)
				stats->add(depth - 1, cum_freq);
			for (i = 0; i < ALPHABET_SIZE; i++) 
				if (child[i])
                    child[i]->internal_stats(stats, depth + 1);
		}

		int internal_trim(int level) {
			int i;

			for (i = 0; i < ALPHABET_SIZE; i++) {
				if (child[i] && level >= child[i]->internal_trim(level)) {
					delete child[i];
					child[i] = NULL;
				}
			}
			return cum_freq;
		}
};

/*
  Detect if a word contains non-alpha characters.
*/
int contains_non_alpha(char *s) {
	while (*s != '\0') {
		if (*s < 'a' || *s > 'z')
			return TRUE;
		s++;
	}
	return FALSE;
}


/*
  We are not interested in words that contain non-alpha chars.
 */
Vocab::Vocab(ANT_search_engine *search_engine) {
    char *term;
    ANT_btree_iterator iterator(search_engine);

    this->strings = NULL;
    this->string_count = 0;

#ifdef USE_STR_LIST
    int i;

    for (string_count = 0, iterator.first(NULL); (term = iterator.next()) != NULL; string_count++)
		if (contains_non_alpha(term))
			string_count--;

	printf("str_count:%d\n", string_count);

    strings = (char **) malloc(sizeof(strings[0]) * string_count);
    for (i = 0, term = iterator.first(NULL); term != NULL; term = iterator.next()) {
		if (!contains_non_alpha(term)) {
			if(i%100 == 0)
				printf("%s\n", term);
			strings[i] = strdup(term);
			i++;
		}
	}
#else
	trie = new trie_node();
    for (iterator.first(NULL); (term = iterator.next()) != NULL;)
		if (!contains_non_alpha(term))
			trie->add(term);
#endif
}

/* 
   TRIM()
   
   removes all words in the trie that do not occur at least level times.
 */
void Vocab::trim(int level) {
	trie->trim(level);
}

void Vocab::print() {
	if (trie)
		trie->print();
	else
		printf("Vocab is empty\n");
}

void Vocab::print_stats() {
	if (trie)
		trie->print_stats();
	else
		printf("Vocab is empty\n");
}

/* Does not allocate a string, do not free */
char *Vocab::strgen() {
    char *str = strings[rand() % string_count];
    int len = strlen(str);
    str += len;
    if (len > RULE_STRING_MAX)
        len = RULE_STRING_MAX;
    return str - (rand() % len + 1);
}

/* Does not allocate a string, do not free */
char *Vocab::strgen_2() {
    char *str = strings[rand() % string_count];
    int len = strlen(str);
    str += len;
    if (len > RULE_STRING_MAX)
        len = RULE_STRING_MAX;
    return str - rand() % (len + 1);
}
