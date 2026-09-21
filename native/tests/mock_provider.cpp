#include <irmel/library/image/ImageChannel.h>
#include <irmel/library/c2/C2Channel.h>
#include <irmel/library/health-status/HealthStatusChannel.h>
#include <irmel/library/instrumentation/InstrumentationChannel.h>
#include <irmel/library/track/TrackChannel.h>
#include <irmel/library/irmel-types/FrameHeader.h>
#include <irmel/library/irmel-types/ImageListener.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <deque>
#include <thread>
#include <vector>

namespace {
using namespace ams::iface;
using irmel::Return;
std::atomic<std::uint64_t> callback_buffers{};
std::atomic<std::uint64_t> buffer_releases{};

std::string long_rejection_description()
{
    return std::string(510U, 'x') + "\xE2\x82\xAC" + std::string(100U, 'y');
}

mel::UCI_ID metadata_id(std::uint8_t seed, std::string label)
{
    std::array<std::uint8_t, mel::UUID_SIZE> uuid{};
    for (std::size_t index = 0; index < uuid.size(); ++index)
        uuid[index] = static_cast<std::uint8_t>(seed + index * 7U);
    return {uuid, std::move(label)};
}

irmel::FrameHeader rich_frame_header()
{
    mel::ForeignKey key{"face-\xCE\xB1", "system-\xE2\x82\xAC"};
    mel::ComponentLocation location{1.25, -2.5, 3.75, key};
    irmel::SensorInertialState first;
    first.setSystemTime(std::chrono::nanoseconds{-101});
    first.setQ_xyzw(irmel::Quaternion{1.0, 2.0, 3.0, 4.0});
    first.setQECEF_xyzw(irmel::Quaternion{5.0, 6.0, 7.0, 8.0});
    first.setSensorPosition(irmel::IR_Directional{9.0, 10.0, 11.0});
    first.setSensorVelocity(irmel::IR_Directional{12.0, 13.0, 14.0});
    first.setUncertainties(irmel::Uncertainty{0x81234567U, 0xfedcba98U});
    irmel::SensorInertialState second;
    second.setSystemTime(std::chrono::nanoseconds{202});
    second.setQ_xyzw(irmel::Quaternion{15.0, 16.0, 17.0, 18.0});
    second.setQECEF_xyzw(irmel::Quaternion{19.0, 20.0, 21.0, 22.0});
    second.setSensorPosition(irmel::IR_Directional{23.0, 24.0, 25.0});
    second.setSensorVelocity(irmel::IR_Directional{26.0, 27.0, 28.0});
    second.setUncertainties(irmel::Uncertainty{29U, 30U});
    irmel::SensorNavState nav;
    nav.setPosition(irmel::IR_Directional{31.0, 32.0, 33.0}); nav.setPositionError({34,35,36,37});
    nav.setVelocity(irmel::IR_Directional{38.0,39.0,40.0}); nav.setVelocityError({41,42,43,44});
    nav.setAccel(irmel::IR_Directional{45.0,46.0,47.0}); nav.setAccelError({48,49,50,51});
    nav.setEulerOrientation(mel::Euler{52,53,54}); nav.setOrientationError({55,56,57,58});
    nav.setQuaternionOrientationVel(irmel::Quaternion{59,60,61,62}); nav.setOrientationVelError({63,64,65,66});
    nav.setEulerOrientationAccel(mel::Euler{67,68,69}); nav.setOrientationAccelError({70,71,72,73});
    nav.setCoordinateSystem(irmel::CoordinateSystemType::NED_PLATFORM);
    irmel::SensorNavState nav2;
    nav2.setPosition(irmel::IR_Directional{74,75,76}); nav2.setPositionError({77,78,79,80});
    nav2.setVelocity(irmel::IR_Directional{81,82,83}); nav2.setVelocityError({84,85,86,87});
    nav2.setAccel(irmel::IR_Directional{88,89,90}); nav2.setAccelError({91,92,93,94});
    nav2.setQuaternionOrientation(irmel::Quaternion{95,96,97,98}); nav2.setOrientationError({99,100,101,102});
    nav2.setEulerOrientationVel(mel::Euler{103,104,105}); nav2.setOrientationVelError({106,107,108,109});
    nav2.setQuaternionOrientationAccel(irmel::Quaternion{110,111,112,113}); nav2.setOrientationAccelError({114,115,116,117});
    nav2.setCoordinateSystem(irmel::CoordinateSystemType::NED_SENSOR);
    return {std::chrono::nanoseconds{-123456789}, std::chrono::nanoseconds{987654321},
        4U, 3U, 8U, 1U, 1.25, 2.5, {location, 0xf1234567U}, irmel::PixelFormat::Mono,
        0xfedcba98U, 17U, 19U, irmel::ImageType::Reserved13, irmel::ImageFlip::Both,
        {irmel::ImageFlag::StareSnapshot, irmel::ImageFlag::ScanFirst,
         irmel::ImageFlag::StareSnapshot, irmel::ImageFlag::ScanLast},
        -0.75, 0.625, 23U, 29U, {first, second}, {nav, nav2}, 31U};
}

mel::BIT_Configuration rich_bit_configuration()
{
    return mel::BIT_Configuration{{
        mel::BIT_Type{metadata_id(0x80U, "BIT-\xCE\xB1"),
            mel::BIT_ControlInterface::SubsystemBITCommand,
            {"sensor", "optical-\xE2\x82\xAC"},
            {metadata_id(0x90U, "component one"), metadata_id(0xa0U, "component-\xCE\xB2")},
            std::chrono::nanoseconds{123456789}},
        mel::BIT_Type{metadata_id(0xf0U, "startup"),
            mel::BIT_ControlInterface::SubsystemInitiated, {}, {},
            std::chrono::nanoseconds{0}}}};
}

mel::BIT_Status rich_bit_status()
{
    std::vector<mel::ActiveBIT> active{
        {metadata_id(0x81U, "active negative"), std::chrono::nanoseconds{-5}, 1.25},
        {metadata_id(0x82U, "active zero"), std::chrono::nanoseconds{0}, 0.0},
        {metadata_id(0x83U, "active positive"), std::chrono::nanoseconds{987654321}, 0.5}};
    std::vector<mel::CompletedBIT> completed{
        {metadata_id(0x91U, "completed pass"), std::chrono::nanoseconds{42},
         mel::BIT_Result::Pass, "", {{"item pass", mel::BIT_Result::Pass, ""},
          {"item-\xCE\xB3", mel::BIT_Result::Fail, "item reason"}}},
        {metadata_id(0x92U, "completed fail"), std::chrono::nanoseconds{-99},
         mel::BIT_Result::Fail, "failure-\xE2\x82\xAC", {}}};
    std::vector<mel::FaultData> data{{"temperature", "101", "integer", "\xC2\xB0" "C"},
                                     {"phase-\xCE\xB4", "bad", "text", ""}};
    std::vector<mel::FaultAmbiguityGroup> groups{
        {{metadata_id(0xb1U, "diagnostic one"), metadata_id(0xb2U, "diagnostic two")},
         {metadata_id(0xc1U, "ambiguous one"), metadata_id(0xc2U, "ambiguous two")}},
        {{metadata_id(0xb3U, "diagnostic three")}, {metadata_id(0xc3U, "ambiguous three")}}};
    std::vector<mel::Fault> faults{{metadata_id(0xd0U, "fault-\xCE\xB6"),
        mel::FaultSeverity::Warning, mel::FaultState::Set, data,
        std::chrono::nanoseconds{-1234567}, "F-42", "overheat-\xE2\x82\xAC",
        {metadata_id(0xe1U, "fault component one"), metadata_id(0xe2U, "fault component two")},
        groups}};
    return {std::move(active), std::move(completed), std::move(faults)};
}

irmel::ChannelCapability rich_channel_capability()
{
    irmel::ChannelCapability value;
    value.setChanID(metadata_id(0x11U, "channel-\xCE\xB1"));
    value.setHeight(1080U); value.setWidth(1920U); value.setBitDepth(12U);
    value.setRowPitch(4096U); value.setBufferSize(8'388'608U);
    value.setImageSize(4'147'200U); value.setNumberOfBands(3U);
    value.setFormat(irmel::PixelFormat::RGB);
    value.setSensorTypes({irmel::SensorType::GIMBAL_HORIZONTAL,
                           irmel::SensorType::STEPSTARE});
    value.setPlatform(metadata_id(0x31U, "platform-\xE2\x82\xAC"));
    mel::ForeignKey key{"sensor-key", "system-\xCE\xB2"};
    value.setSensorLocation(mel::ComponentLocation{1.25, -2.5, 3.75, key});
    value.setChannelTypes({irmel::ChannelType::CommandAndControl,
                            irmel::ChannelType::Instrumentation,
                            irmel::ChannelType::Reserved2});
    value.setTaskScheduleDepth(17U); value.setOdmAvail(true); value.setNucAvail(true);
    value.setChannelMetadataCapabilities({
        irmel::ChannelMetadataCapabilityType::BadPixelList,
        irmel::ChannelMetadataCapabilityType::CommandStatus,
        irmel::ChannelMetadataCapabilityType::ChannelCommsTestRep,
        irmel::ChannelMetadataCapabilityType::Reserved10});
    value.setImageBands({
        {2U, {{irmel::BandType::IR_Longwave, 8.0e-6, 12.0e-6},
              {irmel::BandType::IR_Midwave, 3.0e-6, 5.0e-6}}},
        {9U, {{irmel::BandType::Visible_Red, 620.0e-9, 750.0e-9}}}});
    value.setNavFrames({irmel::CoordinateSystemType::NED_SENSOR,
                        irmel::CoordinateSystemType::ECEF});
    return value;
}

irmel::BadPixelList rich_bad_pixels(std::uint32_t base = 0U)
{
    std::vector<irmel::BadPixel> pixels;
    pixels.emplace_back(1U + base, UINT32_C(0x80000001), irmel::BadPixelReason::Unknown);
    pixels.emplace_back(UINT32_C(0xf0000002), 17U + base, irmel::BadPixelReason::Unknown);
    pixels.emplace_back(123U + base, 456U, irmel::BadPixelReason::Unknown);
    return {UINT32_C(0xa5a50001), UINT32_C(0x5a5a0003), pixels};
}

irmel::LineOfSightReport rich_line_of_sight_report()
{
    irmel::LineOfSightReport value;
    value.setSystemTime(std::chrono::nanoseconds{-123456789});
    value.setPointingAngle({1.25, -2.5});
    value.setPointingAngleRates({0.125, -0.25});
    value.setAtSpeed(true); value.setInTolerance(false);
    value.setPlatformAttitude(mel::Euler{0.5, -0.75, 1.0});
    value.setValidityFlagBitfield(UINT32_C(0xa5a50003)); value.setImageRotation(-1.5);
    return value;
}

irmel::LineOfSightEuler rich_line_of_sight_euler()
{
    irmel::LineOfSightEuler value;
    value.setSystemTime(std::chrono::nanoseconds{987654321});
    value.setAttitude(mel::Euler{-0.5, 1.25, -2.0});
    value.setAttitudeRates(mel::Euler{0.25, -0.125, 0.0625});
    return value;
}

irmel::NavigationReportResp rich_navigation_response()
{
    irmel::NavigationReportResp value;
    value.setSystemTime(std::chrono::nanoseconds{-8765432109LL});
    value.setCommandID(0xf1234567U);
    value.setReqId(0x89abcdefU);
    return value;
}

bool navigation_report_matches(const mel::NavigationReport& value)
{
    const auto& rate = value.getAttitudeRate();
    const auto& attitude = value.getAttitude();
    const auto& speed = value.getSpeed();
    const auto& acceleration = value.getAcceleration();
    const auto& covariance = value.getPositionVelocityCovarianceUncertainty();
    return value.getSystemTime().count() == -123456789012LL &&
           value.getState() == mel::PositionSolutionState::Blended &&
           value.getLatitude() == 0.523598 && value.getLongitude() == -1.308997 &&
           value.getAltitude() == 987.5 &&
           attitude.getRoll() == 0.1 && attitude.getPitch() == 0.2 && attitude.getYaw() == 0.3 &&
           rate.getAttitudeRate().getRoll() == 0.11 && rate.getAttitudeRate().getPitch() == -0.22 &&
           rate.getAttitudeRate().getYaw() == 0.33 &&
           rate.getAttitudeRateTime().count() == -424242 &&
           speed.getNorth() == 10.0 && speed.getEast() == -20.0 && speed.getDown() == 30.0 &&
           acceleration.getNorth() == -1.0 && acceleration.getEast() == 2.0 &&
           acceleration.getDown() == -3.0 &&
           value.getWanderAngle() == 0.05 && value.getMagneticHeading() == 12.5 &&
           value.getAltitudeMSL() == 1000.25 &&
           covariance.getPositionPositionPnPn() == 1.5 &&
           covariance.getPositionPositionPnPe() == 2.5 &&
           covariance.getPositionPositionPnPd() == 3.5 &&
           covariance.getPositionPositionPePe() == 4.5 &&
           covariance.getPositionPositionPePd() == 5.5 &&
           covariance.getPositionPositionPdPd() == 6.5 &&
           covariance.getPositionVelocityPnVn() == 7.5 &&
           covariance.getPositionVelocityPnVe() == 8.5 &&
           covariance.getPositionVelocityPnVd() == 9.5 &&
           covariance.getPositionVelocityPeVe() == 10.5 &&
           covariance.getPositionVelocityPeVd() == 11.5 &&
           covariance.getPositionVelocityPdVd() == 12.5 &&
           covariance.getVelocityVelocityVnVn() == 13.5 &&
           covariance.getVelocityVelocityVnVe() == 14.5 &&
           covariance.getVelocityVelocityVnVd() == 15.5 &&
           covariance.getVelocityVelocityVeVe() == 16.5 &&
           covariance.getVelocityVelocityVeVd() == 17.5 &&
           covariance.getVelocityVelocityVdVd() == 18.5;
}

struct CallbackBarrier {
    std::mutex mutex;
    std::condition_variable ready;
    bool callback_inside{false};
    bool disable_called{false};
    bool capability_called{false};
    bool callback_returned{false};
};
std::shared_ptr<CallbackBarrier> callback_barrier = std::make_shared<CallbackBarrier>();

void record(const char *event)
{
    if (const char *path = std::getenv("AMS_MEL_TEST_LIFETIME_LOG")) {
        std::ofstream stream(path, std::ios::app);
        stream << event << '\n';
    }
}

bool file_exists(const std::string& path)
{
    return std::ifstream{path}.good();
}

void wait_for_file(const std::string& path)
{
    for (unsigned attempt = 0; attempt < 5000U; ++attempt) {
        if (file_exists(path)) return;
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    throw std::runtime_error("mock metadata callback barrier timed out");
}

void create_file(const std::string& path)
{
    std::ofstream marker{path};
    marker << "release\n";
}

class DiagnosticAllocationFailure final : public std::exception {
public:
    ~DiagnosticAllocationFailure() override { (void)unsetenv("AMS_MEL_TEST_FAIL_ALLOCATION"); }
    const char *what() const noexcept override
    { return "provider exception text intentionally longer than small-string optimization"; }
};
class InvalidDiagnostic final : public std::exception {
public:
    const char *what() const noexcept override { return "invalid \xC3\x28 diagnostic"; }
};
class MockManager final : public API_Manager {
public:
    ~MockManager() override { record("manager_destroyed"); }
};

/* Deterministic provider-side buffer pool, modelled directly on pinned Squall
 * b1015728f904c799fa0c07489fce48e78f67845f
 * interfaces/squall-ir-mel-impl/src/SquallImageChannel.cc:
 *
 *   - the callback removes a registered buffer from available_buffers before
 *     generating the image callback, and drops the frame when the pool is
 *     empty;
 *   - a successful release() pushes the registered buffer back onto
 *     available_buffers, making it reusable;
 *   - the pool is protected by a mutex;
 *   - release is one-shot per checkout;
 *   - channel destruction sets accepting_releases = false and release fails.
 *
 * This is what makes Task 030B backpressure observable without sleeping: the
 * test can assert exactly how many buffers are back in the pool and can gate
 * each production cycle on an explicit condition variable. */
struct BufferPool {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<std::shared_ptr<irmel::Buffer>> available;
    bool accepting_releases{true};
    /* Test-visible history. */
    unsigned produced{};   /* frames actually handed to the listener */
    unsigned starved{};    /* production cycles that found an empty pool */
    unsigned requested{};  /* production cycles the test asked for */
    unsigned completed{};  /* production cycles finished */
};

/* The most recently created pooled channel, so the deterministic backpressure
 * test can observe and drive the provider directly. Test-only. */
std::mutex active_pool_mutex;
/* Weak, so observing the pool from a test never extends provider-side buffer
 * or channel lifetime and cannot mask a genuine teardown-ordering defect. */
std::weak_ptr<BufferPool> active_pool;

class MockBuffer final : public irmel::Buffer {
public:
    MockBuffer(std::string scenario, std::shared_ptr<CallbackBarrier> barrier)
        : scenario_{std::move(scenario)}, barrier_{std::move(barrier)} {}
    ~MockBuffer() override
    {
        if (outstanding_) std::abort();
        record("buffer_destroyed");
    }
    Return init(void *address, std::size_t size, std::int64_t context) override
    {
        record("buffer_initialized");
        address_ = address; size_ = size; context_ = context;
        return address && size ? Return::Success : Return::BadPointer;
    }
    void *getBufferAddress() const override { return address_; }
    void *getImageAddress() const override
    {
        if (scenario_ == "null-image") return nullptr;
        if (scenario_ == "nonquiescing-disable" ||
            scenario_ == "release-fail-blocked") {
            std::unique_lock lock{barrier_->mutex};
            barrier_->callback_inside = true;
            barrier_->ready.notify_all();
            barrier_->ready.wait(lock, [this] { return barrier_->disable_called; });
            if (scenario_ == "release-fail-blocked") return nullptr;
        }
        const auto numeric = reinterpret_cast<std::uintptr_t>(address_);
        if (scenario_ == "image-before")
            return reinterpret_cast<void *>(numeric - 1U);
        if (scenario_ == "image-after") {
            if (size_ >= std::numeric_limits<std::uintptr_t>::max() - numeric)
                return reinterpret_cast<void *>(numeric - 1U);
            return reinterpret_cast<void *>(numeric + size_ + 1U);
        }
        return address_;
    }
    std::int64_t getSize() const override { return static_cast<std::int64_t>(size_); }
    std::int64_t getContext() const override { return context_; }
    /* Squall-shaped release: one-shot per checkout, and on success the buffer
     * goes back into the channel's available pool so it can be reused. A
     * failed release deliberately does NOT requeue, exactly as pinned Squall's
     * RequeueBuffer::release() does not requeue when it returns Fail. */
    Return release() override
    {
        if (!outstanding_.exchange(false)) std::abort();
        ++buffer_releases;
        record("buffer_released");
        if (scenario_ == "release-fail" || scenario_ == "release-fail-blocked")
            return Return::Fail;
        if (scenario_ == "release-throw")
            throw std::runtime_error("mock release exception");
        if (auto pool = pool_.lock()) {
            std::lock_guard lock{pool->mutex};
            if (!pool->accepting_releases) {
                record("release_after_channel_destroyed");
                return Return::Fail;
            }
            pool->available.push_back(self_.lock());
            record("buffer_requeued");
            pool->ready.notify_all();
        }
        return Return::Success;
    }
    void attach_pool(const std::shared_ptr<BufferPool>& pool,
                     const std::shared_ptr<irmel::Buffer>& self)
    { pool_ = pool; self_ = self; }
    Return getFlags(std::vector<irmel::BufferFlag>& out) const override
    { out = flags_; return Return::Success; }
    void addFlag(irmel::BufferFlag flag) override { flags_.push_back(flag); }
    void setFlags(const std::vector<irmel::BufferFlag>& flags) override { flags_ = flags; }
    unsigned char *data() const { return static_cast<unsigned char *>(address_); }
    std::size_t size() const { return size_; }
    void begin_callback()
    {
        if (outstanding_.exchange(true)) std::abort();
    }
private:
    std::string scenario_;
    std::shared_ptr<CallbackBarrier> barrier_;
    void *address_{};
    std::size_t size_{};
    std::int64_t context_{};
    std::vector<irmel::BufferFlag> flags_;
    std::atomic<bool> outstanding_{false};
    /* Weak so the pool and the buffer cannot form an ownership cycle. */
    std::weak_ptr<BufferPool> pool_;
    std::weak_ptr<irmel::Buffer> self_;
};

#define UNSUPPORTED_CALLBACK(Type) \
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const Type *const)>) override \
    { return Return::NotSupported; }

