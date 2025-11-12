# ============================================================================
# MalCREPL - Enhanced C REPL with Encryption and Network Features
# Flexible Makefile with Dynamic and Custom Build Options
# ============================================================================

# Compiler and base flags
CC = gcc
CFLAGS = -Wall -Wextra -Wno-unused-function -std=c11 -D_POSIX_C_SOURCE=200809L
TARGET = malcrepl

# Source files - check what exists
SOURCES = malcrepl.c
# Add optional sources if they exist
ifneq ($(wildcard enclib.c),)
    SOURCES += enclib.c
endif
ifneq ($(wildcard netlib.c),)
    SOURCES += netlib.c
endif

HEADERS = enclib.h netlib.h stb_c_lexer.h
OBJECTS = $(SOURCES:.c=.o)

# ============================================================================
# Architecture Detection
# ============================================================================

ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
    LIBDIR = /usr/lib/x86_64-linux-gnu
else ifeq ($(ARCH),aarch64)
    LIBDIR = /usr/lib/aarch64-linux-gnu
else
    LIBDIR = /usr/lib
endif

# ============================================================================
# Library Configuration
# ============================================================================

# Required libraries
LIBS_TCC = -ltcc
LIBS_FFI = -lffi
LIBS_CURL = -lcurl
LIBS_CRYPTO = -lcrypto
LIBS_SYSTEM = -lm -ldl -lpthread -lreadline

# Static library paths
LIBTCC_STATIC = $(LIBDIR)/libtcc.a
LIBFFI_STATIC = $(LIBDIR)/libffi.a

# Check if static libcurl is available system-wide
LIBCURL_STATIC_SYSTEM = /usr/local/lib/libcurl.a
LIBCURL_STATIC_LOCAL = ./build/curl-8.6.0/lib/.libs/libcurl.a

# Determine which libcurl to use for static builds
ifeq ($(wildcard $(LIBCURL_STATIC_SYSTEM)),)
    LIBCURL_STATIC = $(LIBCURL_STATIC_LOCAL)
    CURL_SOURCE = local
else
    LIBCURL_STATIC = $(LIBCURL_STATIC_SYSTEM)
    CURL_SOURCE = system
endif

# ============================================================================
# Build Configurations
# ============================================================================

# Dynamic linking (default) - smallest binary, needs .so files
LDFLAGS_DYNAMIC = $(LIBS_TCC) $(LIBS_FFI) $(LIBS_CURL) $(LIBS_CRYPTO) $(LIBS_SYSTEM)

# Hybrid static - TCC and FFI static, rest dynamic
LDFLAGS_HYBRID = -static-libgcc \
                 -Wl,-Bstatic $(LIBTCC_STATIC) $(LIBFFI_STATIC) \
                 -Wl,-Bdynamic $(LIBS_CURL) $(LIBS_CRYPTO) $(LIBS_SYSTEM)

# Semi-static - TCC, FFI, and system libs static, curl/openssl dynamic
# This avoids OpenSSL version conflicts
LDFLAGS_SEMI_STATIC = -Wl,-Bstatic \
                      $(LIBTCC_STATIC) $(LIBFFI_STATIC) \
                      -Wl,-Bdynamic \
                      $(LIBS_CURL) $(LIBS_CRYPTO) \
                      -static-libgcc \
                      $(LIBS_SYSTEM)

# Full static - everything embedded (portable)
# Note: Library order matters! OpenSSL before crypto, crypto before zstd
LDFLAGS_STATIC = -static \
                 -Wl,--start-group \
                 $(LIBTCC_STATIC) $(LIBFFI_STATIC) $(LIBCURL_STATIC) \
                 -lssl -lcrypto \
                 -lz -lzstd \
                 -lm -ldl -lpthread \
                 -Wl,--end-group

# Custom build - user can specify what to link statically
# Usage: make custom STATIC_LIBS="tcc ffi" DYNAMIC_LIBS="curl crypto"
STATIC_LIBS ?=
DYNAMIC_LIBS ?=
LDFLAGS_CUSTOM = $(call build_custom_ldflags)

# ============================================================================
# Build Targets
# ============================================================================

.DEFAULT_GOAL := help-quick

