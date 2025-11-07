# Crepl

A simple application that allows you dynamically call C functions from DLL using [libffi](github.com/libffi/libffi). Developed recreationally to test out libffi.

## Quick Start

```console
$ cc -o nob nob.c
$ ./nob
$ ./crepl ./raylib-5.5_linux_amd64/lib/libraylib.so
> InitWindow 800 600 "Bajoding"
> BeginDrawing
> ClearBackground 0xFF0000FF
> EndDrawing
```
