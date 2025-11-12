/*
Normal compilation: ./malcrepl source.c
Encrypt file: ./malcrepl 1 file_to_encrypt.c
Decrypt and compile: ./malcrepl 0 encrypted_file.c
*/

// Define feature test macros before any includes
// #define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libtcc.h>
#include <ffi.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdint.h>
#include <signal.h>

#include <unistd.h>
#include <termios.h>

// Try to use readline if available
#if defined(__has_include)
  #if __has_include(<readline/readline.h>)
    #define HAVE_READLINE
    #include <readline/readline.h>
    #include <readline/history.h>
  #endif
#else
  // Assume readline is available, fail at link time if not
  #include <readline/readline.h>
  #include <readline/history.h>
  #define HAVE_READLINE
#endif

#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

// LibreSSL headers
// #include <tls.h>

#include <curl/curl.h>
// Encryption library
#include "enclib.h"

// ============================================================================
// Signal Handling
// ============================================================================

#include <time.h>

// Global state for Ctrl+C handling
static volatile sig_atomic_t ctrl_c_count = 0;
static time_t last_ctrl_c_time = 0;

#ifdef HAVE_READLINE
// Readline-aware signal handler
static void sigint_handler(int sig) {
    (void)sig;
    
    time_t now = time(NULL);
    
    // Reset count if more than 2 seconds since last Ctrl+C
    if (now - last_ctrl_c_time > 2) {
        ctrl_c_count = 0;
    }
    
    ctrl_c_count++;
    last_ctrl_c_time = now;
    
    if (ctrl_c_count >= 2) {
        // Second Ctrl+C - exit immediately
        printf("\nExiting...\n");
        exit(0);
    }
    
    // First Ctrl+C - show warning and reset readline
    printf("Press Ctrl+C again within 2 seconds to quit (or type :quit)\n");
    rl_on_new_line();      // Tell readline we're on a new line
    rl_replace_line("", 0); // Clear the current line buffer
    rl_redisplay();        // Redisplay the prompt immediately
}
#else
// Basic signal handler for non-readline mode (fgets fallback)
static void sigint_handler(int sig) {
    (void)sig;
    
    time_t now = time(NULL);
    
    if (now - last_ctrl_c_time > 2) {
        ctrl_c_count = 0;
    }
    
    ctrl_c_count++;
    last_ctrl_c_time = now;
    
    if (ctrl_c_count >= 2) {
        printf("\nExiting...\n");
        exit(0);
    }
    
    printf("\nPress Ctrl+C again within 2 seconds to quit (or type :quit)\n");
}
#endif

// Setup signal handlers
static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
}

// ============================================================================
// Dynamic Array Implementation
// ============================================================================

#define da_append(da, item) \
    do { \
        if ((da)->count >= (da)->capacity) { \
            size_t new_cap = (da)->capacity == 0 ? 8 : (da)->capacity * 2; \
            void *new_items = realloc((da)->items, new_cap * sizeof(*(da)->items)); \
            if (!new_items) { \
                fprintf(stderr, "ERROR: Out of memory\n"); \
                exit(1); \
            } \
            (da)->items = new_items; \
            (da)->capacity = new_cap; \
        } \
        (da)->items[(da)->count++] = (item); \
    } while (0)

#define da_free(da) \
    do { \
        free((da)->items); \
        (da)->items = NULL; \
        (da)->count = 0; \
        (da)->capacity = 0; \
    } while (0)

// ============================================================================
// String View Implementation
// ============================================================================

typedef struct {
    const char *data;
    size_t count;
} String_View;

static inline String_View sv_from_cstr(const char *cstr) {
    return (String_View){cstr, strlen(cstr)};
}

static inline String_View sv_trim(String_View sv) {
    while (sv.count > 0 && isspace((unsigned char)sv.data[0])) {
        sv.data++;
        sv.count--;
    }
    while (sv.count > 0 && isspace((unsigned char)sv.data[sv.count - 1])) {
        sv.count--;
    }
    return sv;
}

static inline bool sv_eq(String_View a, String_View b) {
    return a.count == b.count && memcmp(a.data, b.data, a.count) == 0;
}

// ============================================================================
// Memory Arena
// ============================================================================

typedef struct Arena_Block {
    void *memory;
    struct Arena_Block *next;
} Arena_Block;

typedef struct {
    Arena_Block *head;
} Arena;

static Arena temp_arena = {0};

static void arena_reset(Arena *arena) {
    Arena_Block *block = arena->head;
    while (block) {
        Arena_Block *next = block->next;
        free(block->memory);
        free(block);
        block = next;
    }
    arena->head = NULL;
}