# Quick help (shows most common targets)
help-quick:
	@echo "════════════════════════════════════════════════════════════"
	@echo "  MalCREPL - Enhanced C REPL Build System"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "🚀 Quick Start:"
	@echo "  make setup              - Install all dependencies"
	@echo "  make build              - Build dynamic version (recommended)"
	@echo "  make static             - Build fully static (for distribution)"
	@echo ""
	@echo "📦 Common Targets:"
	@echo "  make build              - Dynamic build (fast, needs libraries)"
	@echo "  make semi-static        - Semi-static (recommended if static fails)"
	@echo "  make static             - Full static build (portable, large)"
	@echo "  make hybrid             - Hybrid static (balance of both)"
	@echo "  make custom             - Custom build (see 'make help-custom')"
	@echo ""
	@echo "🛠️  Development:"
	@echo "  make debug              - Build with debug symbols"
	@echo "  make release            - Optimized release build"
	@echo ""
	@echo "📚 More Information:"
	@echo "  make help               - Show all targets"
	@echo "  make help-custom        - Show custom build options"
	@echo "  make info               - Show build configuration"
	@echo ""

# ============================================================================
# Main Build Targets
# ============================================================================

# Alias: 'all' points to 'build'
all: build

# Default: Dynamic build
build: check-deps-minimal $(HEADERS) $(TARGET)
	@echo ""
	@echo "✅ Dynamic build complete!"
	@echo "   Binary: ./$(TARGET) (~30KB)"
	@echo "   Run: ./$(TARGET) <source.c>"
	@echo "   Dependencies: needs .so files installed"
	@echo ""

# Build with dynamic linking
$(TARGET): $(SOURCES) $(HEADERS)
	@echo "🔨 Building dynamic version..."
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS_DYNAMIC)

# Static build - fully standalone
# NOTE: May fail with OpenSSL version conflicts. Use 'make semi-static' instead.
static: check-deps-static $(HEADERS) ensure-static-curl $(TARGET)-static
	@echo ""
	@echo "✅ Static build complete!"
	@echo "   Binary: ./$(TARGET)-static (~800KB)"
	@echo "   Portable: works without dependencies"
	@echo "   Run: ./$(TARGET)-static <source.c>"
	@echo ""
	@echo "⚠️  If you see OpenSSL errors, use: make semi-static"
	@echo ""

$(TARGET)-static: $(SOURCES) $(HEADERS)
	@echo "🔨 Building fully static version..."
	$(CC) $(CFLAGS) -o $(TARGET)-static $(SOURCES) $(LDFLAGS_STATIC)

# Hybrid build - balance between size and portability
hybrid: check-deps-hybrid $(HEADERS) $(TARGET)-hybrid
	@echo ""
	@echo "✅ Hybrid build complete!"
	@echo "   Binary: ./$(TARGET)-hybrid (~200KB)"
	@echo "   Portable: TCC/FFI embedded, needs system libs"
	@echo "   Run: ./$(TARGET)-hybrid <source.c>"
	@echo ""

$(TARGET)-hybrid: $(SOURCES) $(HEADERS)
	@echo "🔨 Building hybrid version..."
	$(CC) $(CFLAGS) -o $(TARGET)-hybrid $(SOURCES) $(LDFLAGS_HYBRID)

# Semi-static build - avoids OpenSSL version conflicts
# Use this if full static build fails with OpenSSL errors
semi-static: check-deps-hybrid $(HEADERS) $(TARGET)-semi
	@echo ""
	@echo "✅ Semi-static build complete!"
	@echo "   Binary: ./$(TARGET)-semi (~50KB)"
	@echo "   Portable: TCC/FFI embedded, needs curl/openssl"
	@echo "   Run: ./$(TARGET)-semi <source.c>"
	@echo "   Note: Best option if full static fails"
	@echo ""

$(TARGET)-semi: $(SOURCES) $(HEADERS)
	@echo "🔨 Building semi-static version (TCC/FFI static, curl/openssl dynamic)..."
	$(CC) $(CFLAGS) -o $(TARGET)-semi $(SOURCES) $(LDFLAGS_SEMI_STATIC)

# ============================================================================
# Custom Build System
# ============================================================================

