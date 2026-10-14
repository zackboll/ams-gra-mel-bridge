# DOC-001 initial measured public inventory

Measured with GNATdoc 26.0.0; native output is not complete semantic documentation.

| Package | Elements | Documented |
| --- | ---: | ---: |
| `AMS` | 1 | 0 |
| `AMS.MEL` | 45 | 3 |
| `AMS.MEL.IR` | 106 | 0 |
| `AMS.MEL.IR.C2` | 179 | 1 |
| `AMS.MEL.IR.C2.Common` | 33 | 0 |
| `AMS.MEL.IR.C2.Metadata` | 288 | 0 |
| `AMS.MEL.IR.Channel` | 289 | 1 |
| `AMS.MEL.IR.Health_Status` | 25 | 0 |
| `AMS.MEL.IR.Health_Status.Metadata` | 143 | 0 |
| `AMS.MEL.IR.Image` | 351 | 1 |
| `AMS.MEL.IR.Image.Metadata` | 77 | 1 |
| `AMS.MEL.IR.Instrumentation` | 76 | 0 |
| `AMS.MEL.IR.Instrumentation.Metadata` | 22 | 0 |
| `AMS.MEL.IR.Track` | 55 | 0 |
| `AMS.MEL.IR.Track.Metadata` | 153 | 0 |
| `AMS.MEL.IR.Track.System_Data` | 124 | 0 |
| `AMS.MEL.IR.Track.Updates` | 153 | 0 |
| `AMS.MEL.RF` | 217 | 1 |
| `AMS.MEL.RF.Admin` | 15 | 0 |
| `AMS.MEL.RF.C2` | 771 | 12 |
| `AMS.MEL.RF.C2.Data_Pipes` | 50 | 4 |
| `AMS.MEL.RF.C2.Element_Groups` | 65 | 2 |
| `AMS.MEL.RF.C2.Interval_Status` | 99 | 2 |
| `AMS.MEL.RF.C2.Local_Functions` | 32 | 2 |
| `AMS.MEL.RF.C2.Transmit_Power` | 39 | 0 |
| `AMS.MEL.RF.C2.Virtual_Aperture_Notifications` | 27 | 5 |
| `AMS.MEL.RF.C2.Virtual_Aperture_Queries` | 66 | 1 |
| `AMS.MEL.RF.Product_Rx` | 263 | 2 |
| `AMS.MEL.Status` | 531 | 0 |

Total: 4295 elements; 38 documented; 4257 gaps (0.8847% documented).

There are 1,238 visible declarations, 1,237 native sections and one exact
package-renaming renderer exception. Package totals include 29 library
descriptions and eight public package instances; the alias is separate.

| Element category | Total | Documented |
| --- | ---: | ---: |
| component | 343 | 0 |
| discriminant | 4 | 0 |
| exception | 9 | 1 |
| function | 706 | 20 |
| literal | 587 | 0 |
| object | 17 | 0 |
| package | 37 | 0 |
| parameter | 1388 | 0 |
| procedure | 168 | 8 |
| renderer-exception | 1 | 0 |
| return | 706 | 0 |
| subtype | 11 | 0 |
| type | 318 | 9 |

Warnings: 3,924 raw = 3,851 public + 73 private-part + 0 private-unit.
Mapping failures: zero. Raw and structured all-warning baselines are retained.

Detailed reproducible reports are generated in `obj/docs/coverage.json`,
`obj/docs/inventory.json` and `obj/docs/warnings.json` and uploaded by CI.
All package names are derived from actual library-unit declarations.