class MockImageChannel final : public irmel::ImageChannel {
public:
    MockImageChannel(std::string scenario, std::shared_ptr<irmel::ImageListener> listener)
        : scenario_{std::move(scenario)}, listener_{std::move(listener)},
          barrier_{callback_barrier}
    {
        std::lock_guard lock{active_pool_mutex};
        active_pool = pool_;
    }
    ~MockImageChannel() override
    {
        stopping_ = true;
        if (scenario_ == "image-metadata-navigation-register-fail" ||
            scenario_ == "image-metadata-navigation-register-throw") {
            if (!bad_pixel_callback_) std::abort();
            record("retained_callback_invoked");
            auto value = rich_bad_pixels();
            bad_pixel_callback_(*this, &value);
            record("retained_callback_returned");
        }
        if (scenario_ == "image-metadata-nonquiescing") {
            const char *base = std::getenv("AMS_MEL_TEST_IMAGE_METADATA_CALLBACK_BARRIER");
            if (!base) std::abort();
            create_file(std::string{base} + ".release");
        }
        {
            std::lock_guard lock{barrier_->mutex};
            barrier_->disable_called = true;
            barrier_->ready.notify_all();
        }
        {
            std::lock_guard lock{mutex_};
            release_ = true;
        }
        ready_.notify_all();
        pool_->ready.notify_all();
        if (producer_.joinable()) producer_.join();
        if (metadata_producer_.joinable()) metadata_producer_.join();
        if (navigation_producer_.joinable()) navigation_producer_.join();
        record("callbacks_quiesced_by_channel_destruction");
        /* Pinned Squall's ~SquallImageChannel stops accepting releases here;
         * mirroring that is what makes "release after channel destruction
         * fails" a reproducible mock behavior rather than an assumption. */
        {
            std::lock_guard lock{pool_->mutex};
            pool_->accepting_releases = false;
            /* Drop the pool's Buffer references here so channel destruction
             * remains the point at which provider-side buffer ownership ends,
             * exactly as before Task 030B. */
            pool_->available.clear();
            pool_->ready.notify_all();
        }
        buffers_.clear();
        record("channel_destroyed");
    }
    mel::RequestFor<Return> sendKeepAliveRep() override { return {}; }
    mel::RequestFor<irmel::ChannelCommsTestRep> send(irmel::ChannelCommsTestReq) override { return {}; }
    mel::RequestFor<irmel::CameraCommandResp> send(irmel::CameraCommand) override { return {}; }
    mel::RequestFor<irmel::NavigationReportResp> send(mel::NavigationReport report) override
    {
        record("navigation_sent");
        if (scenario_ == "navigation-fidelity" && !navigation_report_matches(report))
            throw std::runtime_error("NavigationReport conversion mismatch");
        if (scenario_ == "navigation-send-throw") throw std::runtime_error("mock navigation send exception");
        std::promise<mel::ErrorOr<std::shared_ptr<irmel::NavigationReportResp>>> promise;
        auto future = promise.get_future();
        if (scenario_ == "navigation-reject") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::NavigationReportResp>>{
                mel::Error{mel::ErrorCode::InvalidParameters, "invalid navigation report"}});
        } else if (scenario_ == "navigation-reject-long") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::NavigationReportResp>>{
                mel::Error{mel::ErrorCode::InvalidParameters, long_rejection_description()}});
        } else if (scenario_ == "navigation-null-result") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::NavigationReportResp>>{
                std::shared_ptr<irmel::NavigationReportResp>{}});
        } else if (scenario_ == "navigation-future-throw") {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error{"mock navigation future exception"}));
        } else if (scenario_ == "navigation-sync" || scenario_ == "navigation-sync-two") {
            auto value = rich_navigation_response();
            if (navigation_response_callback_) navigation_response_callback_(*this, &value);
            record("navigation_completed");
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::NavigationReportResp>>{
                std::make_shared<irmel::NavigationReportResp>(value)});
        } else if ((scenario_ == "navigation-hold" ||
                    scenario_ == "navigation-hold-detach-fail") &&
                   !navigation_producer_.joinable()) {
            /* Deterministic barrier-driven request. The request stays pending
               until the test explicitly releases it, so logical-stop behavior
               is observable while physical teardown is still deferred. If a
               NavigationReportResp metadata callback is registered it is
               invoked after the release barrier, i.e. after the test has
               performed a logical Stop, proving post-Stop provider callbacks
               remain safe but cannot enqueue. */
            navigation_producer_ = std::thread{[this, promise = std::move(promise)]() mutable {
                const char *base = std::getenv("AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER");
                if (!base) std::abort();
                const std::string prefix{base};
                wait_for_file(prefix + ".release");
                if (navigation_response_callback_) {
                    record("post_stop_metadata_callback_entered");
                    auto value = rich_navigation_response();
                    navigation_response_callback_(*this, &value);
                    record("post_stop_metadata_callback_returned");
                }
                create_file(prefix + ".callback-done");
                wait_for_file(prefix + ".complete");
                record("navigation_completed");
                promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::NavigationReportResp>>{
                    std::make_shared<irmel::NavigationReportResp>(rich_navigation_response())});
            }};
        } else if ((scenario_ == "navigation-delayed" || scenario_ == "navigation-lifetime" ||
                    scenario_ == "navigation-pending-close") && !navigation_producer_.joinable()) {
            navigation_producer_ = std::thread{[this, promise = std::move(promise)]() mutable {
                std::unique_lock lock{mutex_};
                (void)ready_.wait_for(lock, std::chrono::milliseconds{40},
                                      [this] { return release_; });
                lock.unlock();
                record("navigation_completed");
                promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::NavigationReportResp>>{
                    std::make_shared<irmel::NavigationReportResp>(rich_navigation_response())});
            }};
        } else {
            record("navigation_completed");
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::NavigationReportResp>>{
                std::make_shared<irmel::NavigationReportResp>(rich_navigation_response())});
        }
        return future;
    }
    Return registerBuffer(std::shared_ptr<irmel::Buffer> buffer) override
    {
        record("buffer_registered");
        if (scenario_ == "register-fail") return Return::Fail;
        /* Squall-shaped: every registered buffer starts in the available
         * pool, and a buffer handed to the listener leaves the pool until it
         * is released. */
        if (auto mock = std::dynamic_pointer_cast<MockBuffer>(buffer))
            mock->attach_pool(pool_, buffer);
        {
            std::lock_guard lock{pool_->mutex};
            pool_->available.push_back(buffer);
        }
        buffers_.push_back(std::move(buffer)); return Return::Success;
    }
    Return unregisterBuffer(std::shared_ptr<irmel::Buffer> buffer) override
    {
        if (producer_.joinable()) std::abort();
        record("buffer_unregistered");
        const auto it = std::find(buffers_.begin(), buffers_.end(), buffer);
        if (it == buffers_.end()) return Return::BadPointer;
        buffers_.erase(it); return Return::Success;
    }
    Return enable() override
    {
        record("channel_enabled");
        if (scenario_ == "enable-fail") return Return::Fail;
        stopping_ = false;
        producer_ = std::thread([this] { produce(); });
        if (scenario_ == "nonquiescing-disable" ||
            scenario_ == "release-fail-blocked") {
            std::unique_lock lock{barrier_->mutex};
            barrier_->ready.wait(lock, [this] { return barrier_->callback_inside; });
        }
        return Return::Success;
    }
    Return disable() override
    {
        if (!producer_.joinable()) return Return::Success;
        record("channel_disabled"); stopping_ = true;
        pool_->ready.notify_all();
        if (scenario_ == "nonquiescing-disable" ||
            scenario_ == "release-fail-blocked") {
            std::unique_lock lock{barrier_->mutex};
            barrier_->ready.wait(lock, [this] { return barrier_->callback_inside; });
            barrier_->disable_called = true;
            barrier_->ready.notify_all();
            record("disable_returned_with_callback_active");
            return Return::Success;
        }
        if (scenario_ == "disable-fail") return Return::Fail;
        producer_.join();
        record("callbacks_quiesced"); return Return::Success;
    }
    irmel::ChannelCapability getCapabilities() const override
    {
        if (scenario_ == "capability-inflight" && capability_calls_++ != 0U) {
            std::unique_lock lock{barrier_->mutex};
            barrier_->capability_called = true;
            barrier_->ready.notify_all();
            barrier_->ready.wait(lock, [this] { return barrier_->callback_returned; });
        }
        if (scenario_ == "capability-throw")
            throw std::runtime_error("mock capability exception");
        if (scenario_ == "image-capability-rich") {
            irmel::ChannelCapability capability;
            capability.setHeight(200U); capability.setWidth(320U); capability.setBitDepth(8U);
            capability.setNumberOfBands(1U); capability.setFormat(irmel::PixelFormat::Mono);
            capability.setChannelTypes({irmel::ChannelType::IRSTImage});
            capability.setChannelMetadataCapabilities(
                {irmel::ChannelMetadataCapabilityType::BadPixelList,
                 irmel::ChannelMetadataCapabilityType::LineOfSightReport,
                 irmel::ChannelMetadataCapabilityType::LineOfSightEuler,
                 irmel::ChannelMetadataCapabilityType::NavigationReportResp});
            return capability;
        }
        irmel::ChannelCapability capability;
        capability.setFormat(scenario_ == "capability-format" ?
                             irmel::PixelFormat::RGB : irmel::PixelFormat::Mono);
        capability.setBitDepth(scenario_ == "capability-depth" ? 16U : 8U);
        capability.setNumberOfBands(scenario_ == "capability-bands" ? 2U : 1U);
        return capability;
    }
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const irmel::ChannelCommsTestRep *const)>) override
    { return Return::NotSupported; }
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&, const irmel::BadPixelList *const)> callback) override
    {
        ++bad_pixel_registration_count_;
        record("bad_pixel_registration_attempted");
        bad_pixel_callback_ = std::move(callback);
        if (scenario_ == "image-metadata-register-blocked") {
            const char *base = std::getenv("AMS_MEL_TEST_IMAGE_METADATA_REGISTER_BARRIER");
            if (!base) std::abort();
            create_file(std::string{base} + ".entered");
            wait_for_file(std::string{base} + ".release");
        }
        if (scenario_ == "image-metadata-register-fail") return Return::Fail;
        if (scenario_ == "image-metadata-register-not-supported") return Return::NotSupported;
        if (scenario_ == "image-metadata-register-throw")
            throw std::runtime_error("BadPixel registration exception");
        if (scenario_ == "image-metadata-sync" || scenario_ == "image-metadata-rich" ||
            scenario_ == "image-metadata-los-mixed" || scenario_ == "image-metadata-four-mixed") {
            auto value = rich_bad_pixels(); bad_pixel_callback_(*this, &value);
        } else if (scenario_ == "image-metadata-malformed") {
            std::vector<irmel::BadPixel> invalid;
            invalid.emplace_back(1U, 2U, static_cast<irmel::BadPixelReason>(99U));
            irmel::BadPixelList bad{1U, 1U, invalid}; bad_pixel_callback_(*this, &bad);
            auto valid = rich_bad_pixels(); bad_pixel_callback_(*this, &valid);
        } else if (scenario_ == "image-metadata-overflow") {
            for (std::uint32_t i = 0; i < 5U; ++i) {
                auto value = rich_bad_pixels(i); bad_pixel_callback_(*this, &value);
            }
        } else if (scenario_ == "image-metadata-allocation") {
            auto failed = rich_bad_pixels(); bad_pixel_callback_(*this, &failed);
            auto valid = rich_bad_pixels(9U); bad_pixel_callback_(*this, &valid);
        } else if (scenario_ == "image-metadata-nonquiescing") {
            metadata_producer_ = std::thread{[this] {
                const char *base = std::getenv("AMS_MEL_TEST_IMAGE_METADATA_CALLBACK_BARRIER");
                if (!base) std::abort();
                wait_for_file(std::string{base} + ".start");
                record("image_metadata_callback_entered");
                create_file(std::string{base} + ".entered");
                wait_for_file(std::string{base} + ".release");
                auto value = rich_bad_pixels();
                bad_pixel_callback_(*this, &value);
                record("image_metadata_callback_returned");
            }};
        }
        return Return::Success;
    }
    UNSUPPORTED_CALLBACK(irmel::OpticalDistortionMap)
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&, const irmel::LineOfSightReport *const)> callback) override
    {
        record("line_of_sight_report_registration_attempted");
        line_of_sight_report_callback_ = std::move(callback);
        if (scenario_ == "image-metadata-report-register-fail") {
            auto value = rich_bad_pixels(); bad_pixel_callback_(*this, &value);
            return Return::Fail;
        }
        if (scenario_ == "image-metadata-report-register-throw")
            throw std::runtime_error("LineOfSightReport registration exception");
        if (scenario_ == "image-metadata-los-rich" || scenario_ == "image-metadata-los-mixed" ||
            scenario_ == "image-metadata-four-mixed") {
            auto value = rich_line_of_sight_report(); line_of_sight_report_callback_(*this, &value);
        } else if (scenario_ == "image-metadata-los-report-null") {
            line_of_sight_report_callback_(*this, nullptr);
            auto value = rich_line_of_sight_report(); line_of_sight_report_callback_(*this, &value);
        } else if (scenario_ == "image-metadata-los-report-allocation") {
            auto failed = rich_line_of_sight_report(); line_of_sight_report_callback_(*this, &failed);
            auto valid = rich_line_of_sight_report(); line_of_sight_report_callback_(*this, &valid);
        }
        return Return::Success;
    }
    UNSUPPORTED_CALLBACK(irmel::LineOfSightQuaternion)
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&, const irmel::LineOfSightEuler *const)> callback) override
    {
        record("line_of_sight_euler_registration_attempted");
        line_of_sight_euler_callback_ = std::move(callback);
        if (scenario_ == "image-metadata-euler-register-fail") {
            auto value = rich_line_of_sight_report(); line_of_sight_report_callback_(*this, &value);
            return Return::Fail;
        }
        if (scenario_ == "image-metadata-euler-register-throw")
            throw std::runtime_error("LineOfSightEuler registration exception");
        if (scenario_ == "image-metadata-los-rich") {
            auto value = rich_line_of_sight_euler(); line_of_sight_euler_callback_(*this, &value);
        } else if (scenario_ == "image-metadata-los-euler-null") {
            line_of_sight_euler_callback_(*this, nullptr);
            auto value = rich_line_of_sight_euler(); line_of_sight_euler_callback_(*this, &value);
        } else if (scenario_ == "image-metadata-los-mixed") {
            auto euler = rich_line_of_sight_euler(); line_of_sight_euler_callback_(*this, &euler);
            auto bad = rich_bad_pixels(1U); bad_pixel_callback_(*this, &bad);
            auto report = rich_line_of_sight_report(); line_of_sight_report_callback_(*this, &report);
        } else if (scenario_ == "image-metadata-four-mixed") {
            auto value = rich_line_of_sight_euler(); line_of_sight_euler_callback_(*this, &value);
        }
        return Return::Success;
    }
    UNSUPPORTED_CALLBACK(irmel::CameraCommandResp)
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&, const irmel::NavigationReportResp *const)> callback) override
    {
        record("navigation_response_registration_attempted");
        navigation_response_callback_ = std::move(callback);
        if (scenario_ == "image-metadata-navigation-register-fail") {
            auto value = rich_bad_pixels(); bad_pixel_callback_(*this, &value);
            record("navigation_registration_failed");
            return Return::Fail;
        }
        if (scenario_ == "image-metadata-navigation-register-throw") {
            auto value = rich_bad_pixels(); bad_pixel_callback_(*this, &value);
            record("navigation_registration_failed");
            throw std::runtime_error("NavigationReportResp registration exception");
        }
        if (scenario_ == "image-metadata-navigation-sync" || scenario_ == "image-metadata-navigation-rich") {
            auto value = rich_navigation_response(); navigation_response_callback_(*this, &value);
        } else if (scenario_ == "navigation-hold") {
            /* Two registration-time events: the deferred-stop test drains one
               before submitting and keeps the other queued across a logical
               Stop to prove already-queued events still drain. */
            auto value = rich_navigation_response();
            navigation_response_callback_(*this, &value);
            navigation_response_callback_(*this, &value);
        } else if (scenario_ == "image-metadata-navigation-null") {
            navigation_response_callback_(*this, nullptr);
            auto value = rich_navigation_response(); navigation_response_callback_(*this, &value);
        } else if (scenario_ == "image-metadata-navigation-allocation") {
            auto failed = rich_navigation_response(); navigation_response_callback_(*this, &failed);
            auto valid = rich_navigation_response(); navigation_response_callback_(*this, &valid);
        } else if (scenario_ == "image-metadata-four-mixed") {
            auto value = rich_navigation_response(); navigation_response_callback_(*this, &value);
            auto bad = rich_bad_pixels(1U); bad_pixel_callback_(*this, &bad);
            navigation_response_callback_(*this, &value);
        }
        return Return::Success;
    }
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const irmel::LOS3D_KinematicsType *const)> const&) override
    { return Return::NotSupported; }
    UNSUPPORTED_CALLBACK(irmel::CandidateObjectMessage)
    UNSUPPORTED_CALLBACK(irmel::CandidateObjectPreProcMessage)
    UNSUPPORTED_CALLBACK(irmel::NUC_TempData)
