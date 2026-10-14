# Adapted documentation tooling

`scripts/compare_gnatdoc_warnings.py` and its unit tests are adapted from
`zackboll/opencv_geometry_ada` commit
`9142e9c3134ded907204ae0304acc28ab602b7a9`, under Apache-2.0 (see the
repository's `LICENSE`, which contains the same license). The original source
descriptions and earlier Core/Imgproc adaptation notices are retained.

MEL modifications change the default source directory to `ada/src`, support
package renamings and multiline component identifier lists, and replace the
Geometry-specific repository tests with MEL repository/syntax tests.

The initial baseline shell script follows the reference pipeline's explicit
version verification, fixed output-directory and incomplete-generation checks.
It is not the completed validation or publication pipeline.

R1: `scripts/gnatdoc_coverage_model.py` adapts Geometry's source/member and
native-section parsing helpers at the same immutable revision and license.
MEL modifications retain observed native anchors, account for the pinned
renderer omission, and separate source profiles from renderer abbreviation.
The foundation validator reuses the reference design's independent HTML
inspection, local-resource policy and mutation-testing principles; no OpenCV
package counts, destinations or numerical link floors are carried over.
