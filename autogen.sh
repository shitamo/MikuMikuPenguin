#!/bin/sh
# Regenerate the autotools build system (configure, Makefile.in, ...).
set -e
cd "$(dirname "$0")"
mkdir -p m4 build-aux
autoreconf --force --install --verbose
