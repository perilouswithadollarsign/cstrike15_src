#!/bin/bash

export CC=clang
export CXX=clang++

# export CPPFLAGS="-isysroot /Developer/SDKs/MacOSX10.6.sdk -mmacosx-version-min=10.6 -Oz -fvisibility=hidden"
export CPPFLAGS="-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk -mmacosx-version-min=10.5 -Oz -fvisibility=hidden"
export CFLAGS="-arch i386 -g $CPPFLAGS"
export CXXFLAGS="$CFLAGS"

./configure --enable-shared=no
