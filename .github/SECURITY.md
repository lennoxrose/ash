# Security Policy

## Supported versions

Ash is in beta. Only the **latest release** (see the
[Releases page](https://github.com/lennoxrose/ash/releases/latest)) and `main` receive fixes.

## Reporting a vulnerability

Please do **not** open a public issue for a security problem.

Report it privately through GitHub: **Security tab -> [Report a vulnerability](https://github.com/lennoxrose/ash/security/advisories/new)**.

Include the Ash version (`ashvm --version`), the platform, a minimal `.ash` program or input that triggers
it, and what you expected versus what happened. Memory errors in `ashvm`, `kiln`'s generated code, the
compiler front end, `apm` (path handling, downloads) or the ownership pass (a use-after-free, a double free
or a missed move on code that `memory-safety: on` accepts) are all in scope.

You can expect an acknowledgement within a week. Confirmed issues are fixed on `main`, released as a new
version, and credited in the changelog unless you prefer otherwise.

## Out of scope

Bugs that need `memory-safety: off` or `System.free`, which are explicitly unchecked, and denial of service
by a program you chose to run (an infinite loop is a valid Ash program).