private:
    /* Deterministic backpressure scenarios drive production explicitly from
     * the test instead of free-running, so buffer reuse can be asserted with
     * counters and condition variables rather than sleeps. */
    bool pooled() const { return scenario_.rfind("lease-pool", 0U) == 0U; }

    /* Squall-shaped production cycle: check a buffer out of the pool, or
     * record starvation and drop the frame when the pool is empty. */
    void produce_pooled()
    {
        for (;;) {
            std::unique_lock lock{pool_->mutex};
            /* Timed rather than indefinite: stopping_ is an atomic set outside
             * this mutex, so a purely notification-driven wait could miss the
             * transition and stall producer_.join() during teardown. */
            (void)pool_->ready.wait_for(lock, std::chrono::milliseconds{10}, [this] {
                return stopping_.load() || pool_->requested > pool_->completed;
            });
            if (stopping_.load() && pool_->requested <= pool_->completed) return;
            if (pool_->requested <= pool_->completed) continue;
            std::shared_ptr<irmel::Buffer> underlying;
            if (pool_->available.empty()) {
                /* Provider-level backpressure: no reusable buffer exists, so
                 * the provider drops the frame. This is deliberately NOT
                 * reported to the bridge as a callback, so it must never be
                 * counted as a bridge queue-full drop. */
                ++pool_->starved;
                record("provider_dropped_frame_no_buffer");
                ++pool_->completed;
                pool_->ready.notify_all();
                continue;
            }
            underlying = pool_->available.front();
            pool_->available.pop_front();
            const unsigned id = ++pool_->produced;
            lock.unlock();

            auto mock = std::dynamic_pointer_cast<MockBuffer>(underlying);
            if (mock && mock->size() >= 12U)
                for (std::size_t i = 0; i < 12U; ++i)
                    mock->data()[i] = static_cast<unsigned char>(id * 16U + i);
            irmel::FrameHeader header{std::chrono::nanoseconds{1'000'000 + id},
                std::chrono::nanoseconds{20'000 + id}, 4U, 3U, 8U, 1U, 0.25, 0.125, {},
                irmel::PixelFormat::Mono, id, 2U, 4U, irmel::ImageType::Staring,
                irmel::ImageFlip::Horizontal, {irmel::ImageFlag::StareSnapshot},
                0.5, -0.25, 7U, 9U, {}, {}, 3U};
            record("callback_entered");
            if (mock) mock->begin_callback();
            ++callback_buffers;
            listener_->onImage(*this, header, underlying);
            record("callback_returned");

            std::lock_guard done{pool_->mutex};
            ++pool_->completed;
            pool_->ready.notify_all();
        }
    }

    void produce()
    {
        if (pooled()) { produce_pooled(); return; }
        const unsigned count = (scenario_ == "idle" ||
            (scenario_.rfind("c2-", 0U) == 0U && scenario_ != "c2-coexist")) ? 0U :
            (scenario_ == "overflow" ? 20U : 3U);
        for (unsigned id = 1; id <= count; ++id) {
            if (stopping_ && scenario_ != "shutdown-callback") break;
            if (buffers_.empty()) break;
            /* Task 030B: buffers are genuinely checked out now, so a producer
             * can no longer round-robin a buffer that a lease still holds.
             * Wait for a reusable buffer, exactly as a real pooled provider
             * would, and give up if teardown starts first. */
            std::shared_ptr<irmel::Buffer> checked_out;
            {
                /* Bounded in short slices rather than one long wait, so a
                 * concurrent disable()/destructor that sets stopping_ is
                 * observed promptly and producer_.join() cannot stall behind
                 * a single long timeout on a loaded machine. */
                std::unique_lock lock{pool_->mutex};
                for (unsigned slice = 0; slice < 500U; ++slice) {
                    if (!pool_->available.empty()) break;
                    if (stopping_.load() && scenario_ != "shutdown-callback") break;
                    (void)pool_->ready.wait_for(lock, std::chrono::milliseconds{10}, [this] {
                        return !pool_->available.empty() ||
                               (stopping_.load() && scenario_ != "shutdown-callback");
                    });
                }
                if (pool_->available.empty()) break;
                checked_out = pool_->available.front();
                pool_->available.pop_front();
            }
            auto buffer = std::dynamic_pointer_cast<MockBuffer>(checked_out);
            if (!buffer || buffer->size() < 12U) break;
            for (std::size_t i = 0; i < 12U; ++i)
                buffer->data()[i] = static_cast<unsigned char>(id * 16U + i);
            if (scenario_ == "full-frame-rich")
                for (std::size_t i = 0; i < 12U; ++i) buffer->data()[i] = static_cast<unsigned char>(0xa0U + i);
            std::uint32_t width = 4U, height = 3U, bpp = 8U, bands = 1U;
            auto format = irmel::PixelFormat::Mono;
            if (scenario_ == "invalid-dimensions") width = 0U;
            if (scenario_ == "release-throw") width = 0U;
            if (scenario_ == "overflow-dimensions") { width = UINT32_MAX; height = UINT32_MAX; }
            if (scenario_ == "unsupported-bpp") bpp = 16U;
            if (scenario_ == "unsupported-bands") bands = 2U;
            if (scenario_ == "unsupported-format") format = irmel::PixelFormat::RGB;
            irmel::FrameHeader header{std::chrono::nanoseconds{1'000'000 + id},
                std::chrono::nanoseconds{20'000 + id}, width, height, bpp, bands,
                0.25, 0.125, {}, format, id, 2U, 4U, irmel::ImageType::Staring,
                irmel::ImageFlip::Horizontal, {irmel::ImageFlag::StareSnapshot},
                0.5, -0.25, 7U, 9U, {}, {}, 3U};
            if (scenario_ == "full-frame-rich") header = rich_frame_header();
            if (id == 1U && scenario_ == "malformed-image-type")
                header.setImageType(static_cast<irmel::ImageType>(99U));
            if (id == 1U && scenario_ == "malformed-image-flip")
                header.setImageFlip(static_cast<irmel::ImageFlip>(99U));
            if (id == 1U && scenario_ == "malformed-image-flag")
                header.setFlags({static_cast<irmel::ImageFlag>(99U)});
            if (id == 1U && scenario_ == "malformed-coordinate") {
                irmel::SensorNavState bad;
                bad.setCoordinateSystem(static_cast<irmel::CoordinateSystemType>(99U));
                header.setSensorNavState({bad});
            }
            if (id == 1U && scenario_ == "frame-copy-allocation")
                (void)setenv("AMS_MEL_TEST_FRAME_COPY_FAILURE", "allocation", 1);
            record("callback_entered");
            if (scenario_ == "capability-inflight") {
                std::unique_lock lock{barrier_->mutex};
                barrier_->ready.wait(lock, [this] { return barrier_->capability_called; });
            }
            if (scenario_ == "shutdown-callback")
                std::this_thread::sleep_for(std::chrono::milliseconds{20});
            if (scenario_ == "null-buffer") {
                listener_->onImage(*this, header, {});
                /* No Buffer was handed over, so nothing will ever release it;
                 * return it to the pool directly. */
                std::lock_guard lock{pool_->mutex};
                pool_->available.push_back(checked_out);
                pool_->ready.notify_all();
            } else {
                buffer->begin_callback();
                ++callback_buffers;
                listener_->onImage(*this, header, buffer);
            }
            record("callback_returned");
            if (scenario_ == "capability-inflight") {
                std::lock_guard lock{barrier_->mutex};
                barrier_->callback_returned = true;
                barrier_->ready.notify_all();
            }
            if (scenario_ == "shutdown-callback" && stopping_) break;
            if (scenario_ != "overflow") {
                std::this_thread::sleep_for(std::chrono::milliseconds{2});
            }
        }
    }
    std::string scenario_;
    std::shared_ptr<irmel::ImageListener> listener_;
    std::vector<std::shared_ptr<irmel::Buffer>> buffers_;
    std::atomic<bool> stopping_{false};
    std::thread producer_;
    std::thread metadata_producer_;
    std::thread navigation_producer_;
    std::mutex mutex_;
    std::condition_variable ready_;
    bool release_{};
    std::shared_ptr<CallbackBarrier> barrier_;
    std::shared_ptr<BufferPool> pool_{std::make_shared<BufferPool>()};
    mutable unsigned capability_calls_{};
    unsigned bad_pixel_registration_count_{};
    std::function<void(irmel::Channel&, const irmel::BadPixelList *const)> bad_pixel_callback_;
    std::function<void(irmel::Channel&, const irmel::LineOfSightReport *const)> line_of_sight_report_callback_;
    std::function<void(irmel::Channel&, const irmel::LineOfSightEuler *const)> line_of_sight_euler_callback_;
    std::function<void(irmel::Channel&, const irmel::NavigationReportResp *const)> navigation_response_callback_;
};
#undef UNSUPPORTED_CALLBACK

#define C2_UNSUPPORTED_CALLBACK(Type) \
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const Type *const)>) override \
    { return Return::NotSupported; }

template<typename T>
mel::RequestFor<T> unsupported_request()
{
    std::promise<mel::ErrorOr<std::shared_ptr<T>>> promise;
    promise.set_value(mel::ErrorOr<std::shared_ptr<T>>{
        mel::Error{mel::ErrorCode::Unsupported}});
    return promise.get_future();
}

