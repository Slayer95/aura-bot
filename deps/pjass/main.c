#include <errno.h>

#include "hashtable.h"
#include "token.yy.h"
#include "grammar.tab.h"

#include "misc.h"

#ifndef VERSIONSTR
#define VERSIONSTR "1.0-git"
#endif

#include "main.h"

static struct typenode* addPrimitiveType(const char *name)
{
    struct typenode *newty= newtypenode(name, NULL);
    ht_put(&builtin_types, name, newty);
    return newty;
}


static char **flags_in_order;
static int count_flags_in_order;
static int limit_flags_in_order;

static int add_flag(struct hashtable *available_flags, struct hashtable *flags_helpstring, char *name, void *data, const char *helptext)
{
    ht_put(available_flags, name, data);
    ht_put(flags_helpstring, name, (void*)helptext);

    if( count_flags_in_order+1 >= limit_flags_in_order )
    {
        limit_flags_in_order++;
        char **new_ptr = realloc(flags_in_order, sizeof(char*) * limit_flags_in_order);
        if( new_ptr ){
            flags_in_order = new_ptr;
        }else{
            return 0;
        }
    }
    flags_in_order[count_flags_in_order++] = name;
    return 1;
}

static int init(char *n_output, int n_max_out_size)
{
    ht_init(&builtin_types, 1 << 4);
    ht_init(&functions, 1 << 13);
    ht_init(&globals, 1 << 13);
    ht_init(&locals, 1 << 6);
    ht_init(&types, 1 << 7);
    ht_init(&initialized, 1 << 11);

    ht_init(&bad_natives_in_globals, 1 << 4);
    ht_init(&shadowed_variables, 1 << 4);

    ht_init(&uninitialized_globals, 1 << 11);

    ht_init(&string_literals, 1 << 10);

    tree_init(&stringlit_hashes);

    gHandle = addPrimitiveType("handle");
    gInteger = addPrimitiveType("integer");
    gReal = addPrimitiveType("real");
    gBoolean = addPrimitiveType("boolean");
    gString = addPrimitiveType("string");
    gCode = addPrimitiveType("code");

    gCodeReturnsBoolean = newtypenode("codereturnsboolean", gCode);
    gCodeReturnsNoBoolean = newtypenode("codereturnsboolean", gCode);

    gNothing = newtypenode("nothing", NULL);
    gNull = newtypenode("null", NULL);

    gAny = newtypenode("any", NULL);
    gNone = newtypenode("none", NULL);
    gEmpty = newtypenode("empty", NULL);

    pjass_flags = 0;

    fno = 0;
    fnannotations = 0;
    annotations = 0;
    haderrors = 0;
    ignorederrors = 0;
    islinebreak = 1;
    inblock = false;
    isconstant = false;
    inconstant = false;
    infunction = false;

    fCurrent = NULL;
    fFilter = NULL;
    fCondition = NULL;
    fStringHash = NULL;

    ht_init(&available_flags, 16);
    ht_init(&flags_helpstring, 16);

    output = n_output;
    max_out_size = n_max_out_size;
    out_size = 0;
    abort_parse = 0;

    limit_flags_in_order = 10;
    count_flags_in_order = 0;
    flags_in_order = malloc(sizeof(char*) * limit_flags_in_order);

    if (flags_in_order == NULL) {
      return 1;
    }

    if (
      (!add_flag(&available_flags, &flags_helpstring, "rb", (void*)flag_rb, "Toggle returnbug checking")) ||
      (!add_flag(&available_flags, &flags_helpstring, "shadow", (void*)flag_shadowing, "Toggle error on variable shadowing")) ||
      (!add_flag(&available_flags, &flags_helpstring, "filter", (void*)flag_filter,"Toggle error on inappropriate code usage for Filter")) ||
      (!add_flag(&available_flags, &flags_helpstring, "noruntimeerror", (void*)flag_runtimeerror, "Toggle runtime error reporting")) ||
      (!add_flag(&available_flags, &flags_helpstring, "checkglobalsinit", (void*)flag_checkglobalsinit, "Toggle a very bad checker for uninitialized globals usage")) ||
      (!add_flag(&available_flags, &flags_helpstring, "checkstringhash", (void*)flag_checkstringhash, "Toggle StringHash collision checking")) ||
      (!add_flag(&available_flags, &flags_helpstring, "nomodulooperator", (void*)flag_nomodulo, "Error when using the modulo-operator '%'")) ||
      (!add_flag(&available_flags, &flags_helpstring, "checklongnames", (void*)flag_verylongnames, "Error on very long names")) ||
      (!add_flag(&available_flags, &flags_helpstring, "nosyntaxerror", (void*)flag_syntaxerror, "Toggle syntax error reporting")) ||
      (!add_flag(&available_flags, &flags_helpstring, "nosemanticerror", (void*)flag_semanticerror, "Toggle semantic error reporting")) ||
      (!add_flag(&available_flags, &flags_helpstring, "checknumberliterals", (void*)flag_checknumberliterals, "Error on overflowing number literals")) ||
      (!add_flag(&available_flags, &flags_helpstring, "oldpatch", (void*)(flag_verylongnames | flag_nomodulo | flag_filter | flag_rb), "Combined options for older patches")) ||
      (!add_flag(&available_flags, &flags_helpstring, "f", (void*)(flag_filesystem), "Toggle treating the argument as a file path"))
    ) {
      return 1;
    }


    ht_put(&bad_natives_in_globals, "OrderId", (void*)NullInGlobals);
    ht_put(&bad_natives_in_globals, "OrderId2String", (void*)NullInGlobals);
    ht_put(&bad_natives_in_globals, "UnitId2String", (void*)NullInGlobals);

    ht_put(&bad_natives_in_globals, "GetObjectName", (void*)CrashInGlobals);
    ht_put(&bad_natives_in_globals, "CreateQuest", (void*)CrashInGlobals);
    ht_put(&bad_natives_in_globals, "CreateMultiboard", (void*)CrashInGlobals);
    ht_put(&bad_natives_in_globals, "CreateLeaderboard", (void*)CrashInGlobals);
    ht_put(&bad_natives_in_globals, "CreateRegion", (void*)CrashInGlobals);

    return 0;
}

