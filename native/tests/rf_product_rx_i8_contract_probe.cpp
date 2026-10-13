// Actual pinned-header qualification; checks remain active under NDEBUG.
#include <rfmel/data/ProductRxEndpoint.h>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <type_traits>
#include <variant>

namespace rf = ams::iface::rfmel;
using I8 = rf::MELComplex<std::int8_t>;
using I16 = rf::MELComplex<std::int16_t>;
using Callback = std::function<void(std::shared_ptr<rf::ProductRxMetadata>,
                                    rf::JobDataPointer, std::size_t)>;
static_assert(static_cast<int>(rf::JobDataFormat::ComplexINT8) == 2);
static_assert(static_cast<int>(rf::JobDataFormat::ComplexINT16) == 3);
static_assert(std::is_same_v<std::variant_alternative_t<2, rf::JobDataPointer>, I8 *>);
static_assert(std::is_same_v<std::variant_alternative_t<3, rf::JobDataPointer>, I16 *>);
static_assert(std::is_same_v<decltype(&rf::ProductRxEndpoint::setDataReadyCallback),
                             void (rf::ProductRxEndpoint::*)(Callback&)>);
static_assert(std::is_same_v<decltype(std::declval<I8>().real()), std::int8_t>);
static_assert(std::is_same_v<decltype(std::declval<I8>().imag()), std::int8_t>);

int main()
{
    const std::int8_t values[][2] = {{-128, 127}, {127, -128}, {-1, 0},
                                    {0, -1}, {7, -11}, {23, 42}};
    for (const auto& pair : values) {
        I8 original{pair[0], pair[1]};
        I8 copied{original};
        I8 assigned;
        assigned = original;
        original.real(0);
        original.imag(0);
        if (copied.real() != pair[0] || copied.imag() != pair[1] ||
            assigned.real() != pair[0] || assigned.imag() != pair[1]) return 1;
    }
    I8 zero;
    if (zero.real() != 0 || zero.imag() != 0) return 2;
    rf::JobDataPointer selected{static_cast<I8 *>(nullptr)};
    if (!std::holds_alternative<I8 *>(selected) ||
        std::holds_alternative<I16 *>(selected)) return 3;
    std::printf("PASS: native I8 size/alignment=%zu/%zu; I16=%zu/%zu; "
                "component fidelity and independent copies (no C layout claim)\n",
                sizeof(I8), alignof(I8), sizeof(I16), alignof(I16));
    return 0;
}