class MockC2Channel final : public irmel::C2Channel {
public:
    explicit MockC2Channel(std::string scenario) : scenario_{std::move(scenario)} {}
    ~MockC2Channel() override
    {
        if (scenario_ == "comms-register-retain-fail" && comms_callback_) {
            irmel::ChannelCommsTestRep report{91U, 92U};
            record("retained_comms_callback_invoked");
            comms_callback_(*this, &report);
        }
        if (scenario_ == "metadata-nonquiescing-disable") {
            const char *base = std::getenv("AMS_MEL_TEST_METADATA_CALLBACK_BARRIER");
            if (!base) std::abort();
            create_file(std::string{base} + ".release");
        }
        {
            std::lock_guard lock{mutex_};
            release_ = true;
        }
        ready_.notify_all();
        if (producer_.joinable()) producer_.join();
        record("c2_channel_destroyed");
    }
    mel::RequestFor<Return> sendKeepAliveRep() override
    {
        record("keepalive_sent");
        if (scenario_ == "keepalive-send-throw") throw std::runtime_error("mock keepalive send exception");
        std::promise<mel::ErrorOr<std::shared_ptr<Return>>> promise;
        auto future = promise.get_future();
        if (scenario_ == "keepalive-fail") promise.set_value(
            mel::ErrorOr<std::shared_ptr<Return>>{std::make_shared<Return>(Return::Fail)});
        else if (scenario_ == "keepalive-reject") promise.set_value(
            mel::ErrorOr<std::shared_ptr<Return>>{mel::Error{mel::ErrorCode::InvalidState,
                long_rejection_description()}});
        else if (scenario_ == "keepalive-null") promise.set_value(
            mel::ErrorOr<std::shared_ptr<Return>>{std::shared_ptr<Return>{}});
        else if (scenario_ == "keepalive-future-throw") promise.set_exception(std::make_exception_ptr(std::runtime_error{"mock keepalive future exception"}));
        else if (scenario_ == "keepalive-delayed" || scenario_ == "keepalive-lifetime") {
            producer_ = std::thread{[this, promise = std::move(promise)]() mutable {
                std::unique_lock lock{mutex_};
                (void)ready_.wait_for(lock, std::chrono::milliseconds{40}, [this] { return release_; });
                lock.unlock(); record("keepalive_completed");
                promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                    std::make_shared<Return>(Return::Success)});
            }};
        } else promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
            std::make_shared<Return>(Return::Success)});
        return future;
    }
    mel::RequestFor<irmel::ChannelCommsTestRep> send(irmel::ChannelCommsTestReq request) override
    {
        record("comms_sent");
        if (scenario_ == "comms-send-throw") throw std::runtime_error("mock comms send exception");
        if (scenario_ == "comms-high" && (request.getCommandID() != 0x80000001U ||
            request.getChannelID() != 0xf0000002U || request.getRequestID() != 0xe0000003U))
            throw std::runtime_error("CommsTest request conversion mismatch");
        auto response = std::make_shared<irmel::ChannelCommsTestRep>(
            request.getCommandID(), request.getRequestID());
        if (comms_callback_) comms_callback_(*this, response.get());
        std::promise<mel::ErrorOr<std::shared_ptr<irmel::ChannelCommsTestRep>>> promise;
        auto future = promise.get_future();
        if (scenario_ == "comms-reject") promise.set_value(
            mel::ErrorOr<std::shared_ptr<irmel::ChannelCommsTestRep>>{
                mel::Error{mel::ErrorCode::InvalidParameters, long_rejection_description()}});
        else if (scenario_ == "comms-null") promise.set_value(
            mel::ErrorOr<std::shared_ptr<irmel::ChannelCommsTestRep>>{
                std::shared_ptr<irmel::ChannelCommsTestRep>{}});
        else if (scenario_ == "comms-future-throw") promise.set_exception(std::make_exception_ptr(std::runtime_error{"mock comms future exception"}));
        else if (scenario_ == "comms-delayed" || scenario_ == "comms-lifetime") {
            producer_ = std::thread{[this, promise = std::move(promise), response]() mutable {
                std::unique_lock lock{mutex_};
                (void)ready_.wait_for(lock, std::chrono::milliseconds{40}, [this] { return release_; });
                lock.unlock(); record("comms_completed");
                promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::ChannelCommsTestRep>>{response});
            }};
        } else promise.set_value(
            mel::ErrorOr<std::shared_ptr<irmel::ChannelCommsTestRep>>{response});
        return future;
    }
    mel::RequestFor<Return> send(irmel::BIT_Command command) override
    {
        record("bit_sent");
        if (!enabled_) throw std::logic_error("BIT sent before enable");
        if (scenario_ == "bit-full") {
            const std::vector<std::uint32_t> initiate{1U, 0x80000001U, 0xffffffffU};
            const std::vector<std::uint32_t> cancel{7U, 9U};
            const std::vector<std::string> faults{"fault-alpha", "fault-\xE2\x82\xAC"};
            if (command.getCommandID() != 0xfedcba98U ||
                command.getInitiateBIT_ID() != initiate ||
                command.getCancelBIT_ID() != cancel ||
                command.getClearFaultCode() != faults)
                throw std::runtime_error("full BIT conversion mismatch");
        }
        if (scenario_ != "bit-full" && scenario_ != "bit-ada" &&
            (!command.getInitiateBIT_ID().empty() ||
            !command.getCancelBIT_ID().empty() ||
            !command.getClearFaultCode().empty()))
            throw std::runtime_error("unexpected payload-bearing BIT profile");
        if (scenario_ == "bit-send-throw") throw std::runtime_error("mock BIT send exception");
        if (scenario_ == "bit-command-id" && command.getCommandID() != UINT32_C(0x89abcdef))
            throw std::runtime_error("BIT command ID conversion mismatch");
        emit_status(command.getCommandID(), irmel::CommandState::Accepted,
                    irmel::CannotComply::NotSet, "");
        std::promise<mel::ErrorOr<std::shared_ptr<Return>>> promise;
        auto future = promise.get_future();
        if (scenario_ == "bit-fail") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                std::make_shared<Return>(Return::Fail)});
        } else if (scenario_ == "bit-unknown-return") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                std::make_shared<Return>(static_cast<Return>(99U))});
        } else if (scenario_ == "bit-reject") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                mel::Error{mel::ErrorCode::InvalidParameters, "invalid BIT command"}});
        } else if (scenario_ == "bit-reject-long") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                mel::Error{mel::ErrorCode::InvalidParameters, long_rejection_description()}});
        } else if (scenario_ == "bit-null-result") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{std::shared_ptr<Return>{}});
        } else if (scenario_ == "bit-future-throw") {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error{"mock BIT future exception"}));
        } else if (scenario_ == "bit-delayed" || scenario_ == "bit-lifetime") {
            if (producer_.joinable()) throw std::logic_error("only one delayed request supported");
            producer_ = std::thread{[this, promise = std::move(promise)]() mutable {
                std::unique_lock lock{mutex_};
                (void)ready_.wait_for(lock, std::chrono::milliseconds{40},
                                      [this] { return release_; });
                lock.unlock();
                record("bit_completed");
                promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                    std::make_shared<Return>(Return::Success)});
            }};
        } else {
            promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                std::make_shared<Return>(Return::Success)});
        }
        return future;
    }
    mel::RequestFor<mel::CalibrationConfiguration> send(irmel::CalibrationConfigurationCmd) override
    { return unsupported_request<mel::CalibrationConfiguration>(); }
    mel::RequestFor<mel::CalibrationStatus> send(irmel::CalibrationStatusCmd) override
    { return unsupported_request<mel::CalibrationStatus>(); }
    mel::RequestFor<irmel::CameraCommandResp> send(irmel::CameraCommand) override
    { return unsupported_request<irmel::CameraCommandResp>(); }
    mel::RequestFor<irmel::CameraProtectCmdResp> send(irmel::CameraProtectCmd) override
    { return unsupported_request<irmel::CameraProtectCmdResp>(); }
    mel::RequestFor<irmel::EraseCommandType> send(irmel::EraseCommand) override
    { return unsupported_request<irmel::EraseCommandType>(); }
    mel::RequestFor<irmel::CommandStatus> send(irmel::SystemTrackDataResponse) override
    { return unsupported_request<irmel::CommandStatus>(); }
    mel::RequestFor<Return> send(irmel::ConfigSetCommand command) override
    {
        record("config_set_sent");
        if (!enabled_) throw std::logic_error("ConfigSet sent before enable");
        if (scenario_ == "config-full" &&
            (command.getCommandID() != 0xfedcba98U ||
             command.getSystemTime() != std::chrono::nanoseconds{-1234567890123LL} ||
             command.getConfig() != "configuration-\xE2\x82\xAC"))
            throw std::runtime_error("ConfigSet conversion mismatch");
        if (scenario_ == "config-empty" &&
            (command.getCommandID() != 0U || command.getSystemTime().count() != 0 ||
             !command.getConfig().empty()))
            throw std::runtime_error("empty ConfigSet conversion mismatch");
        if (scenario_ == "config-positive" &&
            (command.getCommandID() != 0x80000000U ||
             command.getSystemTime() != std::chrono::nanoseconds{9876543210LL} ||
             command.getConfig() != "positive"))
            throw std::runtime_error("positive ConfigSet conversion mismatch");
        if (scenario_ == "config-fail")
            emit_status(command.getCommandID(), irmel::CommandState::Rejected,
                        irmel::CannotComply::InvalidInputParameter,
                        "invalid configuration payload");
        else
            emit_status(command.getCommandID(), irmel::CommandState::Accepted,
                        irmel::CannotComply::NotSet, "");
        std::promise<mel::ErrorOr<std::shared_ptr<Return>>> promise;
        if (scenario_ == "config-fail")
            promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                std::make_shared<Return>(Return::Fail)});
        else if (scenario_ == "config-reject")
            promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                mel::Error{mel::ErrorCode::InvalidParameters, "invalid configuration"}});
        else
            promise.set_value(mel::ErrorOr<std::shared_ptr<Return>>{
                std::make_shared<Return>(Return::Success)});
        return promise.get_future();
    }
    mel::RequestFor<irmel::MFA_Mode> send(irmel::ModeCmd command) override
    {
        record("mode_sent");
        if (!enabled_) throw std::logic_error("mode sent before enable");
        if (scenario_ == "mode-full") {
            const auto& scan = command.getScanParameters();
            const auto center = scan.getCenter();
            const auto& type = scan.getScanType();
            if (command.getCommandID() != 0x89abcdefU ||
                command.getState() != mel::MFA_State::Operate ||
                command.getMode() != irmel::MFA_Mode::ScanVolumeSched ||
                !scan.getIsDefinedWithRangeAndAltitude() || center.az != 0.25 ||
                center.el != -0.5 || scan.getCentFrameRefEl() != irmel::CoordFrameRef::Aircraft ||
                scan.getCentFrameRefAz() != irmel::CoordFrameRef::Inertial ||
                scan.getScanWidth() != 1.25 || scan.getScanHeight() != 0.75 ||
                type.getContinuousScan() != 1U || type.getReturning() != 2U ||
                type.getAgileScan() != 3U || scan.getScanId() != 0x89abcdefU ||
                scan.getScanRate() != -0.125 || scan.getPreferredRevisitInterval() != 2.5 ||
                scan.getRequiredRevisitInterval() != 3.5 ||
                scan.getMaxRangeOfInterest() != 123456U || scan.getMinRangeOfInterest() != 42U ||
                scan.getElevationScanCenterAltitude() != 7000U ||
                scan.getElevationScanCenterRange() != 9000U ||
                scan.getDegredationType() != irmel::DegradationMethod::REVISIT_DEGRADATION)
                throw std::runtime_error("full ModeCmd conversion mismatch");
        }
        if (scenario_ == "c2-send-throw") throw std::runtime_error("mock send exception");
        if (scenario_ != "mode-full" && scenario_ != "mode-general" &&
            (command.getState() != mel::MFA_State::Operate ||
            command.getMode() != irmel::MFA_Mode::TaskSched ||
            command.getScanParameters().getScanType().getContinuousScan() != 0U ||
            command.getScanParameters().getScanType().getReturning() != 0U ||
            command.getScanParameters().getScanType().getAgileScan() != 0U ||
            command.getScanParameters().getScanId() != 0U))
            throw std::runtime_error("unexpected ModeCmd profile");
        if (scenario_ == "c2-command-id" && command.getCommandID() != UINT32_C(0x89abcdef))
            throw std::runtime_error("command ID conversion mismatch");
        if (scenario_ == "metadata-command-status") {
            emit_status(command.getCommandID(), irmel::CommandState::Rejected,
                        irmel::CannotComply::InvalidInputParameter,
                        long_rejection_description());
        } else {
            emit_status(command.getCommandID(), irmel::CommandState::Received,
                        irmel::CannotComply::NotSet, "");
        }
        std::promise<mel::ErrorOr<std::shared_ptr<irmel::MFA_Mode>>> promise;
        auto future = promise.get_future();
        if (scenario_ == "c2-reject") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::MFA_Mode>>{
                mel::Error{mel::ErrorCode::InvalidParameters, "invalid task schedule"}});
        } else if (scenario_ == "c2-reject-long") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::MFA_Mode>>{
                mel::Error{mel::ErrorCode::InvalidParameters,
                           long_rejection_description()}});
        } else if (scenario_ == "c2-reject-empty") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::MFA_Mode>>{
                mel::Error{mel::ErrorCode::InvalidParameters}});
        } else if (scenario_ == "c2-reject-invalid-utf8") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::MFA_Mode>>{
                mel::Error{mel::ErrorCode::InvalidParameters, std::string{"bad\xC3\x28", 5}}});
        } else if (scenario_ == "c2-null-result") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::MFA_Mode>>{
                std::shared_ptr<irmel::MFA_Mode>{}});
        } else if (scenario_ == "c2-future-throw") {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error{"mock future exception"}));
        } else if (scenario_ == "c2-delayed" || scenario_ == "c2-lifetime" ||
                   scenario_ == "c2-coexist") {
            if (producer_.joinable()) throw std::logic_error("only one delayed request supported");
            producer_ = std::thread{[this, promise = std::move(promise)]() mutable {
                std::unique_lock lock{mutex_};
                (void)ready_.wait_for(lock, std::chrono::milliseconds{40},
                                      [this] { return release_; });
                lock.unlock();
                record("mode_completed");
                promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::MFA_Mode>>{
                    std::make_shared<irmel::MFA_Mode>(irmel::MFA_Mode::TaskSched)});
            }};
        } else if (scenario_ == "mode-full" || scenario_ == "mode-general") {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::MFA_Mode>>{
                std::make_shared<irmel::MFA_Mode>(command.getMode())});
        } else {
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::MFA_Mode>>{
                std::make_shared<irmel::MFA_Mode>(irmel::MFA_Mode::TaskSched)});
        }
        return future;
    }
    Return registerBuffer(std::shared_ptr<irmel::Buffer>) override
    { return Return::NotSupported; }
    Return unregisterBuffer(std::shared_ptr<irmel::Buffer>) override
    { return Return::NotSupported; }
    Return enable() override
    {
        record("c2_enabled");
        if (scenario_ == "c2-enable-fail") return Return::Fail;
        enabled_ = true;
        return Return::Success;
    }
    Return disable() override
    {
        record("c2_disabled");
        enabled_ = false;
        if (scenario_ == "metadata-nonquiescing-disable") {
            const char *base = std::getenv("AMS_MEL_TEST_METADATA_CALLBACK_BARRIER");
            if (!base) throw std::runtime_error("metadata callback barrier is not configured");
            wait_for_file(std::string{base} + ".entered");
            record("metadata_callback_entered");
            record("disable_returned_with_metadata_callback_active");
        }
        {
            std::lock_guard lock{mutex_};
            release_ = true;
        }
        ready_.notify_all();
        if (scenario_ == "c2-disable-fail") return Return::Fail;
        return Return::Success;
    }
    irmel::ChannelCapability getCapabilities() const override
    {
        if (scenario_ == "capability-throw" && capability_calls_++ != 0U)
            throw std::runtime_error("mock capability exception");
        if (scenario_.rfind("capability-", 0U) == 0U) {
            auto capability = rich_channel_capability();
            if (scenario_ == "capability-bad-pixel") capability.setFormat(static_cast<irmel::PixelFormat>(99U));
            else if (scenario_ == "capability-bad-sensor") capability.setSensorTypes({irmel::SensorType::MAXEXCLUSIVE});
            else if (scenario_ == "capability-bad-channel") capability.setChannelTypes(
                {irmel::ChannelType::CommandAndControl, static_cast<irmel::ChannelType>(99U)});
            else if (scenario_ == "capability-bad-metadata") capability.setChannelMetadataCapabilities({static_cast<irmel::ChannelMetadataCapabilityType>(99U)});
            else if (scenario_ == "capability-bad-band") capability.setImageBands({{1U, {{static_cast<irmel::BandType>(99U), 1.0, 2.0}}}});
            else if (scenario_ == "capability-bad-nav") capability.setNavFrames({static_cast<irmel::CoordinateSystemType>(99U)});
            else if (scenario_ == "capability-bad-utf8") capability.setChanID(metadata_id(1U, std::string{"bad\xC3\x28", 5}));
            return capability;
        }
        irmel::ChannelCapability capability;
        if (scenario_ != "c2-channel-capability-wrong")
            capability.setChannelTypes({irmel::ChannelType::CommandAndControl});
        return capability;
    }
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const irmel::ChannelCommsTestRep *const)> callback) override
    {
        comms_callback_ = std::move(callback);
        if (scenario_ == "comms-register-retain-fail") return Return::Fail;
        return Return::Success;
    }
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const mel::BIT_Configuration *const)> callback) override
    {
        bit_configuration_callback_ = std::move(callback);
        if (scenario_ == "metadata-rich" || scenario_ == "metadata-overflow") {
            auto value = rich_bit_configuration(); bit_configuration_callback_(*this, &value);
        } else if (scenario_ == "metadata-malformed") {
            auto value = rich_bit_configuration();
            value.addBIT_Type(mel::BIT_Type{metadata_id(1U, std::string{"bad\xC3\x28", 5}),
                mel::BIT_ControlInterface::NotSet, {}, {}, std::chrono::nanoseconds{0}});
            bit_configuration_callback_(*this, &value);
        }
        return Return::Success;
    }
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const irmel::CommandStatus *const)> callback) override
    {
        command_status_callback_ = std::move(callback);
        if (scenario_ == "metadata-register-fail") {
            irmel::CommandStatus value; value.setCommandID(77U); value.setState(irmel::CommandState::Accepted);
            record("retained_metadata_callback_invoked");
            bit_configuration_callback_(*this, nullptr);
            return Return::Fail;
        }
        if (scenario_ == "metadata-overflow") {
            emit_status(1U, irmel::CommandState::Accepted, irmel::CannotComply::NotSet, "");
            emit_status(2U, irmel::CommandState::Cancelled, irmel::CannotComply::Cancelled, "cancelled");
        }
        if (scenario_ == "metadata-command-states") {
            emit_status(0x80000001U, irmel::CommandState::Accepted,
                        irmel::CannotComply::NotSet, "");
            emit_status(0x80000002U, irmel::CommandState::Rejected,
                        irmel::CannotComply::InvalidInputParameter,
                        long_rejection_description());
            emit_status(0x80000003U, irmel::CommandState::Cancelled,
                        irmel::CannotComply::Cancelled, "cancelled");
            emit_status(0x80000004U, irmel::CommandState::Received,
                        irmel::CannotComply::NotSet, "");
        }
        if (scenario_ == "metadata-malformed") {
            emit_status(3U, static_cast<irmel::CommandState>(99U), irmel::CannotComply::NotSet, "");
            emit_status(4U, irmel::CommandState::Accepted, irmel::CannotComply::NotSet, std::string{"bad\xC3\x28",5});
            emit_status(5U, irmel::CommandState::Cancelled, irmel::CannotComply::Cancelled, "valid");
        }
        return Return::Success;
    }
    C2_UNSUPPORTED_CALLBACK(mel::CalibrationConfiguration)
    C2_UNSUPPORTED_CALLBACK(irmel::CameraProtectCmdResp)
    C2_UNSUPPORTED_CALLBACK(mel::CalibrationStatus)
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const mel::BIT_Status *const)> callback) override
    {
        bit_status_callback_ = std::move(callback);
        if (scenario_ == "metadata-rich") { auto value=rich_bit_status(); bit_status_callback_(*this,&value); }
        if (scenario_ == "metadata-malformed") {
            mel::CompletedBIT bad{metadata_id(1U,"bad result"),std::chrono::nanoseconds{0},static_cast<mel::BIT_Result>(99U),"",{}};
            mel::BIT_Status invalid{{},{bad},{}}; bit_status_callback_(*this,&invalid);
            auto valid=rich_bit_status(); bit_status_callback_(*this,&valid);
        }
        if (scenario_ == "metadata-nonquiescing-disable") {
            producer_ = std::thread{[this] {
                irmel::CommandStatus value;
                value.setCommandID(88U);
                value.setState(irmel::CommandState::Accepted);
                command_status_callback_(*this, &value);
                record("metadata_callback_returned");
            }};
        }
        return Return::Success;
    }
private:
    void emit_status(std::uint32_t id, irmel::CommandState state,
                     irmel::CannotComply reason, std::string description)
    {
        if (!command_status_callback_) return;
        irmel::CommandStatus value; value.setCommandID(id); value.setState(state);
        value.setReasonID(reason); value.setReasonDescription(description);
        command_status_callback_(*this, &value);
    }
    std::string scenario_;
    bool enabled_{};
    std::mutex mutex_;
    std::condition_variable ready_;
    bool release_{};
    std::thread producer_;
    std::function<void(irmel::Channel&, const mel::BIT_Configuration *const)> bit_configuration_callback_;
    std::function<void(irmel::Channel&, const irmel::CommandStatus *const)> command_status_callback_;
    std::function<void(irmel::Channel&, const mel::BIT_Status *const)> bit_status_callback_;
    std::function<void(irmel::Channel&, const irmel::ChannelCommsTestRep *const)> comms_callback_;
    mutable unsigned capability_calls_{};
};
#undef C2_UNSUPPORTED_CALLBACK

mel::MFA_Status rich_mfa_status()
{
    mel::MFA_Status value;
    value.setMFAState(mel::MFA_State::Maintenance);
    value.setMFAStateDescription("healthy-\xCE\xB1");
    value.setMFAModeDescription("mode-\xE2\x82\xAC");
    value.setStateTransitionStatus(mel::StateTransitionStatus::Transitioning);
    value.setAbout(mel::About{"model", "serial", "software", "boot", "hardware"});
    mel::ForeignKey location_id{"rack-\xCE\xB2", "aircraft"};
    mel::ForeignKey physical_id{"bay", "platform-\xE2\x82\xAC"};
    mel::ComponentLocation location{1.5, -2.5, 3.5, physical_id};
    mel::Euler orientation; orientation.SetAllAxis(0.1, 0.2, 0.3);
    mel::Euler boresight; boresight.SetAllAxis(-0.4, 0.5, -0.6);
    value.setMFAComponents({
        mel::MFA_Component{metadata_id(0x91U, "component-\xCE\xB3"),
            mel::ComponentState::Operational,
            mel::TemperatureStatus{42.25, mel::TemperatureState::Normal},
            location_id, mel::InstallationDetails{location, orientation, boresight}},
        mel::MFA_Component{metadata_id(0xa1U, "component two"),
            mel::ComponentState::Degraded,
            mel::TemperatureStatus{-12.5, mel::TemperatureState::UnderTemp},
            location_id, mel::InstallationDetails{location, boresight, orientation}}});
    return value;
}

irmel::SubsystemStatusResp rich_subsystem_status()
{
    irmel::SubsystemStatusResp value;
    value.setSubsystemId(0x80000001U); value.setCriticality(7U);
    value.setStatusSeqNum(0xf0000002U); value.setFailureLevel(irmel::Failure::Major);
    value.setSubsystemCount(99U);
    irmel::SubsystemDepInfo one; one.setSubsystemId(0x80000003U);
    one.setCriticality(8U); one.setFailureLevel(irmel::Failure::Critical);
    irmel::SubsystemDepInfo two; two.setSubsystemId(4U);
    two.setCriticality(9U); two.setFailureLevel(irmel::Failure::Available);
    value.setSubsystems({one, two}); value.setCsciCount(88U);
    irmel::SubsystemCSCIInfo first; first.setCsci("flight-\xCE\xB4");
    first.setMode(irmel::CSCIMode::Operational); first.setVersion({1U,2U,3U,4U});
    first.setCriticality(10U); first.setFailureLevel(irmel::Failure::Parametric);
    first.setBIT_report(0x80000005U); first.setConnectionEstablished(true);
    irmel::SubsystemCSCIInfo second; second.setCsci("maintenance");
    second.setMode(irmel::CSCIMode::Maintenance); second.setVersion({5U,6U,7U,8U});
    second.setCriticality(11U); second.setFailureLevel(irmel::Failure::Informational);
    second.setBIT_report(6U); second.setConnectionEstablished(false);
    value.setCSCI({first, second}); return value;
}

mel::MFA_SecurityAuditRecord security_record(unsigned alternative)
{
    mel::MFA_SecurityAuditRecord value;
    value.setSecurityEventID(metadata_id(0xd1U, "event-\xCE\xB5"));
    value.setEventTimestamp(std::chrono::nanoseconds{-987654321});
    value.setSubsystemID(metadata_id(0xe1U, "security subsystem"));
    value.setSecurityArtifacts({
        {metadata_id(0xf1U, "artifact one"), metadata_id(0x81U, "associated one")},
        {metadata_id(0x82U, "artifact two"), metadata_id(0x83U, "associated two")}});
    value.setOutcome(mel::MFA_SecurityAuditRecord::OutcomeEnum::Failure);
    value.setSeverity(mel::MFA_SecurityAuditRecord::SeverityEnum::Warning);
    switch (alternative) {
    case 0: value.setEventType(std::monostate{}); break;
    case 1: { mel::SecurityAuthenticationType event;
        event.setCategory(mel::SecurityAuthenticationType::SecurityAuthenticationEnum::MDF_Authentication);
        event.setDetails("auth-\xCE\xB6"); event.setSubsystemID(metadata_id(1U,"auth subsystem"));
        event.setServiceID(metadata_id(2U,"auth service")); event.setMDF_ID(metadata_id(3U,"auth mdf"));
        value.setEventType(event); break; }
    case 2: value.setEventType(mel::SecurityIntegrityType{
        mel::SecurityIntegrityType::SecurityIntegrityEnum::OtherIntegrityCheck,
        metadata_id(4U,"integrity mdf"), "integrity-\xCE\xB7"}); break;
    case 3: value.setEventType(mel::SecurityFileManagementType{"file-\xCE\xB8",
        mel::SecurityFileManagementType::SecurityFileManagementEnum::OtherFileUpdate,
        metadata_id(5U,"file mdf")}); break;
    case 4: value.setEventType(mel::SecurityKeyManagementType{"key-\xCE\xB9",
        mel::SecurityKeyManagementType::SecurityKeyManagementEnum::CertificateDeletion}); break;
    case 5: value.setEventType(mel::SecuritySystemType{"system-\xCE\xBA",
        mel::SecuritySystemType::SecuritySystemEnum::Fault}); break;
    default: value.setEventType(mel::SecuritySanitizationType{"sanitize-\xCE\xBB",
        mel::SecuritySanitizationType::SecuritySanitizationEnum::ClassifiedDataErase}); break;
    }
    return value;
}

