#include <rfmel/jobs/JobInterval.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace r = ams::iface::rfmel;
using Fs = ams::util::math::Femtoseconds;
static_assert(std::is_same_v<Fs::rep, int64_t>);
static_assert(std::is_same_v<Fs::period, std::femto>);
static_assert(std::is_same_v<decltype(std::declval<r::WaveformProfileIterations>().getNumChipIterations()), int>);
static_assert(std::is_same_v<decltype(std::declval<r::WaveformProfileIterations>().getWaveformProfileIndex()), int>);
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)

int main()
{
    r::WaveformProfileParams defaults;
    CHECK(defaults.getChipDuration().count() == 0);
    CHECK(!defaults.getUpdatePhase() && !defaults.getUpdateDeltaPhase());
    CHECK(!defaults.getUpdateFrequencyOffset() && !defaults.getUpdateDeltaFrequency());
    CHECK(!defaults.getUpdateFrequencySlope() && !defaults.getUpdateAmplitudeFactor());
    CHECK(defaults.getPhaseDeg() == 0 && defaults.getDeltaPhaseDeg() == 0);
    CHECK(defaults.getFrequencyOffsetHz() == 0 && defaults.getDeltaFrequencyHz() == 0);
    CHECK(defaults.getFrequencySlopeHzSec() == 0 && defaults.getAmplitudeFactor() == 0);
    r::WaveformProfileIterations step;
    CHECK(step.getNumChipIterations() == 1 && step.getWaveformProfileIndex() == 0);
    CHECK(r::EventWaveformProfilePattern{}.getWaveformProfilePattern().empty());
    CHECK(r::CustomWaveform{}.getWaveformProfiles().empty());
    CHECK(r::CustomWaveform{}.getEventWaveformProfilePattern().empty());

    r::JobInterval retained;
    {
        r::WaveformProfileParams profile{Fs{INT64_MIN}, false, true, false, true, false, true,
            90.0, -180.0, -0.0, std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()};
        profile.setChipDuration(Fs{INT64_MAX});
        profile.setUpdatePhase(true);
        profile.setUpdateDeltaPhase(false);
        profile.setUpdateFrequencyOffset(true);
        profile.setUpdateDeltaFrequency(false);
        profile.setUpdateFrequencySlope(true);
        profile.setUpdateAmplitudeFactor(false);
        profile.setPhaseDeg(90.0);
        profile.setDeltaPhaseDeg(-180.0);
        profile.setFrequencyOffsetHz(-0.0);
        profile.setDeltaFrequencyHz(std::numeric_limits<double>::infinity());
        profile.setFrequencySlopeHzSec(-std::numeric_limits<double>::infinity());
        profile.setAmplitudeFactor(std::numeric_limits<double>::quiet_NaN());
        std::vector<r::WaveformProfileParams> profiles{profile, defaults};
        profile.setPhaseDeg(7.0);
        CHECK(profiles[0].getPhaseDeg() == 90.0);
        step.setNumChipIterations(std::numeric_limits<int>::min());
        step.setWaveformProfileIndex(1);
        std::vector<r::WaveformProfileIterations> steps{step, {0, 0}, {std::numeric_limits<int>::max(), 1}};
        r::EventWaveformProfilePattern pattern{steps};
        steps[0].setNumChipIterations(9);
        CHECK(pattern.getWaveformProfilePattern()[0].getNumChipIterations() == std::numeric_limits<int>::min());
        pattern.setWaveformProfilePattern(steps);
        steps[0].setNumChipIterations(10);
        CHECK(pattern.getWaveformProfilePattern()[0].getNumChipIterations() == 9);
        std::vector<r::EventWaveformProfilePattern> patterns{pattern, {}, pattern};
        r::CustomWaveform waveform{patterns, profiles};
        profiles[0].setPhaseDeg(8.0);
        patterns.clear();
        CHECK(waveform.getWaveformProfiles()[0].getPhaseDeg() == 90.0);
        CHECK(waveform.getEventWaveformProfilePattern().size() == 3);
        waveform.setWaveformProfiles(profiles);
        waveform.setEventWaveformProfilePattern({pattern, {}, pattern});
        profiles[0].setPhaseDeg(9.0);
        pattern.setWaveformProfilePattern({});
        r::CustomWaveform copy = waveform;
        waveform.setWaveformProfiles({});
        waveform.setEventWaveformProfilePattern({});
        r::CommonModulations inner{std::in_place_type<r::CustomWaveform>, copy};
        copy.setWaveformProfiles({});
        r::Modulation outer{std::in_place_type<r::CommonModulations>, inner};
        inner = r::Tone{};
        std::vector<r::Modulation> table{outer};
        outer = r::CommonModulations{r::Tone{}};
        retained.setModulations(table);
        table.clear();
    }
    r::JobInterval interval_copy = retained;
    retained.getModulations().clear();
    const auto& table = interval_copy.getModulations();
    CHECK(table.size() == 1 && std::holds_alternative<r::CommonModulations>(table[0]));
    const auto& inner = std::get<r::CommonModulations>(table[0]);
    CHECK(std::holds_alternative<r::CustomWaveform>(inner));
    const auto& waveform = std::get<r::CustomWaveform>(inner);
    CHECK(waveform.getWaveformProfiles().size() == 2);
    auto observed = waveform.getWaveformProfiles()[0];
    CHECK(observed.getChipDuration().count() == INT64_MAX);
    CHECK(observed.getUpdatePhase() && !observed.getUpdateDeltaPhase());
    CHECK(observed.getUpdateFrequencyOffset() && !observed.getUpdateDeltaFrequency());
    CHECK(observed.getUpdateFrequencySlope() && !observed.getUpdateAmplitudeFactor());
    CHECK(observed.getPhaseDeg() == 8.0 && observed.getDeltaPhaseDeg() == -180.0);
    CHECK(observed.getFrequencyOffsetHz() == 0.0 && std::signbit(observed.getFrequencyOffsetHz()));
    CHECK(std::isinf(observed.getDeltaFrequencyHz()) && !std::signbit(observed.getDeltaFrequencyHz()));
    CHECK(std::isinf(observed.getFrequencySlopeHzSec()) && std::signbit(observed.getFrequencySlopeHzSec()));
    CHECK(std::isnan(observed.getAmplitudeFactor()));
    const auto& patterns = waveform.getEventWaveformProfilePattern();
    CHECK(patterns.size() == 3 && patterns[1].getWaveformProfilePattern().empty());
    for (const auto i : {0U, 2U}) {
        const auto& steps = patterns[i].getWaveformProfilePattern();
        CHECK(steps.size() == 3);
        CHECK(steps[0].getNumChipIterations() == 9 && steps[0].getWaveformProfileIndex() == 1);
        CHECK(steps[1].getNumChipIterations() == 0 && steps[1].getWaveformProfileIndex() == 0);
        CHECK(steps[2].getNumChipIterations() == std::numeric_limits<int>::max() && steps[2].getWaveformProfileIndex() == 1);
    }
    observed.setPhaseDeg(17.0);
    CHECK(waveform.getWaveformProfiles()[0].getPhaseDeg() == 8.0);
    return 0;
}
