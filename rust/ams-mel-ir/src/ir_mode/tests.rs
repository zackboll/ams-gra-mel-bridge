use super::*;
use crate::{wait_for_mode_with, ModeResult};
use std::ffi::c_char;

fn command() -> ModeCommand {
    ModeCommand {
        command_id: u32::MAX,
        state: MfaState::Standby,
        mode: MfaMode::Unused,
        scan_parameters: ScanParameters::default(),
    }
}

#[test]
fn published_enum_values_and_invalid_inputs() {
    for (raw, &state) in MfaState::ALL.iter().enumerate() {
        assert_eq!(state as u32, raw as u32);
        assert_eq!(MfaState::try_from(raw as u32).unwrap(), state);
        assert_eq!(
            ModeCommand { state, ..command() }.to_raw().unwrap().state,
            raw as u32
        );
    }
    for (raw, mode) in [
        MfaMode::Unused,
        MfaMode::TaskSched,
        MfaMode::ScanVolumeSched,
        MfaMode::ScanBarSched,
    ]
    .into_iter()
    .enumerate()
    {
        assert_eq!(MfaMode::try_from(raw as u32).unwrap(), mode);
        assert_eq!(
            ModeCommand { mode, ..command() }.to_raw().unwrap().mode,
            raw as u32
        );
    }
    for &frame in CoordinateFrameReference::ALL {
        assert_eq!(
            CoordinateFrameReference::try_from(frame as u32).unwrap(),
            frame
        );
        let mut c = command();
        c.scan_parameters.center_frame_ref_el = frame;
        c.scan_parameters.center_frame_ref_az = frame;
        let raw = c.to_raw().unwrap().scan_parameters;
        assert_eq!(raw.center_frame_ref_el, frame as u32);
        assert_eq!(raw.center_frame_ref_az, frame as u32);
    }
    for &degradation in DegradationMethod::ALL {
        assert_eq!(
            DegradationMethod::try_from(degradation as u32).unwrap(),
            degradation
        );
        let mut c = command();
        c.scan_parameters.degradation_method = degradation;
        assert_eq!(
            c.to_raw().unwrap().scan_parameters.degradation_method,
            degradation as u32
        );
    }
    for raw in [15, 16, u32::MAX] {
        assert_eq!(
            MfaState::try_from(raw).unwrap_err().kind(),
            &ErrorKind::InvalidArgument
        );
    }
    for raw in [4, 15, u32::MAX] {
        assert!(MfaMode::try_from(raw).is_err());
        assert!(DegradationMethod::try_from(raw).is_err());
    }
    for raw in [2, 3, u32::MAX] {
        assert!(CoordinateFrameReference::try_from(raw).is_err());
    }
    for raw in [0, 1, 2, 3, 4, u32::MAX] {
        let error = ModeCommand {
            mode: MfaMode::Unknown(raw),
            ..command()
        }
        .to_raw()
        .unwrap_err();
        assert_eq!(error.kind(), &ErrorKind::InvalidArgument);
    }
}

#[test]
fn default_is_upstream_not_zeroed_ffi() {
    let raw = command().to_raw().unwrap();
    assert_eq!(raw.command_id, u32::MAX);
    let s = raw.scan_parameters;
    assert_eq!(s.elevation_defined_with_range_and_altitude, 0);
    assert_eq!(s.center_frame_ref_el, sys::AMS_MEL_IR_COORD_FRAME_INERTIAL);
    assert_eq!(s.center_frame_ref_az, sys::AMS_MEL_IR_COORD_FRAME_AIRCRAFT);
    assert_eq!(s.degradation_method, sys::AMS_MEL_IR_DEGRADATION_CAPACITY);
    for v in [
        s.center_az_rad,
        s.center_el_rad,
        s.scan_width_rad,
        s.scan_height_rad,
        s.scan_rate_rad_per_second,
        s.preferred_revisit_interval_seconds,
        s.required_revisit_interval_seconds,
    ] {
        assert_eq!(v.to_bits(), 0.0_f64.to_bits());
    }
    for v in [
        s.scan_type.continuous_scan,
        s.scan_type.returning,
        s.scan_type.agile_scan,
        s.scan_id,
        s.max_range_of_interest_m,
        s.min_range_of_interest_m,
        s.elevation_scan_center_altitude_m,
        s.elevation_scan_center_range_m,
    ] {
        assert_eq!(v, 0);
    }
}