static void *arena_alloc(Arena *arena, size_t size) {
    if (size == 0) {
        fprintf(stderr, "WARNING: Attempted to allocate 0 bytes\n");
        return NULL;
    }

    void *mem = calloc(1, size);
    if (!mem) {
        fprintf(stderr, "ERROR: Out of memory (requested %zu bytes)\n", size);
        return NULL;
    }

    Arena_Block *block = malloc(sizeof(Arena_Block));
    if (!block) {
        free(mem);
        fprintf(stderr, "ERROR: Out of memory for arena block\n");
        return NULL;
    }

    block->memory = mem;
    block->next = arena->head;
    arena->head = block;

    return mem;
}

static char *arena_strdup(Arena *arena, const char *str) {
    size_t len = strlen(str);
    char *copy = arena_alloc(arena, len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

// Legacy temp_* functions
#define temp_reset() arena_reset(&temp_arena)
#define temp_alloc(size) arena_alloc(&temp_arena, size)
#define temp_strdup(str) arena_strdup(&temp_arena, str)

// ============================================================================
// FFI Type System
// ============================================================================

typedef struct {
    ffi_type **items;
    size_t count;
    size_t capacity;
} Type_Array;

typedef struct {
    void **items;
    size_t count;
    size_t capacity;
} Value_Array;

// ============================================================================
// TCC Compilation
// ============================================================================

typedef struct {
    TCCState *state;
    char *source_path;
    char *source_code;
} Compiler_Context;

// Find TCC's include directory (cached result)
static const char *find_tcc_include_path(void) {
    static const char *cached_path = NULL;
    static bool searched = false;

    if (searched) return cached_path;
    searched = true;

    static const char *tcc_include_paths[] = {
        "/usr/lib/x86_64-linux-gnu/tcc/include",  // Most common first
        "/usr/lib/tcc/include",
        "/usr/lib/aarch64-linux-gnu/tcc/include",
        "/usr/lib64/tcc/include",
        "/usr/local/lib/tcc/include",
        "/opt/tcc/include",
        "/usr/share/tcc/include",
        NULL
    };

    struct stat st;
    for (int i = 0; tcc_include_paths[i] != NULL; i++) {
        if (stat(tcc_include_paths[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            cached_path = tcc_include_paths[i];
            return cached_path;
        }
    }

    return NULL;
}

static Compiler_Context *compiler_create(void) {
    Compiler_Context *ctx = calloc(1, sizeof(Compiler_Context));
    if (!ctx) return NULL;

    ctx->state = tcc_new();
    if (!ctx->state) {
        free(ctx);
        return NULL;
    }

    ctx->source_code = NULL;
    return ctx;
}

static void compiler_destroy(Compiler_Context *ctx) {
    if (!ctx) return;
    if (ctx->state) tcc_delete(ctx->state);
    free(ctx->source_path);
    free(ctx->source_code);
    free(ctx);
}

static void cleanup_resources(Compiler_Context *compiler, Type_Array *types,
                            Value_Array *values, char *source_code, int encryption_mode) {
    if (compiler) compiler_destroy(compiler);
    da_free(types);
    da_free(values);
    temp_reset();

    if (encryption_mode == 0) {
        free(source_code); // Free decrypted content
    } else if (encryption_mode == -1) {
        free(source_code); // Free normally loaded content
    }
    // Note: encryption_mode == 1 already returns early, so no free needed
}

static bool compiler_configure(Compiler_Context *ctx, const char *source_path) {
    if (!ctx || !ctx->state) return false;

    tcc_set_output_type(ctx->state, TCC_OUTPUT_MEMORY);

    // Add TCC's include path (cached lookup)
    const char *tcc_include = find_tcc_include_path();
    if (tcc_include) {
        tcc_add_include_path(ctx->state, tcc_include);
    } else {
        fprintf(stderr, "WARNING: TCC include directory not found\n");
        fprintf(stderr, "         Install: sudo apt-get install tcc\n\n");
    }

    // Add system include paths
    tcc_add_include_path(ctx->state, "/usr/include");
    tcc_add_include_path(ctx->state, "/usr/local/include");

    // Add architecture-specific includes (conditional compilation could optimize this)
#if defined(__x86_64__) || defined(__amd64__)
    tcc_add_include_path(ctx->state, "/usr/include/x86_64-linux-gnu");
#elif defined(__aarch64__)
    tcc_add_include_path(ctx->state, "/usr/include/aarch64-linux-gnu");
#endif

    // Add source directory for local includes
    if (source_path) {
        ctx->source_path = strdup(source_path);
        char *last_slash = strrchr(ctx->source_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            tcc_add_include_path(ctx->state, ctx->source_path);
            *last_slash = '/';  // Restore for later use
        }
    }

    // Link standard library
    tcc_add_library(ctx->state, "c");

    // Link math (optional, needed for test example)
    tcc_add_library(ctx->state, "m");

    return true;
}

static bool compiler_compile_string(Compiler_Context *ctx, const char *source_code) {
    if (!ctx || !ctx->state || !source_code) return false;

    if (tcc_compile_string(ctx->state, source_code) == -1) {
        fprintf(stderr, "ERROR: Compilation failed\n");
        return false;
    }

    if (tcc_relocate(ctx->state, TCC_RELOCATE_AUTO) < 0) {
        fprintf(stderr, "ERROR: Relocation failed - check for undefined symbols\n");
        return false;
    }

    return true;
}

static inline void *compiler_get_symbol(Compiler_Context *ctx, const char *name) {
    return (ctx && ctx->state && name) ? tcc_get_symbol(ctx->state, name) : NULL;
}

Compiler_Context* compile(char* source_code, char *source_path){
    Compiler_Context *compiler = compiler_create();
    if (!compiler) {
        fprintf(stderr, "ERROR: Could not create compiler context\n");
        free(source_code);
        exit(1);
    }

    if (!compiler_configure(compiler, source_path)) {
        fprintf(stderr, "ERROR: Could not configure compiler\n");
        free(source_code);
        compiler_destroy(compiler);
        exit(1);
    }

    // Compile source
    if (!compiler_compile_string(compiler, source_code)) {
        fprintf(stderr, "ERROR: Failed to compile '%s'\n", source_path);
        free(source_code);
        compiler_destroy(compiler);
        exit(1);
    }

    return compiler;
}

// ============================================================================
// Argument Parsing
// ============================================================================

static bool parse_arguments(stb_lexer *l, Type_Array *types, Value_Array *values) {
    while (stb_c_lexer_get_token(l)) {
        switch (l->token) {
            case CLEX_intlit: {
                // Check suffix for long (L or LL)
                const char *p = l->where_firstchar;
                while (*p && (isdigit(*p) || *p == 'x' || *p == 'X' ||
                             (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) {
                    p++;
                }

                if (*p == 'L' || *p == 'l') {
                    da_append(types, &ffi_type_slong);
                    long *x = temp_alloc(sizeof(long));
                    if (!x) return false;
                    *x = l->int_number;
                    da_append(values, x);
                } else {
                    da_append(types, &ffi_type_sint32);
                    int *x = temp_alloc(sizeof(int));
                    if (!x) return false;
                    *x = (int)l->int_number;
                    da_append(values, x);
                }
                break;
            }

            case CLEX_dqstring: {
                da_append(types, &ffi_type_pointer);
                char **x = temp_alloc(sizeof(char*));
                if (!x) return false;
                *x = temp_strdup(l->string);
                if (!*x) return false;
                da_append(values, x);
                break;
            }

            case CLEX_sqstring: {
                if (l->string[0] && !l->string[1]) {  // Exactly one char
                    da_append(types, &ffi_type_schar);
                    char *x = temp_alloc(sizeof(char));
                    if (!x) return false;
                    *x = l->string[0];
                    da_append(values, x);
                } else {
                    fprintf(stderr, "ERROR: char literal must be single character\n");
                    return false;
                }
                break;
            }

            case CLEX_charlit: {
                // Direct character literal like 'a'
                da_append(types, &ffi_type_schar);
                char *x = temp_alloc(sizeof(char));
                if (!x) return false;
                *x = (char)l->int_number;  // Character value is in int_number
                da_append(values, x);
                break;
            }

            case CLEX_floatlit: {
                // Check suffix for float (f or F)
                const char *p = l->where_firstchar;
                while (*p && (isdigit(*p) || *p == '.' || *p == 'e' ||
                             *p == 'E' || *p == '+' || *p == '-')) {
                    p++;
                }

                if (*p == 'f' || *p == 'F') {
                    da_append(types, &ffi_type_float);
                    float *x = temp_alloc(sizeof(float));
                    if (!x) return false;
                    *x = (float)l->real_number;
                    da_append(values, x);
                } else {
                    da_append(types, &ffi_type_double);
                    double *x = temp_alloc(sizeof(double));
                    if (!x) return false;
                    *x = l->real_number;
                    da_append(values, x);
                }
                break;
            }

            default:
                fprintf(stderr, "ERROR: unsupported argument type (token: %ld)\n", l->token);
                return false;
        }
    }

    return true;
}


// ============================================================================
// Return Type Autodetection
// ============================================================================

// Function to find the start of return type
const char* find_return_type_start(const char* code, const char* func_pos) {
    const char* p = func_pos;

    // Move backwards to find the start of function declaration
    while (p > code && *p != ';' && *p != '}' && *p != '{' && *p != '\n') {
        p--;
    }

    // Skip the delimiter we found
    if (p > code || *p == ';' || *p == '}' || *p == '{' || *p == '\n') {
        p++;
    }

    return p;
}

bool extract_return_type(const char* code, const char* func_name, char* return_type, size_t return_type_size) {
    if (!code || !func_name || !return_type || return_type_size == 0) {
        return false;
    }

    // Build search pattern: look for "function_name("
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%s(", func_name);

    const char *func_decl = strstr(code, pattern);
    if (!func_decl) {
        return false;
    }

    // Walk backwards to find start of return type
    const char *p = func_decl - 1;

    // Skip whitespace before function name
    while (p > code && isspace((unsigned char)*p)) {
        p--;
    }

    // Now p points to last char of return type
    const char *ret_end = p + 1;

    // Walk backwards to find start of return type
    // Stop at: newline, semicolon, closing brace, opening brace
    while (p > code) {
        char c = *p;
        if (c == '\n' || c == ';' || c == '}' || c == '{') {
            p++;
            break;
        }
        p--;
    }

    // Skip leading whitespace
    while (p < ret_end && isspace((unsigned char)*p)) {
        p++;
    }

    // Calculate length
    size_t len = ret_end - p;

    if (len == 0 || len >= return_type_size) {
        return false;
    }

    // Copy return type
    memcpy(return_type, p, len);
    return_type[len] = '\0';

    // Trim trailing whitespace
    while (len > 0 && isspace((unsigned char)return_type[len - 1])) {
        return_type[--len] = '\0';
    }

    return len > 0;
}

static ffi_type* detect_return_type(const char *function_name, const char *source_code) {
    char return_type_str[64];
    memset(return_type_str, 0, sizeof(return_type_str));

    if (!extract_return_type(source_code, function_name, return_type_str, sizeof(return_type_str))) {
        // Default to int if we can't detect
        return &ffi_type_sint;
    }

    // Trim whitespace
    String_View rt = sv_trim(sv_from_cstr(return_type_str));

    // Check for void
    if (rt.count == 4 && memcmp(rt.data, "void", 4) == 0) {
        return &ffi_type_void;
    }

    // Check for pointer types (look for asterisk)
    for (size_t i = 0; i < rt.count; i++) {
        if (rt.data[i] == '*') {
            return &ffi_type_pointer;
        }
    }

    // Check for char (single character, not pointer)
    if (rt.count == 4 && memcmp(rt.data, "char", 4) == 0) {
        return &ffi_type_schar;
    }

    // Check for int types
    if ((rt.count == 3 && memcmp(rt.data, "int", 3) == 0) ||
        (rt.count == 5 && memcmp(rt.data, "short", 5) == 0) ||
        (rt.count >= 6 && memcmp(rt.data, "signed", 6) == 0) ||
        (rt.count >= 8 && memcmp(rt.data, "unsigned", 8) == 0)) {
        return &ffi_type_sint;
    }

    // Check for long
    if (rt.count == 4 && memcmp(rt.data, "long", 4) == 0) {
        return &ffi_type_slong;
    }

    // Check for float
    if (rt.count == 5 && memcmp(rt.data, "float", 5) == 0) {
        return &ffi_type_float;
    }

    // Check for double
    if (rt.count == 6 && memcmp(rt.data, "double", 6) == 0) {
        return &ffi_type_double;
    }

    // Default: assume int
    return &ffi_type_sint;
}

static void display_return_value(ffi_type *return_type, void *result) {
    if (!result && return_type != &ffi_type_void) {
        return;
    }

    // Match by type size and alignment as backup
    int is_char = (return_type == &ffi_type_schar) ||
                  (return_type == &ffi_type_sint8) ||
                  (return_type == &ffi_type_uint8) ||
                  (return_type->size == 1 && return_type->alignment == 1);

    int is_int = (return_type == &ffi_type_sint) ||
                 (return_type == &ffi_type_sint32) ||
                 (return_type == &ffi_type_uint32);

    int is_long = (return_type == &ffi_type_slong) ||
                  (return_type == &ffi_type_sint64);

    if (return_type == &ffi_type_void) {
        // Don't print anything for void
    } else if (is_char) {
        char c = *(char*)result;
        if (isprint((unsigned char)c)) {
            printf("→ '%c' (%d)\n", c, (int)c);
        } else {
            printf("→ %d (non-printable)\n", (int)c);
        }
    } else if (is_int) {
        printf("→ %d\n", *(int*)result);
    } else if (is_long) {
        printf("→ %ld\n", *(long*)result);
    } else if (return_type == &ffi_type_float) {
        printf("→ %f\n", *(float*)result);
    } else if (return_type == &ffi_type_double) {
        printf("→ %lf\n", *(double*)result);
    } else if (return_type == &ffi_type_pointer) {
        void *ptr = *(void**)result;

        if (!ptr) {
            printf("→ NULL\n");
        } else {
            // Try to display as string
            const char *str = (const char*)ptr;
            size_t len = 0;

            // Check if it looks like a string
            while (len < 256 && str[len]) {
                if (!isprint((unsigned char)str[len]) && !isspace((unsigned char)str[len])) {
                    // Not a string
                    printf("→ %p\n", ptr);
                    return;
                }
                len++;
            }

            if (len > 0 && len < 256) {
                printf("→ \"%s\"\n", str);
            } else {
                printf("→ %p\n", ptr);
            }
        }
    } else {
        printf("→ [unknown type, size=%zu]\n", return_type->size);
    }
}

// ============================================================================
// Function Listing
// ============================================================================

typedef struct {
    char *name;
    char *signature;
} Function_Info;

typedef struct {
    Function_Info *items;
    size_t count;
    size_t capacity;
} Function_Array;

// Extract function signature from source code
static bool extract_function_signature(const char *source_code, const char *func_name, 
                                       char *signature, size_t sig_size) {
    if (!source_code || !func_name || !signature || sig_size == 0) {
        return false;
    }
    
    // Build search pattern
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%s(", func_name);
    
    const char *func_decl = strstr(source_code, pattern);
    if (!func_decl) {
        return false;
    }
    
    // Find start of signature (walk back to find return type)
    const char *sig_start = func_decl;
    while (sig_start > source_code && 
           *sig_start != '\n' && *sig_start != ';' && 
           *sig_start != '}' && *sig_start != '{') {
        sig_start--;
    }
    if (*sig_start == '\n' || *sig_start == ';' || *sig_start == '}' || *sig_start == '{') {
        sig_start++;
    }
    
    // Skip leading whitespace
    while (*sig_start && isspace((unsigned char)*sig_start)) {
        sig_start++;
    }
    
    // Find end of signature (closing paren)
    const char *sig_end = func_decl + strlen(func_name) + 1; // After "func_name("
    int paren_depth = 1;
    while (*sig_end && paren_depth > 0) {
        if (*sig_end == '(') paren_depth++;
        if (*sig_end == ')') paren_depth--;
        sig_end++;
    }
    
    // Calculate length and copy
    size_t len = sig_end - sig_start;
    if (len >= sig_size) {
        len = sig_size - 1;
    }
    
    memcpy(signature, sig_start, len);
    signature[len] = '\0';
    
    // Clean up extra whitespace
    char *dst = signature;
    const char *src = signature;
    int last_was_space = 0;
    
    while (*src) {
        if (isspace((unsigned char)*src)) {
            if (!last_was_space && dst != signature) {
                *dst++ = ' ';
            }
            last_was_space = 1;
        } else {
            *dst++ = *src;
            last_was_space = 0;
        }
        src++;
    }
    *dst = '\0';
    
    return true;
}

// List all functions in the compiled source
static void list_functions(Compiler_Context *compiler) {
    if (!compiler || !compiler->source_code) {
        printf("ERROR: No compiled source available\n");
        return;
    }
    
    Function_Array functions = {0};
    
    // Parse source code to find function definitions
    const char *p = compiler->source_code;
    char current_func[256];
    
    while (*p) {
        // Skip whitespace and comments
        while (*p && isspace((unsigned char)*p)) p++;
        
        // Look for function patterns (simplified - looks for identifier followed by '(')
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
            const char *word_start = p;
            
            // Read identifier
            while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || 
                   (*p >= '0' && *p <= '9') || *p == '_') {
                p++;
            }
            
            // Skip whitespace
            while (*p && isspace((unsigned char)*p)) p++;
            
            // Check if this is a function (followed by '(')
            if (*p == '(') {
                size_t name_len = p - word_start;
                while (word_start < p && isspace((unsigned char)*(p-1))) {
                    p--;
                    name_len--;
                }
                
                if (name_len > 0 && name_len < sizeof(current_func)) {
                    memcpy(current_func, word_start, name_len);
                    current_func[name_len] = '\0';
                    
                    // Check if it's actually callable (not a keyword, has a body)
                    void *func_ptr = compiler_get_symbol(compiler, current_func);
                    if (func_ptr) {
                        // Extract full signature
                        char signature[512];
                        if (extract_function_signature(compiler->source_code, current_func, 
                                                      signature, sizeof(signature))) {
                            // Add to list
                            if (functions.count >= functions.capacity) {
                                size_t new_cap = functions.capacity == 0 ? 16 : functions.capacity * 2;
                                Function_Info *new_items = realloc(functions.items, 
                                                                   new_cap * sizeof(Function_Info));
                                if (!new_items) {
                                    fprintf(stderr, "ERROR: Out of memory\n");
                                    da_free(&functions);
                                    return;
                                }
                                functions.items = new_items;
                                functions.capacity = new_cap;
                            }
                            
                            functions.items[functions.count].name = strdup(current_func);
                            functions.items[functions.count].signature = strdup(signature);
                            functions.count++;
                        }
                    }
                }
            }
        } else {
            p++;
        }
    }
    
    // Display functions
    if (functions.count == 0) {
        printf("\nNo callable functions found.\n\n");
    } else {
        printf("\n╔════════════════════════════════════════════════════════════╗\n");
        printf("║  Available Functions (%zu)%*s║\n", functions.count, 
               (int)(37 - snprintf(NULL, 0, "%zu", functions.count)), "");
        printf("╠════════════════════════════════════════════════════════════╣\n");
        
        for (size_t i = 0; i < functions.count; i++) {
            printf("  %s\n", functions.items[i].signature);
        }
        
        printf("╚════════════════════════════════════════════════════════════╝\n\n");
    }
    
    // Cleanup
    for (size_t i = 0; i < functions.count; i++) {
        free(functions.items[i].name);
        free(functions.items[i].signature);
    }
    da_free(&functions);
}

// ============================================================================
// REPL Commands
// ============================================================================

static void print_help(void) {
    printf("\nBuiltin commands:\n"
           "  :help, :h   - Show this help message\n"
           "  :quit, :q   - Exit the REPL\n"
           "  :info       - Show compilation info\n"
           "  :list, :l   - List all available functions\n"
           "  :reload, :r - Reload and recompile source file\n"
           "\nFunction call format:\n"
           "  function_name [args...]\n"
           "\nSupported argument types:\n"
           "  - Integers: 42, -10, 100L (long)\n"
           "  - Floats: 3.14, 2.5f (float), 1.0 (double)\n"
           "  - Strings: \"hello world\"\n"
           "  - Characters: 'a', 'Z'\n\n");
}

// ============================================================================
// Readline History Support
// ============================================================================

#ifdef HAVE_READLINE

// History file location
static char history_file_path[512];

// Global pointer to compiler context for completion
static Compiler_Context *g_compiler_for_completion = NULL;

// Initialize readline and history
static void init_readline_history(void) {
    const char *home = getenv("HOME");
    if (home) {
        snprintf(history_file_path, sizeof(history_file_path), 
                "%s/.malcrepl_history", home);
        read_history(history_file_path);
        stifle_history(1000);
    }
}

// Save history on exit
static void save_readline_history(void) {
    if (history_file_path[0] != '\0') {
        write_history(history_file_path);
    }
}

// Custom completion generator for REPL commands
static char *command_generator(const char *text, int state) {
    static const char *commands[] = {
        ":help", ":h", ":quit", ":q", ":info", 
        ":list", ":l", ":reload", ":r", NULL
    };
    static int list_index;
    static size_t len;
    
    if (!state) {
        list_index = 0;
        len = strlen(text);
    }
    
    const char *name;
    while ((name = commands[list_index++])) {
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    return NULL;
}

// Extract all function names from source code
static char **get_function_names(Compiler_Context *compiler, size_t *count) {
    if (!compiler || !compiler->source_code) {
        *count = 0;
        return NULL;
    }
    
    // Initial allocation
    size_t capacity = 32;
    char **names = malloc(capacity * sizeof(char*));
    if (!names) {
        *count = 0;
        return NULL;
    }
    
    *count = 0;
    
    // Parse source code to find function definitions
    const char *p = compiler->source_code;
    char current_func[256];
    
    while (*p) {
        // Skip whitespace
        while (*p && isspace((unsigned char)*p)) p++;
        
        // Look for identifier followed by '('
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
            const char *word_start = p;
            
            // Read identifier
            while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || 
                   (*p >= '0' && *p <= '9') || *p == '_') {
                p++;
            }
            
            // Skip whitespace
            const char *after_id = p;
            while (*p && isspace((unsigned char)*p)) p++;
            
            // Check if this is a function (followed by '(')
            if (*p == '(') {
                size_t name_len = after_id - word_start;
                
                if (name_len > 0 && name_len < sizeof(current_func)) {
                    memcpy(current_func, word_start, name_len);
                    current_func[name_len] = '\0';
                    
                    // Check if it's actually callable
                    void *func_ptr = compiler_get_symbol(compiler, current_func);
                    if (func_ptr) {
                        // Check if already in list (avoid duplicates)
                        int duplicate = 0;
                        for (size_t i = 0; i < *count; i++) {
                            if (strcmp(names[i], current_func) == 0) {
                                duplicate = 1;
                                break;
                            }
                        }
                        
                        if (!duplicate) {
                            // Expand array if needed
                            if (*count >= capacity) {
                                capacity *= 2;
                                char **new_names = realloc(names, capacity * sizeof(char*));
                                if (!new_names) {
                                    // Cleanup on error
                                    for (size_t i = 0; i < *count; i++) {
                                        free(names[i]);
                                    }
                                    free(names);
                                    *count = 0;
                                    return NULL;
                                }
                                names = new_names;
                            }
                            
                            names[*count] = strdup(current_func);
                            if (names[*count]) {
                                (*count)++;
                            }
                        }
                    }
                }
            }
        } else {
            p++;
        }
    }
    
    return names;
}

// Function name completion generator
static char *function_generator(const char *text, int state) {
    static char **function_names = NULL;
    static size_t function_count = 0;
    static size_t list_index;
    static size_t len;
    
    // Initialize on first call
    if (!state) {
        // Free previous list if it exists
        if (function_names) {
            for (size_t i = 0; i < function_count; i++) {
                free(function_names[i]);
            }
            free(function_names);
            function_names = NULL;
        }
        
        // Get fresh function list
        if (g_compiler_for_completion) {
            function_names = get_function_names(g_compiler_for_completion, &function_count);
        }
        
        list_index = 0;
        len = strlen(text);
    }
    
    // Return matching functions
    while (list_index < function_count) {
        const char *name = function_names[list_index++];
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    
    // Cleanup when done
    if (function_names) {
        for (size_t i = 0; i < function_count; i++) {
            free(function_names[i]);
        }
        free(function_names);
        function_names = NULL;
        function_count = 0;
    }
    
    return NULL;
}

// Combined completion: commands + functions
static char *combined_generator(const char *text, int state) {
    static int phase = 0;  // 0 = commands, 1 = functions
    
    if (!state) {
        phase = 0;
    }
    
    // Try commands first
    if (phase == 0) {
        char *result = command_generator(text, state);
        if (result) {
            return result;
        }
        // Commands exhausted, move to functions
        phase = 1;
        state = 0;  // Reset state for function generator
    }
    
    // Try functions
    return function_generator(text, state);
}

// Completion function
static char **repl_command_completion(const char *text, int start, int end) {
    (void)end;
    
    // Always try completion at start of line
    if (start == 0) {
        // If text starts with ':', complete commands only
        if (text[0] == ':') {
            return rl_completion_matches(text, command_generator);
        }
        // Otherwise complete both commands and functions
        return rl_completion_matches(text, combined_generator);
    }
    
    // Suppress default filename completion
    rl_attempted_completion_over = 1;
    return NULL;
}

// Setup completion
static void setup_readline_completion(Compiler_Context *compiler) {
    // Store compiler context globally for completion
    g_compiler_for_completion = compiler;
    
    // Set our custom completion function
    rl_attempted_completion_function = repl_command_completion;
}

#endif // HAVE_READLINE

// ============================================================================
// Main REPL
// ============================================================================

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source.c> OR %s <0|1> <file>\n", argv[0], argv[0]);
        fprintf(stderr, "ERROR: no input source file provided\n");
        return 1;
    }

launch:
    setup_signal_handlers();
    char *source_code = NULL;
    char source_path[10240];
    memset(source_path, 0, 10240*sizeof(char));
    int encryption_mode = -1;

    source_code = read_enc_dec_managed(argv[1], argv[2], argc, &encryption_mode, source_path);

    // Create and configure compiler
    Compiler_Context *compiler = NULL;
    compiler = compile(source_code, source_path);
    compiler->source_code = source_code;

#ifdef HAVE_READLINE
    // Update global compiler pointer for autocomplete
    g_compiler_for_completion = compiler;
#endif

    // Print welcome message
    printf("╔════════════════════════════════════════════════════════════╗\n"
           "║          C REPL - Interactive C Function Executor          ║\n"
           "╚════════════════════════════════════════════════════════════╝\n"
           "\nSuccessfully compiled: %s\n"
           "Type :help for commands, :quit or Ctrl+C to exit\n\n", source_path);

    // REPL state (allocated once, reused)
    ffi_cif cif;
    Type_Array types = {0};
    Value_Array values = {0};
    stb_lexer lexer;
    static char string_store[4096];
    char *line = NULL;

#ifdef HAVE_READLINE
    // Initialize readline history
    init_readline_history();
    setup_readline_completion(compiler);  // Pass compiler context!
#endif

    // Main REPL loop
    for (;;) {
        // Reset temp memory and arrays (preserve capacity)
        temp_reset();
        types.count = 0;
        values.count = 0;

#ifdef HAVE_READLINE
        // Use readline for better input with history
        if (line) {
            free(line);
            line = NULL;
        }
        
        line = readline("> ");

        // Handle EOF (Ctrl+D)
        if (!line) {
            printf("%s", "");
            fflush(stdout);
            continue;
        }
        
        // Add non-empty lines to history
        if (line[0] != '\0') {
            add_history(line);
        }
        
        String_View input = sv_trim(sv_from_cstr(line));
        if (input.count == 0) {
            continue;
        }
#else
        // Fallback to basic fgets if readline not available
        static char line_buffer[4096];
        printf("> ");
        fflush(stdout);

        if (!fgets(line_buffer, sizeof(line_buffer), stdin)) break;
        
        line = line_buffer;
        String_View input = sv_trim(sv_from_cstr(line));
        if (input.count == 0) continue;
#endif

        // Check for builtin commands
        if (input.data[0] == ':') {
            if (sv_eq(input, sv_from_cstr(":quit")) || sv_eq(input, sv_from_cstr(":q"))) {
                break;
            } else if (sv_eq(input, sv_from_cstr(":help")) || sv_eq(input, sv_from_cstr(":h"))) {
                print_help();
                continue;
            } else if (sv_eq(input, sv_from_cstr(":info"))) {
                printf("\nCompilation info:\n"
                    "  Source: %s\n"
                    "  Arrays capacity: types=%zu, values=%zu\n\n",
                    source_path, types.capacity, values.capacity);
                continue;
            } else if (sv_eq(input, sv_from_cstr(":list")) || sv_eq(input, sv_from_cstr(":l"))) {
                list_functions(compiler);
                continue;
            } else if (sv_eq(input, sv_from_cstr(":reload")) || sv_eq(input, sv_from_cstr(":r"))) {
                // Create and configure compiler again

                // cleanup_resources(compiler, &types, &values, source_code, encryption_mode);

                goto launch;
            } else {
                printf("ERROR: unknown command. Type :help for available commands\n");
                continue;
            }
        }

        // Parse function call - use original line buffer which is properly null-terminated
        stb_c_lexer_init(&lexer, line, line + strlen(line),
                        string_store, sizeof(string_store));

        if (!stb_c_lexer_get_token(&lexer)) continue;

        if (lexer.token != CLEX_id) {
            printf("ERROR: function name must be an identifier\n");
            continue;
        }

        // Save function name (lexer.string gets overwritten during parse_arguments)
        char function_name[256];
        strncpy(function_name, lexer.string, sizeof(function_name) - 1);
        function_name[sizeof(function_name) - 1] = '\0';

        // Look up function
        void *func_ptr = compiler_get_symbol(compiler, function_name);
        if (!func_ptr) {
            printf("ERROR: function '%s' not found\n", function_name);
            printf("Hint: Make sure the function is defined and not static\n");
            continue;
        }

        // Parse arguments
        if (!parse_arguments(&lexer, &types, &values)) continue;

        // Detect return type using saved function name
        ffi_type *return_type = detect_return_type(function_name, compiler->source_code);

        // Prepare storage for return value
        void *result = NULL;
        if (return_type != &ffi_type_void) {
            result = temp_alloc(return_type->size);
            if (!result) {
                printf("ERROR: Could not allocate memory for return value\n");
                continue;
            }
            // Initialize to zero to avoid garbage data
            memset(result, 0, return_type->size);
        }

        // Prepare and execute FFI call
        ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, types.count,
                                        return_type, types.items);
        if (status != FFI_OK) {
            printf("ERROR: could not prepare FFI call (status: %d)\n", status);
            continue;
        }
        // Execute the function
        ffi_call(&cif, (void(*)())func_ptr, result, values.items);

        // Display the return value
        if (return_type == &ffi_type_void || result != NULL) {
            display_return_value(return_type, result);
        } else {
            printf("→ [error: no result available]\n");
        }
    }

    printf("\nGoodbye!\n");

#ifdef HAVE_READLINE
    // Save history before exit
    save_readline_history();
    
    // Free last line
    if (line) {
        free(line);
    }
#endif

    // Cleanup
    cleanup_resources(compiler, &types, &values, source_code, encryption_mode);

    return 0;
}