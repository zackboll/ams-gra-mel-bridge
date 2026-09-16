# Testing
Native tests must include a translation unit compiled as C, not only C++.
Keep tests active in release mode; no disabled-assertion-only validation.
Ordinary builds and tests must not require Squall, credentials, containers,
hardware, or build-time downloads. Test a separate mock provider before a real
one. Report actual commands/results and unavailable tools honestly.