#[test]
fn every_scan_field_retains_full_width_and_ieee_bits() {
    let scan = ScanParameters {
        elevation_defined_with_range_and_altitude: true,
        center_az_rad: -0.0,
        center_el_rad: f64::from_bits(0x7ff8_0000_0000_0042),
        center_frame_ref_el: CoordinateFrameReference::Aircraft,
        center_frame_ref_az: CoordinateFrameReference::Inertial,
        scan_width_rad: f64::INFINITY,
        scan_height_rad: f64::NEG_INFINITY,
        scan_type: ScanType {
            continuous_scan: u32::MAX,
            returning: 0x8000_0001,
            agile_scan: 0xfedc_ba98,
        },
        scan_id: u32::MAX,
        scan_rate_rad_per_second: f64::from_bits(1),
        preferred_revisit_interval_seconds: -123.75,
        required_revisit_interval_seconds: f64::MAX,
        max_range_of_interest_m: u32::MAX,
        min_range_of_interest_m: 0x8000_0001,
        elevation_scan_center_altitude_m: 0xfedc_ba98,
        elevation_scan_center_range_m: 0x8765_4321,
        degradation_method: DegradationMethod::Revisit,
    };
    let raw = ModeCommand {
        scan_parameters: scan,
        ..command()
    }
    .to_raw()
    .unwrap();
    assert_eq!(raw.command_id, u32::MAX);
    let s = raw.scan_parameters;
    assert_eq!(s.elevation_defined_with_range_and_altitude, 1);
    assert_eq!(s.center_frame_ref_el, 1);
    assert_eq!(s.center_frame_ref_az, 0);
    assert_eq!(s.degradation_method, 3);
    for (actual, expected) in [
        (s.center_az_rad, scan.center_az_rad),
        (s.center_el_rad, scan.center_el_rad),
        (s.scan_width_rad, scan.scan_width_rad),
        (s.scan_height_rad, scan.scan_height_rad),
        (s.scan_rate_rad_per_second, scan.scan_rate_rad_per_second),
        (
            s.preferred_revisit_interval_seconds,
            scan.preferred_revisit_interval_seconds,
        ),
        (
            s.required_revisit_interval_seconds,
            scan.required_revisit_interval_seconds,
        ),
    ] {
        assert_eq!(actual.to_bits(), expected.to_bits());
    }
    assert_eq!(s.scan_type.continuous_scan, u32::MAX);
    assert_eq!(s.scan_type.returning, 0x8000_0001);
    assert_eq!(s.scan_type.agile_scan, 0xfedc_ba98);
    assert_eq!(s.scan_id, u32::MAX);
    assert_eq!(s.max_range_of_interest_m, u32::MAX);
    assert_eq!(s.min_range_of_interest_m, 0x8000_0001);
    assert_eq!(s.elevation_scan_center_altitude_m, 0xfedc_ba98);
    assert_eq!(s.elevation_scan_center_range_m, 0x8765_4321);
}

fn wait_output(
    status: i32,
    mode: u32,
    code: u32,
    bytes: &[u8],
    required: usize,
) -> Result<ModeResult, Error> {
    wait_for_mode_with(0, |_, result, out, capacity, size| {
        result.mode = mode;
        result.error_code = code;
        *size = required;
        assert!(bytes.len() <= capacity);
        // SAFETY: wait helper supplies writable storage of capacity bytes.
        unsafe {
            ptr::copy_nonoverlapping(bytes.as_ptr().cast::<c_char>(), out, bytes.len());
        }
        status
    })
}