# Custom build - flexible static/dynamic mixing
# Examples:
#   make custom STATIC_LIBS="tcc ffi" DYNAMIC_LIBS="curl crypto"
#   make custom STATIC_LIBS="all"
#   make custom DYNAMIC_LIBS="all"
custom: check-deps-custom $(HEADERS)
	@echo "🔨 Building custom version..."
	@echo "   Static libs: $(or $(STATIC_LIBS),none)"
	@echo "   Dynamic libs: $(or $(DYNAMIC_LIBS),default)"
	$(CC) $(CFLAGS) -o $(TARGET)-custom $(SOURCES) $(call build_custom_ldflags)
	@echo ""
	@echo "✅ Custom build complete!"
	@echo "   Binary: ./$(TARGET)-custom"
	@ldd ./$(TARGET)-custom 2>/dev/null || echo "   (fully static)"
	@ls -lh ./$(TARGET)-custom
	@echo ""

# Function to build custom LDFLAGS
define build_custom_ldflags
$(strip \
	$(if $(findstring all,$(STATIC_LIBS)), \
		$(LDFLAGS_STATIC), \
		$(if $(STATIC_LIBS),-Wl,-Bstatic,) \
		$(foreach lib,$(STATIC_LIBS),$(call get_static_lib,$(lib))) \
		$(if $(STATIC_LIBS),-Wl,-Bdynamic,) \
	) \
	$(if $(findstring all,$(DYNAMIC_LIBS)), \
		$(LDFLAGS_DYNAMIC), \
		$(foreach lib,$(DYNAMIC_LIBS),$(call get_dynamic_lib,$(lib))) \
	) \
	$(if $(or $(STATIC_LIBS),$(DYNAMIC_LIBS)),,$(LDFLAGS_DYNAMIC)) \
	$(LIBS_SYSTEM) \
)
endef

# Helper functions for custom builds
get_static_lib = $(if $(filter tcc,$(1)),$(LIBTCC_STATIC)) \
                 $(if $(filter ffi,$(1)),$(LIBFFI_STATIC)) \
                 $(if $(filter curl,$(1)),$(LIBCURL_STATIC))

get_dynamic_lib = $(if $(filter tcc,$(1)),$(LIBS_TCC)) \
                  $(if $(filter ffi,$(1)),$(LIBS_FFI)) \
                  $(if $(filter curl,$(1)),$(LIBS_CURL)) \
                  $(if $(filter crypto,$(1)),$(LIBS_CRYPTO))

# ============================================================================
# Optimized Builds
# ============================================================================

# Optimized dynamic build
optimized: CFLAGS += -O3 -march=native -flto
optimized: build

# Debug build
debug: CFLAGS += -g -O0 -DDEBUG
debug: build

# Release build (optimized + stripped)
release: CFLAGS += -O3 -march=native -flto -DNDEBUG
release: build
	@strip $(TARGET)
	@echo "✂️  Stripped $(TARGET)"

# Static release (optimized + stripped)
static-release: CFLAGS += -O3 -march=native -flto -DNDEBUG
static-release: static
	@strip $(TARGET)-static
	@echo "✂️  Stripped $(TARGET)-static"

# ============================================================================
# Dependency Management
# ============================================================================

# Complete setup - installs everything
setup: install-deps
	@echo ""
	@echo "🎉 Setup complete! You can now build with:"
	@echo "   make build    # Dynamic build"
	@echo "   make static   # Static build"
	@echo ""

# Install all dependencies
install-deps:
	@echo "📦 Installing all dependencies..."
	@echo ""
	$(MAKE) install-system-deps
	$(MAKE) build-static-curl
	@echo ""
	@echo "✅ All dependencies installed!"

# Install system packages only
install-system-deps:
	@echo "📦 Installing system dependencies..."
	sudo apt-get update
	sudo apt-get install -y \
		tcc \
		libtcc-dev \
		libffi-dev \
		libcurl4-openssl-dev \
		libssl-dev \
		libzstd-dev \
		build-essential \
		wget \
		pkg-config
	@echo "✅ System dependencies installed"

# Build static libcurl (for static builds)
build-static-curl:
	@echo "📦 Building static libcurl..."
	@if [ -f "$(LIBCURL_STATIC_SYSTEM)" ]; then \
		echo "✅ Static libcurl already installed at $(LIBCURL_STATIC_SYSTEM)"; \
	else \
		echo "🔨 Building libcurl from source..."; \
		$(MAKE) build-curl-from-source; \
	fi

