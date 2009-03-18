/*
	FILE_INTERNALS.H
	----------------
*/

#ifndef __FILE_INTERNALS_H__
#define __FILE_INTERNALS_H__

#ifdef linux
	#ifndef _LARGEFILE_SOURCE
		#define _LARGEFILE_SOURCE
	#endif
	#ifndef _LARGEFILE64_SOURCE
		#define _LARGEFILE64_SOURCE
	#endif
	#define FILE_OFFSET_BITS 64
	#define ftell ftello
	#define fseek fseeko
	#define fstat fstat64
	#define stat stat64
#endif

#ifdef _MSC_VER
	#include <windows.h>
#else
	#include <stdio.h>
#endif

class ANT_file_internals
{
public:
	#ifdef _MSC_VER
		HANDLE fp;
	#else
        FILE *fp;
	#endif
public:
	ANT_file_internals();
#ifdef _MSC_VER
	static int read_file_64(HANDLE fp, void *destination, long long bytes_to_read);
	static int write_file_64(HANDLE fp, void *destination, long long bytes_to_write);
#else
	static int read_file_64(FILE *fp, void *destination, long long bytes_to_read);
	static int write_file_64(FILE *fp, void *destination, long long bytes_to_write);
#endif
} ;

#endif __FILE_INTERNALS_H__