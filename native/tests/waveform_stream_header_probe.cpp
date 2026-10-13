#include <rfmel/jobs/JobInterval.h>
#include <type_traits>
#include <limits>
#include <cstdio>
namespace r = ams::iface::rfmel;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (false)
static_assert(std::is_same_v<decltype(&r::WaveformStream::setEndpointID), void (r::WaveformStream::*)(uint64_t)>);
static_assert(std::is_same_v<decltype(r::WaveformStream{}.getEndpointID()), uint64_t>);
static_assert(std::is_same_v<decltype(&r::WaveformStream::setStartAddress), void (r::WaveformStream::*)(uint64_t)>);
static_assert(std::is_same_v<decltype(r::WaveformStream{}.getStartAddress()), uint64_t>);
static_assert(std::is_same_v<decltype(&r::WaveformStream::setNumBytes), void (r::WaveformStream::*)(size_t)>);
static_assert(std::is_same_v<decltype(r::WaveformStream{}.getNumBytes()), size_t>);
static_assert(std::is_same_v<decltype(&r::WaveformStream::setSampleFormat), void (r::WaveformStream::*)(r::DataSampleFormat)>);
static_assert(std::is_same_v<decltype(r::WaveformStream{}.getSampleFormat()), r::DataSampleFormat>);
static_assert(std::is_same_v<decltype(&r::WaveformStream::setItemFormat), void (r::WaveformStream::*)(r::DataItemFormat)>);
static_assert(std::is_same_v<decltype(r::WaveformStream{}.getItemFormat()), r::DataItemFormat>);
static_assert(std::is_same_v<decltype(&r::WaveformStream::setRepeatMode), void (r::WaveformStream::*)(r::BufferRepeatMode)>);
static_assert(std::is_same_v<decltype(r::WaveformStream{}.getRepeatMode()), r::BufferRepeatMode>);
int main() {
    r::WaveformStream source;
    CHECK(source.getEndpointID() == 0 && source.getStartAddress() == 0 && source.getNumBytes() == 0);
    CHECK(source.getSampleFormat() == r::DataSampleFormat::ComplexCartesian);
    CHECK(source.getItemFormat() == r::DataItemFormat::FixedPointSigned);
    CHECK(source.getRepeatMode() == r::BufferRepeatMode::RepeatFromTop);
    for (unsigned s = 0; s < 3; ++s) for (unsigned i = 0; i < 9; ++i) for (unsigned m = 0; m < 2; ++m) {
        source.setEndpointID(UINT64_MAX - s);
        source.setStartAddress(UINT64_MAX - i);
        source.setNumBytes(std::numeric_limits<size_t>::max() - m);
        source.setSampleFormat(static_cast<r::DataSampleFormat>(s));
        source.setItemFormat(static_cast<r::DataItemFormat>(i));
        source.setRepeatMode(static_cast<r::BufferRepeatMode>(m));
        std::vector<r::Modulation> table;
        table.emplace_back(std::in_place_type<r::WaveformStream>, source);
        r::JobInterval interval;
        interval.setModulations(table);
        auto copy = interval;
        source = r::WaveformStream{};
        table.clear();
        interval.getModulations().clear();
        const auto& outer = copy.getModulations().at(0);
        CHECK(std::holds_alternative<r::WaveformStream>(outer));
        const auto& value = std::get<r::WaveformStream>(outer);
        CHECK(value.getEndpointID() == UINT64_MAX - s);
        CHECK(value.getStartAddress() == UINT64_MAX - i);
        CHECK(value.getNumBytes() == std::numeric_limits<size_t>::max() - m);
        CHECK(static_cast<unsigned>(value.getSampleFormat()) == s);
        CHECK(static_cast<unsigned>(value.getItemFormat()) == i);
        CHECK(static_cast<unsigned>(value.getRepeatMode()) == m);
    }
    return 0;
}