# Force rebuild of libcurl (if you have OpenSSL version issues)
rebuild-curl:
	@echo "🔄 Forcing rebuild of libcurl..."
	@rm -rf build/curl-8.6.0
	@sudo rm -f /usr/local/lib/libcurl.a /usr/local/lib/libcurl.la
	@sudo rm -f /usr/local/lib/pkgconfig/libcurl.pc
	$(MAKE) build-curl-from-source
	@echo "✅ libcurl rebuilt successfully"

# Build curl from source
build-curl-from-source:
	@echo "📥 Downloading curl source..."
	@mkdir -p build
	@cd build && \
	if [ ! -f curl-8.6.0.tar.gz ]; then \
		wget -q https://curl.se/download/curl-8.6.0.tar.gz; \
	fi
	@echo "📦 Extracting and configuring..."
	@cd build && \
	tar xzf curl-8.6.0.tar.gz
	@cd build/curl-8.6.0 && \
	./configure --disable-shared --enable-static \
	            --with-openssl \
	            --without-gnutls --without-nss \
	            --without-libpsl --without-librtmp --without-libidn2 \
	            --without-brotli --without-zstd --without-nghttp2 \
	            --disable-ldap --disable-rtsp --disable-proxy \
	            --disable-telnet --disable-tftp --disable-gopher \
	            --disable-dict --disable-imap --disable-pop3 \
	            --disable-smtp --disable-manual \
	            --prefix=/usr/local \
	            > /dev/null 2>&1
	@echo "🔨 Compiling libcurl (this may take a few minutes)..."
	@cd build/curl-8.6.0 && $(MAKE) -j4 > /dev/null 2>&1
	@echo "📦 Installing libcurl system-wide..."
	@cd build/curl-8.6.0 && sudo $(MAKE) install > /dev/null 2>&1
	@sudo ldconfig
	@echo "✅ Static libcurl installed at /usr/local/lib/libcurl.a"

# Build curl without SSL (for problematic static builds)
build-curl-nossl:
	@echo "📥 Building curl without SSL (for static builds)..."
	@mkdir -p build
	@cd build && \
	if [ ! -f curl-8.6.0.tar.gz ]; then \
		wget -q https://curl.se/download/curl-8.6.0.tar.gz; \
	fi
	@cd build && \
	rm -rf curl-8.6.0 && \
	tar xzf curl-8.6.0.tar.gz
	@cd build/curl-8.6.0 && \
	./configure --disable-shared --enable-static \
	            --without-ssl \
	            --without-gnutls --without-nss \
	            --without-libpsl --without-librtmp --without-libidn2 \
	            --without-brotli --without-zstd --without-nghttp2 \
	            --disable-ldap --disable-rtsp --disable-proxy \
	            --disable-telnet --disable-tftp --disable-gopher \
	            --disable-dict --disable-imap --disable-pop3 \
	            --disable-smtp --disable-manual \
	            --prefix=/usr/local \
	            > /dev/null 2>&1
	@echo "🔨 Compiling libcurl without SSL..."
	@cd build/curl-8.6.0 && $(MAKE) -j4 > /dev/null 2>&1
	@echo "📦 Installing libcurl system-wide..."
	@cd build/curl-8.6.0 && sudo $(MAKE) install > /dev/null 2>&1
	@sudo ldconfig
	@echo "✅ Static libcurl (no SSL) installed"
	@echo "⚠️  Note: HTTP only, no HTTPS support"

# Download stb_c_lexer.h
stb_c_lexer.h:
	@echo "📥 Downloading stb_c_lexer.h..."
	@wget -q https://raw.githubusercontent.com/nothings/stb/master/stb_c_lexer.h
	@echo "✅ stb_c_lexer.h downloaded"

# Ensure static curl is available
ensure-static-curl:
	@if [ ! -f "$(LIBCURL_STATIC)" ]; then \
		echo "❌ Static libcurl not found at $(LIBCURL_STATIC)"; \
		echo "   Run: make build-static-curl"; \
		exit 1; \
	fi

# ============================================================================
# Dependency Checking
# ============================================================================

