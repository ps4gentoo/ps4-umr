/*
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Tom St Denis <tom.stdenis@amd.com>
 */

#include "umr.h"

/**
 * @brief Opens a database file from various possible locations.
 *
 * This function attempts to open a specified database file using different methods:
 * 1. Directly from the provided filename.
 * 2. From a specified path combined with the filename.
 * 3. Using an environment variable path (`UMR_DATABASE_PATH`).
 * 4. Using a predefined directory (`UMR_DB_DIR`), if defined.
 * 5. Using the CMake source directory combined with `/database/`.
 *
 * @param path     The base path where the database file might be located (can be NULL).
 * @param filename The name of the database file to open.
 * @param binary   Flag indicating whether to open the file in binary mode (`1` for binary, `0` for text).
 * @return A pointer to the opened FILE structure if successful, or NULL if all attempts fail.
 */
FILE *umr_database_open(char *path, char *filename, int binary)
{
	FILE *f;
	char p[512];
	const char* mode = binary ? "rb" : "r";

	// 1. try to open it directly
	f = fopen(filename, mode);
	if (f)
		return f;

	// 2. if there is a path option used try that
	if (path && strlen(path)) {
		char *s = (path[strlen(path)-1] == '/') ? "" : "/";
		sprintf(p, "%s%s%s", path, s, filename);
		f = fopen(p, mode);
		if (f)
			return f;
	}

	// 3. try using an environment path
	path = getenv("UMR_DATABASE_PATH");
	if (path) {
		char *s = (path[strlen(path)-1] == '/') ? "" : "/";
		sprintf(p, "%s%s%s", path, s, filename);
		f = fopen(p, mode);
		if (f)
			return f;
	}

	// 4. try using UMR_DB_DIR define
#ifdef UMR_DB_DIR
	sprintf(p, "%s%s", UMR_DB_DIR, filename);
	f = fopen(p, mode);
	if (f)
		return f;
#endif

	// 5. try using CMAKE_SOURCE_DIR/database
	sprintf(p, "%s/database/%s", UMR_SOURCE_DIR, filename);
	return fopen(p, mode);
}
