#include <rfmel/jobs/JobInterval.h>
#include <any>
#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <vector>

namespace r = ams::iface::rfmel;
using Bytes = std::vector<std::uint8_t>;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (false)
static_assert(std::is_same_v<decltype(&r::JobInterval::setUserDefinedContextData),
              void (r::JobInterval::*)(const std::any&)>);

int main()
{
    const Bytes expected{0x00, 0x80, 0xff, 0x01, 0x00};
    r::JobInterval retained;
    {
        Bytes source = expected;
        std::any payload{source};
        CHECK(payload.has_value() && payload.type() == typeid(Bytes));
        retained.setUserDefinedContextData(payload);
        source.assign(40, 7);
        std::any_cast<Bytes&>(payload).clear();
        CHECK(retained.getUserDefinedContextData<Bytes>()->get() == expected);
    }
    auto copy = retained;
    copy.getUserDefinedContextData<Bytes>()->get().at(1) = 17;
    CHECK(retained.getUserDefinedContextData<Bytes>()->get() == expected);
    CHECK(copy.getUserDefinedContextData<Bytes>()->get().at(1) == 17);
    retained.getUserDefinedContextData<Bytes>()->get().clear();
    CHECK(copy.getUserDefinedContextData<Bytes>()->get().size() == expected.size());
    const std::any empty{Bytes{}};
    CHECK(empty.has_value() && empty.type() == typeid(Bytes));
    retained.setUserDefinedContextData(empty);
    const auto empty_copy = retained;
    CHECK(empty_copy.getUserDefinedContextData<Bytes>()->get().empty());
    // Never call the pinned getter on absent context or an incompatible type:
    // it dereferences pointer any_cast without checking for nullptr.
    std::puts("PASS: actual-header user context value copies and present empty");
    return 0;
}
