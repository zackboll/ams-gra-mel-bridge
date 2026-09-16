# C ABI
C11 public headers; private C++20. No STL, C++ references, templates, exceptions,
or provider object layouts cross the C boundary. Use explicit ownership and
error/timeout semantics. Metadata conversions must not silently lose units,
precision, identity, variants, or optional fields. All new exports need C tests.
Read docs/c-abi-policy.md before extending an ABI.