class MockHealthStatusChannel final : public irmel::HealthStatusChannel {
public:
    explicit MockHealthStatusChannel(std::string scenario) : scenario_{std::move(scenario)} {}
    ~MockHealthStatusChannel() override
    {
        if (scenario_ == "health-register-partial" && mfa_callback_) {
            auto value=rich_mfa_status(); record("retained_health_callback_invoked");
            mfa_callback_(*this,&value);
        }
        if (scenario_ == "health-metadata-nonquiescing-disable") {
            const char* base=std::getenv("AMS_MEL_TEST_HEALTH_CALLBACK_BARRIER");
            if(!base)std::abort();
            create_file(std::string{base}+".release");
        }
        if(producer_.joinable())producer_.join();
        record("health_channel_destroyed");
    }
    mel::RequestFor<Return> sendKeepAliveRep() override { return {}; }
    mel::RequestFor<irmel::ChannelCommsTestRep> send(irmel::ChannelCommsTestReq) override { return {}; }
    Return registerBuffer(std::shared_ptr<irmel::Buffer>) override { return Return::NotSupported; }
    Return unregisterBuffer(std::shared_ptr<irmel::Buffer>) override { return Return::NotSupported; }
    Return enable() override { enabled_=true;record("health_enabled");return scenario_=="health-enable-fail"?Return::Fail:Return::Success; }
    Return disable() override { enabled_=false;record("health_disabled");if(scenario_=="health-metadata-nonquiescing-disable"){const char* base=std::getenv("AMS_MEL_TEST_HEALTH_CALLBACK_BARRIER");if(!base)std::abort();wait_for_file(std::string{base}+".entered");record("disable_returned_with_health_callback_active");}return Return::Success; }
    irmel::ChannelCapability getCapabilities() const override
    { auto value=rich_channel_capability();value.setChannelTypes({irmel::ChannelType::HealthAndStatus});value.setChannelMetadataCapabilities({irmel::ChannelMetadataCapabilityType::MFAStatus,irmel::ChannelMetadataCapabilityType::BITStatus,irmel::ChannelMetadataCapabilityType::SubsystemStatusResp,irmel::ChannelMetadataCapabilityType::MFAStatusDetailed});return value; }
    Return registerMetadataCallback(std::function<void(irmel::Channel&,const irmel::ChannelCommsTestRep*const)>) override{return Return::NotSupported;}
    Return registerMetadataCallback(std::function<void(irmel::Channel&,const irmel::LFStatus*const)>) override{return Return::NotSupported;}
    Return registerMetadataCallback(std::function<void(irmel::Channel&,const mel::MFA_Status*const)> cb) override{mfa_callback_=std::move(cb);if(rich()){auto value=rich_mfa_status();mfa_callback_(*this,&value);}if(scenario_=="health-allocation"){auto value=rich_mfa_status();mfa_callback_(*this,&value);}if(scenario_=="health-malformed"){auto bad=rich_mfa_status();bad.setMFAStateDescription(std::string{"bad\xC3\x28",5});mfa_callback_(*this,&bad);auto good=rich_mfa_status();mfa_callback_(*this,&good);}return Return::Success;}
    Return registerMetadataCallback(std::function<void(irmel::Channel&,const mel::BIT_Status*const)> cb) override{bit_callback_=std::move(cb);if(rich()){auto value=rich_bit_status();bit_callback_(*this,&value);}if(scenario_=="health-allocation"){(void)setenv("AMS_MEL_TEST_HEALTH_CALLBACK_FAILURE","allocation",1);auto value=rich_bit_status();bit_callback_(*this,&value);(void)unsetenv("AMS_MEL_TEST_HEALTH_CALLBACK_FAILURE");}return Return::Success;}
    Return registerMetadataCallback(std::function<void(irmel::Channel&,const irmel::SubsystemStatusResp*const)> cb) override{subsystem_callback_=std::move(cb);if(scenario_=="health-register-partial")return Return::Fail;if(rich()){auto value=rich_subsystem_status();subsystem_callback_(*this,&value);}return Return::Success;}
    Return registerMetadataCallback(std::function<void(irmel::Channel&,const mel::DiscreteStatus*const)> cb) override{discrete_callback_=std::move(cb);if(rich()){std::vector<mel::NameValuePair> pairs{{"duplicate","one"},{"duplicate",""},{"utf8","\xE2\x82\xAC"}};mel::DiscreteStatus value{pairs};discrete_callback_(*this,&value);}return Return::Success;}
    Return registerMetadataCallback(std::function<void(irmel::Channel&,const mel::MFA_SecurityAuditRecord*const)> cb) override{security_callback_=std::move(cb);if(rich())for(unsigned i=0;i<7U;++i){auto value=security_record(i);security_callback_(*this,&value);}return Return::Success;}
    Return registerMetadataCallback(std::function<void(irmel::Channel&,const mel::MFA_StatusDetailed*const)> cb) override{detailed_callback_=std::move(cb);if(rich()){std::vector<mel::NameValuePair> pairs{{"load","23"},{"load",""},{"label","\xCE\xBC"}};mel::MFA_StatusDetailed value{pairs};detailed_callback_(*this,&value);}if(scenario_=="health-metadata-nonquiescing-disable")producer_=std::thread{[this]{const char* base=std::getenv("AMS_MEL_TEST_HEALTH_CALLBACK_BARRIER");if(!base)std::abort();wait_for_file(std::string{base}+".start");auto value=rich_mfa_status();mfa_callback_(*this,&value);record("health_callback_returned");}};return Return::Success;}
    Return registerMetadataCallback(std::function<void(irmel::Channel&,const irmel::NUC_TempData*const)>) override{return Return::NotSupported;}
private:
    bool rich()const{return scenario_=="health-rich"||scenario_=="health-overflow";}
    std::string scenario_;bool enabled_{};std::thread producer_;
    std::function<void(irmel::Channel&,const mel::MFA_Status*const)> mfa_callback_;
    std::function<void(irmel::Channel&,const mel::BIT_Status*const)> bit_callback_;
    std::function<void(irmel::Channel&,const irmel::SubsystemStatusResp*const)> subsystem_callback_;
    std::function<void(irmel::Channel&,const mel::DiscreteStatus*const)> discrete_callback_;
    std::function<void(irmel::Channel&,const mel::MFA_SecurityAuditRecord*const)> security_callback_;
    std::function<void(irmel::Channel&,const mel::MFA_StatusDetailed*const)> detailed_callback_;
};

irmel::InstrumentationReport rich_instrumentation_report()
{
    irmel::InstrumentationReport value;
    value.setCommandID(0xf1234567U);
    value.setSize(0x89abcdefU);
    value.setTimestamp(std::chrono::nanoseconds{-8765432109LL});
    value.setInstrumentationPriority(irmel::Priority::Debug);
    return value;
}

class MockInstrumentationChannel final : public irmel::InstrumentationChannel {
public:
    explicit MockInstrumentationChannel(std::string scenario)
        : scenario_{std::move(scenario)} {}
    ~MockInstrumentationChannel() override
    {
        {
            std::lock_guard lock{mutex_};
            release_ = true;
        }
        ready_.notify_all();
        if (producer_.joinable()) producer_.join();
        record("instrumentation_channel_destroyed");
    }
    mel::RequestFor<Return> sendKeepAliveRep() override { return {}; }
    mel::RequestFor<irmel::ChannelCommsTestRep> send(irmel::ChannelCommsTestReq) override
    { return {}; }
    Return registerBuffer(std::shared_ptr<irmel::Buffer>) override
    { return Return::NotSupported; }
    Return unregisterBuffer(std::shared_ptr<irmel::Buffer>) override
    { return Return::NotSupported; }
    Return enable() override
    {
        record("instrumentation_enabled");
        if (scenario_ == "instr-enable-throw")
            throw std::runtime_error("mock Instrumentation enable exception");
        if (scenario_ == "instr-enable-fail") return Return::Fail;
        enabled_ = true;
        return Return::Success;
    }
    Return disable() override
    {
        enabled_ = false;
        record("instrumentation_disabled");
        return Return::Success;
    }
    irmel::ChannelCapability getCapabilities() const override
    {
        auto value = rich_channel_capability();
        value.setChannelTypes(scenario_ == "instr-capability-wrong" ?
            std::vector<irmel::ChannelType>{irmel::ChannelType::HealthAndStatus} :
            std::vector<irmel::ChannelType>{irmel::ChannelType::Instrumentation});
        value.setChannelMetadataCapabilities(
            {irmel::ChannelMetadataCapabilityType::InstrumentationReport,
             irmel::ChannelMetadataCapabilityType::ChannelCommsTestRep});
        return value;
    }
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&, const irmel::ChannelCommsTestRep *const)>) override
    { return Return::NotSupported; }
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&,
                           const irmel::InstrumentationReport *const)> cb) override
    {
        if (scenario_ == "instr-register-fail") return Return::Fail;
        report_callback_ = std::move(cb);
        record("instrumentation_callback_registered");
        /* Synchronous emission from inside registration. */
        if (scenario_ == "instr-rich" || scenario_ == "instr-lifetime") {
            auto value = rich_instrumentation_report();
            report_callback_(*this, &value);
            record("instrumentation_registration_callback_returned");
        }
        if (scenario_ == "instr-overflow")
            for (unsigned index = 0; index < 6U; ++index) {
                auto value = rich_instrumentation_report();
                value.setCommandID(index);
                report_callback_(*this, &value);
            }
        if (scenario_ == "instr-callback-null") report_callback_(*this, nullptr);
        if (scenario_ == "instr-callback-invalid-priority") {
            irmel::InstrumentationReport value;
            value.setInstrumentationPriority(static_cast<irmel::Priority>(7U));
            report_callback_(*this, &value);
        }
        if (scenario_ == "instr-callback-allocation") {
            (void)setenv("AMS_MEL_TEST_INSTRUMENTATION_CALLBACK_FAILURE", "allocation", 1);
            auto value = rich_instrumentation_report();
            report_callback_(*this, &value);
            (void)unsetenv("AMS_MEL_TEST_INSTRUMENTATION_CALLBACK_FAILURE");
        }
        return Return::Success;
    }
    mel::RequestFor<irmel::InstrumentationReport> send(
        irmel::InstrumentationLevelCmd command) override
    {
        record("instrumentation_level_sent");
        if (!enabled_) throw std::logic_error("InstrumentationLevelCmd sent before enable");
        if (scenario_ == "instr-rich" || scenario_ == "instr-lifetime" ||
            scenario_ == "instr-failpoint") {
            /* Rich command fidelity is verified through upstream getters. */
            if (command.getCommandID() != 0xe1234567U ||
                command.getInstrumentationPriority() != irmel::Priority::Debug)
                throw std::runtime_error("InstrumentationLevelCmd conversion mismatch");
        }
        if (scenario_ == "instr-normal-priority" &&
            (command.getCommandID() != 7U ||
             command.getInstrumentationPriority() != irmel::Priority::Normal))
            throw std::runtime_error("Normal InstrumentationLevelCmd conversion mismatch");
        if (scenario_ == "instr-send-throw")
            throw std::runtime_error("mock Instrumentation send exception");

        auto response = std::make_shared<irmel::InstrumentationReport>(
            rich_instrumentation_report());
        /* A provider is permitted to invoke metadata callbacks synchronously
         * from inside send(); prove the adapter does not deadlock. */
        if (report_callback_ && (scenario_ == "instr-rich" || scenario_ == "instr-lifetime")) {
            auto value = rich_instrumentation_report();
            report_callback_(*this, &value);
            record("instrumentation_send_callback_returned");
        }
        std::promise<mel::ErrorOr<std::shared_ptr<irmel::InstrumentationReport>>> promise;
        auto future = promise.get_future();
        if (scenario_ == "instr-reject")
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::InstrumentationReport>>{
                mel::Error{mel::ErrorCode::InvalidState, long_rejection_description()}});
        else if (scenario_ == "instr-unknown-error")
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::InstrumentationReport>>{
                mel::Error{static_cast<mel::ErrorCode>(99U), "unknown"}});
        else if (scenario_ == "instr-null")
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::InstrumentationReport>>{
                std::shared_ptr<irmel::InstrumentationReport>{}});
        else if (scenario_ == "instr-invalid-priority") {
            auto bad = std::make_shared<irmel::InstrumentationReport>();
            bad->setInstrumentationPriority(static_cast<irmel::Priority>(5U));
            promise.set_value(
                mel::ErrorOr<std::shared_ptr<irmel::InstrumentationReport>>{bad});
        } else if (scenario_ == "instr-future-throw")
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error{"mock Instrumentation future exception"}));
        else if (scenario_ == "instr-lifetime" || scenario_ == "instr-delayed") {
            producer_ = std::thread{[this, promise = std::move(promise), response]() mutable {
                std::unique_lock lock{mutex_};
                (void)ready_.wait_for(lock, std::chrono::milliseconds{40},
                                      [this] { return release_; });
                lock.unlock();
                record("instrumentation_completed");
                promise.set_value(
                    mel::ErrorOr<std::shared_ptr<irmel::InstrumentationReport>>{response});
            }};
        } else promise.set_value(
            mel::ErrorOr<std::shared_ptr<irmel::InstrumentationReport>>{response});
        return future;
    }
private:
    std::string scenario_;
    bool enabled_{};
    bool release_{};
    std::mutex mutex_;
    std::condition_variable ready_;
    std::thread producer_;
    std::function<void(irmel::Channel&, const irmel::InstrumentationReport *const)>
        report_callback_;
};

/* Task 029B2 positively implements the @RequiredIfTrack IRSTTrackReport
 * registration, Task 029C adds the @RequiredIfTrackUpdate
 * send(TrackDataUpdate), Task 029D adds the @Optional
 * send(SystemTrackDataResponse), Task 029E adds the @Optional
 * RequestSystemTrackData callback, Task 029F adds the
 * @RequiredIfDetectCandidateObjects CandidateObjectMessage callback, and Task
 * 029G adds the @Optional CandidateObjectPreProcMessage callback.
 *
 * Every published TrackChannel-specific surface is now positively implemented,
 * so there is NO deferred Track surface left and no deferred-call counter.
 * A default Track scenario may still answer Return::NotSupported to the
 * @Optional PreProc registration: that is a legitimate optional refusal, not a
 * violation. */

/* The exact distinctive TrackDataUpdate the Track update tests submit. Every
 * field, including all 21 covariance terms, carries a value that cannot be
 * confused with a default, an adjacent term, or a sign/scale mistake. */
void verify_rich_track_update(const irmel::TrackDataUpdate& value)
{
    const auto fail = [](const char *what) { throw std::runtime_error(what); };
    if (value.getPlatformId() != 0xf1234567U) fail("TrackDataUpdate platformId mismatch");
    if (value.getTrackId() != 0xe2345678U) fail("TrackDataUpdate trackId mismatch");
    if (value.getTrackStatus() != irmel::TrackStatus::Predict)
        fail("TrackDataUpdate trackStatus mismatch");
    if (value.getTimeOfValidity() != -12345.25)
        fail("TrackDataUpdate timeOfValidity mismatch");
    if (value.getTimeOfLastUpdate() != 1700000000.875)
        fail("TrackDataUpdate timeOfLastUpdate mismatch");

    const auto check_id = [&](const mel::UCI_ID& id, std::uint8_t seed,
                              const char *label, const char *what) {
        for (std::size_t index = 0; index < mel::UUID_SIZE; ++index)
            if (id.getUUID()[index] !=
                static_cast<std::uint8_t>(seed + index * 3U)) fail(what);
        if (id.getDescriptiveLabel() != label) fail(what);
    };
    check_id(value.getCapabilityUUID(), 0x10U, "capability-\xCE\xB1",
             "TrackDataUpdate capabilityUUID mismatch");
    check_id(value.getActivityUUID(), 0x40U, "activity-\xCE\xB2",
             "TrackDataUpdate activityUUID mismatch");
    check_id(value.getEntityUUID(), 0x70U, "entity-\xE2\x82\xAC",
             "TrackDataUpdate entityUUID mismatch");

    const auto& position = value.getTrackPosition();
    if (position.getxAxis() != -1.25 || position.getyAxis() != 2.5 ||
        position.getzAxis() != -3.75) fail("TrackDataUpdate trackPosition mismatch");
    const auto& velocity = value.getTrackVelocity();
    if (velocity.getxAxis() != 4.125 || velocity.getyAxis() != -5.25 ||
        velocity.getzAxis() != 6.5) fail("TrackDataUpdate trackVelocity mismatch");

    /* All 21 covariance terms individually, so any swapped pair is caught. */
    if (value.getTrackCovarianceXX() != 1.01) fail("covariance XX mismatch");
    if (value.getTrackCovarianceXY() != 2.02) fail("covariance XY mismatch");
    if (value.getTrackCovarianceXZ() != 3.03) fail("covariance XZ mismatch");
    if (value.getTrackCovarianceXVx() != 4.04) fail("covariance XVx mismatch");
    if (value.getTrackCovarianceXVy() != 5.05) fail("covariance XVy mismatch");
    if (value.getTrackCovarianceXVz() != 6.06) fail("covariance XVz mismatch");
    if (value.getTrackCovarianceYY() != 7.07) fail("covariance YY mismatch");
    if (value.getTrackCovarianceYZ() != 8.08) fail("covariance YZ mismatch");
    if (value.getTrackCovarianceYVx() != 9.09) fail("covariance YVx mismatch");
    if (value.getTrackCovarianceYVy() != 10.10) fail("covariance YVy mismatch");
    if (value.getTrackCovarianceYVz() != 11.11) fail("covariance YVz mismatch");
    if (value.getTrackCovarianceZZ() != 12.12) fail("covariance ZZ mismatch");
    if (value.getTrackCovarianceZVx() != 13.13) fail("covariance ZVx mismatch");
    if (value.getTrackCovarianceZVy() != 14.14) fail("covariance ZVy mismatch");
    if (value.getTrackCovarianceZVz() != 15.15) fail("covariance ZVz mismatch");
    if (value.getTrackCovarianceVxVx() != 16.16) fail("covariance VxVx mismatch");
    if (value.getTrackCovarianceVxVy() != 17.17) fail("covariance VxVy mismatch");
    if (value.getTrackCovarianceVxVz() != 18.18) fail("covariance VxVz mismatch");
    if (value.getTrackCovarianceVyVy() != 19.19) fail("covariance VyVy mismatch");
    if (value.getTrackCovarianceVyVz() != 20.20) fail("covariance VyVz mismatch");
    if (value.getTrackCovarianceVzVz() != 21.21) fail("covariance VzVz mismatch");

    if (value.getManeuverProbability() != 0.625)
        fail("TrackDataUpdate maneuverProbability mismatch");
    if (value.getTrackQuality() != 12.75) fail("TrackDataUpdate trackQuality mismatch");
}

/* The exact distinctive SystemTrackDataResponse the Track response tests
 * submit. Every field, including both AzEl pairs and both published bool
 * values, carries a value that cannot be confused with a default, an adjacent
 * field, or a sign/scale mistake. Every published getter is read exactly once.
 * The az_el_valid/range_valid pair is supplied by the scenario so the
 * false/false combination can be verified with the same distinctive numbers. */
void verify_rich_track_response(const irmel::SystemTrackDataResponse& value,
                                bool az_el_valid, bool range_valid)
{
    const auto fail = [](const char *what) { throw std::runtime_error(what); };
    if (value.getSystemTime() != std::chrono::nanoseconds{-8765432109876LL})
        fail("SystemTrackDataResponse systemTime mismatch");
    if (value.getCommandID() != 0xf1234567U)
        fail("SystemTrackDataResponse commandID mismatch");
    if (value.getRequestId() != 0xe2345678U)
        fail("SystemTrackDataResponse requestId mismatch");
    if (value.getTrackId() != 0xd3456789U)
        fail("SystemTrackDataResponse trackId mismatch");

    if (value.getRange() != 123456.75) fail("SystemTrackDataResponse range mismatch");
    if (value.getRangeRate() != -456.125)
        fail("SystemTrackDataResponse rangeRate mismatch");
    if (value.getRangeError() != 12.5)
        fail("SystemTrackDataResponse rangeError mismatch");
    if (value.getRangeRateError() != -0.875)
        fail("SystemTrackDataResponse rangeRateError mismatch");

    if (value.getAzElValid() != az_el_valid)
        fail("SystemTrackDataResponse AzElValid mismatch");
    if (value.getRangeValid() != range_valid)
        fail("SystemTrackDataResponse rangeValid mismatch");

    /* Both angle pairs individually, so a swapped az/el or a swapped pair is
     * caught. All four values are radians. */
    const AzEl inertial = value.getInertialAzEl();
    if (inertial.az != -1.25 || inertial.el != 0.625)
        fail("SystemTrackDataResponse inertialAzEl mismatch");
    const AzEl error = value.getAzElError();
    if (error.az != 0.03125 || error.el != -0.015625)
        fail("SystemTrackDataResponse AzElError mismatch");
}

/* Distinctive successful provider CommandStatus for SystemTrackDataResponse.
 * It is deliberately different from the TrackDataUpdate status so the two
 * request families can never be confused in a test. */
