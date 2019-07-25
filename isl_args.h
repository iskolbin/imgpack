#ifndef ISL_ARGS_H_
#define ISL_ARGS_H_

/* 
 isl_args.h - v0.3.0
 public domain library for parsing command line arguments;
 supports integers using IA_STRTOL, floats using IA_STRTOD, strings and flags

 author: Ilya Kolbin (iskolbin@gmail.com)
 url: github.com:iskolbin/isl_args.h

 LICENSE

 See end of file for license information.
*/

#ifndef IA_NO_ERRNO
	#include <errno.h>
	#define IA_ERRNO errno
	#define IA_ERANGE ERANGE
#else
/* No check at all */
	#define IA_ERRNO 0
	#define IA_ERANGE 1
#endif

#ifndef IA_NO_STDIO
	#include <stdio.h>
	#define IA_PRINTF printf
#else
	#ifndef IA_PRINTF
		#error "IA_PRINTF not defined"
	#endif
#endif

#ifndef IA_NO_STDLIB
	#include <stdlib.h>
	#define IA_STRTOL strtol
	#define IA_STRTOD strtod
#else
	#ifndef IA_STRTOL
		#error "IA_STRTOL not defined"
	#endif
	#ifndef IA_STRTOD
		#error "IA_STRTOD not defined"
	#endif
#endif

#define IA_BEGIN(argc, argv, helpkey, helpshort, helpmsg) {\
	char **args_parse_v = (argv);\
	int args_parse_unrecognized = 1;\
	for ( int args_parse_i = 0; args_parse_i < argc; args_parse_i++ ) {\
		if ( argc == 1 || !strcmp( args_parse_v[args_parse_i], helpkey ) || !strcmp( args_parse_v[args_parse_i], helpshort )) {\
			printf( helpmsg );\
			return 0;\
		} else if ( args_parse_i == 0 ) args_parse_i++;

#define IA_INT(longname, shortname, var) \
	if ( !strcmp( args_parse_v[args_parse_i], longname ) || !strcmp( args_parse_v[args_parse_i], shortname )) {\
		args_parse_unrecognized = 0;\
		var = IA_STRTOL( argv[++args_parse_i], NULL, 10 );\
		if ( IA_ERRNO == IA_ERANGE ) {\
			IA_PRINTF( "Bad value for parameter %s or %s\n", longname, shortname );\
			return 1;\
		}\
	}

#define IA_FLOAT(longname, shortname, var) \
	if ( !strcmp( args_parse_v[args_parse_i], longname ) || !strcmp( args_parse_v[args_parse_i], shortname )) {\
		args_parse_unrecognized = 0;\
		var = IA_STRTOD( args_parse_v[++args_parse_i], NULL );\
		if ( IA_ERRNO == IA_ERANGE ) {\
			IA_PRINTF( "Bad value for parameter %s or %s\n", longname, shortname );\
			return 1;\
		}\
	}

#define IA_STR(longname, shortname, var) \
	if ( !strcmp( args_parse_v[args_parse_i], longname ) || !strcmp( args_parse_v[args_parse_i], shortname )) {\
		args_parse_unrecognized = 0;\
		var = args_parse_v[++args_parse_i];\
	}

#define IA_FLAG(longname, shortname, var) \
	if (( !strcmp( args_parse_v[args_parse_i], longname ) || !strcmp( args_parse_v[args_parse_i], shortname ))) {\
		args_parse_unrecognized = 0;\
		var = 1;\
	}\

#define IA_END \
	if ( args_parse_unrecognized ) {\
		IA_PRINTF( "Unrecognized parameter %s\n", args_parse_v[args_parse_i] );\
		return 2;\
	}\
}}
/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2018 Ilya Kolbin
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
#endif // ISL_ARGS_H_
