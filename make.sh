#!/usr/bin/env bash

[ -z "${debug}" ] && debug=1
clang -o bfjit -include macros.h -DDEBUG="${debug}" x86_64.c main.c