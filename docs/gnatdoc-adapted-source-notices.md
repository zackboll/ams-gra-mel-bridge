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