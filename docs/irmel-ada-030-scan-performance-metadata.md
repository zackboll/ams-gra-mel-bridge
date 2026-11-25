# IRMEL-ADA-030 — Safe Ada Scheduling ScanPerformanceReport reception

This slice implements only the optional
`SchedulingChannel::registerMetadataCallback(ScanPerformanceReport)` overload.
PR #263 merged at `73a5b4ad16c6ef88d18c500d06e85d29e27fe4f4` and its
TaskEvents implementation is present. Preflight found no overlapping PR or
branch. The initial fetched main was `c882c474fe162615c2e65e6f5864c7a75a9264dd`
(Python Track foundation since #263). The qualification base also includes
`e32457e1db65f92914bcee67bfbe37cb1282ce57` (Rust Scheduling Stare). Neither
merge changes the native/IR Ada coverage baseline. The pinned IR MEL and
Common MEL declarations and all vendored source bytes remain unchanged.

## Complete snapshots

`getCommanded()` and `getAchieved()` each return a `ScanParam` by value. They
map independently to `commanded` and `achieved` in
`ams_mel_ir_scan_performance_report_v1`, and `Commanded` and `Achieved` in
`AMS.MEL.IR.Scheduling.Metadata.Scan_Performance_Report`.

| Published getter | Existing `ams_mel_ir_scan_param_v1` field | Existing `C2.Scan_Parameters` field |
|---|---|---|
| `getIsDefinedWithRangeAndAltitude` | `elevation_defined_with_range_and_altitude` | `Elevation_Defined_With_Range_And_Altitude` |
| `getCenter` (`AzEl.az`, `AzEl.el`) | `center_az_rad`, `center_el_rad` | `Center_Azimuth_Rad`, `Center_Elevation_Rad` |
| `getCentFrameRefEl` | `center_frame_ref_el` | `Center_Frame_Reference_EL` |
| `getCentFrameRefAz` | `center_frame_ref_az` | `Center_Frame_Reference_AZ` |
| `getScanWidth` | `scan_width_rad` | `Scan_Width_Rad` |
| `getScanHeight` | `scan_height_rad` | `Scan_Height_Rad` |
| `getScanType` (`getContinuousScan`, `getReturning`, `getAgileScan`) | `scan_type.{continuous_scan,returning,agile_scan}` | `Continuous_Scan`, `Returning`, `Agile_Scan` |
| `getScanId` | `scan_id` | `Scan_ID` |
| `getScanRate` | `scan_rate_rad_per_second` | `Scan_Rate_Rad_Per_Second` |
| `getPreferredRevisitInterval` | `preferred_revisit_interval_seconds` | `Preferred_Revisit_Interval_Seconds` |
| `getRequiredRevisitInterval` | `required_revisit_interval_seconds` | `Required_Revisit_Interval_Seconds` |
| `getMaxRangeOfInterest` | `max_range_of_interest_m` | `Max_Range_Of_Interest_M` |
| `getMinRangeOfInterest` | `min_range_of_interest_m` | `Min_Range_Of_Interest_M` |
| `getElevationScanCenterAltitude` | `elevation_scan_center_altitude_m` | `Elevation_Scan_Center_Altitude_M` |
| `getElevationScanCenterRange` | `elevation_scan_center_range_m` | `Elevation_Scan_Center_Range_M` |
| `getDegredationType` | `degradation_method` | `Degradation` |

The pinned enums are Inertial (0) and Aircraft (1), and Capacity (0),
Volume (1), Range (2), and Revisit (3) degradation. The elevation-definition
Boolean transports as 0/1. All eight unsigned data scalars in each scan,
including the three ScanType controls, retain the entire uint32 domain.
Published values survive even when inactive. The bridge neither compares
the two scans nor computes, normalizes, or validates physical performance.

The native getter conversion lives alongside the outgoing ScanParam helper;
the Ada inverse lives in private `Scan_Conversion`. No outgoing layout or
conversion changes. Native copies use the existing binary64 type without
arithmetic. Ada copies the seven representations per scan through byte-array
views without evaluating their floating scalars, following SchedCreatedRep's
representation-copy principle. Compile-time checks require binary64 C double
and Ada `Long_Float`. The public Ada regression reads the bits after copying
and owner closure with `-gnatVa` validity checks enabled; its rotating fixtures
exercise signed zeros, signed finite and subnormal numbers, infinities, and
quiet/signaling NaN payloads in all fourteen positions on the tested target.
Transport does not change GNAT validity rules for later floating arithmetic.

## Registration and ownership

`Open_Scan_Performance` is explicit opt-in while Attached or Enabled and is
independent of the other five callbacks. Invalid local arguments do not
consume its attempt; reaching the provider does. The provider is called
without the lifecycle mutex and can deliver synchronously or reenter a
capability query. Known `Return` values are preserved; unknown results and
provider exceptions fail without publishing a stream. `NotSupported` raises
`Metadata.Not_Supported`, not a Scheduling-owner failure. There is no
unregister. Serialize operations on each public owner with its own Close.

The callback admits payload reads only while active. It counts callbacks with
saturation, validates both nested scans even when the queue is full, and
publishes an entirely owned event or none. A full queue drops valid incoming
reports without allocating an event. Null, invalid discriminators, and copy or
allocation failures reject only the affected event. Queue order is successful
enqueue order; duplicate or out-of-order identifiers remain unmodified.

Stream Close discards its queue and stops consumption without affecting any
other metadata family or command future. Scheduling Close stops new reads,
wakes a blocked Receive, and leaves queued reports available before terminal
status. Physical cleanup waits for admitted payload reads outside the lifecycle
mutex, and independently pending requests can defer teardown. Failed detach
retains the uncertain provider graph; successful cleanup permits unload.
Late callbacks do not read payloads or enqueue. These rules are not a claim
of upstream callback quiescence independently of provider teardown.

The Ada receiver validates both nested representations and releases the
native event on success or exception. The public-only example in
`ada/tests/src/ams_mel_ir_scheduling_performance_example.adb` uses the two
owned scan IDs after closing the stream, channel and session.

## ABI and coverage

The six `ams_mel_ir_scheduling_scan_performance_{open,receive,get_counters,close,event_view,event_close}`
exports add to `AMS_MEL_0.1` without removing an existing symbol (356 → 362).
The capability remains 27. The report is 224 bytes, alignment 8, with the
second ScanParam at offset 112 on the tested target. C11 probes check the
record and nested layout, types, opaque pointers, capability and signatures;
Python ctypes and Rust sys mirror the ABI only. Neither gains safe reception.

Unique safe Ada operations increase 65 → 66, and Scheduling-specific coverage
increases 10/11 → 11/11. Only the exact registration and the report change
classification. No additional credit is taken for ScanParam, ScanType,
CoordFrameRef or DegradationMethod. SchedulingChannel remains `partial`:
it lacks standalone Disable, inherited buffer registration/unregistration
and inherited ChannelCommsTestRep callback registration.