std::shared_ptr<irmel::CommandStatus> rich_track_response_command_status()
{
    auto status = std::make_shared<irmel::CommandStatus>();
    status->setCommandID(0xa1b2c3d4U);
    status->setState(irmel::CommandState::Accepted);
    status->setReasonID(irmel::CannotComply::NotSet);
    status->setReasonDescription("Track system response accepted \xC2\xB5");
    return status;
}

/* Distinctive successful provider CommandStatus for TrackDataUpdate. */
std::shared_ptr<irmel::CommandStatus> rich_track_command_status()
{
    auto status = std::make_shared<irmel::CommandStatus>();
    status->setCommandID(0xf0e1d2c3U);
    status->setState(irmel::CommandState::Accepted);
    status->setReasonID(irmel::CannotComply::NotSet);
    status->setReasonDescription("Track update accepted \xC2\xB5");
    return status;
}

/* Distinctive rich IRSTTrackReport. Every field carries a value that cannot be
 * confused with a default, an adjacent field, or a sign/scale mistake. */
irmel::IRSTTrackReport rich_track_report()
{
    irmel::IRSTTrackReport report;
    report.setSystemTime(std::chrono::nanoseconds{-1234567890123LL});
    report.setActivityId(0xF1234567U);
    report.setMeasuredNed(mel::NorthEastDown{-1.25, 2.5, -3.75});
    report.setMeasuredIntensity(4.125);
    report.setMeasuredSnr(-5.25);
    report.setFilteredNed(mel::NorthEastDown{6.5, -7.75, 8.875});
    report.setFilteredIntensity(-9.125);
    report.setFilteredSnr(10.25);
    report.setRange(123456.75);
    report.setRangeError(654.5);
    report.setSpatialExtent(0.0125);
    report.setTrackQuality(0.875);
    report.setClutter(-0.5);
    report.setAge(std::chrono::nanoseconds{9876543210LL});
    report.setState(irmel::IrstTrackState::Coast);
    report.setMode(irmel::IrstTrackMode::Stare);
    return report;
}

/* Distinctive rich RequestSystemTrackData. The system time is deliberately
 * negative so a signed/unsigned mistake in the nanosecond carrier cannot pass,
 * and each identifier has its high bit set so a 32-bit narrowing or a
 * field-order swap is detectable. */
irmel::RequestSystemTrackData rich_request_system_track_data()
{
    irmel::RequestSystemTrackData request;
    request.setSystemTime(std::chrono::nanoseconds{-8765432109876LL});
    request.setCommandID(0xC1234567U);
    request.setRequestId(0xD2345678U);
    request.setTrackId(0xE3456789U);
    return request;
}

/* Distinctive rich CandidateObjectMessage, built through published setters
 * only. Every scalar is unique so a swapped field, a narrowed width, a lost
 * sign, or a float/double confusion cannot pass.
 *
 * numberOfCOs is 3 while the upstream array has 900 slots. Slot 3 carries a
 * recognizable sentinel that the tests assert is NEVER exposed, proving only
 * the meaningful prefix is copied. */
irmel::CandidateObjectMessage rich_candidate_object_message()
{
    irmel::CandidateObjectHeader header;
    header.setNumberOfCOs(3U);
    header.setStackFrameIndex(0xBEEFU);
    /* Exactly representable in binary32, and distinguishable from any double
     * rounding of the same decimal. */
    header.setCFAR(1.5309e-7F);
    header.setValidityFlagBitField(0xA5C3U);
    header.setTOVutcNanoseconds(std::chrono::nanoseconds{-4433221100998877LL});
    header.addHotRegion(irmel::HotRegion{irmel::HOTREGIONTYPE_FLARE,
                                         1111U, 2222U, 3333U, 4444U, 5555U});
    header.addHotRegion(irmel::HotRegion{irmel::HOTREGIONTYPE_SOLAR,
                                         6666U, 7777U, 8888U, 9999U, 10111U});
    header.addHotRegion(irmel::HotRegion{irmel::HOTREGIONTYPE_MASK,
                                         12222U, 13333U, 14444U, 15555U, 16666U});

    irmel::SensorInertialState inertial;
    inertial.setSystemTime(std::chrono::nanoseconds{-1122334455667788LL});
    inertial.setQ_xyzw(irmel::Quaternion{0.125, -0.25, 0.375, -0.5});
    inertial.setQECEF_xyzw(irmel::Quaternion{-0.625, 0.75, -0.875, 1.125});
    inertial.setSensorPosition(irmel::IR_Directional{1234567.25, -2345678.5, 3456789.75});
    inertial.setSensorVelocity(irmel::IR_Directional{-11.125, 22.25, -33.375});
    inertial.setUncertainties(irmel::Uncertainty{0xC0FFEE01U, 0xDEADBE02U});

    std::array<irmel::CandidateObject, irmel::MAX_CANDIDATE_OBJECTS> objects{};
    for (std::uint32_t index = 0; index < 3U; ++index) {
        irmel::CandidateObject object;
        object.setSystemTime(std::chrono::nanoseconds{
            -1000000000000LL - static_cast<std::int64_t>(index) * 7LL});
        object.setDetectionCategory(0x11110000U + index);
        object.setSensorIndex(0x22220000U + index);
        object.setSubpixel(irmel::RowCol{100.5 + index, 200.25 + index});
        object.setIntensity(3000.125 + index);
        object.setSenRelUnit(irmel::IR_Directional{
            0.1 + index, -0.2 - index, 0.3 + index});
        object.setSignalToInterferenceRatio(40.5 + index);
        object.setSignalToNoiseRatio(-50.75 - index);
        objects[index] = object;
    }
    /* Sentinel beyond the meaningful prefix: never exposed when numberOfCOs
     * is 3. Its values are unmistakable. */
    irmel::CandidateObject sentinel;
    sentinel.setSystemTime(std::chrono::nanoseconds{0x7FFFFFFFFFFFFFFFLL});
    sentinel.setDetectionCategory(0xFFFFFFFFU);
    sentinel.setSensorIndex(0xFFFFFFFFU);
    sentinel.setSubpixel(irmel::RowCol{-99999.5, -88888.5});
    sentinel.setIntensity(-77777.5);
    sentinel.setSenRelUnit(irmel::IR_Directional{-66666.5, -55555.5, -44444.5});
    sentinel.setSignalToInterferenceRatio(-33333.5);
    sentinel.setSignalToNoiseRatio(-22222.5);
    objects[3] = sentinel;

    irmel::CandidateObjectMessage message;
    message.setHeader(header);
    message.setInertialState(inertial);
    message.setCandidateObjects(objects);
    return message;
}

/* Distinctive rich CandidateObjectPreProcMessage, built through published
 * setters only. Every scalar is unique so a swapped field, a narrowed width, a
 * lost sign, a float/double confusion, or a row/column transposition cannot
 * pass.
 *
 * The header's numberOfCOs is deliberately 3 while the PreProc VECTOR holds 2
 * entries. The pinned headers publish no invariant tying the two together, so
 * the adapter must preserve both verbatim: the header must still report 3 and
 * the span must still expose exactly 2. A truncating or rejecting adapter
 * fails on this fixture.
 *
 * The two entries deliberately cover edge=false and edge=true. */
irmel::CandidateObjectPreProcMessage rich_candidate_preproc_message()
{
    irmel::CandidateObjectHeader header;
    /* Deliberately NOT equal to the PreProc vector size of 2. */
    header.setNumberOfCOs(3U);
    header.setStackFrameIndex(0xC0DEU);
    /* Exactly representable in binary32 and distinct from the 029F value. */
    header.setCFAR(2.7183e-6F);
    header.setValidityFlagBitField(0x5A3CU);
    header.setTOVutcNanoseconds(std::chrono::nanoseconds{-9988776655443322LL});
    header.addHotRegion(irmel::HotRegion{irmel::HOTREGIONTYPE_SOLAR,
                                         2001U, 2002U, 2003U, 2004U, 2005U});
    header.addHotRegion(irmel::HotRegion{irmel::HOTREGIONTYPE_MASK,
                                         3001U, 3002U, 3003U, 3004U, 3005U});

    /* Top-level message inertial state, distinct from every nested one. */
    irmel::SensorInertialState inertial;
    inertial.setSystemTime(std::chrono::nanoseconds{-5544332211009988LL});
    inertial.setQ_xyzw(irmel::Quaternion{0.0625, -0.125, 0.1875, -0.25});
    inertial.setQECEF_xyzw(irmel::Quaternion{-0.3125, 0.375, -0.4375, 0.5});
    inertial.setSensorPosition(irmel::IR_Directional{7654321.5, -8765432.25, 9876543.125});
    inertial.setSensorVelocity(irmel::IR_Directional{-44.625, 55.75, -66.875});
    inertial.setUncertainties(irmel::Uncertainty{0xBADF00D1U, 0xFEEDBEE2U});

    std::vector<irmel::CandidateObjectPreProc> preprocs;
    for (std::uint32_t index = 0; index < 2U; ++index) {
        const auto step = static_cast<double>(index);
        irmel::CandidateObjectPreProc preproc;
        preproc.setSystemTime(std::chrono::nanoseconds{
            -2000000000000LL - static_cast<std::int64_t>(index) * 13LL});
        preproc.setDetectionCategory(0x33330000U + index);
        preproc.setSensorIndex(0x44440000U + index);
        /* Row and column differ by far more than the per-entry step, so a
         * transposition is unmistakable. */
        preproc.setSubpixel(irmel::RowCol{400.5 + step, 900.25 + step});
        preproc.setIntensity(5000.125 + step);
        preproc.setSenRelUnit(irmel::IR_Directional{
            0.4 + step, -0.5 - step, 0.6 + step});
        preproc.setSignalToInterferenceRatio(60.5 + step);
        preproc.setSignalToNoiseRatio(-70.75 - step);

        /* All nine background samples unique across both entries, with mixed
         * signs and values beyond int8 range, so a width error, a sign loss,
         * or a row/column transposition is caught. Entry 0 uses
         * 1000,-1001,1002 / -1003,1004,-1005 / 1006,-1007,1008 and entry 1
         * offsets every element by 100. */
        std::array<std::array<std::int16_t, 3>, 3> background{};
        for (std::size_t row = 0; row < 3U; ++row)
            for (std::size_t column = 0; column < 3U; ++column) {
                const auto ordinal = static_cast<std::int16_t>(row * 3U + column);
                const auto magnitude = static_cast<std::int16_t>(
                    1000 + ordinal + static_cast<std::int16_t>(index) * 100);
                background[row][column] = (ordinal % 2 == 0)
                    ? magnitude : static_cast<std::int16_t>(-magnitude);
            }
        preproc.setCandidateObjectWithBackground(background);

        preproc.setClutter(-80.375 - step);
        /* Deliberately OUTSIDE the documented 0..1 range: the published setter
         * enforces nothing, so the adapter must not clamp. */
        preproc.setCandidateObjectQuality(1.5 + step);
        preproc.setSirDelta(-90.625 - step);

        /* The PreProc's OWN nested inertial state, distinct from the
         * message-level one and from the other entry's. */
        irmel::SensorInertialState nested;
        nested.setSystemTime(std::chrono::nanoseconds{
            -3344556677889900LL - static_cast<std::int64_t>(index)});
        nested.setQ_xyzw(irmel::Quaternion{
            0.75 + step, -0.8125 - step, 0.875 + step, -0.9375 - step});
        nested.setQECEF_xyzw(irmel::Quaternion{
            -1.0625 - step, 1.125 + step, -1.1875 - step, 1.25 + step});
        nested.setSensorPosition(irmel::IR_Directional{
            111111.5 + step, -222222.25 - step, 333333.125 + step});
        nested.setSensorVelocity(irmel::IR_Directional{
            -77.125 - step, 88.25 + step, -99.375 - step});
        nested.setUncertainties(irmel::Uncertainty{0xA1B2C300U + index,
                                                   0xD4E5F600U + index});
        preproc.setInertialState(nested);

        /* Boolean coverage across the two entries: entry 0 false, entry 1
         * true. */
        preproc.setEdge(index == 1U);
        preproc.setAzSigma(0.03125 + step);
        preproc.setElSigma(-0.015625 - step);
        preproc.setBackgroundNormalizer(123.4375 + step);
        preprocs.push_back(preproc);
    }

    irmel::CandidateObjectPreProcMessage message;
    message.setCandidateObjectHeader(header);
    message.setSensorInertialState(inertial);
    message.setCandidateObjectPreProcs(preprocs);
    return message;
}