static void printversion()
{
    printf("Pjass version %s by Rudi Cilibrasi, modified by AIAndy, PitzerMike, Deaod and lep\n", VERSIONSTR);
}

static void printhelp()
{
    printversion();
    printf("\n");
    printf(
        "To use this program, list the files you would like to parse in order\n"
        "If you would like to parse from standard input (the keyboard), then\n"
        "use - as an argument.  If you supply no arguments to pjass, it will\n"
        "parse the console standard input by default.\n"
        "The most common usage of pjass would be like this:\n"
        "pjass common.j Blizzard.j war3map.j\n"
        "\n"
        "pjass accepts some flags:\n"
        "%-20s print this help message and exit\n"
        "%-20s print pjass version and exit\n"
        "\n"
        "But pjass also allows to toggle some flags with either + or - in front of them.\n"
        "Once a flag is activated it will stay on until disabled and then it will stay disabled\n"
        "until potentially turned on again, and so on, and so forth.\n"
        "Usage could look like this:\n"
        "\n"
        "\tpjass +shadow file1.j +rb file2.j -rb file3.j\n"
        "\n"
        "Which would check all three files with shadow enabled and only file2 with rb enabled.\n"
        "Below you can see a list of all available options. They are all off by default.\n"
        "\n"
        , "-h", "-v"
    );

    int i;
    for(i = 0; i < count_flags_in_order; i++ ) {
        const char *name = flags_in_order[i];
        const char *helpstr = ht_lookup(&flags_helpstring, name);
        if( helpstr ){
            printf("%-20s %s\n", name, helpstr);
        }else{
            printf("%-20s\n", name);
        }
    }
}

static void dofile(FILE *fp, const char *name)
{
    lineno = 1;
    islinebreak = 1;
    isconstant = false;
    inconstant = false;
    inblock = false;
    encoutered_first_function = false;
    inglobals = false;
    YY_BUFFER_STATE buffer_state = yy_create_buffer(fp, BUFSIZE);
    yy_switch_to_buffer(buffer_state);
    curfile = name;
    while ( yyparse() ) ;
    totlines += lineno;
    fno++;
    yy_delete_buffer(buffer_state);
}

static void dobuffer(char* buf, const int buf_size)
{
    lineno = 1;
    islinebreak = 1;
    isconstant = false;
    inconstant = false;
    inblock = false;
    encoutered_first_function = false;
    inglobals = false;
    YY_BUFFER_STATE buffer_state = yy_scan_bytes(buf, buf_size);
    curfile = ".j";
    while ( yyparse() ) ;
    totlines += lineno;
    fno++;
    yy_delete_buffer(buffer_state);
}

