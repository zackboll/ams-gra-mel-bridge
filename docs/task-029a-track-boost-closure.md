# Task 029A Track Boost declaration closure

Task 029A adds no Track application-facing functionality. It makes the pinned
IR MEL `TrackChannel.h` declaration closure reproducible without a system
Boost installation or build-time downloads.

## Source pin

- Boost version: 1.83.0
- Official archive: `boost_1_83_0.tar.bz2`
- Official archive SHA-256:
  `6478edfe2f3305127cffe8caf73ea0176c53769f4bf1585be237eb30798c3b8e`
- Official source commit: `564e2ac16907019696cdaba8a93e3588ec596062`

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` builds in an
environment containing `boost-devel` and its produced runtime explicitly
carries Boost 1.83.0. This does **not** mean the IR MEL standard requires
Boost 1.83.0. It is this project's compatibility pin for satisfying an
accidental published `TrackDataUpdate.h` declaration dependency against the
pinned integration-provider environment.

## Measured closure

The official archive was verified before extraction outside the repository.
GCC 14.2 observed 481 Boost headers; Clang 19.1 observed 482; their union is
483 headers. Exactly those 483 headers are under
`native/vendor/boost-1.83.0/boost/`, with the exact upstream
`LICENSE_1_0.txt` alongside them: 484 Boost vendor files in total. Every
vendored Boost header is byte-identical to Boost 1.83.0; checksums are recorded
in `docs/upstream-files.sha256.md`.

The native `check_track_header_boost_closure` build target dependency-probes
the pinned Track header with the configured compiler and fails if a Boost path
resolves outside `native/vendor/boost-1.83.0/`. GCC and Clang validation
observed no `/usr/include/boost` or `/usr/local/include/boost` dependency.
There is no FetchContent, ExternalProject, or build-time download.

## Scope boundary

`TrackChannel.h` includes `TrackDataUpdate.h`, which includes
`boost/numeric/ublas/io.hpp`; that is a compiler declaration closure only.
The presence of Boost and TrackDataUpdate declarations in the compiler closure
does **not** mean TrackDataUpdate is implemented by `ams_mel_c`.

Task 029A introduces no Track C ABI/API, no IRSTTrackReport callback, and no
safe Ada, Rust, or Python Track surface. TrackDataUpdate remains unimplemented.
Task 029B may implement the `@RequiredIfTrack` IRSTTrackReport channel.
