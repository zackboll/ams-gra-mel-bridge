# Task 001/002 upstream provenance

Verified 2026-09-15 from immutable Git commit objects, not default-branch file
URLs. The selected commits are the candidate submodule revisions recorded by
the umbrella inventory and all have commit message `Release 2026.06.01`.

| Component | Repository | Commit | Tree |
|---|---|---|---|
| Common MEL | `open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel` | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` | `f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f` |
| IR MEL | `open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel` | `8d9224519f12b44e0b28815755c56a32a28d24a0` | `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec` |
| AMS Math | `open-arsenal/ams-gra-hello-world-sk-libraries-ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` | `7101189c30017cc25f277db39866156e4ab5cf38` |

## Selected closure

`native/vendor` contains unmodified files at those revisions: 28 Common MEL
headers, 41 IR MEL headers, and two AMS Math headers. Task 001's Control probe
selected 44 headers. Task 002's GCC 14.2 dependency probe adds
`image/ImageChannel.h`, `ImageListener.h`, `FrameHeader.h`, and `Buffer.h`, whose
published include graph expands the complete closure to 71 headers. Exact file
checksums are recorded in
`upstream-files.sha256.md`.

The required published boundary is:

```cpp
extern "C" std::shared_ptr<API_Manager>
getAPI_Manager(const std::string& instance);

extern "C" std::shared_ptr<ams::iface::irmel::Control>
getControl(std::string_view instance, std::shared_ptr<API_Manager> sam);

extern "C" std::shared_ptr<ams::iface::irmel::Buffer>
getBuffer(std::string_view instance, std::shared_ptr<API_Manager> sam);
```

The used virtual operations are `Control::init(const std::string&)` and
`Control::getVersionInfo() const`. `mel::VersionInfo` contains `uint32_t`
API/library values and `std::string` vendor/description values; all are exposed
by the façade without substitution or truncation.

No upstream compiled source is required for this declaration-only slice.
Common MEL and IR MEL are upstream interface targets. The complete upstream IR
CMake project nevertheless adds the whole AMS Math static library, whose five
`.cpp` files require Boost headers. That nominal full-project dependency is not
in this slice's compiler-observed closure. Local build dependencies are Linux
`dlopen`/`dlsym`, CMake 3.20+, and a C++20 compiler/standard library mutually
ABI-compatible with the provider. The test provider is built by the same CMake
configuration/compiler as the adapter.

Task 002 uses `Control::attachChannel`/`detachChannel`,
`Channel::registerBuffer`/`unregisterBuffer`/`enable`/`disable`, `Buffer::init`
and `release`, and `ImageListener::onImage`. `Config` carries complete UCI IDs,
component location, and the listener. `FrameHeader` supplies signed nanosecond
times, dimensions, FOV radians, format/frame/subframe/image metadata, dither,
offsets, and band index. The broad additional headers are compiler-required by
the unmodified published `ImageChannel` interface; optional camera/metadata
operations remain unsupported by this façade.

## License and notices

Each selected repository supplies Apache License 2.0 `LICENSE` and federal
government `INTENT.md`; both are retained under its `native/vendor` directory
and installed as notices. The three license files share SHA-256
`2f1aa718ddb1a34ed1581e3c57dddd1bb516fb7657a9c4076e8ff95b91d9844d`.

## Open assumptions and upstream issues

- C linkage makes factory names discoverable but does not make their C++
  parameters/results ABI-portable. Matching compiler, standard library,
  exception/RTTI settings, architecture, and compatible declarations remain
  required. Version query is not a universal compatibility probe.
- Upstream does not publish a standalone ABI negotiation operation before the
  first C++ factory call.
- `Control::init` documents no concurrency or rollback details beyond its
  `Return`; task 001 treats every non-`Success` value as failed initialization.
- String encoding is not stated by upstream. This profile validates and
  requires UTF-8 vendor/description values without embedded NUL.
- Provider destructors and shared-pointer deleters must not throw. No callbacks,
  pending requests, channels, buffers, or child resources exist in task 001.
- Task 002's `disable()` declaration does not document callback quiescence.
  Pinned Squall (`b1015728f904c799fa0c07489fce48e78f67845f`) only clears
  `enabled_` and delegates control disable there; `~SquallImageChannel` resets
  its `UdpDataReceiver`. The façade therefore retains listener/buffers/storage
  through channel destruction and then drains its own in-flight callbacks.
- `Control.h` includes a cyclic and broad declaration graph (including Channel
  declarations and Quaternion) even though task 001 invokes no such feature.
- Common MEL's `CMN_MEL_API_VERSION` string (`"4.0"`), numeric provider
  `VersionInfo`, exact source revisions, and façade ABI 0.1 are distinct values.

Normal configure, build, and tests use only the vendored closure and never
fetch source, provider binaries, credentials, containers, or hardware.