# Minimal check for dynamic builds
check-deps-minimal:
	@echo "🔍 Checking dependencies for dynamic build..."
	@command -v $(CC) >/dev/null 2>&1 || (echo "❌ gcc not found"; exit 1)
	@command -v tcc >/dev/null 2>&1 || (echo "❌ tcc not found (run: make install-system-deps)"; exit 1)
	@pkg-config --exists libffi || (echo "❌ libffi not found (run: make install-system-deps)"; exit 1)
	@pkg-config --exists libcurl || (echo "❌ libcurl not found (run: make install-system-deps)"; exit 1)
	@echo "✅ All dependencies available for dynamic build"

# Check for hybrid build
check-deps-hybrid: check-deps-minimal
	@[ -f "$(LIBTCC_STATIC)" ] || (echo "❌ $(LIBTCC_STATIC) not found"; exit 1)
	@[ -f "$(LIBFFI_STATIC)" ] || (echo "❌ $(LIBFFI_STATIC) not found"; exit 1)
	@echo "✅ All dependencies available for hybrid build"

# Check for static build
check-deps-static: check-deps-hybrid
	@if [ ! -f "$(LIBCURL_STATIC)" ]; then \
		echo "❌ Static libcurl not found"; \
		echo "   Building from source (this will take a few minutes)..."; \
		$(MAKE) build-static-curl; \
	fi
	@echo "✅ All dependencies available for static build"

# Check for custom build
check-deps-custom:
	@echo "🔍 Checking dependencies for custom build..."
	@if echo "$(STATIC_LIBS)" | grep -q "tcc\|all"; then \
		[ -f "$(LIBTCC_STATIC)" ] || (echo "❌ $(LIBTCC_STATIC) not found"; exit 1); \
	fi
	@if echo "$(STATIC_LIBS)" | grep -q "ffi\|all"; then \
		[ -f "$(LIBFFI_STATIC)" ] || (echo "❌ $(LIBFFI_STATIC) not found"; exit 1); \
	fi
	@if echo "$(STATIC_LIBS)" | grep -q "curl\|all"; then \
		[ -f "$(LIBCURL_STATIC)" ] || (echo "❌ $(LIBCURL_STATIC) not found, building..."; $(MAKE) build-static-curl); \
	fi
	@echo "✅ Dependencies checked"

# Full dependency report
check-deps:
	@echo "════════════════════════════════════════════════════════════"
	@echo "  Dependency Status Report"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "📦 System Tools:"
	@command -v $(CC) >/dev/null 2>&1 && echo "  ✅ gcc" || echo "  ❌ gcc"
	@command -v tcc >/dev/null 2>&1 && echo "  ✅ tcc" || echo "  ❌ tcc"
	@command -v wget >/dev/null 2>&1 && echo "  ✅ wget" || echo "  ❌ wget"
	@command -v pkg-config >/dev/null 2>&1 && echo "  ✅ pkg-config" || echo "  ❌ pkg-config"
	@echo ""
	@echo "📚 Development Libraries:"
	@[ -f /usr/include/libtcc.h ] && echo "  ✅ libtcc-dev" || echo "  ❌ libtcc-dev"
	@pkg-config --exists libffi && echo "  ✅ libffi-dev" || echo "  ❌ libffi-dev"
	@pkg-config --exists libcurl && echo "  ✅ libcurl-dev" || echo "  ❌ libcurl-dev"
	@pkg-config --exists openssl && echo "  ✅ libssl-dev" || echo "  ❌ libssl-dev"
	@echo ""
	@echo "📦 Static Libraries:"
	@[ -f "$(LIBTCC_STATIC)" ] && echo "  ✅ $(LIBTCC_STATIC)" || echo "  ❌ libtcc.a not found"
	@[ -f "$(LIBFFI_STATIC)" ] && echo "  ✅ $(LIBFFI_STATIC)" || echo "  ❌ libffi.a not found"
	@[ -f "$(LIBCURL_STATIC_SYSTEM)" ] && echo "  ✅ /usr/local/lib/libcurl.a (system)" || \
	 ([ -f "$(LIBCURL_STATIC_LOCAL)" ] && echo "  ✅ ./build/curl-8.6.0/lib/.libs/libcurl.a (local)" || \
	  echo "  ❌ libcurl.a not found (run: make build-static-curl)")
	@echo ""
	@echo "📄 Required Headers:"
	@[ -f stb_c_lexer.h ] && echo "  ✅ stb_c_lexer.h" || echo "  📥 stb_c_lexer.h (will download)"
	@[ -f enclib.h ] && echo "  ✅ enclib.h" || echo "  ❌ enclib.h (required!)"
	@[ -f netlib.h ] && echo "  ✅ netlib.h" || echo "  ❌ netlib.h (required!)"
	@echo ""
	@echo "To install missing dependencies:"
	@echo "  make install-system-deps    # Install system packages"
	@echo "  make build-static-curl      # Build static libcurl"
	@echo "  make setup                  # Install everything"
	@echo ""

