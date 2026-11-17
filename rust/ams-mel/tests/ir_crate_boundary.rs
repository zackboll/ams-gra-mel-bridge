//! Compile-time identity: the compatibility façade re-exports IR's original types.
use ams_mel as legacy;
use ams_mel_ir as ir;

#[test]
fn ir_types_are_identical_not_wrapped() {
    let _session: fn(ir::Session) -> legacy::Session = |value| value;
    let _image: fn(ir::ImageStream) -> legacy::ImageStream = |value| value;
    let _control: fn(ir::ControlChannel) -> legacy::ControlChannel = |value| value;
    let _view: fn(ir::ChannelView) -> legacy::ChannelView = |value| value;
    let _stacked: fn(ir::StackedChannel) -> legacy::StackedChannel = |value| value;
    let _health: fn(ir::HealthStatusChannel) -> legacy::HealthStatusChannel = |value| value;
    let _comms: fn(ir::CommsMetadataStream) -> legacy::CommsMetadataStream = |value| value;
    let _navigation: fn(ir::NavigationReport) -> legacy::NavigationReport = |value| value;
    let _error: fn(ir::Error) -> legacy::Error = |value| value;
    let _version: fn(ir::ProviderVersion) -> legacy::ProviderVersion = |value| value;
    let _uci: fn(ir::UciId) -> legacy::UciId = |value| value;
}

#[test]
fn direct_and_facade_module_paths_are_compatible() {
    let left: ir::ir_nuc::NucPixelValue = ir::NucPixelValue::U8(3);
    let right: legacy::ir_nuc::NucPixelValue = left;
    assert_eq!(right, legacy::NucPixelValue::U8(3));
}
