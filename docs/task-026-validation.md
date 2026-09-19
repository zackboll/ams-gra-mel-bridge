# Task 026 validation

ABI 0.1 remains 56 exports and eight native CTest targets. Image metadata kind 4 is the owned
canonical `NavigationReportResp` value: signed nanoseconds plus full uint32 command/request IDs.
All four required callbacks share the stream-owned bounded DROP-INCOMING FIFO.

Mock tests prove synchronous registration, fourth-stage failure/throw safety, null and allocation
recovery, mixed FIFO overflow, and post-teardown event ownership. Pinned Squall proves capability
advertisement and all four registrations only; it emits a response only after NavigationReport send,
which remains NOT IMPLEMENTED and is deferred to Task 027.