# ============================================================================
# Utility Targets
# ============================================================================

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -f stb_c_lexer.h
	@rm -f $(TARGET) $(TARGET)-static $(TARGET)-semi $(TARGET)-hybrid $(TARGET)-custom
	@rm -f $(OBJECTS)
	@rm -f *.o
	@echo "✅ Cleaned"

# Deep clean (including downloaded files)
distclean: clean
	@echo "🧹 Deep cleaning..."
	@rm -rf build
	@rm -f stb_c_lexer.h
	@echo "✅ Deep cleaned"

# Install to system
install: build
	@echo "📦 Installing to /usr/local/bin..."
	@sudo install -m 755 $(TARGET) /usr/local/bin/
	@echo "✅ Installed: /usr/local/bin/$(TARGET)"

# Install static version
install-static: static
	@echo "📦 Installing static version to /usr/local/bin..."
	@sudo install -m 755 $(TARGET)-static /usr/local/bin/$(TARGET)
	@echo "✅ Installed: /usr/local/bin/$(TARGET) (static)"

# Uninstall from system
uninstall:
	@echo "🗑️  Uninstalling from /usr/local/bin..."
	@sudo rm -f /usr/local/bin/$(TARGET)
	@echo "✅ Uninstalled"

# ============================================================================
# Information and Help
# ============================================================================

# Show build configuration
info:
	@echo "════════════════════════════════════════════════════════════"
	@echo "  MalCREPL Build Configuration"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "🔧 Compiler:"
	@echo "  CC:           $(CC)"
	@echo "  CFLAGS:       $(CFLAGS)"
	@echo "  Architecture: $(ARCH)"
	@echo ""
	@echo "📦 Library Paths:"
	@echo "  LIBDIR:       $(LIBDIR)"
	@echo "  TCC static:   $(LIBTCC_STATIC)"
	@echo "  FFI static:   $(LIBFFI_STATIC)"
	@echo "  CURL static:  $(LIBCURL_STATIC) ($(CURL_SOURCE))"
	@echo ""
	@echo "🔗 Linking Options:"
	@echo "  Dynamic:      $(LDFLAGS_DYNAMIC)"
	@echo ""
	@echo "📊 Build Targets:"
	@echo "  build:        Dynamic build (~30KB)"
	@echo "  static:       Full static (~800KB)"
	@echo "  hybrid:       Hybrid static (~200KB)"
	@echo "  custom:       Custom (see help-custom)"
	@echo ""