static void tryfile(const char *name)
{
    FILE *fp;
#ifdef _MSC_VER
    errno_t err = fopen_s(&fp, name, "rb");
    if (err != 0) {
#else
    fp = fopen(name, "rb");
    if (fp == NULL) {
#endif
        haderrors++;
        return;
    }
    dofile(fp, name);
    fclose(fp);
    didparse = 1;
}

static void trybuffer(char* buf, const int buf_size)
{
  dobuffer(buf, buf_size);
  didparse = 1;
}

static void doparse(const int targets_count, const int* targets_sizes, char **targets, char **flags)
{
    int i;
    char *part;
    char *flags_sep = " ";

    for (i = 0; i < targets_count; ++i) {
#ifdef _MSC_VER
        char *ctx;
        for(part = strtok_s(flags[i], flags_sep, &ctx), pjass_flags = 0; part; part = strtok_s(NULL, flags_sep, &ctx)){
#else
        for(part = strtok(flags[i], flags_sep), pjass_flags = 0; part; part = strtok(NULL, flags_sep)){
#endif
            pjass_flags = updateflag(pjass_flags, part, &available_flags);
        }

        if ( flagenabled(flag_filesystem) ) {
          tryfile(targets[i]);
        } else {
          trybuffer(targets[i], targets_sizes[i]);
        }
    }
}

static void doparse_r(int _argc, const char **_argv)
{
    int i;
    for (i = 0; i < _argc; ++i) {
#ifdef PJASS_STANDALONE
        if( _argv[i][0] == '-' && _argv[i][1] == 0 ) {
            dofile(stdin, "<stdin>");
            didparse = 1;
            continue;
        }
        if( strcmp(_argv[i], "-h") == 0 ) {
            printhelp();
            exit(0);
        }
        if( strcmp(_argv[i], "-v") == 0 ) {
            printf("%s version %s\n", "pjass", VERSIONSTR);
            exit(0);
        }
#else
        if( strcmp(_argv[i], "+f") == 0 ){
            continue;
        }
#endif
        if( isflag(_argv[i], &available_flags)){
            pjass_flags = updateflag(pjass_flags, _argv[i], &available_flags);
            continue;
        }

        tryfile(_argv[i]);
    }
}

static int run_checker_output(char* output, int n_max_out_size, int *n_out_size, char* buffer)
{
    int errcode = 0;
    if (!haderrors && didparse) {
        *n_out_size = _snprintf_s(output, n_max_out_size, n_max_out_size - 1, "Parse successful: %8d lines: %s", totlines, "<total>");
    } else {
        if (haderrors) {
            *n_out_size = _snprintf_s(output, n_max_out_size, n_max_out_size - 1, "Parse failed: %d error%s total\n%s", haderrors, haderrors == 1 ? "" : "s", buffer);
        } else {
            *n_out_size = _snprintf_s(output, n_max_out_size, n_max_out_size - 1, "Parse failed");
        }
        errcode = 1;
    }
    if (*n_out_size == -1) *n_out_size = n_max_out_size - 1;
    return errcode;
}

#ifndef PJASS_STANDALONE
int _cdecl parse_jass(char *output, int n_max_out_size, int *n_out_size, char* target, const int target_size)
{
    if (n_max_out_size < 0) n_max_out_size = 0; // _snprintf_s returns -1
    char buffer[8192];
    int errcode = init(buffer, sizeof(buffer) - 1);
    if (errcode != 0) {
      return errcode;
    }
    dobuffer(target, target_size);
    return run_checker_output(output, n_max_out_size, n_out_size, buffer);
}

int _cdecl parse_jass_triad(char *output, int n_max_out_size, int *n_out_size, char* buf_common, const int size_common, char* buf_blizz, const int size_blizz, char* buf_script, const int size_script)
{
    if (n_max_out_size < 0) n_max_out_size = 0; // _snprintf_s returns -1
    char buffer[8192];
    int errcode = init(buffer, sizeof(buffer) - 1);
    if (errcode != 0) {
      return errcode;
    }
    dobuffer(buf_common, size_common);
    dobuffer(buf_blizz, size_blizz);
    dobuffer(buf_script, size_script);
    return run_checker_output(output, n_max_out_size, n_out_size, buffer);
}

int _cdecl parse_jass_custom_r(char *output, int n_max_out_size, int *n_out_size, const int targets_or_opts_count, const char **targets_or_opts)
{
    if (n_max_out_size < 0) n_max_out_size = 0; // _snprintf_s returns -1
    char buffer[8192];
    int errcode = init(buffer, sizeof(buffer) - 1);
    if (errcode != 0) {
      return errcode;
    }
    doparse_r(targets_or_opts_count, targets_or_opts);
    return run_checker_output(output, n_max_out_size, n_out_size, buffer);
}

int _cdecl parse_jass_custom(char *output, int n_max_out_size, int *n_out_size, const int targets_count, const int *targets_sizes, char **targets, char **flags)
{
    if (n_max_out_size < 0) n_max_out_size = 0; // _snprintf_s returns -1
    char buffer[8192];
    int errcode = init(buffer, sizeof(buffer) - 1);
    if (errcode != 0) {
      return errcode;
    }
    doparse(targets_count, targets_sizes, targets, flags);
    return run_checker_output(output, n_max_out_size, n_out_size, buffer);
}

/*
int _cdecl parse_jass_custom_s(char *output, int n_max_out_size, int *n_out_size, const int targets_count, const int *targets_sizes, const char **targets, const char **flags)
{
  return parse_jass_custom(output, n_max_out_size, n_out_size, targets, targets_or_opts_count, flags);
}
*/

#else
int main(int argc, char **argv)
{
    int errcode = init(NULL, 0);
    if (errcode != 0) {
      return errcode;
    }
    doparse_r(argc - 1, (const char **)(void*)&argv[1]);

    if (!haderrors && didparse) {
        printf("Parse successful: %8d lines: %s\n", totlines, "<total>");
        if (ignorederrors) {
            printf("%d errors ignored\n", ignorederrors);
        }
        return 0;
    } else {
        if (haderrors) {
            printf("Parse failed: %d error%s total\n", haderrors, haderrors == 1 ? "" : "s");
        } else {
            printf("Parse failed\n");
        }
        if (ignorederrors) {
            printf("%d errors ignored\n", ignorederrors);
        }
        return 1;
    }
}
#endif
