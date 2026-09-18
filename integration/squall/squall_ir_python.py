#!/usr/bin/env python3
"""Validate the safe Python IR slice against the real Squall provider."""

from __future__ import annotations

import sys
from collections.abc import Callable
from typing import NoReturn, TypeVar

from ams_mel import (
    CommandReturn,
    ComponentLocation,
    ControlChannel,
    ControlConfig,
    ImageConfig,
    MelError,
    MfaMode,
    ModeRejected,
    ModeSuccess,
    ReturnCompleted,
    ReturnRejected,
    Session,
    UciId,
)


CHANNEL_UUID = bytes.fromhex("00 40 04 00 11 22 43 44 85 66 77 88 99 AA BB CC")
PLATFORM_UUID = bytes.fromhex("00 40 04 01 21 32 43 54 86 67 78 89 9A AB BC CD")
EXPECTED_WIDTH = 320
EXPECTED_HEIGHT = 200
EXPECTED_BYTES = 64_000
UINT32_MAX = 0xFFFF_FFFF
T = TypeVar("T")


class ContractFailure(Exception):
    """A local integration requirement was not satisfied."""


class MelOperationFailure(Exception):
    """A public Python API operation returned a structured MEL failure."""

    def __init__(self, operation: str, error: MelError) -> None:
        self.operation = operation
        self.error = error
        super().__init__(operation)


def mel_call(operation: str, call: Callable[[], T]) -> T:
    try:
        return call()
    except MelError as error:
        raise MelOperationFailure(operation, error) from error


def parse_decimal(text: str, name: str, minimum: int, maximum: int) -> int:
    if not text or not text.isascii() or not text.isdecimal():
        raise ContractFailure(f"{name} must be a decimal integer in {minimum}..{maximum}")
    value = int(text, 10)
    if not minimum <= value <= maximum:
        raise ContractFailure(f"{name} must be a decimal integer in {minimum}..{maximum}")
    return value


def require_task_sched(result: ModeSuccess | ModeRejected, operation: str) -> None:
    if isinstance(result, ModeRejected):
        raise ContractFailure(
            f"{operation} rejected Operate/TaskSched: "
            f"code={result.code.name}({int(result.code)}), description={result.description}"
        )
    if not isinstance(result, ModeSuccess) or result.mode is not MfaMode.TASK_SCHED:
        raise ContractFailure(f"{operation} returned unexpected result: {result!r}")


def require_bit_success(
    result: ReturnCompleted | ReturnRejected, operation: str
) -> None:
    if isinstance(result, ReturnRejected):
        raise ContractFailure(
            f"{operation} rejected BIT no-op: "
            f"code={result.code.name}({int(result.code)}), description={result.description}"
        )
    if (
        not isinstance(result, ReturnCompleted)
        or result.value is not CommandReturn.SUCCESS
    ):
        raise ContractFailure(f"{operation} returned unexpected result: {result!r}")


def checksum(pixels: bytes) -> int:
    value = 1_469_598_103_934_665_603
    for pixel in pixels:
        value = ((value ^ pixel) * 1_099_511_628_211) & 0xFFFF_FFFF_FFFF_FFFF
    return value


def close_control(control: ControlChannel) -> None:
    try:
        mel_call("close ControlChannel", control.close)
    except MelOperationFailure as first_failure:
        if not control.is_open:
            raise first_failure
        mel_call("retry retained ControlChannel close", control.close)
    if control.is_open:
        raise ContractFailure("ControlChannel remained open after explicit close")


