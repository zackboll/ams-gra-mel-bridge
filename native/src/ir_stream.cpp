#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/ir_channel.hpp"
#include "internal/ir_stream.hpp"

#include <irmel/library/image/ImageChannel.h>
#include <irmel/library/irmel-types/FrameHeader.h>
#include <irmel/library/irmel-types/ImageListener.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace ams::iface;

void diagnostic(std::string_view text, char *out, std::size_t capacity,
                std::size_t *required) noexcept
{
    if (required != nullptr) {
        *required = text.size() + 1U;
    }
    if (out != nullptr && capacity != 0U) {
        const std::size_t copied = std::min(text.size(), capacity - 1U);
        std::memcpy(out, text.data(), copied);
        out[copied] = '\0';
    }
}

bool valid_utf8(std::string_view value) noexcept
{
    std::size_t index = 0;
    while (index < value.size()) {
        const auto lead = static_cast<unsigned char>(value[index]);
        if (lead == 0U) return false;
        if (lead <= 0x7fU) { ++index; continue; }
        std::size_t trailing{};
        std::uint32_t point{};
        if (lead >= 0xc2U && lead <= 0xdfU) { trailing = 1U; point = lead & 0x1fU; }
        else if (lead >= 0xe0U && lead <= 0xefU) { trailing = 2U; point = lead & 0x0fU; }
        else if (lead >= 0xf0U && lead <= 0xf4U) { trailing = 3U; point = lead & 0x07U; }
        else return false;
        if (index + trailing >= value.size()) return false;
        for (std::size_t offset = 1; offset <= trailing; ++offset) {
            const auto byte = static_cast<unsigned char>(value[index + offset]);
            if ((byte & 0xc0U) != 0x80U) return false;
            point = (point << 6U) | (byte & 0x3fU);
        }
        if ((trailing == 2U && point < 0x800U) ||
            (trailing == 3U && point < 0x10000U) || point > 0x10ffffU ||
            (point >= 0xd800U && point <= 0xdfffU)) return false;
        index += trailing + 1U;
    }
    return true;
}

bool valid_view(const ams_mel_string_view_v1& value) noexcept
{
    if (value.data == nullptr) return value.size == 0U;
    return valid_utf8({value.data, value.size});
}

std::string copy_view(const ams_mel_string_view_v1& value)
{
    return value.size == 0U ? std::string{} : std::string{value.data, value.size};
}

mel::UCI_ID convert_id(const ams_mel_uci_id_v1& value)
{
    std::array<std::uint8_t, mel::UUID_SIZE> uuid{};
    std::copy(std::begin(value.uuid), std::end(value.uuid), uuid.begin());
    return {uuid, copy_view(value.descriptive_label)};
}

void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) {
        ++value;
    }
}

/* Task 030B corrective (PR #42 second review): stream-owned, preallocated,
 * allocation-free emergency ownership for a provider Buffer whose release()
 * left ownership UNCERTAIN.
 *
 * Central rule, which every path below is built to guarantee:
 *
 *     the bridge never calls Buffer::release() unless it already owns a
 *     dedicated, preallocated retention slot for that exact Buffer.
 *
 * The slot lives in CallbackState::retention_slots, a vector sized from the
 * configured buffer_count and filled in ams_mel_ir_stream_start() BEFORE
 * channel->enable(), i.e. strictly before any provider callback can occur.
 * Moving the exact callback shared_ptr<Buffer> into an already-constructed
 * empty slot allocates nothing and cannot throw, so an uncertain release
 * always has a permanent owner and the exact callback wrapper can never be
 * destroyed. CallbackState is kept alive for process lifetime by
 * image_stream_retain_failed(), so the slot, the Buffer object, its vtable,
 * its registered host bytes, and therefore the provider library can never be
 * destroyed after an uncertain release.
 *
 * Bounding argument -- why buffer_count slots suffice, with no arbitrary
 * process-global constant involved:
 *
 *   - a slot is acquired when a frame callback arrives and returned to the
 *     pool when that frame's release() SUCCEEDS;
 *   - a slot is consumed permanently when that frame's release() is uncertain,
 *     which also permanently removes the underlying registered buffer from
 *     provider reuse;
 *   - the bridge only accepts a buffer whose base address is one of the
 *     buffer_count registered host ranges this stream published;
 *
 *   so slots in use == provider buffers this stream currently has checked out
 *   plus the ones permanently lost to uncertain release, which a conforming
 *   provider cannot drive above buffer_count. Capacity therefore follows the
 *   configured provider-buffer capacity, not a fixed global reserve.
 *
 * If a non-conforming provider does exhaust the pool, the bridge does NOT
 * release: it keeps the buffer instead. That branch runs BEFORE any release()
 * for the buffer, where dropping the wrapper is harmless, because a
 * never-released wrapper's destructor performs its FIRST release. The
 * forbidden case -- dropping a wrapper AFTER a failed release, making that
 * destructor retry release() -- is unreachable by construction.
 *
 * The process-global list below is only a publication/diagnostic record plus
 * extra depth for that pre-release branch. No safety property depends on it,
 * and the previous fixed 64-slot reserve is no longer a correctness
 * dependency. */
struct RetainedBufferNode {
    std::shared_ptr<irmel::Buffer> buffer;
    RetainedBufferNode *next{};
};

std::atomic<RetainedBufferNode *> retained_buffer_head{};
/* Observable only by tests and diagnostics; never used for a safety decision. */
std::atomic<std::size_t> retained_buffer_count{};

/* Index sentinel for "this frame owns no retention slot". */
constexpr std::size_t no_retention_slot = static_cast<std::size_t>(-1);

/* Statically allocated nodes used ONLY by the pre-release overflow branch,
 * i.e. when a non-conforming provider exhausted the stream's own slot pool and
 * the bridge consequently refuses to call release() at all. They exist for the
 * whole process before any provider call, so that branch allocates nothing
 * either. Correctness does not depend on this count: exhausting it merely
 * drops a wrapper whose release() was never attempted, which is the provider's
 * own ordinary first-release path. */
constexpr std::size_t retained_reserve_size = 64U;
struct RetainedReserveSlot {
    RetainedBufferNode node;
    std::atomic<bool> claimed{};
};
RetainedReserveSlot retained_reserve[retained_reserve_size];

RetainedBufferNode *claim_reserved_node() noexcept
{
    for (auto& slot : retained_reserve) {
        bool expected = false;
        if (slot.claimed.compare_exchange_strong(expected, true,
                                                 std::memory_order_acq_rel))
            return &slot.node;
    }
    return nullptr;
}

/* Test-only deterministic allocation failure at exactly the point where a
 * failed callback Buffer used to be parked by a potentially allocating
 * container insertion. Arming it proves that the corrected fail-safe path
 * does not depend on that insertion succeeding: the Buffer is already owned by
 * its preallocated retention slot before this can fire. Compiled out entirely
 * in a non-test build. */
void failed_buffer_park_allocation_failpoint()
{
#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS
    if (const char *failure =
            std::getenv("AMS_MEL_TEST_FAILED_BUFFER_PARK_ALLOCATION");
        failure != nullptr && std::strcmp(failure, "fail") == 0) {
        throw std::bad_alloc{};
    }
#endif
}

/* Test-only deterministic allocation failure at exactly the enqueue-allocation
 * boundary, i.e. the std::deque::push_back that publishes an accepted frame to
 * the receive queue. push_back may allocate and therefore may throw, so the
 * callback Releaser must still be ARMED when it runs (PR #42 third-review
 * corrective). Arming this failpoint proves the handoff order is exception
 * safe: a failed insertion still performs exactly one bridge-controlled
 * release and never leaks the frame's retention slot. Compiled out entirely in
 * a non-test build. */
void enqueue_allocation_failpoint()
{
#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS
    if (const char *failure = std::getenv("AMS_MEL_TEST_ENQUEUE_ALLOCATION");
        failure != nullptr && std::strcmp(failure, "fail") == 0) {
        throw std::bad_alloc{};
    }
#endif
}

/* Test-only NEGATIVE CONTROL for the post-callback-drain teardown recheck.
 * When armed, image_stream_cleanup() skips that recheck and therefore
 * reintroduces exactly the TOCTOU defect the corrective task forbids: host
 * storage and registered Buffers are freed although an in-flight callback
 * published an uncertain release after the initial gate. Compiled out in a
 * non-test build. */
bool skip_post_drain_recheck_failpoint() noexcept
{
#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS
    const char *value = std::getenv("AMS_MEL_TEST_SKIP_POST_DRAIN_RECHECK");
    return value != nullptr && std::strcmp(value, "skip") == 0;
#else
    return false;
#endif
}

/* Test-only. Forces the stream's preallocated retention slot pool to report
 * exhaustion, so a regression can drive the non-conforming-provider branch in
 * which the bridge must refuse to call release() rather than risk an uncertain
 * result it cannot own. Compiled out entirely in a non-test build. */
bool exhaust_retention_slots_failpoint() noexcept
{
#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS
    const char *value = std::getenv("AMS_MEL_TEST_RETENTION_SLOTS");
    return value != nullptr && std::strcmp(value, "exhaust") == 0;
#else
    return false;
#endif
}

/* Test-only NEGATIVE CONTROL. When armed, the fail-safe path deliberately
 * drops the exact callback wrapper after an uncertain release instead of
 * moving it into its retention slot. That reintroduces precisely the defect
 * the corrective task forbids, so the regressions can demonstrate they detect
 * it rather than passing vacuously. Compiled out in a non-test build. */
bool drop_failed_buffer_failpoint() noexcept
{
#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS
    const char *value = std::getenv("AMS_MEL_TEST_DROP_FAILED_BUFFER");
    return value != nullptr && std::strcmp(value, "drop") == 0;
#else
    return false;
#endif
}

/* Publishes one permanent owner for a Buffer whose release() has NOT been
 * attempted and never will be, using the process-global preallocated reserve.
 * Allocation-free and noexcept. Returns false only when the reserve is
 * exhausted, in which case the caller drops a never-released wrapper, which is
 * safe: the provider's own wrapper destructor then performs its first and only
 * release. */
bool park_pre_release_buffer(const std::shared_ptr<irmel::Buffer>& buffer) noexcept
{
    if (!buffer) return true;
    RetainedBufferNode *parked = claim_reserved_node();
    if (parked == nullptr) return false;
    parked->buffer = buffer;
    RetainedBufferNode *head = retained_buffer_head.load(std::memory_order_relaxed);
    do { parked->next = head; }
    while (!retained_buffer_head.compare_exchange_weak(
        head, parked, std::memory_order_release, std::memory_order_relaxed));
    retained_buffer_count.fetch_add(1U, std::memory_order_acq_rel);
    return true;
}

