#include <irmel/library/irmel-types/Control.h>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

class DiagnosticAllocationFailure final : public std::exception {
public:
    ~DiagnosticAllocationFailure() override
    {
        (void)unsetenv("AMS_MEL_TEST_FAIL_ALLOCATION");
    }

    const char *what() const noexcept override
    {
        return "provider exception text intentionally longer than small-string optimization";
    }
};

class InvalidDiagnostic final : public std::exception {
public:
    const char *what() const noexcept override
    {
        return "invalid \xC3\x28 diagnostic";
    }
};

void record(const char *event)
{
    const char *path = std::getenv("AMS_MEL_TEST_LIFETIME_LOG");
#ifdef AMS_MEL_TEST_LIFETIME_LOG
    if (path == nullptr) {
        path = AMS_MEL_TEST_LIFETIME_LOG;
    }
#endif
    if (path != nullptr) {
        std::ofstream stream(path, std::ios::app);
        stream << event << '\n';
    }
}

class MockManager final : public API_Manager {
public:
    ~MockManager() override { record("manager_destroyed"); }
};

class MockControl final : public ams::iface::irmel::Control {
public:
    explicit MockControl(std::string instance) : instance_{std::move(instance)} {}
    ~MockControl() override { record("control_destroyed"); }

    ams::iface::irmel::Return init(const std::string& aperture) override
    {
        record("init_called");
        if (instance_ == "bad-alloc-init") {
            throw std::bad_alloc{};
        }
        if (instance_ == "throw-init") {
            throw std::runtime_error("mock init exception");
        }
        if (instance_ == "init-fail" || aperture == "reject") {
            return ams::iface::irmel::Return::Fail;
        }
        initialized_ = true;
        return ams::iface::irmel::Return::Success;
    }

    const std::vector<ams::iface::irmel::ChannelCapability>&
    getCapabilities() const override
    {
        throw std::logic_error("mock capabilities unsupported");
    }

    ams::iface::mel::VersionInfo getVersionInfo() const override
    {
        if (!initialized_) {
            throw std::logic_error("mock not initialized");
        }
        if (instance_ == "throw-version") {
            throw std::runtime_error("mock version exception");
        }
        if (instance_ == "throw-version-utf8") {
            throw std::runtime_error("mock \xC2\xB5 exception");
        }
        if (instance_ == "bad-alloc-version") {
            throw std::bad_alloc{};
        }
        if (instance_ == "invalid-utf8-version") {
            return {1, 2, std::string{"bad\xC3\x28", 5}, "unchanged"};
        }
        if (instance_ == "nul-version") {
            return {1, 2, std::string{"bad\0vendor", 10}, "unchanged"};
        }
        return {UINT32_C(0x12345678), UINT32_C(0x90abcdef),
                "Mock IR Provider \xC2\xB5", "Deterministic task 001 provider"};
    }

    std::shared_ptr<ams::iface::irmel::Channel>
    attachChannel(const ams::iface::irmel::Config&) override
    {
        throw std::logic_error("mock channel attachment unsupported");
    }

    ams::iface::irmel::Return detachChannel(
        std::shared_ptr<ams::iface::irmel::Channel>) override
    {
        return ams::iface::irmel::Return::NotSupported;
    }

private:
    std::string instance_;
    bool initialized_{false};
};

struct UnloadRecorder {
    ~UnloadRecorder() { record("library_unloaded"); }
} unload_recorder;

} // namespace

extern "C" __attribute__((visibility("default")))
std::shared_ptr<API_Manager> getAPI_Manager(const std::string& instance)
{
    record("manager_factory_called");
    if (instance == "fail-diagnostic-allocation") {
        (void)setenv("AMS_MEL_TEST_FAIL_ALLOCATION", "1", 1);
        throw DiagnosticAllocationFailure{};
    }
    if (instance == "invalid-diagnostic") {
        throw InvalidDiagnostic{};
    }
    if (instance == "bad-alloc-manager") {
        throw std::bad_alloc{};
    }
    if (instance == "throw-manager") {
        throw std::runtime_error("mock manager exception");
    }
    if (instance == "null-manager") {
        return {};
    }
    return std::make_shared<MockManager>();
}

extern "C" __attribute__((visibility("default")))
std::shared_ptr<ams::iface::irmel::Control> getControl(
    std::string_view instance, std::shared_ptr<API_Manager>)
{
    record("control_factory_called");
    if (instance == "bad-alloc-control") {
        throw std::bad_alloc{};
    }
    if (instance == "throw-control") {
        throw std::runtime_error("mock control exception");
    }
    if (instance == "null-control") {
        return {};
    }
    return std::make_shared<MockControl>(std::string{instance});
}
