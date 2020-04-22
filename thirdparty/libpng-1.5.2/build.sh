#!/bin/bash

CC="clang -arch i386 -arch x86_64 -g -isysroot /Developer/SDKs/MacOSX10.6.sdk -mmacosx-version-min=10.5" \
AR_RC="libtool -static -o" \
make -f scripts/makefile.gcc "$@"

lipo -info libpng.a