/* Task 030B: the queued/snapshot representation owns the provider Buffer
 * itself instead of a payload-sized copy of its bytes.
 *
 *   metadata ownership        small Ada/C-shaped vectors and strings
 *   provider Buffer owner     buffer, checked out of the provider pool
 *   validated payload address payload, validated inside the callback
 *   validated payload size    payload_size, validated inside the callback
 *
 * There is deliberately no std::vector<std::uint8_t> pixels member: a
 * payload-sized byte vector whose purpose is to duplicate the provider image
 * is exactly what this task removes. The only vectors here are small metadata
 * vectors whose length is bounded by the FrameHeader, not by the image size.
 *
 * While buffer is non-null the frame counts as one retained provider-buffer
 * lease in ImageStreamState::retained_frames and the provider cannot reuse
 * the underlying registered host bytes. */
struct QueuedFrame {
    ams_mel_ir_frame_snapshot_v1 view{};
    std::string location_key;
    std::string location_system_name;
    std::vector<std::uint32_t> flags;
    std::vector<ams_mel_ir_sensor_inertial_state_v1> inertial;
    std::vector<ams_mel_ir_sensor_nav_state_v1> nav;
    /* Retained provider buffer. Destroying this shared_ptr is NOT a release:
     * the published release() protocol must be invoked deliberately. */
    std::shared_ptr<irmel::Buffer> buffer;
    /* Index of this frame's dedicated retention slot in
     * CallbackState::retention_slots, acquired in the provider callback before
     * the frame was accepted and therefore before release() can ever be
     * attempted for it. Never no_retention_slot while buffer is non-null:
     * a frame with no slot is never accepted and never released. */
    std::size_t retention{no_retention_slot};
    const std::uint8_t *payload{};
    std::size_t payload_size{};

    void bind() noexcept
    {
        view.contributing_sensor.location.key = {location_key.data(), location_key.size()};
        view.contributing_sensor.location.system_name =
            {location_system_name.data(), location_system_name.size()};
        view.image_flags = {flags.data(), flags.size()};
        view.sensor_inertial_states = {inertial.data(), inertial.size()};
        view.sensor_nav_states = {nav.data(), nav.size()};
        /* Borrowed span directly over provider Buffer::getImageAddress(). */
        view.pixels = {payload, payload_size};
    }
};

ams_mel_ir_directional_v1 directional(const irmel::IR_Directional& value) noexcept
{ return {value.getX(), value.getY(), value.getZ()}; }
ams_mel_ir_quaternion_v1 quaternion(const irmel::Quaternion& value) noexcept
{ return {value.getQuaternionX(), value.getQuaternionY(), value.getQuaternionZ(), value.getQuaternionW()}; }
ams_mel_ir_nav_error_v1 nav_error(const irmel::NavError& value) noexcept
{ return {value.getX(), value.getY(), value.getZ(), value.getW()}; }
ams_mel_euler_v1 euler(const mel::Euler& value) noexcept
{ return {value.getRoll(), value.getPitch(), value.getYaw()}; }

template<typename EulerGetter, typename QuaternionGetter>
ams_mel_ir_orientation_v1 orientation(EulerGetter get_euler,
                                      QuaternionGetter get_quaternion)
{
    try {
        return {AMS_MEL_IR_ORIENTATION_EULER, euler(get_euler()), {}};
    } catch (const std::bad_variant_access&) {
        return {AMS_MEL_IR_ORIENTATION_QUATERNION, {}, quaternion(get_quaternion())};
    }
}

/* Converts only the small FrameHeader metadata. The bulk payload is recorded
 * as the already-validated provider address/length pair; nothing
 * payload-sized is allocated or copied. Task 030A's frame.pixels.assign() was
 * removed here. */
QueuedFrame snapshot_frame(const irmel::FrameHeader& header, const std::uint8_t *pixels,
                           std::size_t pixel_count)
{
#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS
    if (const char *failure = std::getenv("AMS_MEL_TEST_FRAME_COPY_FAILURE");
        failure != nullptr && std::strcmp(failure, "allocation") == 0) {
        (void)unsetenv("AMS_MEL_TEST_FRAME_COPY_FAILURE");
        throw std::bad_alloc{};
    }
#endif
    QueuedFrame frame;
    const auto image_type = static_cast<std::uint32_t>(header.getImageType());
    const auto image_flip = static_cast<std::uint32_t>(header.getImageFlip());
    if (image_type > 2U || image_flip > 3U) throw std::runtime_error("invalid image enum");
    frame.view.system_time_ns = header.getSystemTime().count();
    frame.view.integration_time_ns = header.getIntegrationTime().count();
    frame.view.width = header.getWidth(); frame.view.height = header.getHeight();
    frame.view.bits_per_pixel = header.getBitsPerPixel(); frame.view.number_of_bands = header.getNumBands();
    frame.view.horizontal_fov_rad = header.getHorizontalFieldOfView();
    frame.view.vertical_fov_rad = header.getVerticalFieldOfView();
    const auto& sensor = header.getFace(); const auto& location = sensor.first;
    frame.view.contributing_sensor.location.offset_x_m = location.getOffsetX();
    frame.view.contributing_sensor.location.offset_y_m = location.getOffsetY();
    frame.view.contributing_sensor.location.offset_z_m = location.getOffsetZ();
    frame.location_key = location.getLocationId().getKey();
    frame.location_system_name = location.getLocationId().getSystemName();
    if (!valid_utf8(frame.location_key) || !valid_utf8(frame.location_system_name))
        throw std::runtime_error("invalid contributing sensor string");
    frame.view.contributing_sensor.sensor_id = sensor.second;
    frame.view.pixel_format = static_cast<std::uint32_t>(header.getFormat());
    frame.view.frame_id = header.getFrameID(); frame.view.subframe_id = header.getSubframeID();
    frame.view.subframe_total = header.getSubframeTotal(); frame.view.image_type = image_type;
    frame.view.image_flip = image_flip;
    for (const auto flag : header.getFlags()) {
        const auto value = static_cast<std::uint32_t>(flag);
        if (value > 3U) throw std::runtime_error("invalid image flag");
        frame.flags.push_back(value);
    }
    frame.view.dither_row = header.getDitherRow(); frame.view.dither_column = header.getDitherCol();
    frame.view.row_offset = header.getRowOffset(); frame.view.column_offset = header.getColumnOffset();
    for (const auto& state : header.getSensorInertialState()) {
        frame.inertial.push_back({state.getSystemTime().count(), quaternion(state.getQ_xyzw()),
            quaternion(state.getQECEF_xyzw()), directional(state.getSensorPosition()),
            directional(state.getSensorVelocity()), {state.getUncertainties().getSensorUncertainties(),
            state.getUncertainties().getPlatformUncertainties()}});
    }
    for (const auto& state : header.getSensorNavState()) {
        const auto coordinate = static_cast<std::uint32_t>(state.getCoordinateSystem());
        if (coordinate > 3U) throw std::runtime_error("invalid coordinate system");
        frame.nav.push_back({directional(state.getPosition()), nav_error(state.getPositionError()),
            directional(state.getVelocity()), nav_error(state.getVelocityError()),
            directional(state.getAccel()), nav_error(state.getAccelError()),
            orientation([&state]() -> const mel::Euler& { return state.getEulerOrientation(); },
                        [&state]() -> const irmel::Quaternion& { return state.getQuaternionOrientation(); }),
            nav_error(state.getOrientationError()),
            orientation([&state]() -> const mel::Euler& { return state.getEulerOrientationVel(); },
                        [&state]() -> const irmel::Quaternion& { return state.getQuaternionOrientationVel(); }),
            nav_error(state.getOrientationVelError()),
            orientation([&state]() -> const mel::Euler& { return state.getEulerOrientationAccel(); },
                        [&state]() -> const irmel::Quaternion& { return state.getQuaternionOrientationAccel(); }),
            nav_error(state.getOrientationAccelError()), coordinate});
    }
    frame.view.band_index = header.getBandIndex();
    /* Zero-copy: retain the validated provider image address and length. The
     * caller attaches the provider Buffer owner before enqueueing. */
    frame.payload = pixels;
    frame.payload_size = pixel_count;
    frame.bind();
    return frame;
}

} // namespace

using namespace ams::iface;

enum class Lifecycle {
    Attached,
    Starting,
    Running,
    Stopping,
    Stopped,
    Failed
};

struct CallbackState {
    struct HostRange {
        std::uintptr_t base;
        std::size_t size;
    };
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<QueuedFrame> queue;
    std::size_t capacity{};
    bool accepting{false};
    Lifecycle lifecycle{Lifecycle::Attached};
    ams_mel_ir_stream_counters_v1 counters{};
    std::vector<HostRange> registered_ranges;
    std::atomic<std::size_t> callbacks_in_flight{0U};
    /* Task 030B outstanding provider-buffer accounting. Counts provider
     * Buffer objects currently withheld from provider reuse by bridge logic:
     * queued frames plus live frame snapshots. It lives here, beside the
     * queue, because the provider callback increments it, but it is emphatically
     * NOT derived from queue.size(): a snapshot outlives its queue entry and
     * several snapshots may be outstanding at once. Guarded by mutex.
     *
     * Transitions:
     *
     *   accepted into the receive queue        +1
     *   queue-full rejection                    0  (released in the callback)
     *   malformed/unsupported rejection         0  (released in the callback)
     *   not-accepting rejection                 0  (released in the callback)
     *   queue -> snapshot handoff          unchanged
     *   legacy Receive copy + release          -1
     *   snapshot/lease close + release         -1
     *   queue discard during Close + release   -1
     *
     * Uncertain release, i.e. release() returned non-Success or threw, is the
     * corrective (PR #42 second review) transition and is documented exactly:
     *
     *   uncertain release of an ACCEPTED frame        unchanged (stays counted)
     *   uncertain release of a REJECTED frame              +1 (becomes counted)
     *
     * Both end in the same state -- the buffer is counted in retained_frames
     * forever and is never decremented -- so physical teardown for this graph
     * is permanently blocked either way. The asymmetry exists only because a
     * rejected frame was never counted in the first place: the callback
     * rejection paths release the buffer before queue acceptance, so an
     * uncertain result there must ADD the permanent obligation that the
     * accepted path already holds. This is not counter fudging: the
     * post-condition "an uncertain buffer contributes exactly one
     * never-removed unit to retained_frames" holds uniformly.
     *
     * Physical provider teardown is deferred while this is nonzero. */
    std::size_t retained_frames{};

    /* Task 030B corrective (PR #42 second review): stream-owned preallocated
     * emergency ownership for provider Buffers whose release() left ownership
     * uncertain.
     *
     * Sized from the configured buffer_count and populated in
     * ams_mel_ir_stream_start() before channel->enable(), so it is complete
     * before any provider callback can run. Each element is an empty
     * shared_ptr; moving a shared_ptr<Buffer> into an existing empty element
     * allocates nothing and cannot throw.
     *
     * free_retention_slots lists the indices currently available. acquire and
     * return are both O(1) and allocation-free (pop_back/push_back on a vector
     * whose capacity was reserved at Start). Guarded by mutex. */
    std::vector<std::shared_ptr<irmel::Buffer>> retention_slots;
    std::vector<std::size_t> free_retention_slots;

