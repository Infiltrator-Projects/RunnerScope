# cJSON 1.7.19

Unmodified upstream `cJSON.c`, `cJSON.h`, and MIT `LICENSE`, from
<https://github.com/DaveGamble/cJSON/tree/c859b25da02955fef659d658b8f324b5cde87be3>.
The upstream `v1.7.19` tag identifies that exact commit. Files were obtained
through the GitHub contents API with `ref=v1.7.19` and decoded without editing.

SHA-256 identities:

```text
298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d  cJSON.c
25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee  cJSON.h
a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c  LICENSE
```

RunnerScope builds this as static implementation code with
`CJSON_HIDE_SYMBOLS` and `CJSON_NESTING_LIMIT=128`. GitHub response limits,
schema checks, UTF-8 validation, separator normalization, credential handling,
pagination and error presentation remain RunnerScope policy in
`src/core/github.c` and `src/core/json.c`. No cJSON global allocation hooks or error-pointer API are
used. Monitoring and sign-in parsing share one lock because the upstream decoder retains a global
last-error record; projected snapshots are request-owned.