# Comprehensive help
help:
	@echo "════════════════════════════════════════════════════════════"
	@echo "  MalCREPL - Complete Build System Help"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "🚀 Setup & Installation:"
	@echo "  make setup                  - Complete setup (recommended)"
	@echo "  make install-system-deps    - Install system packages only"
	@echo "  make build-static-curl      - Build static libcurl"
	@echo "  make check-deps             - Check all dependencies"
	@echo ""
	@echo "🔨 Build Targets:"
	@echo "  make build                  - Dynamic build (default)"
	@echo "  make all                    - Same as 'make build'"
	@echo "  make semi-static            - Semi-static (TCC/FFI static, rest dynamic)"
	@echo "  make static                 - Full static build"
	@echo "  make hybrid                 - Hybrid static build"
	@echo "  make custom                 - Custom build (see help-custom)"
	@echo ""
	@echo "⚡ Optimized Builds:"
	@echo "  make optimized              - Optimized dynamic"
	@echo "  make release                - Optimized + stripped dynamic"
	@echo "  make static-release         - Optimized + stripped static"
	@echo "  make debug                  - Debug build with symbols"
	@echo ""
	@echo "📦 Installation:"
	@echo "  sudo make install           - Install dynamic version"
	@echo "  sudo make install-static    - Install static version"
	@echo "  sudo make uninstall         - Remove from system"
	@echo ""
	@echo "🧹 Cleanup:"
	@echo "  make clean                  - Remove build artifacts"
	@echo "  make distclean              - Deep clean (includes downloads)"
	@echo ""
	@echo "📚 Information:"
	@echo "  make info                   - Show build configuration"
	@echo "  make help                   - This help message"
	@echo "  make help-custom            - Custom build help"
	@echo ""
	@echo "💡 Quick Examples:"
	@echo "  make && ./malcrepl test.c              # Build and run"
	@echo "  make static && ./malcrepl-static test.c  # Static build"
	@echo "  make custom STATIC_LIBS=\"tcc ffi\"      # Custom build"
	@echo ""

# Custom build help
help-custom:
	@echo "════════════════════════════════════════════════════════════"
	@echo "  Custom Build System - Flexible Static/Dynamic Linking"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "🎯 Purpose:"
	@echo "  Create builds with specific libraries linked statically or"
	@echo "  dynamically. Useful for balancing portability and size."
	@echo ""
	@echo "📝 Syntax:"
	@echo "  make custom STATIC_LIBS=\"lib1 lib2\" DYNAMIC_LIBS=\"lib3 lib4\""
	@echo ""
	@echo "📦 Available Libraries:"
	@echo "  tcc       - TinyCC compiler library"
	@echo "  ffi       - Foreign Function Interface"
	@echo "  curl      - HTTP/HTTPS client library"
	@echo "  crypto    - OpenSSL cryptography"
	@echo "  all       - All libraries (for STATIC_LIBS or DYNAMIC_LIBS)"
	@echo ""
	@echo "💡 Examples:"
	@echo ""
	@echo "  # TCC and FFI static, others dynamic"
	@echo "  make custom STATIC_LIBS=\"tcc ffi\" DYNAMIC_LIBS=\"curl crypto\""
	@echo ""
	@echo "  # Everything static (same as 'make static')"
	@echo "  make custom STATIC_LIBS=\"all\""
	@echo ""
	@echo "  # Everything dynamic (same as 'make build')"
	@echo "  make custom DYNAMIC_LIBS=\"all\""
	@echo ""
	@echo "  # Only TCC static, rest dynamic"
	@echo "  make custom STATIC_LIBS=\"tcc\" DYNAMIC_LIBS=\"ffi curl crypto\""
	@echo ""
	@echo "  # Curl static, others dynamic (for portability)"
	@echo "  make custom STATIC_LIBS=\"curl\" DYNAMIC_LIBS=\"tcc ffi crypto\""
	@echo ""
	@echo "📊 Comparison:"
	@echo "  Configuration          Size    Portability  Use Case"
	@echo "  ─────────────────────  ──────  ───────────  ─────────────────"
	@echo "  all dynamic            ~30KB   Low          Development"
	@echo "  tcc+ffi static         ~200KB  Medium       Docker containers"
	@echo "  all static             ~800KB  High         Distribution"
	@echo "  custom                 varies  varies       Specific needs"
	@echo ""
	@echo "🔍 Check Results:"
	@echo "  ldd ./malcrepl-custom         # See dynamic dependencies"
	@echo "  ls -lh ./malcrepl-custom      # Check binary size"
	@echo "  file ./malcrepl-custom        # Check binary type"
	@echo ""

# ============================================================================
# Special Targets
# ============================================================================

.PHONY: all build static semi-static hybrid custom optimized debug release static-release \
        setup install-deps install-system-deps build-static-curl rebuild-curl \
        build-curl-from-source ensure-static-curl \
        check-deps check-deps-minimal check-deps-hybrid check-deps-static check-deps-custom \
        clean distclean \
        install install-static uninstall \
        info help help-quick help-custom \
        stb_c_lexer.h

# Prevent make from deleting intermediate files
.SECONDARY:

# Silent mode for cleaner output
ifndef VERBOSE
.SILENT:
endif