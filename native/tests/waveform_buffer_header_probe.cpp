#include <rfmel/jobs/JobInterval.h>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace r = ams::iface::rfmel;
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
static_assert(std::variant_size_v<r::JobTransmitVector> == 2);
static_assert(std::is_same_v<std::variant_alternative_t<0, r::JobTransmitVector>, r::ComplexI8VecPtr>);
static_assert(std::is_same_v<std::variant_alternative_t<1, r::JobTransmitVector>, r::ComplexI16VecPtr>);

template<class T, class Ptr>
int qualify()
{
    using Sample = r::MELComplex<T>;
    using Vector = std::vector<Sample>;
    static_assert(std::is_same_v<decltype(Sample{}.real()), T>);
    static_assert(std::is_same_v<decltype(Sample{}.imag()), T>);
    static_assert(sizeof(Sample) == 2 * sizeof(T));
    static_assert(alignof(Sample) == 2 * sizeof(T));
    static_assert(std::is_same_v<Ptr, std::shared_ptr<Vector>>);
    std::weak_ptr<Vector> weak;
    r::JobInterval retained;
    {
        auto samples = std::make_shared<Vector>();
        samples->emplace_back(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        samples->emplace_back(T{-7}, T{3});
        weak = samples;
        r::JobTransmitVector variant{std::in_place_type<Ptr>, samples};
        r::WaveformBuffer waveform{r::DataSampleFormat::ComplexCartesian, variant};
        waveform.setFormat(r::DataSampleFormat::ComplexCartesian);
        waveform.setTxSamples(variant);
        r::WaveformBuffer copy = waveform;
        r::WaveformBuffer moved = std::move(copy);
        r::CommonModulations common{std::in_place_type<r::WaveformBuffer>, moved};
        r::Modulation modulation{std::in_place_type<r::CommonModulations>, common};
        std::vector<r::Modulation> table{modulation};
        retained.setModulations(table);
        // Native copies share THIS allocation, not deep-copy its contents.
        samples->at(1).real(T{-9});
        CHECK(std::get<Ptr>(moved.getTxSamples())->at(1).real() == T{-9});
        samples.reset();
        CHECK(!weak.expired());
    }
    r::JobInterval interval_copy = retained;
    retained.getModulations().clear();
    CHECK(!weak.expired());
    {
        const auto& outer = interval_copy.getModulations().at(0);
        CHECK(std::holds_alternative<r::CommonModulations>(outer));
        const auto& inner = std::get<r::CommonModulations>(outer);
        CHECK(std::holds_alternative<r::WaveformBuffer>(inner));
        const auto& waveform = std::get<r::WaveformBuffer>(inner);
        CHECK(waveform.getFormat() == r::DataSampleFormat::ComplexCartesian);
        CHECK(std::holds_alternative<Ptr>(waveform.getTxSamples()));
        const auto& samples = std::get<Ptr>(waveform.getTxSamples());
        CHECK(samples && samples->size() == 2);
        CHECK(samples->at(0).real() == std::numeric_limits<T>::min());
        CHECK(samples->at(0).imag() == std::numeric_limits<T>::max());
        CHECK(samples->at(1).real() == T{-9} && samples->at(1).imag() == T{3});
        auto shared_copy = samples;
        shared_copy->at(1).imag(T{-4});
        CHECK(samples->at(1).imag() == T{-4});
    }
    interval_copy.getModulations().clear();
    // The weak observer retains control-block storage, not the vector object.
    CHECK(weak.expired() && !weak.lock());
    return 0;
}

int main()
{
    r::WaveformBuffer defaults;
    CHECK(defaults.getFormat() == r::DataSampleFormat::ComplexCartesian);
    CHECK(std::holds_alternative<r::ComplexI8VecPtr>(defaults.getTxSamples()));
    CHECK(!std::get<r::ComplexI8VecPtr>(defaults.getTxSamples()));
    const auto i8 = qualify<int8_t, r::ComplexI8VecPtr>();
    CHECK(i8 == 0);
    const auto i16 = qualify<int16_t, r::ComplexI16VecPtr>();
    CHECK(i16 == 0);
    return 0;
}
