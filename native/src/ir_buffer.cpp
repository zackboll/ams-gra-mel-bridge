#include <ams_mel/abi.h>
#include "internal.hpp"
#include "internal/provider_common.hpp"
#include <irmel/library/irmel-types/Buffer.h>

#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

using namespace ams::iface;
using namespace ams_mel::internal;

/* Reverse member destruction is intentional: provider, storage, Session DSO.
 * Never resized. No channel, callback or checkout can reach this owner. */
struct ams_mel_ir_buffer {
    std::shared_ptr<SessionState> session;
    std::vector<std::uint8_t> storage;
    std::shared_ptr<irmel::Buffer> provider;
    std::int64_t context{};
    std::mutex mutex;
};

namespace {
using Factory = decltype(&irmel::getBuffer);
static_assert(static_cast<std::uint32_t>(irmel::BufferFlag::Host) == AMS_MEL_IR_BUFFER_HOST);
static_assert(static_cast<std::uint32_t>(irmel::BufferFlag::GPU) == AMS_MEL_IR_BUFFER_GPU);
static_assert(static_cast<std::uint32_t>(irmel::BufferFlag::RDMA) == AMS_MEL_IR_BUFFER_RDMA);

template<class Action>
ams_mel_status_t boundary(char *d, std::size_t cap, std::size_t *required,
                          Action action) noexcept
{
    clear_diagnostic(d, cap, required);
    try {
        const auto status = action();
        if (status != AMS_MEL_OK)
            write_diagnostic("standalone Buffer operation refused", d, cap, required);
        return status;
    } catch (...) {
        return translate_provider_exception("Buffer provider exception",
            "unknown Buffer provider exception", d, cap, required);
    }
}

bool span(const ams_mel_ir_buffer& b, std::uint64_t offset,
          const void *bytes, std::size_t count) noexcept
{
    return (bytes || !count) && offset <= b.storage.size() &&
           count <= b.storage.size() - static_cast<std::size_t>(offset);
}
}

extern "C" ams_mel_status_t ams_mel_ir_buffer_open(
    ams_mel_session *session, std::uint64_t size, std::int64_t context,
    ams_mel_ir_buffer **output, std::uint32_t *result,
    char *d, std::size_t cap, std::size_t *required) noexcept
{
    if (output) *output = nullptr;
    if (result) *result = UINT32_MAX;
    return boundary(d, cap, required, [&]() -> ams_mel_status_t {
        if (!session || !session->state || !output || !result || !size ||
            size > static_cast<std::uint64_t>(INT64_MAX) ||
            size > std::numeric_limits<std::size_t>::max() ||
            size > std::vector<std::uint8_t>{}.max_size()) return AMS_MEL_INVALID_ARGUMENT;
        auto owner = std::make_unique<ams_mel_ir_buffer>();
        owner->session = session->state;
        Factory factory{};
        try { factory = owner->session->library->symbol<Factory>("getBuffer"); }
        catch (...) { return AMS_MEL_SYMBOL_NOT_FOUND; }
        owner->storage.resize(static_cast<std::size_t>(size));
        owner->context = context;
        owner->provider = factory(owner->session->instance, owner->session->manager);
        if (!owner->provider) return AMS_MEL_FACTORY_FAILED;
        try {
            *result = static_cast<std::uint32_t>(owner->provider->init(
                owner->storage.data(), owner->storage.size(), context));
        } catch (...) {
            /* init may have retained the address before throwing. Conservatively
             * root the complete graph without any further allocation. */
            (void)owner.release();
            throw;
        }
        if (*result != static_cast<std::uint32_t>(irmel::Return::Success)) {
            if (owner->provider.use_count() != 1) (void)owner.release();
            return AMS_MEL_INITIALIZATION_FAILED;
        }
        *output = owner.release();
        return AMS_MEL_OK;
    });
}

extern "C" ams_mel_status_t ams_mel_ir_buffer_get_info(
    ams_mel_ir_buffer *b, ams_mel_ir_buffer_info_v1 *output,
    char *d, std::size_t cap, std::size_t *required) noexcept
{
    return boundary(d, cap, required, [&]() -> ams_mel_status_t {
        if (!b || !output) return AMS_MEL_INVALID_ARGUMENT;
        std::lock_guard lock{b->mutex};
        const auto address = b->provider->getBufferAddress();
        const auto image = b->provider->getImageAddress();
        const auto start = reinterpret_cast<std::uintptr_t>(b->storage.data());
        const auto point = reinterpret_cast<std::uintptr_t>(image);
        const std::uint32_t location = !image ? AMS_MEL_IR_BUFFER_IMAGE_NULL :
            (point >= start && point - start < b->storage.size() ?
             AMS_MEL_IR_BUFFER_IMAGE_IN_STORAGE : AMS_MEL_IR_BUFFER_IMAGE_OUTSIDE_STORAGE);
        const ams_mel_ir_buffer_info_v1 value{b->storage.size(), b->context,
            b->provider->getSize(), b->provider->getContext(),
            address == b->storage.data() ? 1U : 0U, location};
        *output = value;
        return AMS_MEL_OK;
    });
}

