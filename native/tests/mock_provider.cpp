#include <irmel/library/image/ImageChannel.h>
#include <irmel/library/c2/C2Channel.h>
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
#include <stdexcept>
#include <string>
#include <string_view>
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

struct CallbackBarrier {
    std::mutex mutex;
    std::condition_variable ready;
    bool callback_inside{false};
    bool disable_called{false};
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
    Return release() override
    {
        if (!outstanding_.exchange(false)) std::abort();
        ++buffer_releases;
        record("buffer_released");
        if (scenario_ == "release-fail" || scenario_ == "release-fail-blocked")
            return Return::Fail;
        if (scenario_ == "release-throw")
            throw std::runtime_error("mock release exception");
        return Return::Success;
    }
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
};

#define UNSUPPORTED_CALLBACK(Type) \
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const Type *const)>) override \
    { return Return::NotSupported; }

class MockImageChannel final : public irmel::ImageChannel {
public:
    MockImageChannel(std::string scenario, std::shared_ptr<irmel::ImageListener> listener)
        : scenario_{std::move(scenario)}, listener_{std::move(listener)},
          barrier_{callback_barrier} {}
    ~MockImageChannel() override
    {
        stopping_ = true;
        {
            std::lock_guard lock{barrier_->mutex};
            barrier_->disable_called = true;
            barrier_->ready.notify_all();
        }
        if (producer_.joinable()) producer_.join();
        record("callbacks_quiesced_by_channel_destruction");
        buffers_.clear();
        record("channel_destroyed");
    }
    mel::RequestFor<Return> sendKeepAliveRep() override { return {}; }
    mel::RequestFor<irmel::ChannelCommsTestRep> send(irmel::ChannelCommsTestReq) override { return {}; }
    mel::RequestFor<irmel::CameraCommandResp> send(irmel::CameraCommand) override { return {}; }
    mel::RequestFor<irmel::NavigationReportResp> send(mel::NavigationReport) override { return {}; }
    Return registerBuffer(std::shared_ptr<irmel::Buffer> buffer) override
    {
        record("buffer_registered");
        if (scenario_ == "register-fail") return Return::Fail;
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
        if (scenario_ == "capability-throw")
            throw std::runtime_error("mock capability exception");
        irmel::ChannelCapability capability;
        capability.setFormat(scenario_ == "capability-format" ?
                             irmel::PixelFormat::RGB : irmel::PixelFormat::Mono);
        capability.setBitDepth(scenario_ == "capability-depth" ? 16U : 8U);
        capability.setNumberOfBands(scenario_ == "capability-bands" ? 2U : 1U);
        return capability;
    }
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const irmel::ChannelCommsTestRep *const)>) override
    { return Return::NotSupported; }
    UNSUPPORTED_CALLBACK(irmel::BadPixelList)
    UNSUPPORTED_CALLBACK(irmel::OpticalDistortionMap)
    UNSUPPORTED_CALLBACK(irmel::LineOfSightReport)
    UNSUPPORTED_CALLBACK(irmel::LineOfSightQuaternion)
    UNSUPPORTED_CALLBACK(irmel::LineOfSightEuler)
    UNSUPPORTED_CALLBACK(irmel::CameraCommandResp)
    UNSUPPORTED_CALLBACK(irmel::NavigationReportResp)
    Return registerMetadataCallback(std::function<void(irmel::Channel&, const irmel::LOS3D_KinematicsType *const)> const&) override
    { return Return::NotSupported; }
    UNSUPPORTED_CALLBACK(irmel::CandidateObjectMessage)
    UNSUPPORTED_CALLBACK(irmel::CandidateObjectPreProcMessage)
    UNSUPPORTED_CALLBACK(irmel::NUC_TempData)
private:
    void produce()
    {
        const unsigned count = (scenario_ == "idle" ||
            (scenario_.rfind("c2-", 0U) == 0U && scenario_ != "c2-coexist")) ? 0U :
            (scenario_ == "overflow" ? 20U : 3U);
        for (unsigned id = 1; id <= count; ++id) {
            if (stopping_ && scenario_ != "shutdown-callback") break;
            if (buffers_.empty()) break;
            auto buffer = std::dynamic_pointer_cast<MockBuffer>(buffers_[(id - 1U) % buffers_.size()]);
            if (!buffer || buffer->size() < 12U) break;
            for (std::size_t i = 0; i < 12U; ++i)
                buffer->data()[i] = static_cast<unsigned char>(id * 16U + i);
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
            record("callback_entered");
            if (scenario_ == "shutdown-callback")
                std::this_thread::sleep_for(std::chrono::milliseconds{20});
            if (scenario_ == "null-buffer") listener_->onImage(*this, header, {});
            else {
                buffer->begin_callback();
                ++callback_buffers;
                listener_->onImage(*this, header, buffer);
            }
            record("callback_returned");
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
    std::shared_ptr<CallbackBarrier> barrier_;
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

class MockControl final : public irmel::Control {
public:
    explicit MockControl(std::string instance) : instance_{std::move(instance)}
    {
        irmel::ChannelCapability capability;
        if (instance_ != "c2-control-capability-wrong")
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
            !std::dynamic_pointer_cast<MockC2Channel>(channel)) std::abort();
        if ((instance_ == "detach-fail" || instance_ == "c2-detach-fail") && !detach_failed_) {
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
