# MalCREPL

Interactive C REPL with encryption and remote source loading.

## Features

- **Interactive C execution** - Call functions dynamically without main()
- **User-key encryption** - Encrypt/decrypt source files
- **Remote sources** - Download and compile from URLs
- **In-memory compilation** - Uses TinyCC for instant execution
- **Autocomplete** - Autocomplete for commands and loaded program functions
- **Command history** - UP|DOWN arrows to explore command history, using readline library

## Quick Start

```bash
# Install dependencies
sudo apt-get install tcc libtcc-dev libffi-dev libcurl4-openssl-dev libssl-dev libzstd-dev

# Build
make

# Run
./malcrepl source.c
```

## Usage

### Normal Mode
```bash
./malcrepl source.c           # Compile and run local file
./malcrepl https://url/code.c # Download and compile from URL
```

### Encryption Mode
```bash
./malcrepl 1 file.c           # Encrypt file (prompts for password)
./malcrepl 0 file.c           # Decrypt and run (prompts for password)
```

## Example

```c
// test.c
#include <stdio.h>

void greet(char *name) {
    printf("Hello, %s!\n", name);
}

void add(int a, int b) {
    printf("%d + %d = %d\n", a, b, a + b);
}
```

```bash
$ ./malcrepl test.c
> greet "World"
Hello, World!
> add 5 10
5 + 10 = 15
> :quit
```

## Build Options

```bash
make build         # Dynamic build (default, ~30KB)
make semi-static   # Recommended for distribution (~50KB)
make static        # Full static (may have OpenSSL issues)
make help          # Show all options
```

## REPL Commands

| Command | Description |
|---------|-------------|
| `:help` | Show help |
| `:quit` | Exit |
| `:info` | Show compilation info |
| `Ctrl+D` | Exit |

## Supported Types (input)

- Integers: `42`
- Floats: `3.14`
- Strings: `"hello"`
- Chars: `'a'`


## Support for Return Types' Autodetection

The program checks for function definitions in the source code, as an autodetection mechanism for function return types.
If you want to have proper return value types, it is suggested that function definitions are included in the source code for all non-void-returning functions you intend to call.

## Supported Return Types

- Integers: `42`
- Floats: `3.14`
- Strings: `"hello"`
- Chars via integers: `97` (lowercase a)

## Dependencies

| Library | Required | Purpose | Size Impact | Static? |
|---------|----------|---------|-------------|---------|
| libc | ✅ Yes | Standard C | ~2MB | Usually dynamic |
| libtcc | ✅ Yes | Runtime compilation | ~150KB | Can be static |
| libffi | ✅ Yes | Dynamic FFI calls | ~50KB | Can be static |
| libcurl | ✅ Yes (enclib) | Networking | ~500KB | Usually dynamic |
| libssl | ✅ Yes (enclib) | SSL/TLS | ~1MB | Usually dynamic |
| libcrypto | ✅ Yes (enclib) | Encryption | ~3MB | Usually dynamic |
| libreadline | ⚠️ Optional | CLI features | ~200KB | Usually dynamic |
| libm | ⚠️ Optional | Math (TCC) | ~100KB | Usually dynamic |
| stb_c_lexer | ✅ Yes | Argument parsing | 0 (header) | N/A |
| enclib.h | ✅ Yes | Encryption | 0 (header) | N/A |


## Troubleshooting

**"stddef.h not found"**
```bash
sudo apt-get install tcc  # Not just libtcc-dev!
```

**Static build fails with OpenSSL errors**
```bash
make semi-static  # Use this instead of 'make static'
```

**Function not found**
- Make sure function is not `static`
- Check function name spelling (case-sensitive)

## License

Uses TinyCC (LGPL), libffi (MIT), libcurl (MIT-style), OpenSSL (Apache 2.0).

---

**Minimal. Secure. Interactive.**
Inspired by https://github.com/tsoding/crepl
