# MalCREPL

Interactive C REPL with encryption, remote source loading, and advanced features.

## Features

- **Interactive C execution** - Call functions dynamically without main()
- **User-key in-memory encryption** - Encrypt/decrypt source files with password protection
- **Remote sources** - Download and compile from URLs
- **In-memory compilation** - Uses TinyCC for instant execution
- **Advanced autocomplete** - Commands and loaded program functions with readline
- **Command history** - UP/DOWN arrows to explore command history
- **Return type autodetection** - Automatically detects function return types from source
- **Signal handling** - Graceful Ctrl+C handling with double-tap to exit
- **Function listing** - Browse all available functions with signatures
- **Flexible builds** - Multiple build options from dynamic to fully static

## Quick Start

```bash
# Install dependencies
make setup

# Build (choose one)
make build          # Dynamic (recommended for development)
make semi-static    # Semi-static (recommended for distribution)
make static         # Full static (portable but large -- not properly implemented)
```

# Usage
```bash
# Normal Mode
./malcrepl source.c           # Compile and run local file
./malcrepl https://url/code.c # Download and compile from URL

# Encryption Mode
./malcrepl 1 file.c                # Encrypt file (prompts for password)
./malcrepl 0 file.c                # Decrypt and run (prompts for password)
./malcrepl 0 https://url/code.c    # Download, decrypt and compile from URL
```

# Example
```c
// test.c
#include <stdio.h>
#include <math.h>

void greet(char *name) {
    printf("Hello, %s!\n", name);
}

int add(int a, int b) {
    return a + b;
}

double calculate_hypotenuse(double a, double b) {
    return sqrt(a*a + b*b);
}
```

```bash
$ ./malcrepl test.c
╔════════════════════════════════════════════════════════════╗
║          C REPL - Interactive C Function Executor          ║
╚════════════════════════════════════════════════════════════╝

Successfully compiled: test.c
Type :help for commands, :quit or Ctrl+C to exit

> greet "World"
Hello, World!
> add 5 10
→ 15
> calculate_hypotenuse 3.0 4.0
→ 5.000000
> :list
╔════════════════════════════════════════════════════════════╗
║  Available Functions (3)                                  ║
╠════════════════════════════════════════════════════════════╣
  void greet(char *name)
  int add(int a, int b)
  double calculate_hypotenuse(double a, double b)
╚════════════════════════════════════════════════════════════╝
> :quit
Goodbye!
```

# Build Options
```bash
make build         # Dynamic build (default, ~30KB)
make semi-static   # Recommended for distribution (~50KB)
make hybrid        # Balanced approach (~200KB)
make static        # Full static (portable, ~800KB)
make custom        # Custom static/dynamic mix
make help          # Show all options
```

# REPL Commands

| Command | Description | 
|---------|----------|
| :help, :h | 	Show help message | 
| :quit, :q | 	Exit the REPL | 
| :info	Show |  compilation info | 
| :list, :l | 	List all available functions with signatures | 
| :reload, :r | 	Reload and recompile source file | 
| Ctrl+C | Once: clear line, twice: exit | 

# Supported Argument Types
* Integers: 42, -10, 100L (long)
* Floats: 3.14, 2.5f (float), 1.0 (double)
* Strings: "hello world", "escaped\"string"
* Characters: 'a', 'Z', '\n'

# Return Type Autodetection
The program automatically detects function return types by parsing source code. For best results:
- Include function definitions in your source code
- Place functions at file scope (not inside other functions)
- Avoid complex preprocessor macros in return types

## Supported return types:

* void - No return value
* int, short, long - Integer types
* float, double - Floating point types
* char - Character type
* Pointer types (char*, void*, etc.)
* Other types default to int

# Advanced Features
### Function Signature Display
The :list command shows complete function signatures extracted from source code, making it easy to see parameter types and return types.
### Readline Integration
Tab completion: Commands and function names
### History navigation
Up/down arrows
### History persistence
Saved between sessions in ~/.malcrepl_history
### Line editing
Emacs-style editing shortcuts
### Signal Handling
First Ctrl+C: Clears current input line, shows warning

Second Ctrl+C within 2 seconds: Exits immediately

### Graceful cleanup
Proper resource deallocation on exit

### Memory Management
Temporary arena allocator: Efficient memory reuse during REPL cycles

### Automatic cleanup
No memory leaks on normal exit or errors

### Capacity preservation
Dynamic arrays preserve capacity between commands

# Dependencies

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

# Architecture
* Compiler Layer: TinyCC for fast in-memory compilation
* REPL Engine: libffi for dynamic function calls
* Type System: Automatic return type detection and argument parsing
* Memory Management: Arena allocator for temporary data
* UI Layer: Readline for interactive experience

# Performance
* Compilation: ~1-10ms for typical functions
* Execution: Native speed after compilation
* Memory: Efficient arena allocation with ~0 leaks
* Startup: Instantaneous for pre-compiled sources

# License
Uses TinyCC (LGPL), libffi (MIT), libcurl (MIT-style), OpenSSL (Apache 2.0), stb_c_lexer (public domain).

Inspired by [crepl](https://github.com/tsoding/crepl) (MIT) with enhanced security and features.
