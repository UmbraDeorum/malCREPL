#include <stdio.h>
#include <dlfcn.h>
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"
#include <ffi.h>
#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

char line[1024];

typedef void (*fn_t)(void);

typedef struct {
    ffi_type **items;
    size_t count;
    size_t capacity;
} Types;

typedef struct {
    void **items;
    size_t count;
    size_t capacity;
} Values;

int main(int argc, char **argv)
{
    const char *program_name = shift(argv, argc);

    if (argc <= 0) {
        fprintf(stderr, "Usage: %s <input>\n", program_name);
        fprintf(stderr, "ERROR: no input is provided\n");
        return 1;
    }

    const char *dll_path = shift(argv, argc);

    void *dll = dlopen(dll_path, RTLD_NOW);
    if (dll == NULL) {
        fprintf(stderr, "ERROR: %s\n", dlerror());
        return 1;
    }

    ffi_cif cif = {0};
    Types args = {0};
    Values values = {0};
    size_t mark = temp_save();
    stb_lexer l = {0};
    static char string_store[1024];
    for (;;) {
next:
        temp_rewind(mark);

        printf("> ");
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        fflush(stdout);
        String_View sv = sv_trim(sv_from_cstr(line));

        stb_c_lexer_init(&l, sv.data, sv.data + sv.count, string_store, ARRAY_LEN(string_store));

        if (!stb_c_lexer_get_token(&l)) continue;

        if (l.token != CLEX_id) {
            printf("ERROR: function name must be an identifier\n");
            continue;
        }

        void *fn = dlsym(dll, l.string);
        if (fn == NULL) {
            printf("ERROR: no function %s found\n", l.string);
            continue;
        }

        args.count = 0;
        values.count = 0;

        while (stb_c_lexer_get_token(&l)) {
            switch (l.token) {
            case CLEX_intlit: {
                da_append(&args, &ffi_type_sint32);
                int *x = temp_alloc(sizeof(int));
                *x = l.int_number;
                da_append(&values, x);
            } break;
            case CLEX_dqstring: {
                da_append(&args, &ffi_type_pointer);
                char **x = temp_alloc(sizeof(char*));
                *x = temp_strdup(l.string);
                da_append(&values, x);
            } break;
            default:
                printf("ERROR: invalid argument token\n");
                goto next;
            }
        }

        ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, args.count, &ffi_type_void, args.items);
        if (status != FFI_OK) {
            printf("ERROR: could not create cif\n");
            continue;
        }

        ffi_call(&cif, fn, NULL, values.items);
    }

    printf("Quit\n");

    return 0;
}