def run(arguments: list[str]) -> None:
    if not 3 <= len(arguments) <= 5:
        raise ContractFailure(
            f"usage: {arguments[0]} PROVIDER_SO PROFILE_JSON [FRAME_COUNT] [TIMEOUT_MS]"
        )

    frame_count = parse_decimal(arguments[3], "frame count", 3, 1000) if len(arguments) >= 4 else 3
    timeout_ms = (
        parse_decimal(arguments[4], "timeout", 1, UINT32_MAX)
        if len(arguments) >= 5
        else 10_000
    )

    session = mel_call("open Session", lambda: Session.open(arguments[1], arguments[2], ""))
    version = mel_call("query provider version", session.provider_version)
    print(
        "provider version: "
        f"api={version.api_version} library={version.library_version} "
        f"vendor={version.vendor} description={version.description}"
    )

    platform = UciId(PLATFORM_UUID, "Task 004 integration platform")
    location = ComponentLocation(
        0.0,
        0.0,
        0.0,
        "task-004-station",
        "ams-mel-squall-integration",
    )
    image_config = ImageConfig.new(
        UciId(CHANNEL_UUID, "Task 013 Python IRSTImage"),
        platform,
        location,
        buffer_count=4,
        buffer_size=1024 * 1024,
        queue_capacity=8,
    )
    control_config = ControlConfig.new(
        UciId(CHANNEL_UUID, "Task 013 Python IR C2"), platform, location
    )

    stream = mel_call("open ImageStream", lambda: session.open_image_stream(image_config))
    mel_call("start ImageStream", stream.start)
    control = mel_call(
        "open ControlChannel", lambda: session.open_control_channel(control_config)
    )
    mel_call("enable ControlChannel", control.enable)
    bit_request = mel_call(
        "submit BIT no-op", lambda: control.submit_bit_noop(0x0040_0402)
    )
    bit_result = mel_call("wait for BIT no-op", lambda: bit_request.wait(5_000))
    require_bit_success(bit_result, "BIT")
    print("BIT result: SUCCESS")
    request = mel_call(
        "submit Operate/TaskSched", lambda: control.submit_operate(0x0040_0403)
    )
    result = mel_call("wait for Operate/TaskSched", lambda: request.wait(5_000))
    require_task_sched(result, "C2")
    print("C2 result: TASK_SCHED")

    mel_call("close parent Session", session.close)
    if session.is_open:
        raise ContractFailure("Session remained open after parent-first close")
    cached_bit = mel_call(
        "repeat cached ReturnRequest wait", lambda: bit_request.wait(0)
    )
    require_bit_success(cached_bit, "cached BIT")
    print("cached BIT success")
    cached = mel_call("repeat cached ModeRequest wait", lambda: request.wait(0))
    require_task_sched(cached, "cached C2")
    print("cached C2 result: TASK_SCHED")

    previous_id: int | None = None
    for index in range(1, frame_count + 1):
        frame = mel_call("receive image frame", lambda: stream.receive(timeout_ms))
        if (
            frame.width != EXPECTED_WIDTH
            or frame.height != EXPECTED_HEIGHT
            or frame.bits_per_pixel != 8
            or frame.number_of_bands != 1
            or not isinstance(frame.pixels, bytes)
            or len(frame.pixels) != EXPECTED_BYTES
        ):
            raise ContractFailure(
                f"frame {index} has invalid Mono8 geometry/ownership: "
                f"{frame.width}x{frame.height}, {frame.bits_per_pixel} bits, "
                f"{frame.number_of_bands} bands, {len(frame.pixels)} bytes, "
                f"pixels={type(frame.pixels).__name__}"
            )
        if previous_id is not None and frame.frame_id <= previous_id:
            raise ContractFailure(
                f"frame {index} ID {frame.frame_id} is not greater than its predecessor"
            )
        previous_id = frame.frame_id
        print(
            f"frame {index}: id={frame.frame_id} "
            f"geometry={frame.width}x{frame.height} bytes={len(frame.pixels)} "
            f"checksum={checksum(frame.pixels):016x}"
        )

    counters = mel_call("query ImageStream counters", stream.counters)
    print(
        f"counters: received={counters.frames_received} "
        f"dropped={counters.frames_dropped_queue_full} "
        f"malformed={counters.malformed_or_unsupported}"
    )
    if (
        counters.frames_received < frame_count
        or counters.malformed_or_unsupported != 0
    ):
        raise ContractFailure(
            "stream counters violate contract: "
            f"requested={frame_count}, received={counters.frames_received}, "
            f"malformed={counters.malformed_or_unsupported}"
        )

    mel_call("close ReturnRequest", bit_request.close)
    if bit_request.is_open:
        raise ContractFailure("ReturnRequest remained open after explicit close")
    mel_call("close ModeRequest", request.close)
    if request.is_open:
        raise ContractFailure("ModeRequest remained open after explicit close")
    close_control(control)
    mel_call("close ImageStream", stream.close)
    if stream.is_open:
        raise ContractFailure("ImageStream remained open after explicit close")


def fail(message: str) -> NoReturn:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    try:
        run(sys.argv)
    except MelOperationFailure as failure:
        error = failure.error
        fail(
            f"{failure.operation}: kind={error.kind.value} "
            f"diagnostic={error.diagnostic if error.diagnostic is not None else '<absent>'} "
            f"diagnostic_required={error.diagnostic_required if error.diagnostic_required is not None else '<absent>'} "
            f"native_status={error.native_status if error.native_status is not None else '<absent>'}"
        )
    except MelError as error:
        fail(
            f"local public API validation: kind={error.kind.value} "
            f"diagnostic={error.diagnostic if error.diagnostic is not None else '<absent>'} "
            f"diagnostic_required={error.diagnostic_required if error.diagnostic_required is not None else '<absent>'} "
            f"native_status={error.native_status if error.native_status is not None else '<absent>'}"
        )
    except ContractFailure as failure:
        fail(str(failure))
    print("PASS: real Squall IR Python integration")


if __name__ == "__main__":
    main()
