# Python MEL binding

This directory provides the dependency-free `ams_mel` Python API over the
existing native façade:

```text
Python safe API -> private ctypes -> ams_mel_c -> published C++ MEL provider
```

Set `AMS_MEL_NATIVE_LIB` to the absolute full path of `libams_mel_c.so.0` (or
`libams_mel_c.so`) before importing `ams_mel`. The provider library remains a
separate path passed to `Session.open`; Python never loads provider factories or
models provider C++ interfaces directly.

The current API supports façade ABI/version queries, provider `Session`
open/version/close, and host-memory Mono8 `ImageStream`
open/start/receive/counters/stop/close. Frames preserve the supported metadata,
and `Frame.pixels` is an owned `bytes` copy: no view into ctypes storage, provider
storage, or the native receive queue escapes. Timeout and clean stream stop are
distinct structured errors, and a zero receive timeout polls.

An `ImageStream` independently owns the native child and does not retain its
Python parent `Session`; the Session may close first while the stream continues
receiving. Native close decides whether an owner was cleared after cleanup
failure, so a retained stream remains open for a later close retry.

Operations using one Session or ImageStream must follow the native external-
serialization contract. At most one receive operation may consume a stream at a
time. The GIL is not a substitute for that contract, the Python API does not
claim ImageStream is thread-safe, and concurrent stop/receive is outside this
profile. Context managers do not implicitly start streams. If a `with` body
raises, cleanup is attempted without replacing that exception; close failures
propagate on normal context exit.

This is not a native extension or a zero-copy interface. Python C2, RF, real
Squall Python validation, NumPy/zero-copy image views, wheels, and PyPI
publication are not included.

From the repository root, run:

```sh
make test-python
```
