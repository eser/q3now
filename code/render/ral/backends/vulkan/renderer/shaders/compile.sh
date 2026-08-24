#!/bin/sh
# Thin wrapper. Single source of truth is compile.mjs.
exec node "$(dirname "$0")/compile.mjs" "$@"
