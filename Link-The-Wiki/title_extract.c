/*
	TITLE_EXTRACT.C
	--------------
	Written (w) 2009 by Andrew Trotman & David Alexander, University of Otago
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../source/disk.h"
#include "link_parts.h"

char doc_title[1024];

/*
	STRIP_SPACE_INLINE()
	--------------------
*/
char *strip_space_inline(char *source)
{
char *end, *start = source;

while (isspace(*start))
	start++;

if (start > source)
	memmove(source, start, strlen(start) + 1);		// copy the '\0'

end = source + strlen(source) - 1;
while ((end >= source) && (isspace(*end)))
	*end-- = '\0';

return source;
}

/*
	MAIN()
	------
*/
int main(int argc, char *argv[])
{
ANT_disk disk;
char *file, *start, *end, *from;
long param, file_number, current_docid;

if (argc < 2)
	exit(printf("Usage:%s <filespec> ...\n", argv[0]));

file_number = 1;
for (param = 1; param < argc; param++)
	{
	file = disk.read_entire_file(disk.get_first_filename(argv[param]));
	while (file != NULL)
		{
		current_docid = get_doc_id(file);
		from = file;

                if ((start = strstr(from, "<name")) != NULL) {
                   if ((end = strstr(start, "</name>")) != NULL) {
                      start = strchr(start, '>') + 1;
                      strncpy(doc_title, start, end - start);
                      doc_title[end - start] = '\0';
                      
                      printf("%ld:%ld:%s\n", current_docid, current_docid, doc_title);
                   }
                }

		if (file_number % 1000 == 0)
			fprintf(stderr, "Files processed:%d\n", file_number);
		file_number++;

		delete [] file;
		file = disk.read_entire_file(disk.get_next_filename());
		}
	}

fprintf(stderr, "%s Completed\n", argv[0]);
return 0;
}