class MockTrackChannel final : public irmel::TrackChannel {
public:
    explicit MockTrackChannel(std::string scenario) : scenario_{std::move(scenario)} {}
    ~MockTrackChannel() override
    {
        /* The pending-update producer must be joined before this channel dies;
         * the adapter guarantees this destructor runs only after the request
         * that owns the future has completed. */
        if (update_producer_.joinable()) update_producer_.join();
        if (response_producer_.joinable()) response_producer_.join();
        /* The asynchronous metadata producer must also be quiescent before the
         * channel dies: it is the callback-quiescence boundary for the
         * @Optional RequestSystemTrackData exactly as for the report. */
        if (request_producer_.joinable()) request_producer_.join();
        /* The candidate producer is likewise joined before destruction, so
         * this destructor remains the callback-quiescence boundary for the
         * @RequiredIfDetectCandidateObjects kind too. */
        if (candidate_producer_.joinable()) candidate_producer_.join();
        /* The PreProc producer is joined on the same basis, so this destructor
         * remains the callback-quiescence boundary for the @Optional
         * CandidateObjectPreProcMessage kind too. */
        if (preproc_producer_.joinable()) preproc_producer_.join();
        record("track_channel_destroyed");
    }
    mel::RequestFor<Return> sendKeepAliveRep() override { return {}; }
    mel::RequestFor<irmel::ChannelCommsTestRep> send(irmel::ChannelCommsTestReq) override
    { return {}; }
    Return registerBuffer(std::shared_ptr<irmel::Buffer>) override
    { return Return::NotSupported; }
    Return unregisterBuffer(std::shared_ptr<irmel::Buffer>) override
    { return Return::NotSupported; }
    Return enable() override
    {
        record("track_enabled");
        if (scenario_ == "track-enable-throw")
            throw std::runtime_error("mock Track enable exception");
        if (scenario_ == "track-enable-fail") return Return::Fail;
        enabled_ = true;
        return Return::Success;
    }
    Return disable() override
    {
        enabled_ = false;
        record("track_disabled");
        /* Deterministic late-callback lifetime coverage: the retained
         * IRSTTrackReport callback is invoked during teardown, after the public
         * metadata owner was closed. The ordered log proves the callback
         * returned before this channel was destroyed. */
        if (scenario_ == "track-report-late" && report_callback_) {
            const auto report = rich_track_report();
            record("track_late_callback_entered");
            report_callback_(*this, &report);
            record("track_late_callback_returned");
        }
        /* The same deterministic late-callback proof for the
         * @RequiredIfDetectCandidateObjects kind: the retained candidate
         * callback is invoked during teardown, after the public metadata owner
         * was closed. The ordered log proves the callback entered and returned
         * strictly before this channel was destroyed. */
        if (scenario_ == "track-candidate-late" && candidate_callback_) {
            const auto message = rich_candidate_object_message();
            record("track_candidate_late_callback_entered");
            candidate_callback_(*this, &message);
            record("track_candidate_late_callback_returned");
        }
        /* The same deterministic late-callback proof for the @Optional
         * CandidateObjectPreProcMessage kind: the retained PreProc callback is
         * invoked during teardown, after the public metadata owner was closed.
         * The ordered log proves the callback entered and returned strictly
         * before this channel was destroyed. */
        if (scenario_ == "track-preproc-late" && preproc_callback_) {
            const auto message = rich_candidate_preproc_message();
            record("track_preproc_late_callback_entered");
            preproc_callback_(*this, &message);
            record("track_preproc_late_callback_returned");
        }
        return scenario_ == "track-disable-fail" ? Return::Fail : Return::Success;
    }
    irmel::ChannelCapability getCapabilities() const override
    {
        if (scenario_ == "track-capability-throw")
            throw std::runtime_error("mock Track capability exception");
        auto value = rich_channel_capability();
        value.setChannelTypes(scenario_ == "track-capability-wrong" ?
            std::vector<irmel::ChannelType>{irmel::ChannelType::HealthAndStatus} :
            std::vector<irmel::ChannelType>{irmel::ChannelType::IRSTTrack});
        /* The advertised metadata set is assembled from one common base plus
         * the two conditionally supported kinds, so the reported capabilities
         * and the registerMetadataCallback answers always agree. Pinned
         * ChannelCapability: channelMetadataCapabilities is "the set of
         * Metadata types that are supported by a channel", and a registration
         * for a kind with no corresponding entry is expected to return
         * Return::NotSupported.
         *
         * - Default / non-PreProc scenarios omit CandidateObjectPreProcMessage
         *   and therefore answer NotSupported when the adapter attempts that
         *   @Optional registration; that refusal must stay non-fatal.
         * - The explicit PreProc scenarios advertise the capability and
         *   positively implement the callback, including the deliberate
         *   conflict/error/exception registration scenarios, which model a
         *   provider that claims the kind and then fails to register it.
         * - CandidateObjectMessage stays separately controlled by its own
         *   advertisement rule, so every pre-existing scenario keeps its exact
         *   previous candidate behavior. */
        std::set<irmel::ChannelMetadataCapabilityType> metadata{
            irmel::ChannelMetadataCapabilityType::IRSTTrackReport,
            irmel::ChannelMetadataCapabilityType::ChannelCommsTestRep};
        if (advertises_candidate_objects())
            metadata.insert(
                irmel::ChannelMetadataCapabilityType::CandidateObjectMessage);
        if (exercises_preproc())
            metadata.insert(
                irmel::ChannelMetadataCapabilityType::CandidateObjectPreProcMessage);
        value.setChannelMetadataCapabilities(metadata);
        return value;
    }
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&, const irmel::ChannelCommsTestRep *const)>) override
    { return Return::NotSupported; }

    /* The one positively implemented @Optional Track send. */
    mel::RequestFor<irmel::CommandStatus> send(
        irmel::SystemTrackDataResponse response) override
    {
        record("track_system_track_data_response_sent");
        if (!enabled_)
            throw std::logic_error("SystemTrackDataResponse sent before enable");
        /* Boolean coverage: the false/false scenario carries exactly the same
           distinctive numbers, so only the two bool values differ. */
        if (scenario_ == "track-response-flags-false")
            verify_rich_track_response(response, false, false);
        else if (scenario_ != "track-response-any")
            verify_rich_track_response(response, true, true);
        if (scenario_ == "track-response-send-throw")
            throw std::runtime_error("mock Track response send exception");

        /* A provider may invoke a registered metadata callback synchronously
         * from inside send(); prove the adapter does not deadlock. */
        if (report_callback_ && scenario_ == "track-response-reentrant") {
            const auto report = rich_track_report();
            record("track_response_send_callback_entered");
            report_callback_(*this, &report);
            record("track_response_send_callback_returned");
        }

        std::promise<mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>> promise;
        auto future = promise.get_future();
        if (scenario_ == "track-response-reject")
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{
                mel::Error{mel::ErrorCode::InvalidParameters,
                           std::string(510U, 'x') + "\xE2\x82\xAC" +
                           " Track system response rejected \xC2\xB5"}});
        else if (scenario_ == "track-response-unknown-error")
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{
                mel::Error{static_cast<mel::ErrorCode>(99U), "unknown"}});
        else if (scenario_ == "track-response-null-status")
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{
                std::shared_ptr<irmel::CommandStatus>{}});
        else if (scenario_ == "track-response-bad-state") {
            auto bad = rich_track_response_command_status();
            /* One past Cancelled: upstream declares no MaxExclusive value. */
            bad->setState(static_cast<irmel::CommandState>(5U));
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{bad});
        } else if (scenario_ == "track-response-bad-reason") {
            auto bad = rich_track_response_command_status();
            /* One past Alignment_Maneuver. */
            bad->setReasonID(static_cast<irmel::CannotComply>(47U));
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{bad});
        } else if (scenario_ == "track-response-bad-description") {
            auto bad = rich_track_response_command_status();
            bad->setReasonDescription(std::string{"bad\xC3\x28 utf8"});
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{bad});
        } else if (scenario_ == "track-response-status-rejected") {
            /* A successful future whose CommandStatus state is Rejected. This
             * is NOT an ErrorOr rejection and must stay AMS_MEL_OK. */
            auto rejected = std::make_shared<irmel::CommandStatus>();
            rejected->setCommandID(0xa1b2c3d4U);
            rejected->setState(irmel::CommandState::Rejected);
            rejected->setReasonID(irmel::CannotComply::InvalidInputParameter);
            rejected->setReasonDescription("Track system response rejected \xC2\xB5");
            promise.set_value(
                mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{rejected});
        } else if (scenario_ == "track-response-future-throw")
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error{"mock Track response future exception"}));
        else if (scenario_ == "track-response-pending" ||
                 scenario_ == "track-response-detach-fail" ||
                 scenario_ == "track-mixed-requests") {
            /* Deterministic pending completion: the background thread waits on
             * its own test-controlled barrier file rather than on a sleep. The
             * mixed-request scenario uses a separate barrier from the
             * TrackDataUpdate one so each future is released independently. */
            const char *barrier = std::getenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER");
            const std::string path = barrier ? barrier : std::string{};
            auto status = rich_track_response_command_status();
            response_producer_ = std::thread{
                [path, promise = std::move(promise), status]() mutable {
                    if (!path.empty()) wait_for_file(path);
                    record("track_response_completed");
                    promise.set_value(
                        mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{status});
                }};
        } else promise.set_value(
            mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{
                rich_track_response_command_status()});
        return future;
    }

    /* The one positively implemented @RequiredIfTrackUpdate Track send. */
    mel::RequestFor<irmel::CommandStatus> send(irmel::TrackDataUpdate update) override
    {
        record("track_data_update_sent");
        if (!enabled_) throw std::logic_error("TrackDataUpdate sent before enable");
        if (scenario_ != "track-update-any" && scenario_ != "track-mixed-requests")
            verify_rich_track_update(update);
        if (scenario_ == "track-update-send-throw")
            throw std::runtime_error("mock Track update send exception");

        /* A provider may invoke a registered metadata callback synchronously
         * from inside send(); prove the adapter does not deadlock. */
        if (report_callback_ && scenario_ == "track-update-reentrant") {
            const auto report = rich_track_report();
            record("track_update_send_callback_entered");
            report_callback_(*this, &report);
            record("track_update_send_callback_returned");
        }

        std::promise<mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>> promise;
        auto future = promise.get_future();
        if (scenario_ == "track-update-reject")
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{
                mel::Error{mel::ErrorCode::InvalidParameters,
                           std::string(510U, 'x') + "\xE2\x82\xAC" +
                           " Track update rejected \xC2\xB5"}});
        else if (scenario_ == "track-update-unknown-error")
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{
                mel::Error{static_cast<mel::ErrorCode>(99U), "unknown"}});
        else if (scenario_ == "track-update-null-status")
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{
                std::shared_ptr<irmel::CommandStatus>{}});
        else if (scenario_ == "track-update-bad-state") {
            auto bad = rich_track_command_status();
            /* One past Cancelled: upstream declares no MaxExclusive value. */
            bad->setState(static_cast<irmel::CommandState>(5U));
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{bad});
        } else if (scenario_ == "track-update-bad-reason") {
            auto bad = rich_track_command_status();
            /* One past Alignment_Maneuver. */
            bad->setReasonID(static_cast<irmel::CannotComply>(47U));
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{bad});
        } else if (scenario_ == "track-update-bad-description") {
            auto bad = rich_track_command_status();
            bad->setReasonDescription(std::string{"bad\xC3\x28 utf8"});
            promise.set_value(mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{bad});
        } else if (scenario_ == "track-update-status-rejected") {
            /* A successful future whose CommandStatus state is Rejected. This
             * is NOT an ErrorOr rejection and must stay AMS_MEL_OK. */
            auto rejected = std::make_shared<irmel::CommandStatus>();
            rejected->setCommandID(0xf0e1d2c3U);
            rejected->setState(irmel::CommandState::Rejected);
            rejected->setReasonID(irmel::CannotComply::InvalidInputParameter);
            rejected->setReasonDescription("Track update parameters rejected \xC2\xB5");
            promise.set_value(
                mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{rejected});
        } else if (scenario_ == "track-update-future-throw")
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error{"mock Track update future exception"}));
        else if (scenario_ == "track-update-pending" ||
                 scenario_ == "track-update-detach-fail" ||
                 scenario_ == "track-mixed-requests") {
            /* Deterministic pending completion: the background thread waits on
             * a test-controlled barrier file rather than on a sleep. */
            const char *barrier = std::getenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER");
            const std::string path = barrier ? barrier : std::string{};
            auto status = rich_track_command_status();
            update_producer_ = std::thread{
                [path, promise = std::move(promise), status]() mutable {
                    if (!path.empty()) wait_for_file(path);
                    record("track_update_completed");
                    promise.set_value(
                        mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{status});
                }};
        } else promise.set_value(
            mel::ErrorOr<std::shared_ptr<irmel::CommandStatus>>{
                rich_track_command_status()});
        return future;
    }
    /* The @RequiredIfDetectCandidateObjects CandidateObjectMessage callback is
     * now POSITIVELY implemented, so it is deliberately NOT counted as a
     * deferred-operation violation. The adapter only reaches this override when
     * the scenario advertised the capability. */
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&,
                           const irmel::CandidateObjectMessage *const)> callback)
        override
    {
        record("track_candidate_object_registered");
        if (scenario_ == "track-candidate-register-throw")
            throw std::runtime_error("mock CandidateObjectMessage registration exception");
        /* Advertised-but-refusing providers: the adapter must fail closed on
         * each of these, because the advertisement promised the type. */
        if (scenario_ == "track-candidate-register-not-supported")
            return Return::NotSupported;
        if (scenario_ == "track-candidate-register-fail") return Return::Fail;
        if (scenario_ == "track-candidate-register-unknown")
            return static_cast<Return>(99U);
        if (!callback) return Return::Fail;
        candidate_callback_ = std::move(callback);
        emit_synchronous_candidates();
        return Return::Success;
    }
    /* The @RequiredIfTrack Track report callback. The report is emitted
     * synchronously from inside registration, which is the hardest ordering the
     * facade must survive. */
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&, const irmel::IRSTTrackReport *const)> callback)
        override
    {
        record("track_report_registered");
        if (scenario_ == "track-report-register-throw")
            throw std::runtime_error("mock Track report registration exception");
        if (scenario_ == "track-report-register-fail") return Return::Fail;
        if (!callback) return Return::Fail;
        report_callback_ = std::move(callback);
        emit_synchronous_reports();
        return Return::Success;
    }
    /* The @Optional RequestSystemTrackData callback, positively implemented so
     * the inbound request has real positive evidence even though pinned Squall
     * cannot attach a Track channel at all. Like the report callback, the
     * emission happens synchronously from inside registration. */
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&,
                           const irmel::RequestSystemTrackData *const)> callback)
        override
    {
        record("track_request_system_track_data_registered");
        if (scenario_ == "track-request-register-throw")
            throw std::runtime_error("mock RequestSystemTrackData registration exception");
        /* Proves an optional-callback refusal never breaks the required one. */
        if (scenario_ == "track-request-register-not-supported")
            return Return::NotSupported;
        if (scenario_ == "track-request-register-fail") return Return::Fail;
        if (!callback) return Return::Fail;
        request_callback_ = std::move(callback);
        emit_synchronous_requests();
        return Return::Success;
    }
    /* The @Optional CandidateObjectPreProcMessage callback, positively
     * implemented as of task 029G. It is NOT gated on the advertised
     * capability set: the callback's own annotation is @Optional, so the
     * adapter always attempts registration and a NotSupported answer is a
     * legitimate refusal rather than a violation. Every pre-029G scenario
     * reaches the default branch below and answers NotSupported, which must
     * leave Metadata Open succeeding. */
    Return registerMetadataCallback(
        std::function<void(irmel::Channel&,
                           const irmel::CandidateObjectPreProcMessage *const)> callback)
        override
    {
        record("track_candidate_object_preproc_registered");
        if (scenario_ == "track-preproc-register-throw")
            throw std::runtime_error(
                "mock CandidateObjectPreProcMessage registration exception");
        if (scenario_ == "track-preproc-register-fail") return Return::Fail;
        if (scenario_ == "track-preproc-register-unknown")
            return static_cast<Return>(99U);
        /* Every scenario that does not positively exercise this @Optional kind
         * refuses it, exactly as a provider without CandidateObjectPreProc
         * support does. This must stay non-fatal. */
        if (!exercises_preproc()) return Return::NotSupported;
        if (!callback) return Return::Fail;
        preproc_callback_ = std::move(callback);
        emit_synchronous_preprocs();
        return Return::Success;
    }

private:
    /* Deterministic scenario-selected synchronous emission from inside
     * registerMetadataCallback. No thread and no sleep is involved. */
    void emit_synchronous_reports()
    {
        if (scenario_ == "track-report-null") {
            record("track_report_emitted_null");
            report_callback_(*this, nullptr);
            return;
        }
        if (scenario_ == "track-report-bad-state") {
            auto report = rich_track_report();
            /* One past Dropped: upstream declares no MaxExclusive value. */
            report.setState(static_cast<irmel::IrstTrackState>(4U));
            record("track_report_emitted_bad_state");
            report_callback_(*this, &report);
            return;
        }
        if (scenario_ == "track-report-bad-mode") {
            auto report = rich_track_report();
            /* One past Stare. */
            report.setMode(static_cast<irmel::IrstTrackMode>(3U));
            record("track_report_emitted_bad_mode");
            report_callback_(*this, &report);
            return;
        }
        if (scenario_ == "track-report-overflow") {
            /* Six reports into a capacity-2 queue proves DROP-INCOMING and
             * FIFO retention of the first two. activity_id counts the arrival
             * order so the retained pair is identifiable. */
            for (std::uint32_t index = 0; index < 6U; ++index) {
                auto report = rich_track_report();
                report.setActivityId(index);
                report_callback_(*this, &report);
            }
            record("track_report_emitted_six");
            return;
        }
        if (scenario_ == "track-report-none") return;
        /* First kind of the deterministic FOUR-kind FIFO scenario. The adapter
         * registers this callback first, so the report is queued first. */
        if (scenario_ == "track-preproc-mixed") {
            const auto report = rich_track_report();
            record("track_preproc_mixed_report_emitted");
            report_callback_(*this, &report);
            return;
        }
        /* Every other 029G PreProc scenario drives its own emission ordering
         * from the PreProc callback, which the adapter registers last, so the
         * shared counters and the shared queue isolate the optional kind
         * exactly. */
        if (exercises_preproc()) return;
        /* The 029E scenarios that exercise the @Optional RequestSystemTrackData
         * payload emit no report, so the shared counters and the shared queue
         * isolate the optional kind exactly. The register-refusal scenarios are
         * deliberately NOT listed: they must still deliver the required report
         * to prove an optional refusal leaves it working. The mixed scenario is
         * listed because its interleave is driven from the request callback,
         * which the adapter registers second. */
        if (scenario_ == "track-request-rich" || scenario_ == "track-request-zero" ||
            scenario_ == "track-request-null" ||
            scenario_ == "track-request-overflow" ||
            scenario_ == "track-request-async" ||
            scenario_ == "track-mixed-requests" ||
            scenario_ == "track-metadata-mixed") return;
        /* Every 029F candidate scenario drives its own emission ordering from
         * the candidate/request callbacks, which the adapter registers after
         * the report callback, so no report is emitted from here. */
        if (advertises_candidate_objects()) return;
        const auto report = rich_track_report();
        record("track_report_emitted_rich");
        report_callback_(*this, &report);
    }

    /* Deterministic scenario-selected synchronous emission of the @Optional
     * RequestSystemTrackData from inside registerMetadataCallback. */
    void emit_synchronous_requests()
    {
        if (scenario_ == "track-request-null") {
            record("track_request_emitted_null");
            request_callback_(*this, nullptr);
            return;
        }
        if (scenario_ == "track-request-overflow") {
            /* Six requests into a capacity-2 queue proves the optional kind
             * obeys the same DROP-INCOMING policy and shares the one queue.
             * requestId counts the arrival order. */
            for (std::uint32_t index = 0; index < 6U; ++index) {
                auto request = rich_request_system_track_data();
                request.setRequestId(index);
                request_callback_(*this, &request);
            }
            record("track_request_emitted_six");
            return;
        }
        if (scenario_ == "track-request-zero") {
            /* A default-constructed request is well formed: upstream declares
             * no field as optional and no value as invalid. */
            const irmel::RequestSystemTrackData request;
            record("track_request_emitted_zero");
            request_callback_(*this, &request);
            return;
        }
        /* The mixed scenario interleaves the required report kind and the
         * optional request kind through the one shared queue, proving both the
         * FIFO order across kinds and that each event carries only its own
         * payload. The report callback is registered first by the adapter. */
        if (scenario_ == "track-metadata-mixed") {
            const auto request = rich_request_system_track_data();
            request_callback_(*this, &request);
            if (report_callback_) {
                const auto report = rich_track_report();
                report_callback_(*this, &report);
            }
            request_callback_(*this, &request);
            record("track_request_emitted_mixed");
            return;
        }
        if (scenario_ == "track-request-rich") {
            const auto request = rich_request_system_track_data();
            record("track_request_emitted_rich");
            request_callback_(*this, &request);
            return;
        }
        /* Third and last kind of the deterministic three-kind FIFO scenario.
         * The report and candidate were already emitted from the earlier
         * registrations, so the resulting queue order is exactly
         * report, candidate, request. */
        if (scenario_ == "track-candidate-mixed") {
            const auto request = rich_request_system_track_data();
            request_callback_(*this, &request);
            record("track_candidate_mixed_request_emitted");
            return;
        }
        /* Third kind of the deterministic FOUR-kind FIFO scenario. The report
         * and the candidate message were already emitted from the earlier
         * registrations, and the PreProc message follows from the last one, so
         * the resulting queue order is exactly
         * report, candidate, request, preproc. */
        if (scenario_ == "track-preproc-mixed") {
            const auto request = rich_request_system_track_data();
            request_callback_(*this, &request);
            record("track_preproc_mixed_request_emitted");
            return;
        }
        /* The cross-mechanism scenario delivers BOTH metadata kinds while two
         * RequestFor futures are outstanding, so a test can prove that inbound
         * metadata never perturbs async request accounting. */
        if (scenario_ == "track-mixed-requests") {
            const auto request = rich_request_system_track_data();
            request_callback_(*this, &request);
            if (report_callback_) {
                const auto report = rich_track_report();
                report_callback_(*this, &report);
            }
            record("track_mixed_metadata_emitted");
            return;
        }
        /* Asynchronous delivery: the request arrives on a separate provider
         * thread strictly AFTER registerMetadataCallback has returned, which is
         * the ordering a real provider uses. The test releases the barrier, so
         * no sleep is involved and the ordering is deterministic. */
        if (scenario_ == "track-request-async") {
            const char *barrier = std::getenv("AMS_MEL_TEST_TRACK_REQUEST_BARRIER");
            const std::string path = barrier ? barrier : std::string{};
            auto callback = request_callback_;
            request_producer_ = std::thread{[this, path, callback]() {
                if (!path.empty()) wait_for_file(path);
                const auto request = rich_request_system_track_data();
                record("track_request_emitted_async");
                callback(*this, &request);
                record("track_request_async_returned");
            }};
            return;
        }
        /* Unlike the required report callback, the optional request emits
         * nothing by default. Every pre-029E scenario must keep observing
         * exactly the events it already expected, so this kind only ever
         * appears when a test explicitly selects it. */
    }

    /* Every scenario that positively exercises the
     * @RequiredIfDetectCandidateObjects callback, including the
     * advertised-but-refusing ones. Anything else keeps its historical
     * advertised set exactly. */
    bool advertises_candidate_objects() const
    {
        /* The four-kind FIFO scenario additionally needs the
         * @RequiredIfDetectCandidateObjects callback registered, so it
         * advertises the capability too. */
        return scenario_.rfind("track-candidate", 0U) == 0U ||
               scenario_ == "track-preproc-mixed";
    }

    /* Deterministic scenario-selected synchronous emission of
     * CandidateObjectMessage from inside registerMetadataCallback. */
    void emit_synchronous_candidates()
    {
        if (scenario_ == "track-candidate-null") {
            record("track_candidate_emitted_null");
            candidate_callback_(*this, nullptr);
            return;
        }
        if (scenario_ == "track-candidate-too-many") {
            /* One past MAX_CANDIDATE_OBJECTS: the count cannot describe a
             * prefix of the published 900-entry array. */
            auto message = rich_candidate_object_message();
            auto header = message.getHeader();
            header.setNumberOfCOs(901U);
            message.setHeader(header);
            record("track_candidate_emitted_too_many");
            candidate_callback_(*this, &message);
            return;
        }
        if (scenario_ == "track-candidate-bad-region") {
            /* One past MASK: upstream declares no MaxExclusive value. */
            auto message = rich_candidate_object_message();
            auto header = message.getHeader();
            std::vector<irmel::HotRegion> regions = header.getHotRegions();
            regions[1].setType(static_cast<irmel::HotRegionTypeEnum>(4U));
            header.setHotRegions(regions);
            message.setHeader(header);
            record("track_candidate_emitted_bad_region");
            candidate_callback_(*this, &message);
            return;
        }
        if (scenario_ == "track-candidate-overflow") {
            /* Six messages into a capacity-2 queue proves this kind obeys the
             * same DROP-INCOMING policy on the same shared queue.
             * stackFrameIndex counts arrival order. */
            for (std::uint16_t index = 0; index < 6U; ++index) {
                auto message = rich_candidate_object_message();
                auto header = message.getHeader();
                header.setStackFrameIndex(index);
                message.setHeader(header);
                candidate_callback_(*this, &message);
            }
            record("track_candidate_emitted_six");
            return;
        }
        if (scenario_ == "track-candidate-mixed") {
            /* Deterministic three-kind FIFO: report, candidate, request. The
             * report callback is registered first and the request callback
             * last, so this ordering is driven from here and from
             * emit_synchronous_requests. */
            if (report_callback_) {
                const auto report = rich_track_report();
                report_callback_(*this, &report);
            }
            const auto message = rich_candidate_object_message();
            candidate_callback_(*this, &message);
            record("track_candidate_emitted_mixed");
            return;
        }
        /* Asynchronous delivery strictly AFTER registerMetadataCallback has
         * returned, on a separate provider thread, which is the ordering a real
         * provider uses. The test releases a barrier file, so no sleep is
         * involved and the ordering stays deterministic. */
        if (scenario_ == "track-candidate-async") {
            const char *barrier = std::getenv("AMS_MEL_TEST_TRACK_CANDIDATE_BARRIER");
            const std::string path = barrier ? barrier : std::string{};
            auto callback = candidate_callback_;
            candidate_producer_ = std::thread{[this, path, callback]() {
                if (!path.empty()) wait_for_file(path);
                const auto message = rich_candidate_object_message();
                record("track_candidate_emitted_async");
                callback(*this, &message);
                record("track_candidate_async_returned");
            }};
            return;
        }
        /* The late-callback and event-lifetime scenarios emit from disable(),
         * after the public metadata owner has been closed. */
        if (scenario_ == "track-candidate-late") return;
        if (scenario_ == "track-candidate-lifetime") {
            const auto message = rich_candidate_object_message();
            record("track_candidate_emitted_lifetime");
            candidate_callback_(*this, &message);
            return;
        }
        if (scenario_ == "track-candidate-rich") {
            const auto message = rich_candidate_object_message();
            record("track_candidate_emitted_rich");
            candidate_callback_(*this, &message);
            return;
        }
        /* Second kind of the deterministic FOUR-kind FIFO scenario. */
        if (scenario_ == "track-preproc-mixed") {
            const auto message = rich_candidate_object_message();
            record("track_preproc_mixed_candidate_emitted");
            candidate_callback_(*this, &message);
            return;
        }
    }

    /* Every scenario that positively exercises the @Optional
     * CandidateObjectPreProcMessage callback. Anything else refuses it with
     * NotSupported, which must remain non-fatal. The register-failure
     * scenarios are handled before this check, so they are deliberately not
     * listed. */
    bool exercises_preproc() const
    { return scenario_.rfind("track-preproc", 0U) == 0U; }

    /* Deterministic scenario-selected synchronous emission of
     * CandidateObjectPreProcMessage from inside registerMetadataCallback. No
     * thread and no sleep is involved. */
    void emit_synchronous_preprocs()
    {
        if (scenario_ == "track-preproc-null") {
            record("track_preproc_emitted_null");
            preproc_callback_(*this, nullptr);
            return;
        }
        if (scenario_ == "track-preproc-bad-region") {
            /* One past MASK: upstream declares no MaxExclusive value. */
            auto message = rich_candidate_preproc_message();
            auto header = message.getCandidateObjectHeader();
            std::vector<irmel::HotRegion> regions = header.getHotRegions();
            regions[1].setType(static_cast<irmel::HotRegionTypeEnum>(4U));
            header.setHotRegions(regions);
            message.setCandidateObjectHeader(header);
            record("track_preproc_emitted_bad_region");
            preproc_callback_(*this, &message);
            return;
        }
        if (scenario_ == "track-preproc-overflow") {
            /* Six messages into a capacity-2 queue proves this kind obeys the
             * same DROP-INCOMING policy on the same shared queue.
             * stackFrameIndex counts arrival order. */
            for (std::uint16_t index = 0; index < 6U; ++index) {
                auto message = rich_candidate_preproc_message();
                auto header = message.getCandidateObjectHeader();
                header.setStackFrameIndex(index);
                message.setCandidateObjectHeader(header);
                preproc_callback_(*this, &message);
            }
            record("track_preproc_emitted_six");
            return;
        }
        if (scenario_ == "track-preproc-mixed") {
            /* Deterministic FOUR-kind FIFO: report, candidate, request,
             * preproc. The adapter registers report first, then candidate,
             * then request, then preproc, so the first three were already
             * emitted from their own registrations and this is the last. */
            const auto message = rich_candidate_preproc_message();
            preproc_callback_(*this, &message);
            record("track_preproc_emitted_mixed");
            return;
        }
        /* Asynchronous delivery strictly AFTER registerMetadataCallback has
         * returned, on a separate provider thread, which is the ordering a
         * real provider uses. The test releases a barrier file, so no sleep is
         * involved and the ordering stays deterministic. */
        if (scenario_ == "track-preproc-async") {
            const char *barrier = std::getenv("AMS_MEL_TEST_TRACK_PREPROC_BARRIER");
            const std::string path = barrier ? barrier : std::string{};
            auto callback = preproc_callback_;
            preproc_producer_ = std::thread{[this, path, callback]() {
                if (!path.empty()) wait_for_file(path);
                const auto message = rich_candidate_preproc_message();
                record("track_preproc_emitted_async");
                callback(*this, &message);
                record("track_preproc_async_returned");
            }};
            return;
        }
        /* The late-callback scenario emits from disable(), after the public
         * metadata owner has been closed. */
        if (scenario_ == "track-preproc-late") return;
        if (scenario_ == "track-preproc-lifetime") {
            const auto message = rich_candidate_preproc_message();
            record("track_preproc_emitted_lifetime");
            preproc_callback_(*this, &message);
            return;
        }
        if (scenario_ == "track-preproc-rich") {
            const auto message = rich_candidate_preproc_message();
            record("track_preproc_emitted_rich");
            preproc_callback_(*this, &message);
            return;
        }
    }

    std::string scenario_;
    bool enabled_{};
    std::thread update_producer_;
    std::thread response_producer_;
    std::thread request_producer_;
    std::function<void(irmel::Channel&, const irmel::IRSTTrackReport *const)>
        report_callback_;
    std::function<void(irmel::Channel&, const irmel::RequestSystemTrackData *const)>
        request_callback_;
    std::thread candidate_producer_;
    std::function<void(irmel::Channel&, const irmel::CandidateObjectMessage *const)>
        candidate_callback_;
    std::thread preproc_producer_;
    std::function<void(irmel::Channel&,
                       const irmel::CandidateObjectPreProcMessage *const)>
        preproc_callback_;
};

