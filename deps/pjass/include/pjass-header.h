#ifndef PJASS_EXPORT_H
#define PJASS_EXPORT_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

int _cdecl parse_jass(char *output, int n_max_out_size, int *n_out_size, char* target, const int target_size);
int _cdecl parse_jass_triad(char *output, int n_max_out_size, int *n_out_size, char* buf_common, const int size_common, char* buf_blizz, const int size_blizz, char* buf_script, const int size_script);
int _cdecl parse_jass_custom_r(char *output, int n_max_out_size, int *n_out_size, const int targets_or_opts_count, const char **targets_or_opts);
int _cdecl parse_jass_custom(char *output, int n_max_out_size, int *n_out_size, const int targets_count, const int *targets_sizes, char **targets, char **flags);
/*
int _cdecl parse_jass_custom_s(char *output, int n_max_out_size, int *n_out_size, const int targets_count, const int *targets_sizes, const char **targets, const char **flags);
*/

#endif

#ifdef __cplusplus
}
#endif // PJASS_EXPORT_H
