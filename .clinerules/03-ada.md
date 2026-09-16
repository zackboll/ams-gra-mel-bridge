# Ada
Idiomatic public Ada; C imports and raw representations remain private.
Use limited owners for resources and explicit closure with non-raising cleanup
fallback. One crate owns each root package. The current AMS root lives here.
Do not add standalone-library Interfaces clauses without a demonstrated need.
No blanket SPARK claims for an unproved provider/FFI boundary. Build features
vertically with C and Ada tests together.