class MockControl final : public irmel::Control {
public:
    explicit MockControl(std::string instance) : instance_{std::move(instance)}
    {
        irmel::ChannelCapability capability;
        if (instance_.rfind("health-",0)==0)
            capability.setChannelTypes({irmel::ChannelType::HealthAndStatus});
        else if (instance_.rfind("instr-",0)==0)
            capability.setChannelTypes({irmel::ChannelType::Instrumentation});
        else if (instance_.rfind("track-",0)==0)
            capability.setChannelTypes({irmel::ChannelType::IRSTTrack});
        else if (instance_ != "c2-control-capability-wrong")
            capability.setChannelTypes({irmel::ChannelType::CommandAndControl});
        capabilities_.push_back(std::move(capability));
    }
    ~MockControl() override
    {
        if (callback_buffers.load() != buffer_releases.load()) std::abort();
        record("control_destroyed");
    }
    Return init(const std::string& aperture) override
    {
        record("init_called");
        if (instance_ == "bad-alloc-init") throw std::bad_alloc{};
        if (instance_ == "throw-init") throw std::runtime_error("mock init exception");
        if (instance_ == "init-fail" || aperture == "reject") return Return::Fail;
        initialized_ = true; return Return::Success;
    }
    const std::vector<irmel::ChannelCapability>& getCapabilities() const override
    { return capabilities_; }
    mel::VersionInfo getVersionInfo() const override
    {
        if (!initialized_) throw std::logic_error("mock not initialized");
        if (instance_ == "throw-version") throw std::runtime_error("mock version exception");
        if (instance_ == "throw-version-utf8") throw std::runtime_error("mock \xC2\xB5 exception");
        if (instance_ == "throw-version-oversized")
            throw std::runtime_error(std::string(5000U, 'x') + "\xC2\xB5");
        if (instance_ == "bad-alloc-version") throw std::bad_alloc{};
        if (instance_ == "invalid-utf8-version") return {1, 2, std::string{"bad\xC3\x28", 5}, "unchanged"};
        if (instance_ == "nul-version") return {1, 2, std::string{"bad\0vendor", 10}, "unchanged"};
        return {UINT32_C(0x12345678), UINT32_C(0x90abcdef),
                "Mock IR Provider \xC2\xB5", "Deterministic task 001 provider"};
    }
    std::shared_ptr<irmel::Channel> attachChannel(const irmel::Config& config) override
    {
        record("channel_attached");
        if (instance_ == "attach-throw") throw std::runtime_error("mock attach exception");
        if (instance_ == "attach-null" || instance_ == "c2-attach-null") return {};
        if (config.getChannelType() == irmel::ChannelType::IRSTTrack) {
            if (instance_ == "track-attach-null") return {};
            if (instance_ == "track-wrong-concrete" ||
                instance_ == "track-open-detach-fail")
                return std::make_shared<MockHealthStatusChannel>(instance_);
            if (instance_ == "track-config") {
                const auto& channel_id = config.getChanID();
                const auto& platform = config.getPlatform();
                const auto& location = config.getSensorLocation();
                for (std::size_t i = 0; i < mel::UUID_SIZE; ++i)
                    if (channel_id.getUUID()[i] != i ||
                        platform.getUUID()[i] != static_cast<std::uint8_t>(0xf0U + i))
                        throw std::runtime_error("mock Track ID conversion mismatch");
                if (channel_id.getDescriptiveLabel() != "IR track channel" ||
                    platform.getDescriptiveLabel() != "test platform" ||
                    location.getOffsetX() != 1.25 || location.getOffsetY() != -2.5 ||
                    location.getOffsetZ() != 3.75 ||
                    location.getLocationId().getKey() != "station-1" ||
                    location.getLocationId().getSystemName() != "mock-aircraft" ||
                    config.getImgLstnr())
                    throw std::runtime_error("mock Track configuration conversion mismatch");
            }
            return std::make_shared<MockTrackChannel>(instance_);
        }
        if (config.getChannelType() == irmel::ChannelType::Instrumentation) {
            if (instance_ == "instr-attach-null") return {};
            if (instance_ == "instr-wrong-type")
                return std::make_shared<MockHealthStatusChannel>(instance_);
            if (instance_ == "instr-config") {
                const auto& channel_id = config.getChanID();
                const auto& platform = config.getPlatform();
                const auto& location = config.getSensorLocation();
                for (std::size_t i = 0; i < mel::UUID_SIZE; ++i)
                    if (channel_id.getUUID()[i] != i ||
                        platform.getUUID()[i] != static_cast<std::uint8_t>(0xf0U + i))
                        throw std::runtime_error("mock Instrumentation ID conversion mismatch");
                if (channel_id.getDescriptiveLabel() != "IR instrumentation channel" ||
                    platform.getDescriptiveLabel() != "test platform" ||
                    location.getOffsetX() != 1.25 || location.getOffsetY() != -2.5 ||
                    location.getOffsetZ() != 3.75 ||
                    location.getLocationId().getKey() != "station-1" ||
                    location.getLocationId().getSystemName() != "mock-aircraft" ||
                    config.getImgLstnr())
                    throw std::runtime_error(
                        "mock Instrumentation configuration conversion mismatch");
            }
            return std::make_shared<MockInstrumentationChannel>(instance_);
        }
        if (config.getChannelType() == irmel::ChannelType::HealthAndStatus)
            return std::make_shared<MockHealthStatusChannel>(instance_);
        if (config.getChannelType() == irmel::ChannelType::CommandAndControl) {
            if (instance_ == "c2-wrong-type")
                return std::make_shared<MockImageChannel>(instance_, config.getImgLstnr());
            if (instance_ == "c2-config") {
                const auto& channel_id = config.getChanID();
                const auto& platform = config.getPlatform();
                const auto& location = config.getSensorLocation();
                for (std::size_t i = 0; i < mel::UUID_SIZE; ++i)
                    if (channel_id.getUUID()[i] != i ||
                        platform.getUUID()[i] != static_cast<std::uint8_t>(0xf0U + i))
                        throw std::runtime_error("mock C2 ID conversion mismatch");
                if (channel_id.getDescriptiveLabel() != "IR C2 channel" ||
                    platform.getDescriptiveLabel() != "test platform" ||
                    location.getOffsetX() != 1.25 || location.getOffsetY() != -2.5 ||
                    location.getOffsetZ() != 3.75 ||
                    location.getLocationId().getKey() != "station-1" ||
                    location.getLocationId().getSystemName() != "mock-aircraft" ||
                    config.getImgLstnr())
                    throw std::runtime_error("mock C2 configuration conversion mismatch");
            }
            return std::make_shared<MockC2Channel>(instance_);
        }
        if (config.getChannelType() != irmel::ChannelType::IRSTImage) return {};
        if (instance_ == "success") {
            const auto& channel_id = config.getChanID();
            const auto& platform = config.getPlatform();
            const auto& location = config.getSensorLocation();
            for (std::size_t i = 0; i < mel::UUID_SIZE; ++i) {
                if (channel_id.getUUID()[i] != i ||
                    platform.getUUID()[i] != static_cast<std::uint8_t>(0xf0U + i))
                    throw std::runtime_error("mock ID conversion mismatch");
            }
            if (channel_id.getDescriptiveLabel() != "IR image channel" ||
                platform.getDescriptiveLabel() != "test platform" ||
                location.getOffsetX() != 1.25 || location.getOffsetY() != -2.5 ||
                location.getOffsetZ() != 3.75 || location.getLocationId().getKey() != "station-1" ||
                location.getLocationId().getSystemName() != "mock-aircraft")
                throw std::runtime_error("mock configuration conversion mismatch");
        }
        {
            std::lock_guard lock{callback_barrier->mutex};
            callback_barrier->callback_inside = false;
            callback_barrier->disable_called = false;
        }
        return std::make_shared<MockImageChannel>(instance_, config.getImgLstnr());
    }
    Return detachChannel(std::shared_ptr<irmel::Channel> channel) override
    {
        if (!std::dynamic_pointer_cast<MockImageChannel>(channel) &&
            !std::dynamic_pointer_cast<MockC2Channel>(channel) &&
            !std::dynamic_pointer_cast<MockHealthStatusChannel>(channel) &&
            !std::dynamic_pointer_cast<MockInstrumentationChannel>(channel) &&
            !std::dynamic_pointer_cast<MockTrackChannel>(channel)) std::abort();
        if ((instance_ == "detach-fail" ||
             /* Deterministic Image Navigation completion-vs-Close corrective
                regression: the first detach fails exactly once, a later Close
                retry detaches successfully. */
             instance_ == "navigation-hold-detach-fail" ||
             instance_ == "c2-detach-fail" ||
             instance_ == "health-detach-fail" ||
             instance_ == "instr-detach-fail" ||
             instance_ == "track-detach-fail" ||
             instance_ == "track-update-detach-fail" ||
             instance_ == "track-response-detach-fail" ||
             instance_ == "track-open-detach-fail") && !detach_failed_) {
            detach_failed_ = true;
            record("channel_detach_failed");
            return Return::Fail;
        }
        record("channel_detached"); return Return::Success;
    }
private:
    std::string instance_;
    bool initialized_{};
    bool detach_failed_{};
    std::vector<irmel::ChannelCapability> capabilities_;
};
struct UnloadRecorder { ~UnloadRecorder() { record("library_unloaded"); } } unload_recorder;
} // namespace

/* Test-only deterministic backpressure control surface. These are NOT part of
 * the MEL provider interface and are not used by the production facade; they
 * exist so the Task 030B backpressure test can assert real provider buffer
 * reuse with explicit state instead of sleeping. */
extern "C" __attribute__((visibility("default")))
unsigned long ams_mel_mock_pool_available(void)
{
    std::shared_ptr<BufferPool> pool;
    { std::lock_guard lock{active_pool_mutex}; pool = active_pool.lock(); }
    if (!pool) return 0UL;
    std::lock_guard held{pool->mutex};
    return static_cast<unsigned long>(pool->available.size());
}

extern "C" __attribute__((visibility("default")))
unsigned long ams_mel_mock_pool_produced(void)
{
    std::shared_ptr<BufferPool> pool;
    { std::lock_guard lock{active_pool_mutex}; pool = active_pool.lock(); }
    if (!pool) return 0UL;
    std::lock_guard held{pool->mutex};
    return pool->produced;
}

extern "C" __attribute__((visibility("default")))
unsigned long ams_mel_mock_pool_starved(void)
{
    std::shared_ptr<BufferPool> pool;
    { std::lock_guard lock{active_pool_mutex}; pool = active_pool.lock(); }
    if (!pool) return 0UL;
    std::lock_guard held{pool->mutex};
    return pool->starved;
}

/* Requests exactly one production cycle and blocks until the provider has
 * finished it, whether it produced a frame or starved. No sleeping. */
extern "C" __attribute__((visibility("default")))
int ams_mel_mock_pool_produce_once(void)
{
    std::shared_ptr<BufferPool> pool;
    {
        std::lock_guard lock{active_pool_mutex};
        pool = active_pool.lock();
    }
    if (!pool) return 0;
    unsigned target = 0U;
    {
        std::lock_guard lock{pool->mutex};
        target = ++pool->requested;
    }
    pool->ready.notify_all();
    std::unique_lock lock{pool->mutex};
    /* Bounded so a test can never hang if the channel is torn down mid-cycle;
     * the wait is still driven by explicit provider state, never by sleeping
     * for a fixed duration and hoping. */
    const bool finished = pool->ready.wait_for(lock, std::chrono::seconds{10},
                                               [&] { return pool->completed >= target; });
    return finished ? 1 : 0;
}

extern "C" __attribute__((visibility("default")))
void ams_mel_mock_pool_reset(void)
{
    std::lock_guard lock{active_pool_mutex};
    active_pool.reset();
}

extern "C" __attribute__((visibility("default")))
std::shared_ptr<API_Manager> getAPI_Manager(const std::string& instance)
{
    record("manager_factory_called");
    if (instance == "fail-diagnostic-allocation") { (void)setenv("AMS_MEL_TEST_FAIL_ALLOCATION", "1", 1); throw DiagnosticAllocationFailure{}; }
    if (instance == "invalid-diagnostic") throw InvalidDiagnostic{};
    if (instance == "bad-alloc-manager") throw std::bad_alloc{};
    if (instance == "throw-manager") throw std::runtime_error("mock manager exception");
    if (instance == "null-manager") return {};
    return std::make_shared<MockManager>();
}
extern "C" __attribute__((visibility("default")))
std::shared_ptr<ams::iface::irmel::Control> getControl(
    std::string_view instance, std::shared_ptr<API_Manager>)
{
    record("control_factory_called");
    if (instance == "bad-alloc-control") throw std::bad_alloc{};
    if (instance == "throw-control") throw std::runtime_error("mock control exception");
    if (instance == "null-control") return {};
    return std::make_shared<MockControl>(std::string{instance});
}
extern "C" __attribute__((visibility("default")))
std::shared_ptr<ams::iface::irmel::Buffer> getBuffer(
    std::string_view instance, std::shared_ptr<API_Manager>)
{
    record("buffer_factory_called");
    if (instance == "buffer-factory-null") return {};
    if (instance == "buffer-factory-bad-alloc") throw std::bad_alloc{};
    return std::make_shared<MockBuffer>(std::string{instance}, callback_barrier);
}
