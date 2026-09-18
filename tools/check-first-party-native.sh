#!/bin/sh
set -eu

files="$(find src/native2 tests/first_party_core.c -type f -print)"
forbidden='gtk|glib|gobject|qt|tkinter|python|libcurl|curl/curl|openssl|libssl|gnutls|nss3|gh api|gh auth'
if grep -Eini "$forbidden" $files; then
  echo "RunnerScope native-next contains a forbidden external dependency." >&2
  exit 1
fi
