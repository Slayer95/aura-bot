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


static const char **flags_in_order;
static int count_flags_in_order;
static int limit_flags_in_order;

static int add_flag(struct hashtable *available_flags, struct hashtable *flags_helpstring, const char *name, void *data, const char *helptext)
{
    ht_put(available_flags, name, data);
    ht_put(flags_helpstring, name, (void*)helptext);

    if( count_flags_in_order+1 >= limit_flags_in_order )
    {
        limit_flags_in_order++;
        const char **new_ptr = realloc(flags_in_order, sizeof(char*) * limit_flags_in_order);
        if( new_ptr ){
            flags_in_order = new_ptr;
        }else{
            return 0;
        }
    }
    flags_in_order[count_flags_in_order++] = name;
    return 1;
}

static void init(char *n_output, int n_max_out_size)
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

    limit_flags_in_order = 10;
    count_flags_in_order = 0;
    flags_in_order = malloc(sizeof(char*) * limit_flags_in_order);

    if(0 == (1
      & add_flag(&available_flags, &flags_helpstring, "rb", (void*)flag_rb, "Toggle returnbug checking")
      & add_flag(&available_flags, &flags_helpstring, "shadow", (void*)flag_shadowing, "Toggle error on variable shadowing")
      & add_flag(&available_flags, &flags_helpstring, "filter", (void*)flag_filter,"Toggle error on inappropriate code usage for Filter")
      & add_flag(&available_flags, &flags_helpstring, "noruntimeerror", (void*)flag_runtimeerror, "Toggle runtime error reporting")
      & add_flag(&available_flags, &flags_helpstring, "checkglobalsinit", (void*)flag_checkglobalsinit, "Toggle a very bad checker for uninitialized globals usage")
      & add_flag(&available_flags, &flags_helpstring, "checkstringhash", (void*)flag_checkstringhash, "Toggle StringHash collision checking")
      & add_flag(&available_flags, &flags_helpstring, "nomodulooperator", (void*)flag_nomodulo, "Error when using the modulo-operator '%'")
      & add_flag(&available_flags, &flags_helpstring, "checklongnames", (void*)flag_verylongnames, "Error on very long names")
      & add_flag(&available_flags, &flags_helpstring, "nosyntaxerror", (void*)flag_syntaxerror, "Toggle syntax error reporting")
      & add_flag(&available_flags, &flags_helpstring, "nosemanticerror", (void*)flag_semanticerror, "Toggle semantic error reporting")
      & add_flag(&available_flags, &flags_helpstring, "checknumberliterals", (void*)flag_checknumberliterals, "Error on overflowing number literals")
      & add_flag(&available_flags, &flags_helpstring, "oldpatch", (void*)(flag_verylongnames | flag_nomodulo | flag_filter | flag_rb), "Combined options for older patches")
    )) {
      // Abort on realloc error
      return;
    }


    ht_put(&bad_natives_in_globals, "OrderId", (void*)NullInGlobals);
    ht_put(&bad_natives_in_globals, "OrderId2String", (void*)NullInGlobals);
    ht_put(&bad_natives_in_globals, "UnitId2String", (void*)NullInGlobals);

    ht_put(&bad_natives_in_globals, "GetObjectName", (void*)CrashInGlobals);
    ht_put(&bad_natives_in_globals, "CreateQuest", (void*)CrashInGlobals);
    ht_put(&bad_natives_in_globals, "CreateMultiboard", (void*)CrashInGlobals);
    ht_put(&bad_natives_in_globals, "CreateLeaderboard", (void*)CrashInGlobals);
    ht_put(&bad_natives_in_globals, "CreateRegion", (void*)CrashInGlobals);
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
    yy_switch_to_buffer(yy_create_buffer(fp, BUFSIZE));
    curfile = name;
    while ( yyparse() ) ;
    totlines += lineno;
    fno++;
}

static void doparse(int _argc, const char **_argv)
{
    int i;
    for (i = 0; i < _argc; ++i) {
        if( isflag(_argv[i], &available_flags)){
            pjass_flags = updateflag(pjass_flags, _argv[i], &available_flags);
            continue;
        }

        FILE *fp;
        fp = fopen(_argv[i], "rb");
        if (fp == NULL) {
            haderrors++;
            continue;
        }

        dofile(fp, _argv[i]);
        didparse = 1;
        fclose(fp);
    }
}

int _cdecl parse_jass_files(const int file_count, const char **file_paths, char *output, int* out_size)
{
    char buffer[8192];
    init(buffer, sizeof(buffer) - 1);
    doparse(file_count, file_paths); 

    if (!haderrors && didparse) {
        *out_size = _snprintf_s(output, *out_size, *out_size - 1, "Parse successful: %8d lines: %s", totlines, "<total>");
        return 0;
    } else {
        if (haderrors) {
            *out_size = _snprintf_s(output, *out_size, *out_size - 1, "Parse failed: %d error%s total\n%s", haderrors, haderrors == 1 ? "" : "s", buffer);
        } else {
            *out_size = _snprintf_s(output, *out_size, *out_size - 1, "Parse failed");
        }
        return 1;
    }
}
