#!/bin/bash
# Setup script for C REPL
# Installs all required dependencies

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║           C REPL - Dependency Installation                 ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo

echo "This script will install the following packages:"
echo "  - tcc           (TinyCC compiler with include files)"
echo "  - libtcc-dev    (TCC development library)"
echo "  - libffi-dev    (Foreign Function Interface library)"
echo "  - libcurl4-openssl-dev    (development files and documentation for libcurl - OpenSSL flavour)"
echo "  - build-essential (GCC and build tools)"
echo "  - wget          (For downloading stb_c_lexer.h)"
echo

read -p "Continue? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Installation cancelled."
    exit 1
fi

echo
echo "Updating package list..."
sudo apt-get update

echo
echo "Installing packages..."
sudo apt-get install -y tcc libtcc-dev libffi-dev libcurl4-openssl-dev build-essential wget 

echo
echo "Verifying installation..."

# Check TCC
if command -v tcc > /dev/null 2>&1; then
    echo "✓ tcc installed: $(tcc -v 2>&1 | head -1)"
else
    echo "✗ tcc installation failed"
    exit 1
fi

# Check for TCC include directory
TCC_INCLUDE_FOUND=0
for dir in /usr/lib/tcc/include /usr/lib/x86_64-linux-gnu/tcc/include; do
    if [ -d "$dir" ]; then
        echo "✓ TCC include directory found: $dir"
        if [ -f "$dir/stddef.h" ]; then
            echo "  ✓ stddef.h present"
        fi
        if [ -f "$dir/stdarg.h" ]; then
            echo "  ✓ stdarg.h present"
        fi
        TCC_INCLUDE_FOUND=1
        break
    fi
done

if [ $TCC_INCLUDE_FOUND -eq 0 ]; then
    echo "✗ Warning: TCC include directory not found"
    echo "  The tcc package may not have installed correctly"
fi

# Check libtcc-dev
if [ -f "/usr/include/libtcc.h" ]; then
    echo "✓ libtcc.h installed"
else
    echo "✗ libtcc.h not found"
fi

# Check libffi-dev
if pkg-config --exists libffi; then
    echo "✓ libffi-dev installed"
else
    echo "✗ libffi-dev not found"
fi

# Check libcurl4-openssl-dev 
if pkg-config --exists libcurl4; then
    echo "✓ libcurl4-openssl-dev installed"
else
    echo "✗ libcurl4-openssl-dev not found"
fi
# # Check libtls-dev
# if pkg-config --exists libtls; then
#     echo "✓ libtls-dev installed"
# else
#     echo "✗ libtls-dev not found"
# fi

# # Check libssl-dev
# if pkg-config --exists libssl; then
#     echo "✓ libssl-dev installed"
# else
#     echo "✗ libssl-dev not found"
# fi

# Download stb_c_lexer.h if not present
echo
if [ -f "stb_c_lexer.h" ]; then
    echo "✓ stb_c_lexer.h already present"
else
    echo "Downloading stb_c_lexer.h..."
    wget -q https://raw.githubusercontent.com/nothings/stb/master/stb_c_lexer.h
    if [ -f "stb_c_lexer.h" ]; then
        echo "✓ stb_c_lexer.h downloaded"
    else
        echo "✗ Failed to download stb_c_lexer.h"
        echo "  You can download it manually from:"
        echo "  https://raw.githubusercontent.com/nothings/stb/master/stb_c_lexer.h"
    fi
fi



echo
echo "════════════════════════════════════════════════════════════"
echo "Installation complete!"
echo
echo "Next steps:"
echo "  1. Build the project:  make <|static|partial-static>"
echo "  2. Run the REPL:       ./malcrepl example.c"
echo "════════════════════════════════════════════════════════════"

