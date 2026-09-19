# Task 025 validation

Task 025 keeps ABI 0.1 and exactly 56 exports. The existing six Image metadata operations
now carry BadPixelList, LineOfSightReport, and LineOfSightEuler in one stream-owned bounded
DROP-INCOMING FIFO. Registration order is BadPixelList, Report, then Euler; native tests
cover synchronous callbacks, partial registration, null LOS pointers, allocation recovery,
mixed-kind retention/order, and owned event lifetime after provider teardown.

Safe Ada exposes complete owned Azimuth_Elevation, Line_Of_Sight_Report, and
Line_Of_Sight_Euler values and rejects wrong-kind accessors deterministically. Raw Rust and
private Python synchronize constants/records/layout probes only; no safe Rust or public
Python LOS API is added.

NavigationReportResp, NavigationReport send, LineOfSightQuaternion, CameraCommand,
OpticalDistortionMap, CandidateObject metadata, and NUC_TempData remain unimplemented.