    /* Acquires one dedicated retention slot, or no_retention_slot when the
     * pool is exhausted. Caller must hold mutex. Allocation-free. */
    std::size_t acquire_retention_slot() noexcept
    {
        if (free_retention_slots.empty() || exhaust_retention_slots_failpoint())
            return no_retention_slot;
        const std::size_t index = free_retention_slots.back();
        free_retention_slots.pop_back();
        return index;
    }

    /* Returns a slot whose buffer was released SUCCESSFULLY, so it can serve
     * the next callback. Never called for an uncertain release: such a slot is
     * consumed permanently. Caller must hold mutex. Allocation-free because
     * free_retention_slots never exceeds its reserved capacity. */
    void return_retention_slot(std::size_t index) noexcept
    {
        if (index == no_retention_slot || index >= retention_slots.size()) return;
        retention_slots[index].reset();
        if (free_retention_slots.size() < free_retention_slots.capacity())
            free_retention_slots.push_back(index);
    }

    /* Set as soon as any Buffer belonging to this stream reaches uncertain
     * ownership, from ANY path including a callback-side rejection. It is an
     * atomic rather than mutex-guarded state deliberately: publishing it
     * cannot fail, so physical teardown stays blocked even if every subsequent
     * lock acquisition or allocation on the fail-safe path fails. Physical
     * teardown requires requests == 0 AND retained_frames == 0 AND
     * !uncertain_release. */
    std::atomic<bool> uncertain_release{false};

    /* Weak back-pointer to the owning ImageStreamState, established during
     * stream construction, so a callback-side uncertain release can invoke the
     * same graph-retention policy as snapshot/lease release. Weak because
     * ImageStreamState owns this CallbackState; the strong retention that
     * actually keeps the graph alive is image_stream_retain_failed(). */
    std::weak_ptr<ImageStreamState> owner;

    /* THE single uncertain-ownership policy, shared by the callback rejection
     * paths and by the accepted-frame release paths.
     *
     * Ordering is deliberate and each step is strictly weaker than the last:
     *
     *   1. publish uncertain_release and move the exact callback shared_ptr
     *      into this frame's dedicated preallocated slot. Allocation-free,
     *      lock-free, and cannot throw: the slot was constructed at Start, is
     *      owned exclusively by this frame, and retention_slots is never
     *      resized or cleared again. This alone guarantees the wrapper is
     *      never destroyed, so no destructor-driven second release can occur,
     *      and it alone blocks physical teardown.
     *   2. retain the whole provider graph, so registered host storage,
     *      ImageStreamState, the provider channel and the provider library
     *      survive for process lifetime.
     *   3. best-effort bookkeeping: poison the lifecycle and publish the
     *      retained_frames transition. Failure here changes nothing about
     *      safety.
     *
     * already_counted is true for a frame that was accepted into the queue and
     * therefore already contributes one unit to retained_frames, and false for
     * a frame rejected in the callback before acceptance, which must now start
     * contributing one. release() is never retried on any path. */
    void retain_uncertain_buffer(std::shared_ptr<irmel::Buffer> buffer,
                                 std::size_t slot, bool already_counted) noexcept
    {
        uncertain_release.store(true, std::memory_order_release);
        if (slot != no_retention_slot && slot < retention_slots.size() &&
            !drop_failed_buffer_failpoint())
            retention_slots[slot] = std::move(buffer);
        if (auto state = owner.lock()) image_stream_retain_failed(state);
        try {
            std::lock_guard lock{mutex};
            if (!already_counted) ++retained_frames;
            lifecycle = Lifecycle::Failed;
            accepting = false;
            ready.notify_all();
            /* Deterministically forces the historical allocating-park failure
             * at exactly the point where the old design could still lose the
             * wrapper. By now the wrapper is already owned by its slot. */
            failed_buffer_park_allocation_failpoint();
        } catch (...) {
            /* Safety is already established by steps 1 and 2. */
        }
    }

    void fail() noexcept
    {
        try {
            std::lock_guard lock{mutex};
            accepting = false;
            lifecycle = Lifecycle::Failed;
            ready.notify_all();
        } catch (...) {
            /* No exception may cross the provider callback boundary. */
        }
    }

    void image(const irmel::FrameHeader& header,
               const std::shared_ptr<irmel::Buffer>& buffer) noexcept
    {
        /* Task 030B checked-out-buffer state machine. Exactly one of the
         * following happens to every Buffer the provider hands the bridge:
         *
         *   malformed / unsupported  -> release() here
         *   not accepting            -> release() here
         *   queue full               -> count DROP, release() here
         *   accepted                 -> retained; release() happens later, in
         *                               legacy Receive, snapshot close, or
         *                               Close-time queue discard
         *
         * The releaser below owns the "release here" arm only. When the frame
         * is accepted, ownership is disarmed by clearing release.value, so the
         * callback releaser and the later lease owner can never both release
         * the same accepted buffer.
         *
         * Corrective (PR #42 second review): the releaser carries this frame's
         * dedicated preallocated retention slot index, and on an uncertain
         * result it invokes retain_uncertain_buffer(), exactly the policy the
         * accepted-frame lease/Receive/discard paths use. That establishes,
         * atomically with respect to teardown:
         *
         *   - the exact callback wrapper is owned by the slot indefinitely, so
         *     no destructor-driven second release() can occur;
         *   - uncertain_release is published and retained_frames gains the
         *     permanent unit this rejected frame never had, so a later
         *     Stop/Close can no longer observe requests == 0 and
         *     retained_frames == 0 and tear the provider graph down;
         *   - image_stream_retain_failed() keeps ImageStreamState, the
         *     registered host storage, and the provider library alive.
         *
         * If no slot could be acquired at all -- only possible for a
         * non-conforming provider handing out more simultaneous buffers than
         * it registered -- release() is deliberately NOT attempted, because an
         * uncertain result could then not be owned. See the acquisition site
         * below. */
        struct Releaser {
            CallbackState& state;
            std::shared_ptr<irmel::Buffer> value;
            std::size_t retention{no_retention_slot};
            ~Releaser() noexcept
            {
                if (!value) return;
                bool ok = false;
                try {
                    ok = value->release() == irmel::Return::Success;
                } catch (...) {
                    ok = false;
                }
                if (ok) {
                    /* Provider ownership is proven handed back; the slot can
                     * serve the next callback. */
                    try {
                        std::lock_guard lock{state.mutex};
                        state.return_retention_slot(retention);
                    } catch (...) {
                    }
                    return;
                }
                /* Uncertain ownership: never destroy the wrapper, never retry
                 * release(), and permanently block physical teardown. The
                 * frame was rejected before queue acceptance, so it is not yet
                 * counted in retained_frames. */
                state.retain_uncertain_buffer(std::move(value), retention, false);
            }
        } release{*this, buffer, no_retention_slot};

        try {
            /* A null Buffer is rejected BEFORE any retention slot is acquired
             * (PR #42 third-review corrective).
             *
             * Emergency ownership only ever exists for an object that exists.
             * The Releaser destructor returns immediately when value is null,
             * so it can never recycle a slot it was given for a null Buffer;
             * acquiring one here would therefore consume a stream-sized,
             * never-recycled retention slot for every null callback, and a
             * provider emitting more than buffer_count of them would starve a
             * later valid frame out of the emergency ownership the corrective
             * design requires it to have. There is also nothing to own: no
             * release() is possible or owed for a Buffer that does not exist.
             *
             * This arm is a plain malformed/unsupported rejection. */
            if (!buffer) {
                std::lock_guard lock{mutex};
                increment(counters.frames_received);
                increment(counters.malformed_or_unsupported_frames);
                return;
            }
            {
                std::lock_guard lock{mutex};
                increment(counters.frames_received);
                /* Acquire this frame's dedicated retention slot while it is
                 * still in a normal-success state, strictly before any
                 * release() for this buffer can be attempted. Allocation-free:
                 * the slot objects and the free list were built at Start. */
                release.retention = acquire_retention_slot();
            }
            if (release.retention == no_retention_slot) {
                /* No slot: a non-conforming provider has more buffers
                 * simultaneously outstanding than it registered. Refuse to
                 * release, because an uncertain result could not be owned.
                 * Disarm the releaser and park the buffer instead. This runs
                 * BEFORE any release() for it, so even if the process-global
                 * reserve is also exhausted and the wrapper is dropped, the
                 * wrapper's own destructor performs its FIRST release rather
                 * than a forbidden retry -- never a retry after a failure.
                 *
                 * Ownership is then permanently withheld from the provider,
                 * which is exactly the same obligation an uncertain release
                 * creates, so it gets the same treatment: the graph is
                 * retained and physical teardown is permanently blocked, which
                 * keeps the registered host storage this buffer points at
                 * alive. */
                release.value.reset();
                (void)park_pre_release_buffer(buffer);
                {
                    std::lock_guard lock{mutex};
                    increment(counters.malformed_or_unsupported_frames);
                }
                retain_uncertain_buffer(buffer, no_retention_slot, false);
                return;
            }
            if (header.getWidth() == 0U || header.getHeight() == 0U ||
                header.getBitsPerPixel() != 8U || header.getNumBands() != 1U ||
                header.getFormat() != irmel::PixelFormat::Mono ||
                static_cast<std::uint32_t>(header.getImageType()) > 2U ||
                static_cast<std::uint32_t>(header.getImageFlip()) > 3U) {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported_frames);
                return;
            }
            const std::size_t width = header.getWidth();
            const std::size_t height = header.getHeight();
            if (height > std::numeric_limits<std::size_t>::max() / width) {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported_frames);
                return;
            }
            const std::size_t bytes = width * height;
            void *const base_pointer = buffer->getBufferAddress();
            void *const image_pointer = buffer->getImageAddress();
            const std::int64_t signed_size = buffer->getSize();
            if (base_pointer == nullptr || image_pointer == nullptr || signed_size < 0) {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported_frames);
                return;
            }
            const auto base = reinterpret_cast<std::uintptr_t>(base_pointer);
            const auto image = reinterpret_cast<std::uintptr_t>(image_pointer);
            const auto size = static_cast<std::uint64_t>(signed_size);
            bool registered = false;
            {
                std::lock_guard lock{mutex};
                for (const auto& range : registered_ranges) {
                    if (range.base == base && size <= range.size) {
                        registered = true;
                        break;
                    }
                }
            }
            if (size > std::numeric_limits<std::uintptr_t>::max() - base ||
                !registered || image < base || image > base + size ||
                bytes > base + size - image) {
                std::lock_guard lock{mutex};
                increment(counters.malformed_or_unsupported_frames);
                return;
            }

            QueuedFrame frame = snapshot_frame(header,
                static_cast<const std::uint8_t *>(image_pointer), bytes);

            std::lock_guard lock{mutex};
            /* Both rejection arms leave release.value armed, so the buffer is
             * handed straight back to the provider when this call returns. */
            if (!accepting) return;
            if (queue.size() >= capacity) {
                increment(counters.frames_dropped_queue_full);
                return;
            }
            /* Accepted. The bridge now owns a checked-out provider buffer.
             *
             * Exception-safety ordering (PR #42 third-review corrective).
             * std::deque::push_back MAY ALLOCATE AND THROW. The releaser must
             * therefore stay ARMED across the insertion, so a throwing enqueue
             * still funnels this buffer through the single bridge-controlled
             * release policy -- exactly one release(), and the uncertain-
             * ownership path with its dedicated retention slot if that release
             * fails -- instead of silently dropping an owned provider buffer.
             *
             *   1. prepare frame.buffer and frame.retention;
             *   2. releaser stays armed (release.value / release.retention
             *      still describe this exact buffer and slot);
             *   3. queue.push_back(...)  <- the only throwing step;
             *   4. only now disarm the releaser;
             *   5. publish retained_frames and notify the consumer.
             *
             * frame.retention is a COPY of release.retention until step 4, so
             * a throw leaves exactly one owner of the slot, the releaser, and
             * the discarded frame is destroyed without ever having released
             * anything. All of this happens under the mutex, so the queued
             * frame cannot be consumed between steps 3 and 4 and the two
             * owners can never both release. */
            frame.buffer = buffer;
            frame.retention = release.retention;
            enqueue_allocation_failpoint();
            queue.push_back(std::move(frame));
            /* Insertion succeeded: the queue now owns the buffer and the slot.
             * Disarming is noexcept, so ownership transfer is complete. */
            release.retention = no_retention_slot;
            release.value.reset();
            ++retained_frames;
            ready.notify_one();
        } catch (...) {
            std::lock_guard lock{mutex};
            increment(counters.malformed_or_unsupported_frames);
        }
    }
};

