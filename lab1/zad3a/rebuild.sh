#!/usr/bin/env sh
# Note: This is necessary because on NixOS static glibc is not provided by default, and currently there's a bug that makes shared libraries fail to build with static libs in path.
make clean
make dyn
nix-shell -p glibc.static --run 'make stat'
