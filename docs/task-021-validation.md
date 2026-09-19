# Task 021 validation

Task 021 keeps ABI version 0.1 and preserves the fixed legacy `ams_mel_ir_frame_v1`
record and `ams_mel_ir_stream_receive` signature.  It adds three exports (49 total):
snapshot receive, view, and close.  Both receive forms consume the same bounded FIFO.
Thus a legacy size probe returning `BUFFER_TOO_SMALL` leaves the frame at the head;
a later snapshot receive consumes that same frame, while a successful snapshot receive
consumes immediately because its storage is adapter-owned.

The immutable snapshot deep-copies complete `FrameHeader` data: contributing sensor,
ordered/duplicate image flags, all inertial and nav state, independent orientation
variants, and Mono8 pixels.  Its view remains valid after stream/session/provider
teardown until snapshot close.  Safe `AMS.MEL.IR.Image.Full_Frame` copies that graph
again and closes the native owner before returning.

The rich mock validates `Reserved13`, UTF-8 contributing sensor strings, high-bit IDs,
ordered duplicate flags, asymmetric quaternion x/y/z/w values, two mixed orientation
nav states, nested malformed enums, and a deterministic metadata-copy allocation
failpoint.  Malformed/failpoint frames are released and counted without poisoning a
later valid frame.  The profile remains host-memory Mono8 only; ImageChannel metadata
callbacks and image commands are not implemented.  Task 021 required no vendor-closure
expansion.
