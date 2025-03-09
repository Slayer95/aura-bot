#ifndef PJASS_EXPORT_H
#define PJASS_EXPORT_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

int _cdecl parse_jass_files(int file_count, const char **file_paths, char *output, int n_max_out_size, int *n_out_size);

#endif

#ifdef __cplusplus
}
#endif // PJASS_EXPORT_H
