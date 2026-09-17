# Python MEL session foundation

This directory provides the dependency-free `ams_mel` Python API over the
existing native façade:

```text
Python safe API -> private ctypes -> ams_mel_c -> published C++ MEL provider
```

Set `AMS_MEL_NATIVE_LIB` to the absolute full path of `libams_mel_c.so.0` (or
`libams_mel_c.so`) before importing `ams_mel`. The provider library remains a
separate path passed to `Session.open`; Python never loads provider factories or
models provider C++ interfaces directly.

The current API supports façade ABI version queries, provider Session
open/version/close, context-manager cleanup, best-effort finalization, strict
UTF-8 inputs, and structured diagnostics. Operations using one Session must be
externally serialized. If a `with` body raises, cleanup is attempted without
replacing that exception; close failures propagate on a normal context exit.

This is not a native extension or a zero-copy interface. IR image reception,
C2, RF, real Squall validation, wheels, and PyPI publication are not included.

From the repository root, run:

```sh
make test-python
```
