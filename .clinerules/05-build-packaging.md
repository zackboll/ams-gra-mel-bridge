# Build and packaging
CMake owns native compilation; GPR imports the native library. Do not compile
the same C++ implementation again in the Ada project. Local Alire pins are for
development, not proof of registry readiness. No unpinned upstream fetches or
implicit sibling source dependencies in release archives. Preserve licenses.
Do not publish, tag, push, or change global Git settings without authorization.
