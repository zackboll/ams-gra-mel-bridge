#include <rfmel/data/DataMEL.h>
#include <rfmel/data/ProductRxEndpoint.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <type_traits>
#include <variant>
namespace rf = ams::iface::rfmel;
using Callback = std::function<void(std::shared_ptr<rf::ProductRxMetadata>, rf::JobDataPointer, std::size_t)>;
static_assert(static_cast<int>(rf::JobDataFormat::DirectINT8) == 0);
static_assert(static_cast<int>(rf::JobDataFormat::DirectINT16) == 1);
static_assert(static_cast<int>(rf::JobDataFormat::ComplexINT8) == 2);
static_assert(static_cast<int>(rf::JobDataFormat::ComplexINT16) == 3);
static_assert(std::is_same_v<std::variant_alternative_t<0, rf::JobDataPointer>, std::int8_t *>);
static_assert(std::is_same_v<std::variant_alternative_t<1, rf::JobDataPointer>, std::int16_t *>);
static_assert(std::is_same_v<std::variant_alternative_t<2, rf::JobDataPointer>, rf::MELComplex<std::int8_t> *>);
static_assert(std::is_same_v<std::variant_alternative_t<3, rf::JobDataPointer>, rf::MELComplex<std::int16_t> *>);
static_assert(std::is_same_v<decltype(&rf::ProductRxEndpoint::setDataReadyCallback), void (rf::ProductRxEndpoint::*)(Callback&)>);
static_assert(sizeof(std::int8_t) == 1 && sizeof(std::int16_t) == 2);
static_assert(std::is_signed_v<std::int8_t> && std::is_signed_v<std::int16_t>);
int main() {
    std::array<std::int8_t, 8> a{-128,127,-1,0,7,-11,23,42};
    std::array<std::int16_t, 8> b{-32768,32767,-1,0,12345,-23456,17,-19};
    rf::JobDataPointer x{a.data()}, y{b.data()};
    if (x.index() != 0 || y.index() != 1 || std::get_if<std::int16_t *>(&x) ||
        std::get_if<rf::MELComplex<std::int8_t> *>(&x) ||
        std::get_if<rf::MELComplex<std::int16_t> *>(&y)) return 1;
    std::size_t observed = 0;
    Callback callback = [&](auto, rf::JobDataPointer data, std::size_t count) {
        if (data.index() == 0 && count == a.size() && *std::get<std::int8_t *>(data) == -128) observed += count;
        if (data.index() == 1 && count == b.size() && *std::get<std::int16_t *>(data) == -32768) observed += count;
    };
    callback(nullptr, x, a.size()); callback(nullptr, y, b.size());
    if (observed != 16 || a[1] != 127 || b[1] != 32767) return 2;
    std::printf("PASS direct exact alternatives 0/1, complex 2/3; signed scalar size/alignment %zu/%zu %zu/%zu; callback observations %zu elements; nonconst lvalue callback\n", sizeof(std::int8_t), alignof(std::int8_t), sizeof(std::int16_t), alignof(std::int16_t), observed);
}
