# Runner Monitor dependency-minimisation core

`src/native2/` is the stricter first-party dependency-minimisation track. It is not a separate user-facing product and must never displace the shipping native application with Python or another interpreted fallback.

Allowed shared project dependency:
- the exact pinned Infiltratr Common release.

Target end-state restrictions for this core:
- no GTK/GLib;
- no Qt;
- no Tk/Tkinter;
- no Python;
- no GitHub CLI runtime dependency;
- no libcurl;
- no OpenSSL-family, GnuTLS or NSS dependency;
- no second shared Infiltrator library.

The shipping product may retain a platform UI/API adapter until this core reaches parity. Replacement is capability-by-capability and must preserve behaviour, data and release quality.

Common is consumed as-is and is not changed to absorb Runner Monitor-specific policy.