bool claim_image_metadata(
    ImageStreamState& stream,
    const std::shared_ptr<ImageMetadataState>& state,
    std::shared_ptr<irmel::ImageChannel>& image_channel) noexcept
{
    std::lock_guard lock{stream.callback->mutex};
    if (!stream.image_channel || stream.callback->lifecycle == Lifecycle::Stopping ||
        stream.callback->lifecycle == Lifecycle::Stopped ||
        stream.callback->lifecycle == Lifecycle::Failed ||
        stream.image_metadata_attempted)
        return false;
    stream.image_metadata_attempted = true;
    stream.image_metadata = state;
    image_channel = stream.image_channel;
    return true;
}

bool claim_navigation_submission(
    ImageStreamState& stream,
    std::shared_ptr<irmel::ImageChannel>& image_channel) noexcept
{
    std::lock_guard lock{stream.callback->mutex};
    const auto lifecycle = stream.callback->lifecycle;
    if (!stream.image_channel ||
        (lifecycle != Lifecycle::Attached && lifecycle != Lifecycle::Running))
        return false;
    image_channel = stream.image_channel;
    ++stream.requests;
    return true;
}

void release_navigation_submission(ImageStreamState& stream) noexcept
{
    std::lock_guard lock{stream.callback->mutex};
    if (stream.requests != 0U) --stream.requests;
}

namespace {
/* Returns exactly one checked-out provider buffer to the provider and
 * publishes the resulting retained_frames transition.
 *
 * Locking discipline, per the updated single-lock invariant:
 *
 *   (caller already moved the Buffer owner out of the queue/snapshot)
 *   unlock: Buffer::release()       provider call, never under the mutex
 *   lock:   publish the count transition / fail-safe retention
 *
 * release() is called from the consumer/snapshot-close path, not necessarily
 * the original provider callback thread. The published irmel::Buffer
 * interface states no callback-thread affinity for release(), and pinned
 * Squall's RequeueBuffer::release() is an atomic one-shot guarded by a mutex,
 * so it is not callback-thread-affine there either. No stronger claim is made
 * for providers whose implementation has not been inspected.
 *
 * Failure semantics. A non-Success return or a thrown exception means the
 * hand-back of ownership is UNCERTAIN. The repository's fail-safe rule then
 * applies: the Buffer object, its registered host bytes, and the provider
 * library must not be destroyed, and retained_frames is deliberately NOT
 * decremented, which permanently defers physical teardown for this graph.
 * release() is never retried, because the published interface guarantees no
 * safe retry and pinned Squall makes release one-shot.
 *
 * Corrective (PR #42 second review). The authoritative owner of a failed
 * Buffer is this frame's dedicated retention slot in
 * CallbackState::retention_slots, which was constructed at Start, before any
 * provider callback and therefore long before this release() was attempted.
 * Moving the callback shared_ptr into that existing empty slot is
 * allocation-free and cannot throw, so the exact callback wrapper survives for
 * process lifetime even if every subsequent allocation and lock acquisition
 * fails. ImageStreamState::retained_failed_buffers is kept only as an
 * additional diagnostic record; safety never depends on it.
 *
 * `retention` must be a valid slot index whenever buffer is non-null. It
 * always is: a frame is only accepted, and therefore only ever reaches this
 * function, when its slot was acquired in the callback.
 *
 * Returns true when the provider accepted the release. */
bool release_retained_buffer(const std::shared_ptr<ImageStreamState>& state,
                             std::shared_ptr<irmel::Buffer> buffer,
                             std::size_t retention) noexcept
{
    if (!buffer) return true;
    if (retention == no_retention_slot) {
        /* Unreachable for an accepted frame; defensive only. Without a
         * dedicated owner for an uncertain result the only safe action is not
         * to release at all: park the buffer in the process-global reserve
         * BEFORE any release, and block teardown permanently. */
        (void)park_pre_release_buffer(buffer);
        state->callback->retain_uncertain_buffer(std::move(buffer),
                                                 no_retention_slot, true);
        return false;
    }
    bool ok = false;
    try {
        ok = buffer->release() == irmel::Return::Success;
    } catch (...) {
        ok = false;
    }
    if (ok) {
        try {
            std::lock_guard lock{state->callback->mutex};
            if (state->callback->retained_frames != 0U)
                --state->callback->retained_frames;
            state->callback->return_retention_slot(retention);
        } catch (...) {
            /* Publication must never throw out of a noexcept release path.
             * Leaving the count high only over-defers teardown, which is the
             * safe direction. */
        }
        return true;
    }
    /* Uncertain ownership. This frame was accepted into the queue, so it is
     * already counted in retained_frames and that unit is never removed. */
    state->callback->retain_uncertain_buffer(buffer, retention, true);
    try {
        std::lock_guard lock{state->callback->mutex};
        /* Additional diagnostic record only; failing here changes nothing. */
        state->retained_failed_buffers.push_back(buffer);
    } catch (...) {
        /* The callback Buffer is already owned by its retention slot, the
         * graph is already retained, and release() is never retried. */
    }
    return false;
}

/* Drains and releases every still-queued frame. Ownership is moved out under
 * the lock and every provider release() runs unlocked afterwards, so provider
 * code is never entered while the teardown mutex is held. */
void discard_queued_frames(const std::shared_ptr<ImageStreamState>& state) noexcept
{
    std::deque<QueuedFrame> discarded;
    try {
        std::lock_guard lock{state->callback->mutex};
        discarded.swap(state->callback->queue);
    } catch (...) {
        return;
    }
    for (auto& frame : discarded)
        (void)release_retained_buffer(state, std::move(frame.buffer),
                                      frame.retention);
}

/* Called after a provider-buffer release may have removed the last
 * outstanding lifetime obligation. image_stream_cleanup(deferred=true)
 * already encodes the whole decision safely under the single teardown lock:
 * it performs physical teardown only when a logical Stop/Close has begun, and
 * only when requests == 0 and retained_frames == 0; it claims cleanup
 * ownership exactly once; and a caller that loses the claim blocks on
 * cleanup_done and adopts the published result instead of racing. It is
 * therefore safe against public Stop, public Close, Navigation completion,
 * another lease closing concurrently, and callback completion. */
void maybe_finish_deferred_cleanup(const std::shared_ptr<ImageStreamState>& state) noexcept
{
    if (!state) return;
    (void)image_stream_cleanup(state, true);
}
} // namespace

class Listener final : public irmel::ImageListener {
public:
    explicit Listener(std::shared_ptr<CallbackState> state) : state_{std::move(state)} {}
    void onImage(const irmel::Channel&, const irmel::FrameHeader& header,
                 std::shared_ptr<irmel::Buffer> buffer) override
    {
        struct CallbackEntry {
            CallbackState& state;
            explicit CallbackEntry(CallbackState& value) noexcept : state{value}
            { state.callbacks_in_flight.fetch_add(1U, std::memory_order_acq_rel); }
            ~CallbackEntry() noexcept
            {
                state.callbacks_in_flight.fetch_sub(1U, std::memory_order_acq_rel);
                state.callbacks_in_flight.notify_all();
            }
        } entry{*state_};
        state_->image(header, buffer);
    }
private:
    std::shared_ptr<CallbackState> state_;
};

/* Task 030B: the opaque snapshot is now a provider-buffer lease.
 *
 *   frame   metadata + retained provider Buffer + borrowed payload span
 *   state   the whole Image provider graph
 *
 * Retaining the state is what keeps the four things a live borrowed span
 * needs alive: the registered host storage, the irmel::Buffer object, the
 * provider library that contains the virtual release() implementation, and
 * enough of the channel graph that release() is still valid. It is a strong
 * reference and that is deliberate and cycle-free: the queue lives inside
 * the state and holds no back-pointer, and a snapshot only exists after its
 * frame has been moved OUT of that queue, so ImageStreamState is never
 * reachable from anything the state itself owns.
 *
 * No public C type or export changed: the handle was already opaque and the
 * view already carried a pixel span. Only what that span points at changed,
 * from snapshot-owned copied bytes to retained MEL Buffer memory. */
struct ams_mel_ir_frame_snapshot {
    QueuedFrame frame;
    std::shared_ptr<ImageStreamState> state;
};