extern "C" ams_mel_status_t ams_mel_ir_buffer_get_flags(
    ams_mel_ir_buffer *b, std::uint32_t *flags, std::size_t capacity,
    std::size_t *count, std::uint32_t *result,
    char *d, std::size_t cap, std::size_t *required) noexcept
{
    if (count) *count = 0;
    if (result) *result = UINT32_MAX;
    return boundary(d, cap, required, [&]() -> ams_mel_status_t {
        if (!b || !count || !result || (!flags && capacity)) return AMS_MEL_INVALID_ARGUMENT;
        std::lock_guard lock{b->mutex};
        std::vector<irmel::BufferFlag> values;
        *result = static_cast<std::uint32_t>(b->provider->getFlags(values));
        if (*result != static_cast<std::uint32_t>(irmel::Return::Success))
            return AMS_MEL_PROVIDER_FAILED;
        for (auto flag : values)
            if (static_cast<std::uint32_t>(flag) > AMS_MEL_IR_BUFFER_RDMA)
                return AMS_MEL_PROVIDER_FAILED;
        *count = values.size();
        if (capacity < values.size()) return AMS_MEL_BUFFER_TOO_SMALL;
        for (std::size_t i = 0; i < values.size(); ++i)
            flags[i] = static_cast<std::uint32_t>(values[i]);
        return AMS_MEL_OK;
    });
}

extern "C" ams_mel_status_t ams_mel_ir_buffer_set_flags(
    ams_mel_ir_buffer *b, const std::uint32_t *flags, std::size_t count,
    char *d, std::size_t cap, std::size_t *required) noexcept
{
    return boundary(d, cap, required, [&]() -> ams_mel_status_t {
        if (!b || (!flags && count) || count > std::vector<irmel::BufferFlag>{}.max_size())
            return AMS_MEL_INVALID_ARGUMENT;
        std::lock_guard lock{b->mutex};
        std::vector<irmel::BufferFlag> values;
        values.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            if (flags[i] > AMS_MEL_IR_BUFFER_RDMA) return AMS_MEL_INVALID_ARGUMENT;
            values.push_back(static_cast<irmel::BufferFlag>(flags[i]));
        }
        b->provider->setFlags(values);
        return AMS_MEL_OK;
    });
}

extern "C" ams_mel_status_t ams_mel_ir_buffer_add_flag(
    ams_mel_ir_buffer *b, std::uint32_t flag,
    char *d, std::size_t cap, std::size_t *required) noexcept
{
    return boundary(d, cap, required, [&]() -> ams_mel_status_t {
        if (!b || flag > AMS_MEL_IR_BUFFER_RDMA) return AMS_MEL_INVALID_ARGUMENT;
        std::lock_guard lock{b->mutex};
        b->provider->addFlag(static_cast<irmel::BufferFlag>(flag));
        return AMS_MEL_OK;
    });
}

extern "C" ams_mel_status_t ams_mel_ir_buffer_read_storage(
    ams_mel_ir_buffer *b, std::uint64_t offset, std::uint8_t *bytes, std::size_t count,
    char *d, std::size_t cap, std::size_t *required) noexcept
{
    return boundary(d, cap, required, [&]() -> ams_mel_status_t {
        if (!b) return AMS_MEL_INVALID_ARGUMENT;
        std::lock_guard lock{b->mutex};
        if (!span(*b, offset, bytes, count)) return AMS_MEL_INVALID_ARGUMENT;
        if (count) std::memcpy(bytes, b->storage.data() + offset, count);
        return AMS_MEL_OK;
    });
}

extern "C" ams_mel_status_t ams_mel_ir_buffer_write_storage(
    ams_mel_ir_buffer *b, std::uint64_t offset, const std::uint8_t *bytes, std::size_t count,
    char *d, std::size_t cap, std::size_t *required) noexcept
{
    return boundary(d, cap, required, [&]() -> ams_mel_status_t {
        if (!b) return AMS_MEL_INVALID_ARGUMENT;
        std::lock_guard lock{b->mutex};
        if (!span(*b, offset, bytes, count)) return AMS_MEL_INVALID_ARGUMENT;
        if (count) std::memcpy(b->storage.data() + offset, bytes, count);
        return AMS_MEL_OK;
    });
}

extern "C" ams_mel_status_t ams_mel_ir_buffer_close(
    ams_mel_ir_buffer **b, char *d, std::size_t cap, std::size_t *required) noexcept
{
    return boundary(d, cap, required, [&]() -> ams_mel_status_t {
        if (!b) return AMS_MEL_INVALID_ARGUMENT;
        if (!*b) return AMS_MEL_OK;
        {
            std::lock_guard lock{(*b)->mutex};
            if ((*b)->provider.use_count() != 1) return AMS_MEL_PROVIDER_FAILED;
            (*b)->provider.reset();
        }
        delete *b;
        *b = nullptr;
        return AMS_MEL_OK;
    });
}
