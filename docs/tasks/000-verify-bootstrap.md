# Cline task 000 — verify bootstrap locally

Read AGENTS.md and .clinerules/. Do not add provider APIs or new features yet.
Run make test-native, alr -C ada build, and alr -C ada/tests run. If a local
GNAT/GPRbuild is already on PATH, also run make test-ada and make check.
The prepared archive was not Ada-compiled: GNAT/GPRbuild/Alire were unavailable
in its generation environment. TOML parsing is not Alire validation.

Check transitive GPR imports, native pre-build order, and library loading at
runtime. Fix any bootstrap build/link errors without having GPR recompile the
C++ code, weakening tests, or moving C imports into the public Ada API.
Do not modify global toolchain or Git configuration.

Update docs/bootstrap-validation.md with exact results and tool versions.
Keep the implementation scoped to the ABI-version operation. Report changed
files and commands. Do not commit, push, or publish unless requested.
Stop before task 001.
