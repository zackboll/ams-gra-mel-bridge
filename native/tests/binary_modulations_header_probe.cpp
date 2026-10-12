#include <rfmel/jobs/JobInterval.h>
#include <type_traits>
#include <limits>
#include <cmath>
namespace r = ams::iface::rfmel;
using Fs = ams::util::math::Femtoseconds;
static_assert(std::is_same_v<Fs::rep, int64_t>);
static_assert(std::is_same_v<Fs::period, std::femto>);
static_assert(std::is_same_v<r::Frequency, double>);
static_assert(std::is_same_v<decltype(std::declval<const r::JobInterval&>().getSequenceRepeatCount()), size_t>);
static_assert(std::is_same_v<decltype(&r::JobInterval::setSequenceRepeatCount), void(r::JobInterval::*)(size_t)>);
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main() {
    r::OnOffKeying ook;
    r::CpfskPulse cpfsk;
    CHECK(ook.getPulseEnabled().empty());
    CHECK(cpfsk.getChipData().empty());
    CHECK(cpfsk.getPulseDuration().count() == 0);
    CHECK(cpfsk.getChipDuration().count() == 0);
    CHECK(cpfsk.getFrequencyOffset() == 0);
    r::JobInterval retained;
    {
        std::vector<bool> source{true, false, false, true, false};
        r::OnOffKeying selected{source};
        source[0] = false;
        CHECK(selected.getPulseEnabled()[0]);
        selected.setPulseEnabled(source);
        selected.addPulseEnabled(true);
        CHECK(selected.getPulseEnabled() == std::vector<bool>({false,false,false,true,false,true}));
        r::CpfskPulse chips{source, Fs{INT64_MIN}, Fs{INT64_MAX}, -0.0};
        source[3] = false;
        CHECK(chips.getChipData()[3]);
        chips.setChipData(source);
        chips.setPulseDuration(Fs{-37});
        chips.setChipDuration(Fs{59});
        chips.setFrequencyOffset(-std::numeric_limits<double>::infinity());
        std::vector<r::Modulation> table;
        table.emplace_back(std::in_place_type<r::CommonModulations>, std::in_place_type<r::OnOffKeying>, selected);
        table.emplace_back(std::in_place_type<r::CommonModulations>, std::in_place_type<r::CpfskPulse>, chips);
        retained.setModulations(table);
        retained.setSequenceRepeatCount(size_t{6});
        selected.setPulseEnabled({});
        chips.setChipData({});
        table.clear();
        source.clear();
    }
    r::JobInterval copy = retained;
    retained.getModulations().clear();
    CHECK(copy.getSequenceRepeatCount() == size_t{6});
    const auto& table = static_cast<const r::JobInterval&>(copy).getModulations();
    CHECK(std::holds_alternative<r::CommonModulations>(table[0]));
    CHECK(std::holds_alternative<r::CommonModulations>(table[1]));
    const auto& value = std::get<r::OnOffKeying>(std::get<r::CommonModulations>(table[0]));
    CHECK(value.getPulseEnabled() == std::vector<bool>({false,false,false,true,false,true}));
    auto observed = std::get<r::CpfskPulse>(std::get<r::CommonModulations>(table[1]));
    CHECK(observed.getChipData() == std::vector<bool>({false,false,false,false,false}));
    CHECK(observed.getPulseDuration().count() == -37);
    CHECK(observed.getChipDuration().count() == 59);
    CHECK(std::isinf(observed.getFrequencyOffset()) && std::signbit(observed.getFrequencyOffset()));
    observed.setChipData({true});
    auto independent = std::get<r::CpfskPulse>(std::get<r::CommonModulations>(table[1]));
    CHECK(independent.getChipData().size() == 5);
    return 0;
}
