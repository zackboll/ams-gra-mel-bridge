#include <rfmel/jobs/JobInterval.h>
#include <limits>
#include <type_traits>

namespace value_modulations_contract {
namespace r = ams::iface::rfmel;
using Fs = ams::util::math::Femtoseconds;
static_assert(std::is_same_v<Fs::rep, int64_t>);
static_assert(std::numeric_limits<int>::is_signed);
static_assert(std::is_same_v<decltype(&r::TransmitEvent::setModIndex), void(r::TransmitEvent::*)(const std::vector<int>&)>);
static_assert(std::is_same_v<decltype(&r::JobInterval::setModulations), void(r::JobInterval::*)(const std::vector<r::Modulation>&)>);
static_assert(std::is_same_v<decltype(&r::PhaseCode::setChipDuration), void(r::PhaseCode::*)(Fs)>);
static_assert(std::is_same_v<decltype(&r::PhaseCode::setPhaseChipData), void(r::PhaseCode::*)(const std::vector<double>&)>);
static_assert(std::is_same_v<decltype(&r::LinearFrequencyModulation::setFrequencyExtent), void(r::LinearFrequencyModulation::*)(double)>);
static_assert(std::is_same_v<decltype(std::declval<const r::TransmitEvent&>().getModIndex()), std::vector<int>>);
static_assert(std::is_same_v<decltype(std::declval<r::JobInterval&>().getModulations()), std::vector<r::Modulation>&>);
static_assert(std::is_same_v<decltype(std::declval<const r::JobInterval&>().getModulations()), const std::vector<r::Modulation>&>);

int probe()
{
    r::PhaseCode defaults;
    r::LinearFrequencyModulation lfm;
    r::TransmitEvent event;
    r::JobInterval interval;
    if (defaults.getChipDuration().count() != 0 || !defaults.getPhaseChipData().empty() ||
        lfm.getFrequencyExtent() != 0 || event.getModIndex() != std::vector<int>{-1} ||
        !interval.getModulations().empty()) return 1;
    {
        std::vector<double> phases{1.25, -2.5, 1.25};
        r::PhaseCode phase{Fs{INT64_MIN}, phases};
        phases[0] = 99;
        if (phase.getPhaseChipData()[0] != 1.25) return 2;
        phase.setChipDuration(Fs{INT64_MAX});
        phase.setPhaseChipData(phases);
        phases.clear();
        r::CommonModulations common{std::in_place_type<r::PhaseCode>, phase};
        r::Modulation modulation{std::in_place_type<r::CommonModulations>, common};
        std::vector<r::Modulation> table{modulation};
        interval.setModulations(table);
        phase.setPhaseChipData({});
        std::get<r::PhaseCode>(common).setPhaseChipData({});
        std::get<r::CommonModulations>(modulation).emplace<r::Tone>();
        table.clear();
    }
    const auto& copied = std::get<r::PhaseCode>(std::get<r::CommonModulations>(
        static_cast<const r::JobInterval&>(interval).getModulations()[0]));
    if (copied.getChipDuration().count() != INT64_MAX ||
        copied.getPhaseChipData() != std::vector<double>{99, -2.5, 1.25}) return 3;
    r::JobInterval independent = interval;
    interval.getModulations().clear();
    if (independent.getModulations().size() != 1) return 4;
    lfm.setFrequencyExtent(-7.5);
    r::LinearFrequencyModulation constructed{3.25};
    r::CommonModulations common_lfm{std::in_place_type<r::LinearFrequencyModulation>, lfm};
    r::Modulation modulation_lfm{std::in_place_type<r::CommonModulations>, common_lfm};
    lfm.setFrequencyExtent(0);
    if (constructed.getFrequencyExtent() != 3.25 ||
        std::get<r::LinearFrequencyModulation>(std::get<r::CommonModulations>(modulation_lfm)).getFrequencyExtent() != -7.5) return 5;
    r::CommonModulations tone{std::in_place_type<r::Tone>};
    r::Modulation modulation_tone{std::in_place_type<r::CommonModulations>, tone};
    if (!std::holds_alternative<r::Tone>(std::get<r::CommonModulations>(modulation_tone))) return 6;
    event.setModIndex({2, -1, 0, 2});
    r::TransmitEvent event_copy = event;
    event.setModIndex({});
    if (!event.getModIndex().empty() || event_copy.getModIndex() != std::vector<int>{2, -1, 0, 2}) return 7;
    return 0;
}
}
