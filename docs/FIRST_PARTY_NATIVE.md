# Runner Monitor first-party native boundary

The native rewrite is owned entirely by Runner Monitor.

Allowed project dependency:
- the exact pinned Infiltratr Common release

Allowed platform boundary:
- C/POSIX/Linux system calls and kernel/desktop protocols implemented by Runner Monitor itself

Not allowed in the native product:
- GTK / GLib
- Qt
- Tk / Tkinter
- Python
- GitHub CLI as an application runtime dependency
- libcurl
- OpenSSL / LibreSSL / BoringSSL
- GnuTLS
- NSS
- a second shared Infiltrator library

GitHub authentication, HTTPS/TLS, protocol parsing, Linux desktop integration and
runner/service integration must remain Runner Monitor-owned code. Common is consumed
as-is and must not be changed to satisfy Runner Monitor.
