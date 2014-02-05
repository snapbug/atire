/*
	DIRECTORY_ITERATOR_TAR.C
	------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include "str.h"
#include "pragma.h"
#include "ctypes.h"
#include "directory_iterator_tar.h"

long ANT_directory_iterator_tar::tid = 0;

/*
	ANT_DIRECTORY_ITERATOR_TAR::ANT_DIRECTORY_ITERATOR_TAR()
	--------------------------------------------------------
*/
ANT_directory_iterator_tar::ANT_directory_iterator_tar(ANT_instream *source, long get_file) : ANT_directory_iterator("", get_file)
{
	this->source = source;

message = new char[50];
sprintf(message, "ANT_directory_iterator_tar %ld ", ANT_directory_iterator_tar::tid++);
}

/*
	ANT_DIRECTORY_ITERATOR_TAR::FILENAME()
	--------------------------------------
*/
void ANT_directory_iterator_tar::filename(ANT_directory_iterator_object *object)
{
if (header.filename_prefix[0] != '\0')
	{
	object->filename = new char[strlen(header.filename_prefix) + 1 + strlen(header.filename) + 1];
	sprintf(object->filename, "%s/%s", header.filename_prefix, header.filename);
	}
else
	object->filename = strnew(header.filename);
}

/*
	ANT_DIRECTORY_ITERATOR_TAR::DETOX()
	-----------------------------------
*/
inline long ANT_directory_iterator_tar::detox(char *octal)
{
long result;
char *end;

if (*octal == ' ')
	{
	end = octal + 11;		// the spec states 12 bytes of which the final must be a space or '\0'
	result = 0;
	while (!ANT_isdigit(*octal) && octal < end)
		octal++;
	while (octal < end)
		result = result * 8 + (*octal++ - '0');
	return result;
	}
else
	{
	result = *octal++ - '0';
	result = result * 8 + (*octal++ - '0');
	result = result * 8 + (*octal++ - '0');
	result = result * 8 + (*octal++ - '0');
	result = result * 8 + (*octal++ - '0');
	result = result * 8 + (*octal++ - '0');
	result = result * 8 + (*octal++ - '0');
	result = result * 8 + (*octal++ - '0');
	result = result * 8 + (*octal++ - '0');
	result = result * 8 + (*octal++ - '0');
	return result * 8 + (*octal - '0');
	}
}

/*
	ANT_DIRECTORY_ITERATOR_TAR::FIRST()
	-----------------------------------
*/
ANT_directory_iterator_object *ANT_directory_iterator_tar::first(ANT_directory_iterator_object *object)
{
bytes_read = 0;
if (source->read((unsigned char *)&header, TAR_BLOCK_SIZE) == TAR_BLOCK_SIZE)
	{
	printf("%sstart_process %lld\n", message, clock->start_timer());
	length_of_file_in_bytes = detox(header.filesize_in_bytes);
	if (header.file_type == '0' || header.file_type == 0)
		{
		filename(object);		// we're a file
		if (get_file)
			read_entire_file(object);
		printf("%send_process %lld\n", message, clock->start_timer());
		return object;
		}
	else
		{
		printf("%send_process %lld\n", message, clock->start_timer());
		return next(object);			// we're not a file so go and get one
		}
	}

return NULL;
}

/*
	ANT_DIRECTORY_ITERATOR_TAR::NEXT()
	----------------------------------
*/
ANT_directory_iterator_object *ANT_directory_iterator_tar::next(ANT_directory_iterator_object *object)
{
long long extras;
printf("%sstart_process %lld\n", message, clock->start_timer());

/*
	read up to the end of the current block
*/
if (bytes_read % TAR_BLOCK_SIZE != 0)
	if ((extras = TAR_BLOCK_SIZE - (bytes_read % TAR_BLOCK_SIZE)) != 0)
		{
		printf("%send_process %lld\n", message, clock->start_timer());
		source->read(buffer, extras);
		printf("%sstart_process %lld\n", message, clock->start_timer());
		}

/*
	If we're not at the end of the file then read one block at a time until we are.
*/
while (bytes_read < length_of_file_in_bytes)
	{
	printf("%send_process %lld\n", message, clock->start_timer());
	source->read(buffer, TAR_BLOCK_SIZE);				// read to end of file
	printf("%sstart_process %lld\n", message, clock->start_timer());
	bytes_read += TAR_BLOCK_SIZE;
	}

/*
	We've read no bytes from the current file
*/
bytes_read = 0;

/*
	Read the header of the next file
*/
printf("%send_process %lld\n", message, clock->start_timer());
if (source->read((unsigned char *)&header, TAR_BLOCK_SIZE) == TAR_BLOCK_SIZE)
	{
	printf("%sstart_process %lld\n", message, clock->start_timer());
	if (header.filename[0] == '\0')
		{
		printf("%send_process %lld\n", message, clock->start_timer());
		return NULL;		// at end of archive (kinda-sorta, there should be two of these)
		}

	length_of_file_in_bytes = detox(header.filesize_in_bytes);
	if (header.file_type == '0' || header.file_type == '\0')
		{
		filename(object);		// we're a file
		if (get_file)
			read_entire_file(object);
		printf("%send_process %lld\n", message, clock->start_timer());
		return object;
		}
	else
		{
		printf("%send_process %lld\n", message, clock->start_timer());
		return next(object);			// we're not a file so go and get one
		}
	}

/*
	Read has failed so we are probably at EOF (and the TAR file is broken)
*/
return NULL;
}

/*
	ANT_DIRECTORY_ITERATOR_TAR::READ_ENTIRE_FILE()
	----------------------------------------------
*/
void ANT_directory_iterator_tar::read_entire_file(ANT_directory_iterator_object *object)
{
long long bytes_got;

object->file =  new (std::nothrow) char [(size_t)(length_of_file_in_bytes + 1)];
if (object->file == NULL)
	exit(printf("Out of memory trying to read a document from a TAR file to index (%lld bytes requested)\n", (long long)length_of_file_in_bytes + 1));
if ((bytes_got = source->read((unsigned char *)object->file, length_of_file_in_bytes)) != length_of_file_in_bytes)
	exit(printf("The TAR file is corrupt (%lld bytes requested, %lld bytes returned)\n", (long long)length_of_file_in_bytes, bytes_got));
object->file[length_of_file_in_bytes] = '\0';			// NULL terminate the contents of the file.
object->length = bytes_read = length_of_file_in_bytes;
}