namespace {
bool supported(const irmel::ChannelCapability& capability) noexcept
{
    return capability.getFormat() == irmel::PixelFormat::Mono &&
           capability.getBitDepth() == 8U && capability.getNumberOfBands() == 1U;
}

/* Stops acceptance and transitions frame lifecycle to Stopping/Failed, and
 * logically stops Image metadata. This runs unconditionally and immediately,
 * independent of pending Navigation requests: already queued frames and
 * metadata events still drain, and Receive reports STREAM_STOPPED once the
 * respective queue empties even while physical teardown remains deferred.
 * Later provider callbacks stay memory-safe but can no longer enqueue.
 *
 * Lock separation is mandatory: image_metadata_stream_stopped acquires the
 * metadata mutex and must never be called while CallbackState::mutex is
 * held. The metadata owner is therefore copied out under the frame mutex and
 * stopped after it is released. */
void logical_stop(ImageStreamState& stream, bool failed) noexcept
{
    std::shared_ptr<ImageMetadataState> metadata;
    try {
        {
            std::lock_guard lock{stream.callback->mutex};
            stream.callback->accepting = false;
            if (stream.callback->lifecycle != Lifecycle::Failed)
                stream.callback->lifecycle = failed ? Lifecycle::Failed : Lifecycle::Stopping;
            stream.callback->ready.notify_all();
            metadata = stream.image_metadata;
        }
        image_metadata_stream_stopped(metadata);
    } catch (...) {
        /* Logical stop must remain noexcept. */
    }
}
} // namespace