#[test]
fn malformed_native_wait_outputs_are_not_success() {
    for (status, mode, code, bytes, size) in [
        (sys::AMS_MEL_OK, 0, 0, &b"\0"[..], 0),
        (sys::AMS_MEL_OK, 0, 0, &b"bad\0"[..], 4),
        (sys::AMS_MEL_OK, 0, 1, &b"\0"[..], 1),
        (sys::AMS_MEL_OK, u32::MAX, 0, &b"\0"[..], 1),
        (sys::AMS_MEL_COMMAND_REJECTED, 0, 3, &b"\xff\0"[..], 2),
        (sys::AMS_MEL_COMMAND_REJECTED, 0, 3, &b"x"[..], 1),
        (sys::AMS_MEL_COMMAND_REJECTED, 0, 3, &b"a\0b\0"[..], 4),
    ] {
        assert!(wait_output(status, mode, code, bytes, size).is_err());
    }
    assert_eq!(
        wait_output(sys::AMS_MEL_TIMEOUT, 0, 0, b"\0", 1)
            .unwrap_err()
            .kind(),
        &ErrorKind::Timeout
    );
    assert_eq!(
        wait_output(sys::AMS_MEL_COMMAND_REJECTED, 0, u32::MAX, b"reason\0", 7).unwrap(),
        ModeResult::Rejected {
            code: crate::MelErrorCode::Unknown(u32::MAX),
            description: "reason".into()
        }
    );
}

#[test]
fn changing_terminal_retry_is_rejected() {
    for field in 0..3 {
        let mut calls = 0;
        let result = wait_for_mode_with(99, |timeout, result, out, capacity, required| {
            calls += 1;
            assert_eq!(timeout, if calls == 1 { 99 } else { 0 });
            *required = 600;
            if calls == 2 {
                assert_eq!(capacity, 600);
                if field == 0 {
                    *required = 599;
                }
                if field == 1 {
                    result.mode = 1;
                }
            }
            // SAFETY: output has at least one byte of writable capacity.
            unsafe {
                *out = 0;
            }
            if calls == 2 && field == 2 {
                sys::AMS_MEL_PROVIDER_FAILED
            } else {
                sys::AMS_MEL_COMMAND_REJECTED
            }
        });
        assert!(result.is_err());
        assert_eq!(calls, 2);
    }
}

#[test]
fn missing_owner_and_failed_submission() {
    assert!(submit_with(|out| {
        assert!(out.is_null());
        Ok(())
    })
    .is_err());
    let error =
        submit_with(|_| Err(Error::new(ErrorKind::ResourceExhausted, "limit"))).unwrap_err();
    assert_eq!(error.kind(), &ErrorKind::ResourceExhausted);
}

#[test]
fn unexpected_error_owner_is_a_real_unique_native_owner() {
    use crate::{ComponentLocation, ControlConfig, Session, UciId};
    let path = std::env::var_os("AMS_MEL_TEST_PROVIDER_DIR")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(|| {
            std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
                .join("../../native/build-tests/test-providers")
        })
        .join("libmock_ir_provider.so");
    let session = Session::open(path, "mode-general", "").unwrap();
    let config = ControlConfig::new(
        UciId::new([0; 16], "channel").unwrap(),
        UciId::new([0; 16], "platform").unwrap(),
        ComponentLocation::new(1.25, -2.5, 3.75, "station-1", "mock-aircraft").unwrap(),
    );
    let mut channel = session.open_control_channel(&config).unwrap();
    channel.enable().unwrap();
    let mut request = channel.submit_mode(&command()).unwrap();
    request.wait(1000).unwrap();
    let raw = std::mem::replace(&mut request.raw, ptr::null_mut());
    let error = submit_with(|out| {
        *out = raw;
        Err(Error::new(
            ErrorKind::ProviderException,
            "injected error with owner",
        ))
    })
    .unwrap_err();
    assert_eq!(error.kind(), &ErrorKind::ProviderException);
    drop(request); // transferred owner must not be freed a second time
    channel.close().unwrap();
    session.close().unwrap();
}