namespace {
#if defined(AMS_MEL_ENABLE_TEST_FAILPOINTS)
/* Test-only deterministic interleaving support for the Image teardown race.
 * Production behavior never depends on this: without the environment variable
 * the function returns immediately, and the whole body is compiled out of a
 * non-test build.
 *
 * A stage blocks only when the test has armed it by creating
 * "<base>.<stage>.arm" beforehand, so a regression controls exactly the
 * interleaving it needs and every other stage stays free-running. An armed
 * stage publishes "<base>.<stage>.reached" and then waits for
 * "<base>.<stage>.release". */
void image_cleanup_barrier(const char *stage) noexcept
{
    const char *base = std::getenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER");
    if (!base) return;
    try {
        const std::string prefix = std::string{base} + "." + stage;
        if (!std::ifstream{prefix + ".arm"}.good()) return;
        { std::ofstream marker{prefix + ".reached"}; marker << "reached\n"; }
        const std::string release = prefix + ".release";
        for (unsigned attempt = 0; attempt < 15000U; ++attempt) {
            if (std::ifstream{release}.good()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    } catch (...) {
        /* A barrier failure must never change teardown behavior. */
    }
}
/* Test-only, strictly nonblocking observation point. Called with
 * CallbackState::mutex held, so it must never wait: it only records that a
 * public Stop/Close is about to block because a cleanup is already in
 * progress. This lets a regression prove real contention instead of assuming
 * it from elapsed time. */
void close_cleanup_wait_barrier(bool cleanup_in_progress) noexcept
{
    if (!cleanup_in_progress) return;
    const char *base = std::getenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER");
    if (!base) return;
    try {
        const std::string prefix = std::string{base} + ".close-waiting";
        if (!std::ifstream{prefix + ".arm"}.good()) return;
        std::ofstream marker{prefix + ".reached"};
        marker << "reached\n";
    } catch (...) {
        /* An observation failure must never change teardown behavior. */
    }
}
#else
void image_cleanup_barrier(const char *) noexcept {}
void close_cleanup_wait_barrier(bool) noexcept {}
#endif
} // namespace

/* A failed deferred detach cannot safely destroy its graph. Keep it for
 * process lifetime rather than unload provider code that may still own the
 * channel. Mirrors ChannelState::retain_failed in ir_c2.cpp exactly. */
void image_stream_retain_failed(const std::shared_ptr<ImageStreamState>& state) noexcept
{
    static std::atomic<ImageStreamState *> retained{};
    bool expected = false;
    if (!state->emergency_retained.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) return;
    state->emergency_self = state;
    ImageStreamState *head = retained.load(std::memory_order_relaxed);
    do { state->emergency_next = head; }
    while (!retained.compare_exchange_weak(
        head, state.get(), std::memory_order_release, std::memory_order_relaxed));
}

/* Physical provider teardown. Reports NotRequired while requests remain
 * outstanding; the final Navigation request completion performs it instead.
 * Never called with the frame callback mutex held.
 *
 * Synchronization (see the invariant in internal/ir_stream.hpp):
 *
 *   lock:   validate lifecycle, claim cleanup ownership, move the provider
 *           shared_ptrs into local owners so no other thread can observe or
 *           modify them mid-teardown.
 *   unlock: disable(), detachChannel(), provider channel destruction. These
 *           may call back synchronously or need callback progress, so they
 *           must never run under the frame callback mutex.
 *   lock:   publish cleanup_ok/cleanup_complete, release ownership, wake any
 *           Stop/Close waiting on cleanup_done.
 *
 * A caller that finds cleanup already owned by another thread blocks on
 * cleanup_done and adopts the published result; it never inspects channel
 * concurrently and never spins. */
ImageCleanupOutcome image_stream_cleanup(
    const std::shared_ptr<ImageStreamState>& state_ptr, bool deferred) noexcept
{
    auto& stream = *state_ptr;
    bool failed = false;
    /* Local owners. The provider graph stays alive here across the unlocked
     * provider phase while the shared state members are already cleared. */
    std::shared_ptr<irmel::Channel> channel;
    std::shared_ptr<irmel::ImageChannel> image_channel;
    std::shared_ptr<ImageMetadataState> metadata;
    bool disable_needed = false;
    bool owner_closed = false;
    try {
        {
            std::unique_lock lock{stream.callback->mutex};
            /* Synchronize with a cleanup already owned by another thread
             * rather than racing it on channel. */
            stream.cleanup_done.wait(lock,
                [&stream] { return !stream.cleanup_in_progress; });
            if (stream.requests != 0U) return ImageCleanupOutcome::NotRequired;
            /* Task 030B: a queued frame or a live snapshot still holds a
             * provider Buffer checked out. Destroying the channel, the Buffer
             * objects, or the registered host storage now would invalidate a
             * live borrowed span and make the owed release() invalid, so
             * physical teardown is deferred exactly as it is for a pending
             * Navigation request. The final lease close performs it instead.
             *
             * uncertain_release is the corrective (PR #42 second review)
             * backstop. It is published lock-free the instant ANY release for
             * this stream is uncertain, including a callback-side rejection,
             * so physical teardown is blocked even if the retained_frames
             * publication under the lock could not be performed. */
            if (stream.callback->retained_frames != 0U ||
                stream.callback->uncertain_release.load(std::memory_order_acquire))
                return ImageCleanupOutcome::NotRequired;
            if (stream.cleanup_complete)
                return stream.cleanup_ok ? ImageCleanupOutcome::Succeeded
                                         : ImageCleanupOutcome::Failed;
            /* A final Navigation request completion performs teardown only if
             * a logical Stop/Close already began. A completed request on a
             * still-Attached or Running stream must never detach the provider
             * channel the application is still using. */
            if (deferred) {
                const bool stopping =
                    stream.callback->lifecycle == Lifecycle::Stopping ||
                    stream.callback->lifecycle == Lifecycle::Stopped ||
                    stream.callback->lifecycle == Lifecycle::Failed;
                if (!stopping && !stream.public_owner_closed)
                    return ImageCleanupOutcome::NotRequired;
            }
            if (!stream.channel) return ImageCleanupOutcome::NotRequired;
            stream.cleanup_in_progress = true;
            failed = stream.callback->lifecycle == Lifecycle::Failed;
            owner_closed = stream.public_owner_closed;
            disable_needed = stream.enable_attempted;
            /* Move ownership out of the shared state under the lock so the
             * unlocked provider phase touches only local owners. */
            channel = std::move(stream.channel);
            image_channel = std::move(stream.image_channel);
            stream.channel.reset();
            stream.image_channel.reset();
            metadata = stream.image_metadata;
        }

        image_cleanup_barrier("before-detach");

        if (disable_needed) {
            try {
                if (channel->disable() != irmel::Return::Success) failed = true;
            } catch (...) {
                failed = true;
            }
        }

        bool detached = false;
        try {
            detached = stream.session->control->detachChannel(channel) ==
                       irmel::Return::Success;
        } catch (...) {
            failed = true;
        }
        image_cleanup_barrier(detached ? "detach-ok" : "detach-failed");
        if (!detached) {
            /* Detach ownership is uncertain: the provider may still own the
             * channel. Restore the graph into the shared state under the lock
             * so a retry (public Close) can attempt detach again, and make the
             * failure visible to any waiter. cleanup_complete stays false
             * precisely because this is not a terminal outcome. */
            {
                std::lock_guard lock{stream.callback->mutex};
                stream.channel = std::move(channel);
                stream.image_channel = std::move(image_channel);
                stream.callback->lifecycle = Lifecycle::Failed;
                stream.callback->ready.notify_all();
                stream.cleanup_failed = true;
                stream.cleanup_in_progress = false;
                stream.cleanup_done.notify_all();
            }
            /* Only a failure with no public owner left has nobody to retry;
             * retain the whole graph rather than unload provider code that may
             * still own the channel. */
            if (owner_closed) image_stream_retain_failed(state_ptr);
            return ImageCleanupOutcome::Failed;
        }

        /* disable() is not a quiescence boundary. Provider channel
         * destruction occurs while every callback-accessible object and
         * host byte remains retained by stream. */
        image_channel.reset();
        channel.reset();

        image_metadata_stream_stopped(metadata);

        std::size_t in_flight = stream.callback->callbacks_in_flight.load(
            std::memory_order_acquire);
        while (in_flight != 0U) {
            stream.callback->callbacks_in_flight.wait(
                in_flight, std::memory_order_acquire);
            in_flight = stream.callback->callbacks_in_flight.load(
                std::memory_order_acquire);
        }
        /* Post-drain teardown recheck (PR #42 third-review corrective).
         *
         * The initial gate above proved requests == 0, retained_frames == 0
         * and !uncertain_release at the instant cleanup ownership was claimed.
         * That is NOT sufficient: a callback that was ALREADY in flight when
         * the gate was passed can still fail a Buffer::release() and publish
         * its uncertainty afterwards. Freeing registered host storage on the
         * strength of the stale gate result would destroy the exact host bytes
         * and the exact Buffer objects an uncertain provider hand-back forbids
         * destroying.
         *
         * Visibility is established, not assumed. retain_uncertain_buffer()
         * publishes uncertain_release with release ordering and, for the
         * accepted-frame path, retained_frames under the mutex; both happen
         * strictly BEFORE the callback's callbacks_in_flight decrement, which
         * is a release (acq_rel) RMW. The drain loop above observes that count
         * reach zero with acquire ordering, so everything the callback
         * published is visible here. The mutex below additionally orders the
         * retained_frames read.
         *
         * Achievable guarantee, stated exactly. The provider channel has
         * already crossed its documented quiescence/destruction boundary by
         * this point -- disable(), detachChannel() and channel destruction all
         * ran above -- so this path deliberately does NOT claim the channel
         * remained attached. What it does guarantee is what an uncertain
         * hand-back actually requires: the registered host storage, the
         * registered Buffer objects, CallbackState, ImageStreamState, the
         * Session graph and therefore the provider library are retained for
         * process lifetime, no retained Buffer ownership is destroyed, the
         * uncertain release is never retried, and the failure is reported
         * truthfully. */
        bool late_uncertain = false;
        {
            std::lock_guard lock{stream.callback->mutex};
            failed = failed || stream.callback->lifecycle == Lifecycle::Failed;
            late_uncertain =
                (stream.callback->retained_frames != 0U ||
                 stream.callback->uncertain_release.load(
                     std::memory_order_acquire)) &&
                !skip_post_drain_recheck_failpoint();
            /* Registered ranges are part of the host-storage description an
             * uncertain buffer still points at, so they are cleared only when
             * the storage itself is about to be freed. */
            if (!late_uncertain) stream.callback->registered_ranges.clear();
        }
        if (late_uncertain) {
            image_stream_retain_failed(state_ptr);
            {
                std::lock_guard lock{stream.callback->mutex};
                stream.callback->accepting = false;
                stream.callback->lifecycle = Lifecycle::Failed;
                stream.callback->ready.notify_all();
                stream.enable_attempted = false;
                /* detachChannel() succeeded, so there is nothing to retry and
                 * this cleanup is terminal; it is simply terminal in FAILURE,
                 * because provider ownership of at least one Buffer is
                 * uncertain. */
                stream.cleanup_ok = false;
                stream.cleanup_complete = true;
                stream.cleanup_failed = true;
                stream.cleanup_in_progress = false;
                stream.cleanup_done.notify_all();
            }
            return ImageCleanupOutcome::Failed;
        }
        /* Safe only because this point is reachable only when
         * callback->retained_frames == 0 and !uncertain_release, checked under
         * the teardown lock before cleanup ownership was claimed AND rechecked
         * above after the callback drain. That is the proof of the
         * Task 030B host-storage invariant:
         *
         *     a retained provider buffer exists
         *         => its registered host byte range exists unchanged
         *
         * A buffer whose release() failed is never counted down and its exact
         * callback shared_ptr is owned permanently by a preallocated
         * RetainedBufferNode, so uncertain ownership keeps both the Buffer
         * object and these bytes alive for process lifetime. */
        stream.buffers.clear();
        stream.storage.clear();
        {
            std::lock_guard lock{stream.callback->mutex};
            stream.enable_attempted = false;
            stream.callback->lifecycle = failed ? Lifecycle::Failed : Lifecycle::Stopped;
            stream.callback->ready.notify_all();
            /* detachChannel succeeded, so ownership safety is proven and this
             * is a terminal cleanup even when disable() failed. */
            stream.cleanup_ok = !failed;
            stream.cleanup_complete = true;
            stream.cleanup_in_progress = false;
            stream.cleanup_done.notify_all();
        }
        return failed ? ImageCleanupOutcome::Failed : ImageCleanupOutcome::Succeeded;
    } catch (...) {
        try {
            std::lock_guard lock{stream.callback->mutex};
            /* Ownership is uncertain: restore anything not yet released so a
             * retry can run, and never leave cleanup ownership claimed. */
            if (channel && !stream.channel) stream.channel = std::move(channel);
            if (image_channel && !stream.image_channel)
                stream.image_channel = std::move(image_channel);
            stream.callback->accepting = false;
            stream.callback->lifecycle = Lifecycle::Failed;
            stream.callback->ready.notify_all();
            stream.cleanup_in_progress = false;
            stream.cleanup_done.notify_all();
        } catch (...) {}
        return ImageCleanupOutcome::Failed;
    }
}

namespace {
/* Performs logical Stop and, when no Navigation request is outstanding, the
 * physical teardown. The returned status reflects the cleanup actually
 * observed under the teardown lock, including a cleanup owned by the
 * Navigation completion thread that this call synchronized with. */
ams_mel_status_t teardown(const std::shared_ptr<ImageStreamState>& state_ptr, bool failed,
                          char *out, std::size_t capacity, std::size_t *required) noexcept
{
    auto& stream = *state_ptr;
    logical_stop(stream, failed);
    {
        std::lock_guard lock{stream.callback->mutex};
        /* Task 030B: Stop is logical now, physical later. Stop never blocks
         * waiting for an application-held Frame_Lease, exactly as it already
         * never blocks on an outstanding Navigation request. Frames accepted
         * before the logical Stop stay consumable and live snapshots stay
         * valid; the last release performs the deferred physical teardown. */
        if (stream.requests != 0U || stream.callback->retained_frames != 0U ||
            stream.callback->uncertain_release.load(std::memory_order_acquire)) {
            /* Deferral is not success when the stream is already poisoned.
             * The common case, a live lease on a healthy stream, still
             * reports OK; but a graph whose provider release left ownership
             * uncertain is permanently un-teardownable, and Stop must say so
             * rather than imply a clean logical stop. */
            if (stream.callback->lifecycle == Lifecycle::Failed) {
                diagnostic("provider stream operation or cleanup failed", out,
                           capacity, required);
                return AMS_MEL_PROVIDER_FAILED;
            }
            return AMS_MEL_OK;
        }
    }
    const ImageCleanupOutcome outcome = image_stream_cleanup(state_ptr, false);
    if (outcome == ImageCleanupOutcome::Failed) {
        diagnostic("channel detach failed; callback resources retained", out,
                   capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    bool poisoned;
    {
        std::lock_guard lock{stream.callback->mutex};
        poisoned = stream.callback->lifecycle == Lifecycle::Failed;
    }
    if (poisoned) {
        diagnostic("provider stream operation or cleanup failed", out,
                   capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    return AMS_MEL_OK;
}
} // namespace

extern "C" ams_mel_status_t ams_mel_ir_stream_open(
    const ams_mel_session *session, const ams_mel_ir_stream_config_v1 *config,
    ams_mel_ir_stream **out_stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!session || !session->state || !config || !out_stream || *out_stream ||
        (!out && capacity != 0U) ||
        config->channel_type != AMS_MEL_IR_CHANNEL_IRST_IMAGE ||
        config->buffer_count == 0U || config->buffer_size == 0U ||
        config->queue_capacity == 0U ||
        config->buffer_size > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max()) ||
        config->buffer_count - 1U > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max()) ||
        !valid_view(config->channel_id.descriptive_label) ||
        !valid_view(config->platform_id.descriptive_label) ||
        !valid_view(config->sensor_location.key) ||
        !valid_view(config->sensor_location.system_name)) {
        diagnostic("invalid IR stream configuration", out, capacity, required);
        return AMS_MEL_INVALID_ARGUMENT;
    }
    try {
        auto stream = std::make_unique<ams_mel_ir_stream>();
        auto state = std::make_shared<ImageStreamState>();
        stream->state = state;
        state->session = session->state;
        state->callback = std::make_shared<CallbackState>();
        state->callback->capacity = config->queue_capacity;
        /* Corrective (PR #42 second review): established during stream
         * construction, before any provider callback can exist, so a
         * callback-side uncertain release can reach the same graph-retention
         * policy as snapshot/lease release. Weak, so it cannot create an
         * ownership cycle with the state that owns this CallbackState. */
        state->callback->owner = state;
        state->listener = std::make_shared<Listener>(state->callback);
        state->buffer_count = config->buffer_count;
        state->buffer_size = config->buffer_size;
        state->instance = state->session->instance;
        state->buffer_factory = state->session->library->symbol<BufferFactory>("getBuffer");
        mel::ForeignKey key{copy_view(config->sensor_location.key),
                            copy_view(config->sensor_location.system_name)};
        mel::ComponentLocation location{config->sensor_location.offset_x_m,
            config->sensor_location.offset_y_m,
            config->sensor_location.offset_z_m, key};
        irmel::Config upstream{convert_id(config->channel_id),
            irmel::ChannelType::IRSTImage, convert_id(config->platform_id),
            std::move(location), state->listener, false, false};
        state->channel = state->session->control->attachChannel(upstream);
        if (!state->channel) {
            diagnostic("attachChannel returned null", out, capacity, required);
            return AMS_MEL_FACTORY_FAILED;
        }
        state->image_channel =
            std::dynamic_pointer_cast<irmel::ImageChannel>(state->channel);
        bool compatible = false;
        try {
            compatible = state->image_channel &&
                         supported(state->channel->getCapabilities());
        } catch (...) {
            bool detached = false;
            try {
                detached = state->session->control->detachChannel(state->channel) ==
                           irmel::Return::Success;
            } catch (...) {}
            if (!detached) {
                diagnostic("capability query and detach failed", out, capacity, required);
                (void)stream.release();
                return AMS_MEL_PROVIDER_FAILED;
            }
            state->image_channel.reset();
            state->channel.reset();
            throw;
        }
        if (!compatible) {
            bool detached = false;
            try {
                detached = state->session->control->detachChannel(state->channel) ==
                           irmel::Return::Success;
            } catch (...) {}
            if (detached) {
            state->image_channel.reset();
            state->channel.reset();
            } else {
                /* No owner may be destroyed while provider access can remain. */
                (void)stream.release();
            }
            diagnostic(detached ? "provider does not advertise Mono8 single-band capability" :
                       "incompatible channel and detach failed", out, capacity, required);
            return detached ? AMS_MEL_INITIALIZATION_FAILED : AMS_MEL_PROVIDER_FAILED;
        }
        stream->state = std::move(state);
        *out_stream = stream.release();
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        diagnostic("provider exception during stream open", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_start(
    ams_mel_ir_stream *stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    try {
        {
            std::lock_guard lock{stream->state->callback->mutex};
            switch (stream->state->callback->lifecycle) {
            case Lifecycle::Running: return AMS_MEL_OK;
            case Lifecycle::Stopped: return AMS_MEL_STREAM_STOPPED;
            case Lifecycle::Failed: return AMS_MEL_PROVIDER_FAILED;
            /* A concurrent Start is genuinely a misuse. */
            case Lifecycle::Starting: return AMS_MEL_PROVIDER_FAILED;
            /* Stopping is a LOGICALLY STOPPED stream whose physical teardown
             * is merely deferred (PR #42 third-review corrective).
             *
             * Task 030B made Stop logical-now/physical-later: with a queued
             * frame or a live lease still holding a provider buffer, Stop
             * returns AMS_MEL_OK and the lifecycle stays Stopping until the
             * last release performs the deferred teardown and publishes
             * Stopped. Reporting AMS_MEL_PROVIDER_FAILED for a Start in that
             * window described a healthy deferral as a provider failure, and
             * did so nondeterministically -- whether a Start after a healthy
             * Stop saw Stopping or Stopped depended purely on whether the
             * provider's producer happened to leave a frame queued.
             *
             * STREAM_STOPPED is both the truthful answer and the one Receive
             * already gives for this same state, so Start and Receive now
             * agree. A genuinely poisoned stream is Failed, not Stopping, and
             * still reports AMS_MEL_PROVIDER_FAILED above. */
            case Lifecycle::Stopping: return AMS_MEL_STREAM_STOPPED;
            case Lifecycle::Attached: break;
            }
            stream->state->callback->lifecycle = Lifecycle::Starting;
        }
    } catch (...) {
        diagnostic("stream state transition failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    try {
        auto& state = *stream->state;
        /* Copy the provider channel owner out under the teardown lock, then
           call into the provider unlocked. */
        std::shared_ptr<irmel::Channel> channel;
        {
            std::lock_guard lock{state.callback->mutex};
            channel = state.channel;
        }
        if (!channel) throw std::runtime_error("stream has no provider channel");
        state.storage.reserve(state.buffer_count);
        state.buffers.reserve(state.buffer_count);
        for (std::size_t i = 0; i < state.buffer_count; ++i) {
            auto buffer = state.buffer_factory(state.instance, state.session->manager);
            if (!buffer) throw std::runtime_error("getBuffer returned null");
            state.storage.emplace_back(state.buffer_size);
            if (buffer->init(state.storage.back().data(), state.buffer_size,
                             static_cast<std::int64_t>(i)) != irmel::Return::Success ||
                channel->registerBuffer(buffer) != irmel::Return::Success) {
                throw std::runtime_error("buffer initialization or registration failed");
            }
            state.buffers.push_back(std::move(buffer));
        }
        {
            std::lock_guard lock{state.callback->mutex};
            state.callback->registered_ranges.clear();
            state.callback->registered_ranges.reserve(state.storage.size());
            for (const auto& bytes : state.storage) {
                state.callback->registered_ranges.push_back(
                    {reinterpret_cast<std::uintptr_t>(bytes.data()), bytes.size()});
            }
            /* Corrective (PR #42 second review): build this stream's
             * emergency failed-buffer ownership BEFORE channel->enable(),
             * i.e. strictly before any provider callback can occur, and size
             * it from the configured buffer_count rather than a process-global
             * constant. Every simultaneously registered provider buffer can
             * therefore be owned permanently without allocating anything after
             * a release() has failed.
             *
             * Restart-safe: a slot already holding an uncertain buffer is
             * never reset or freed. Slots are only appended, and only indices
             * whose slot is currently empty go back on the free list. */
            if (state.callback->retention_slots.size() < state.buffer_count)
                state.callback->retention_slots.resize(state.buffer_count);
            state.callback->free_retention_slots.clear();
            state.callback->free_retention_slots.reserve(
                state.callback->retention_slots.size());
            for (std::size_t i = state.callback->retention_slots.size(); i-- > 0U;) {
                if (!state.callback->retention_slots[i])
                    state.callback->free_retention_slots.push_back(i);
            }
            state.callback->accepting = true;
            /* Teardown-participating state: record under the teardown lock
               that disable() is now owed to the provider. The lifecycle is
               Starting here, so no deferred cleanup can be claiming ownership,
               but the single-lock invariant is kept uniform. */
            state.enable_attempted = true;
        }
        if (channel->enable() != irmel::Return::Success) {
            throw std::runtime_error("channel enable failed");
        }
        bool callback_failed = false;
        {
            std::lock_guard lock{state.callback->mutex};
            callback_failed = state.callback->lifecycle == Lifecycle::Failed;
            if (!callback_failed) state.callback->lifecycle = Lifecycle::Running;
        }
        if (callback_failed) return teardown(stream->state, true, out, capacity, required);
        return AMS_MEL_OK;
    } catch (const std::bad_alloc&) {
        stream->state->callback->fail();
        (void)teardown(stream->state, true, nullptr, 0U, nullptr);
        diagnostic("allocation failed during stream start", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        stream->state->callback->fail();
        (void)teardown(stream->state, true, nullptr, 0U, nullptr);
        diagnostic("provider failure during stream start", out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_receive(
    ams_mel_ir_stream *stream, std::uint32_t timeout_ms,
    ams_mel_ir_frame_v1 *frame, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || !frame || (!frame->pixels && frame->pixel_capacity != 0U) ||
        (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    /* Owned-copy compatibility path, unchanged externally: the caller's bytes
     * are filled from the retained provider buffer and the provider buffer is
     * then released, so the returned frame keeps no provider dependency. The
     * copy happens under the lock because the queued frame is still owned by
     * the queue there; the provider release() afterwards is deliberately
     * outside the lock. */
    std::shared_ptr<irmel::Buffer> consumed;
    /* Travels with the consumed buffer so the release below already owns its
     * allocation-free emergency owner. */
    std::size_t consumed_retention = no_retention_slot;
    ams_mel_status_t status = AMS_MEL_INTERNAL_ERROR;
    try {
        const auto& state = *stream->state;
        std::unique_lock lock{state.callback->mutex};
        /* Stopping is terminal for waiting: a logical Stop/Close has already
         * happened, so no further frame can be queued even though physical
         * teardown may remain deferred behind a pending Navigation request or
         * a retained provider buffer. */
        const auto terminal = [&state] {
            return state.callback->lifecycle == Lifecycle::Stopping ||
                   state.callback->lifecycle == Lifecycle::Stopped ||
                   state.callback->lifecycle == Lifecycle::Failed;
        };
        if (state.callback->queue.empty() && !terminal()) {
            (void)state.callback->ready.wait_for(lock,
                std::chrono::milliseconds{timeout_ms}, [&] {
                    return !state.callback->queue.empty() || terminal();
                });
        }
        if (state.callback->queue.empty()) {
            if (state.callback->lifecycle == Lifecycle::Failed)
                return AMS_MEL_PROVIDER_FAILED;
            return terminal() ? AMS_MEL_STREAM_STOPPED : AMS_MEL_TIMEOUT;
        }
        QueuedFrame& queued = state.callback->queue.front();
        frame->pixel_required = queued.payload_size;
        if (!frame->pixels || frame->pixel_capacity < queued.payload_size) {
            /* The frame stays queued and the buffer stays retained. */
            return AMS_MEL_BUFFER_TOO_SMALL;
        }
        std::uint8_t *pixels = frame->pixels;
        const std::size_t pixel_capacity = frame->pixel_capacity;
        const auto& metadata = queued.view;
        frame->system_time_ns = metadata.system_time_ns;
        frame->integration_time_ns = metadata.integration_time_ns;
        frame->width = metadata.width; frame->height = metadata.height;
        frame->bits_per_pixel = metadata.bits_per_pixel;
        frame->number_of_bands = metadata.number_of_bands;
        frame->horizontal_fov_rad = metadata.horizontal_fov_rad;
        frame->vertical_fov_rad = metadata.vertical_fov_rad;
        frame->pixel_format = metadata.pixel_format;
        frame->frame_id = metadata.frame_id; frame->subframe_id = metadata.subframe_id;
        frame->subframe_total = metadata.subframe_total; frame->image_type = metadata.image_type;
        frame->image_flip = metadata.image_flip; frame->image_flags = 0U;
        for (const auto flag : queued.flags) frame->image_flags |= UINT32_C(1) << flag;
        frame->dither_row = metadata.dither_row; frame->dither_column = metadata.dither_column;
        frame->row_offset = metadata.row_offset; frame->column_offset = metadata.column_offset;
        frame->band_index = metadata.band_index;
        frame->pixels = pixels;
        frame->pixel_capacity = pixel_capacity;
        frame->pixel_required = queued.payload_size;
        if (queued.payload_size != 0U && queued.payload != nullptr)
            std::memcpy(pixels, queued.payload, queued.payload_size);
        /* Take the checked-out buffer out of the queue; the release below
         * returns it to the provider and decrements retained_frames. */
        consumed = std::move(queued.buffer);
        consumed_retention = queued.retention;
        queued.retention = no_retention_slot;
        state.callback->queue.pop_front();
        status = AMS_MEL_OK;
    } catch (...) {
        diagnostic("receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
    if (consumed && !release_retained_buffer(stream->state, std::move(consumed),
                                             consumed_retention)) {
        diagnostic("provider buffer release failed; provider graph retained",
                   out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    /* A lease release can be the last outstanding obligation. */
    maybe_finish_deferred_cleanup(stream->state);
    return status;
}

extern "C" ams_mel_status_t ams_mel_ir_stream_receive_snapshot(
    ams_mel_ir_stream *stream, std::uint32_t timeout_ms,
    ams_mel_ir_frame_snapshot **snapshot, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || !snapshot || *snapshot || (!out && capacity != 0U))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        const auto& state = *stream->state;
        std::unique_lock lock{state.callback->mutex};
        /* Identical terminal semantics to the legacy Receive: Stopping is a
         * logical stop and must not wait for deferred physical teardown. */
        const auto terminal = [&state] { return state.callback->lifecycle == Lifecycle::Stopping ||
            state.callback->lifecycle == Lifecycle::Stopped ||
            state.callback->lifecycle == Lifecycle::Failed; };
        if (state.callback->queue.empty() && !terminal()) {
            (void)state.callback->ready.wait_for(lock, std::chrono::milliseconds{timeout_ms}, [&] {
                return !state.callback->queue.empty() || terminal(); });
        }
        if (state.callback->queue.empty()) {
            if (state.callback->lifecycle == Lifecycle::Failed) return AMS_MEL_PROVIDER_FAILED;
            return terminal() ? AMS_MEL_STREAM_STOPPED : AMS_MEL_TIMEOUT;
        }
        auto owner = std::make_unique<ams_mel_ir_frame_snapshot>();
        /* Queue -> snapshot handoff. The retained provider Buffer moves with
         * the frame, so retained_frames is deliberately unchanged here: the
         * same buffer is still checked out, only its owner changed. The
         * snapshot also retains the provider graph so the borrowed span, the
         * Buffer object, and the provider release() implementation all stay
         * valid until snapshot_close. */
        owner->frame = std::move(state.callback->queue.front());
        state.callback->queue.pop_front();
        owner->state = stream->state;
        owner->frame.bind();
#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS
        /* Test-only malformed-span injection. Real providers cannot produce
         * these spans through bind(), but every consumer binding must still
         * fail closed instead of dereferencing them, so the test facade can
         * publish exactly the three degenerate shapes. The variable is not
         * consumed, so a scenario stays armed for every frame it receives. */
        if (const char *span = std::getenv("AMS_MEL_TEST_SNAPSHOT_PIXEL_SPAN"); span != nullptr) {
            if (std::strcmp(span, "null-nonzero") == 0)
                owner->frame.view.pixels = {nullptr, owner->frame.payload_size};
            else if (std::strcmp(span, "oversize") == 0)
                owner->frame.view.pixels = {owner->frame.payload,
                                            std::numeric_limits<std::size_t>::max()};
            else if (std::strcmp(span, "empty") == 0)
                owner->frame.view.pixels = {nullptr, 0U};
        }
        /* Test-only zero-copy alias hook. Task 030B extends it to the fourth
         * field, the provider's own Buffer::getImageAddress(), so a consumer
         * binding can prove the full three-way identity
         *
         *     Buffer::getImageAddress() == snapshot pixels.data == Ada view
         *
         * rather than only the native-to-Ada half. It publishes no new export
         * and no new ABI type, writes only when the test explicitly names a
         * log, and records
         * "<frame_id> <pixel-data-address> <size> <provider-image-address>". */
        if (const char *log = std::getenv("AMS_MEL_TEST_SNAPSHOT_ADDRESS_LOG"); log != nullptr) {
            std::uintptr_t provider_address = 0U;
            if (owner->frame.buffer) {
                try {
                    provider_address = reinterpret_cast<std::uintptr_t>(
                        owner->frame.buffer->getImageAddress());
                } catch (...) {
                    provider_address = 0U;
                }
            }
            std::ofstream stream{log, std::ios::app};
            stream << owner->frame.view.frame_id << ' '
                   << reinterpret_cast<std::uintptr_t>(owner->frame.view.pixels.data) << ' '
                   << owner->frame.view.pixels.size << ' '
                   << provider_address << '\n';
        }
#endif
        *snapshot = owner.release();
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("snapshot receive failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_frame_snapshot_view(
    const ams_mel_ir_frame_snapshot *snapshot, const ams_mel_ir_frame_snapshot_v1 **view,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!snapshot || !view || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    *view = &snapshot->frame.view;
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_frame_snapshot_close(
    ams_mel_ir_frame_snapshot **snapshot, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!snapshot || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
    /* Closing a null handle succeeds, so explicit Close stays idempotent. */
    if (!*snapshot) return AMS_MEL_OK;
    /* Take ownership of the owner and of the checked-out provider buffer
     * before any provider call, so the snapshot cannot be closed twice and
     * release() cannot be invoked twice for the same buffer. */
    std::unique_ptr<ams_mel_ir_frame_snapshot> owner{*snapshot};
    *snapshot = nullptr;
    auto state = std::move(owner->state);
    auto buffer = std::move(owner->frame.buffer);
    /* Dedicated retention slot, acquired in the provider callback before any
     * release() for this buffer could be attempted. */
    const std::size_t retention = owner->frame.retention;
    owner->frame.retention = no_retention_slot;
    /* The borrowed span is dead as soon as the buffer goes back. */
    owner->frame.payload = nullptr;
    owner->frame.payload_size = 0U;
    owner->frame.view.pixels = {nullptr, 0U};
    owner.reset();
    if (!state) return AMS_MEL_OK;
    /* Task 030B: closing a lease now performs a provider call and can fail.
     * A failed release leaves ownership uncertain, so the graph is retained
     * rather than freed and the caller is told the truth. */
    const bool released = release_retained_buffer(state, std::move(buffer),
                                                  retention);
    /* This may have been the final outstanding obligation. */
    maybe_finish_deferred_cleanup(state);
    if (!released) {
        diagnostic("provider buffer release failed; provider graph retained",
                   out, capacity, required);
        return AMS_MEL_PROVIDER_FAILED;
    }
    return AMS_MEL_OK;
}

extern "C" ams_mel_status_t ams_mel_ir_stream_get_counters(
    const ams_mel_ir_stream *stream, ams_mel_ir_stream_counters_v1 *counters,
    char *out, std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    try {
        if (!stream || !counters || (!out && capacity != 0U))
            return AMS_MEL_INVALID_ARGUMENT;
        std::lock_guard lock{stream->state->callback->mutex};
        *counters = stream->state->callback->counters;
        return AMS_MEL_OK;
    } catch (...) {
        diagnostic("counter query failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_get_capabilities(
    ams_mel_ir_stream *stream, ams_mel_ir_channel_capability **output, char *out,
    std::size_t capacity, std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    if (!stream || !output || *output || (!out && capacity != 0U))
        return AMS_MEL_INVALID_ARGUMENT;
    try {
        std::shared_ptr<irmel::ImageChannel> image_channel;
        {
            std::lock_guard lock{stream->state->callback->mutex};
            if (!stream->state->image_channel ||
                stream->state->callback->lifecycle == Lifecycle::Stopping ||
                stream->state->callback->lifecycle == Lifecycle::Stopped ||
                stream->state->callback->lifecycle == Lifecycle::Failed)
                return AMS_MEL_PROVIDER_FAILED;
            image_channel = stream->state->image_channel;
        }
        return ams_mel::internal::snapshot_capability(*image_channel, output,
                                                       out, capacity, required);
    } catch (...) {
        diagnostic("Image capability query failed", out, capacity, required);
        return AMS_MEL_PROVIDER_EXCEPTION;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_stop(
    ams_mel_ir_stream *stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    try {
        if (!stream || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
        bool failed;
        {
            /* channel must be inspected under the teardown lock: the
             * Navigation completion thread may be resetting it concurrently. */
            std::unique_lock lock{stream->state->callback->mutex};
            close_cleanup_wait_barrier(stream->state->cleanup_in_progress);
            stream->state->cleanup_done.wait(lock,
                [&stream] { return !stream->state->cleanup_in_progress; });
            if (stream->state->callback->lifecycle == Lifecycle::Stopped) return AMS_MEL_OK;
            failed = stream->state->callback->lifecycle == Lifecycle::Failed;
            if (!stream->state->channel)
                return failed ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK;
        }
        return teardown(stream->state, failed, out, capacity, required);
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed during stream stop", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        diagnostic("stream stop failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}

extern "C" ams_mel_status_t ams_mel_ir_stream_close(
    ams_mel_ir_stream **stream, char *out, std::size_t capacity,
    std::size_t *required) noexcept
{
    diagnostic("", out, capacity, required);
    try {
        if (!stream || (!out && capacity != 0U)) return AMS_MEL_INVALID_ARGUMENT;
        ams_mel_ir_stream *owned = *stream;
        if (!owned) return AMS_MEL_OK;
        auto status = ams_mel_ir_stream_stop(owned, out, capacity, required);
        auto& state = *owned->state;
        /* Task 030B Close policy. Close is not Stop: once the public stream
         * owner is gone, a frame that was queued but never acquired can never
         * be consumed by that caller, so leaving it queued would strand a
         * provider buffer in an unreachable queue. Discard those frames here
         * and hand their provider buffers back.
         *
         *   Close -> logical stop
         *         -> discard still-queued, not-yet-acquired frames
         *         -> release those provider buffers safely
         *         -> preserve already-dequeued live Frame_Lease objects
         *         -> defer final physical teardown until
         *              requests == 0 AND retained_frames == 0
         *
         * Ownership is moved out under the lock and every provider release()
         * runs unlocked, so no provider code is entered while the teardown
         * mutex is held. */
        discard_queued_frames(owned->state);
        /* Test-only: allows a regression to run the final Navigation
         * completion and its deferred cleanup exactly here, after the logical
         * Stop but before the owner-release decision is committed. */
        image_cleanup_barrier("close-decision");
        bool release_owner = false;
        /* The final Navigation completion decrements requests and only then
         * claims cleanup ownership, so there is a window in which
         * requests == 0, cleanup_in_progress == false, and channel is still
         * set while that completion is already committed to cleaning up.
         * Close must not decide from that transient state: it runs/joins the
         * cleanup itself and decides from the published result. Cleanup
         * ownership is single-claim, so whichever thread wins performs the
         * teardown exactly once and the other adopts its outcome. */
        /* Bounded purely as a defensive guard: each iteration that runs
         * cleanup either completes it (channel released) or records a failure,
         * so at most one retry is ever needed. */
        for (unsigned attempt = 0; attempt < 4U; ++attempt) {
            bool run_cleanup = false;
            {
                /* Waiting on cleanup_done means a cleanup owned by another
                 * thread has already published its outcome before this
                 * decision is taken, so Close can neither inspect channel
                 * concurrently nor act on a stale earlier Stop result. */
                std::unique_lock lock{state.callback->mutex};
                close_cleanup_wait_barrier(state.cleanup_in_progress);
                state.cleanup_done.wait(lock,
                    [&state] { return !state.cleanup_in_progress; });
                if (state.requests != 0U ||
                    state.callback->retained_frames != 0U ||
                    state.callback->uncertain_release.load(
                        std::memory_order_acquire)) {
                    /* A pending Navigation request OR a live provider-buffer
                     * lease keeps the provider channel
                     * attached by design. Release the public owner so the
                     * logical close is externally observable; whichever
                     * obligation finishes last, the final request completion
                     * or the final lease close, performs deferred physical
                     * teardown and, if that detach fails, permanent
                     * allocation-free retention. A live lease therefore keeps
                     * its borrowed bytes valid across public stream Close and
                     * even across Session close, not by copying but by
                     * deferring the actual provider unload. Committing the
                     * owner release and
                     * public_owner_closed inside this same critical section is
                     * what makes the deferred-cleanup decision atomic with
                     * respect to that completion. */
                    release_owner = true;
                } else if (state.channel) {
                    if (state.cleanup_failed) {
                        /* Detach ownership is not established: a synchronous
                         * detach failure, or a deferred cleanup that failed
                         * detach and restored the graph. Retain the public
                         * owner for a later retry and never report success. */
                        release_owner = false;
                        status = AMS_MEL_PROVIDER_FAILED;
                    } else {
                        /* No cleanup has failed and none is in progress, yet
                         * the channel is still attached with no request
                         * outstanding: physical teardown is owed. Perform it
                         * (or join the owner that wins the claim) before
                         * deciding. */
                        run_cleanup = true;
                        release_owner = false;
                    }
                } else {
                    /* Cleanup is finished. Adopt its published outcome rather
                     * than the possibly stale status from the earlier logical
                     * Stop. */
                    release_owner = true;
                    if (state.cleanup_complete && !state.cleanup_ok)
                        status = AMS_MEL_PROVIDER_FAILED;
                }
                if (release_owner) state.public_owner_closed = true;
            }
            if (!run_cleanup) break;
            /* Outside the lock: image_stream_cleanup performs its own claim
             * and never runs provider code under CallbackState::mutex. */
            if (image_stream_cleanup(owned->state, false) ==
                ImageCleanupOutcome::Failed)
                status = AMS_MEL_PROVIDER_FAILED;
            /* Re-decide from the now-published cleanup result. */
        }
        if (!release_owner) {
            if (status == AMS_MEL_PROVIDER_FAILED)
                diagnostic("channel detach failed; callback resources retained",
                           out, capacity, required);
            return status;
        }
        *stream = nullptr;
        delete owned;
        return status;
    } catch (const std::bad_alloc&) {
        diagnostic("allocation failed during stream close", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    } catch (...) {
        diagnostic("stream close failed", out, capacity, required);
        return AMS_MEL_INTERNAL_ERROR;
    }
}
