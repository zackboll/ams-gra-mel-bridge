// Task 033C declaration-only probe for the pinned RF MEL ProductRxEndpoint
// receive contract. It compiles the published receive declaration roots and
// implements no RF behavior: no provider object is instantiated, no endpoint
// is derived or faked, and no callback is registered. Every assertion below is
// compile-time evidence about the pinned upstream declarations only.
#include <rfmel/data/DataMEL.h>
#include <rfmel/data/ProductRxEndpoint.h>

#include <any>
#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <ratio>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rfmel = ams::iface::rfmel;
namespace mel = ams::iface::mel;
namespace math = ams::util::math;

namespace {

using Endpoint = rfmel::ProductRxEndpoint;
using Metadata = rfmel::ProductRxMetadata;
using Format = rfmel::JobDataFormat;
using Complex16 = rfmel::MELComplex<std::int16_t>;

// ---------------------------------------------------------------------------
// Endpoint type relationships and exact published method inventory.
// ---------------------------------------------------------------------------
static_assert(std::is_base_of_v<rfmel::BaseEndpoint, Endpoint>);
static_assert(std::is_abstract_v<Endpoint>);
static_assert(std::has_virtual_destructor_v<Endpoint>);
static_assert(!std::is_copy_constructible_v<Endpoint>);
static_assert(!std::is_move_constructible_v<Endpoint>);
static_assert(std::is_same_v<rfmel::EndpointID, std::uint64_t>);

static_assert(std::is_same_v<decltype(&rfmel::BaseEndpoint::getEndpointID),
                             rfmel::EndpointID (rfmel::BaseEndpoint::*)() const>);
static_assert(std::is_same_v<decltype(&Endpoint::getEndpointID),
                             rfmel::EndpointID (Endpoint::*)() const>);
static_assert(std::is_same_v<decltype(&Endpoint::getAssignedDataFormat),
                             Format (Endpoint::*)() const>);
// RDMAMemoryRegionParams is only forward-declared in the measured closure;
// naming the by-value return type of the pure virtual does not require it.
static_assert(std::is_same_v<decltype(&Endpoint::getRDMAMemoryRegionParams),
                             rfmel::RDMAMemoryRegionParams (Endpoint::*)() const>);

// The exact published data-ready callback, taken by NON-CONST lvalue reference.
using DataReadyCallback =
    std::function<void(std::shared_ptr<Metadata>, rfmel::JobDataPointer, std::size_t)>;
static_assert(std::is_same_v<decltype(&Endpoint::setDataReadyCallback),
                             void (Endpoint::*)(DataReadyCallback&)>);

template <class E, class F>
concept accepts_data_ready_callback =
    requires(E& endpoint, F&& callback) { endpoint.setDataReadyCallback(std::forward<F>(callback)); };
static_assert(accepts_data_ready_callback<Endpoint, DataReadyCallback&>);
static_assert(!accepts_data_ready_callback<Endpoint, DataReadyCallback>);
static_assert(!accepts_data_ready_callback<Endpoint, const DataReadyCallback&>);

// No unregister / stop operation is published. These names are checked for
// absence so that a future pin adding one is noticed rather than assumed.
#define AMS_MEL_RF_PUBLISHES(name)                                            \
    template <class T>                                                        \
    concept publishes_##name = requires { &T::name; };
AMS_MEL_RF_PUBLISHES(setDataReadyCallback)
AMS_MEL_RF_PUBLISHES(removeDataReadyCallback)
AMS_MEL_RF_PUBLISHES(clearDataReadyCallback)
AMS_MEL_RF_PUBLISHES(unregisterDataReadyCallback)
AMS_MEL_RF_PUBLISHES(stopCallbacks)
AMS_MEL_RF_PUBLISHES(stop)
AMS_MEL_RF_PUBLISHES(close)
AMS_MEL_RF_PUBLISHES(shutdown)
#undef AMS_MEL_RF_PUBLISHES
static_assert(publishes_setDataReadyCallback<Endpoint>);
static_assert(!publishes_removeDataReadyCallback<Endpoint>);
static_assert(!publishes_clearDataReadyCallback<Endpoint>);
static_assert(!publishes_unregisterDataReadyCallback<Endpoint>);
static_assert(!publishes_stopCallbacks<Endpoint>);
static_assert(!publishes_stop<Endpoint>);
static_assert(!publishes_close<Endpoint>);
static_assert(!publishes_shutdown<Endpoint>);
static_assert(publishes_shutdown<rfmel::DataMEL>); // RFMEL::shutdown exists on the parent only

// ---------------------------------------------------------------------------
// Asynchronous endpoint creation.
// ---------------------------------------------------------------------------
static_assert(std::is_same_v<mel::RequestFor<Endpoint>,
                             std::future<mel::ErrorOr<std::shared_ptr<Endpoint>>>>);
static_assert(std::is_same_v<decltype(&rfmel::DataMEL::createProductRxEndpoint),
                             mel::RequestFor<Endpoint> (rfmel::DataMEL::*)(Format, std::size_t, char*)>);
template <class D>
concept creates_with_default_region = requires(D& data) {
    { data.createProductRxEndpoint(Format::ComplexINT16, std::size_t{0}) }
        -> std::same_as<mel::RequestFor<Endpoint>>;
};
static_assert(creates_with_default_region<rfmel::DataMEL>);

// ---------------------------------------------------------------------------
// JobDataPointer: eight raw-pointer alternatives whose indices equal the first
// eight JobDataFormat enumerators. PDW/LF formats have no alternative.
// ---------------------------------------------------------------------------
static_assert(std::variant_size_v<rfmel::JobDataPointer> == 8);
template <std::size_t I, class T, Format F>
constexpr bool alternative_is =
    std::is_same_v<std::variant_alternative_t<I, rfmel::JobDataPointer>, T> &&
    static_cast<std::size_t>(F) == I;
static_assert(alternative_is<0, std::int8_t*, Format::DirectINT8>);
static_assert(alternative_is<1, std::int16_t*, Format::DirectINT16>);
static_assert(alternative_is<2, rfmel::MELComplex<std::int8_t>*, Format::ComplexINT8>);
static_assert(alternative_is<3, Complex16*, Format::ComplexINT16>);
static_assert(alternative_is<4, rfmel::DataPacketSmall*, Format::AMSVitaSmall>);
static_assert(alternative_is<5, rfmel::DataPacketMedium*, Format::AMSVitaMedium>);
static_assert(alternative_is<6, rfmel::DataPacketLarge*, Format::AMSVitaLarge>);
static_assert(alternative_is<7, rfmel::DataPacketExtraLarge*, Format::AMSVitaExtraLarge>);
static_assert(static_cast<int>(Format::PDWType1) == 8);
static_assert(static_cast<int>(Format::LFType3) == 13);
static_assert(sizeof(rfmel::DataPacketSmall) == 512 && sizeof(rfmel::DataPacketMedium) == 1024);
static_assert(sizeof(rfmel::DataPacketLarge) == 8192 && sizeof(rfmel::DataPacketExtraLarge) == 32768);
static_assert(sizeof(rfmel::MELComplex<std::int8_t>) == 2);

// ---------------------------------------------------------------------------
// ComplexINT16 element: MELComplex<int16_t>.
// ---------------------------------------------------------------------------
static_assert(sizeof(Complex16) == 4);
static_assert(alignof(Complex16) == 4);
static_assert(std::is_standard_layout_v<Complex16>);
static_assert(std::is_same_v<decltype(std::declval<const Complex16&>().real()), std::int16_t>);
static_assert(std::is_same_v<decltype(std::declval<const Complex16&>().imag()), std::int16_t>);
static_assert(std::is_nothrow_copy_constructible_v<Complex16>);
static_assert(std::is_trivially_destructible_v<Complex16>);
// User-provided move operations make MELComplex NOT trivially copyable, so a
// byte-wise memcpy of MELComplex objects is not sanctioned by the language.
// A future bridge copies element-by-element through real()/imag().
static_assert(!std::is_trivially_copyable_v<Complex16>);
static_assert(!std::is_trivial_v<Complex16>);

// ---------------------------------------------------------------------------
// ProductRxMetadata: every getter returns BY VALUE (auto / explicit types).
// ---------------------------------------------------------------------------
static_assert(std::is_copy_constructible_v<Metadata>);
template <class Getter, class T>
constexpr bool returns = std::is_same_v<std::invoke_result_t<Getter, const Metadata&>, T>;
static_assert(returns<decltype(&Metadata::getMelProtocolVersionID), std::uint32_t>);
static_assert(returns<decltype(&Metadata::getVaDefinitionID), rfmel::VirtualApertureDefinitionID>);
static_assert(returns<decltype(&Metadata::getVaInstanceID), rfmel::VirtualApertureInstanceID>);
static_assert(returns<decltype(&Metadata::getJobDetailsID), std::uint32_t>);
static_assert(returns<decltype(&Metadata::getJobIntervalID), std::uint32_t>);
static_assert(returns<decltype(&Metadata::getLfTypeID), rfmel::LocalFunctionTypeID>);
static_assert(returns<decltype(&Metadata::getLfInstanceID), std::uint32_t>);
static_assert(returns<decltype(&Metadata::getUserDefinedData), std::any>);
static_assert(returns<decltype(&Metadata::getPhaseCoherenceWithPrior), bool>);
static_assert(returns<decltype(&Metadata::getFirstReceiveEventStart), math::UTCTime>);
static_assert(returns<decltype(&Metadata::getStabPoints), std::vector<rfmel::PointingType>>);
static_assert(returns<decltype(&Metadata::getReceiveEvents), std::vector<rfmel::ReceiveEvent>>);
static_assert(returns<decltype(&Metadata::getReceiveEventAssociations),
                      std::optional<std::vector<rfmel::JobEventID>>>);
static_assert(returns<decltype(&Metadata::getRxStreamIDs), std::vector<rfmel::StreamID>>);
static_assert(std::is_same_v<rfmel::VirtualApertureDefinitionID, std::uint32_t>);
static_assert(std::is_same_v<rfmel::VirtualApertureInstanceID, std::uint32_t>);
static_assert(std::is_same_v<rfmel::LocalFunctionTypeID, std::uint32_t>);
static_assert(std::is_same_v<rfmel::JobEventID, std::uint32_t>);
static_assert(std::is_same_v<rfmel::StreamID, std::uint32_t>);

// UTCTime: signed 64-bit integral seconds + signed 64-bit fractional
// femtoseconds. There is no nanosecond form to preserve.
static_assert(std::is_same_v<math::Femtoseconds, std::chrono::duration<std::int64_t, std::femto>>);
static_assert(std::is_same_v<decltype(std::declval<const math::UTCTime&>().getIntegralSeconds()),
                             std::chrono::seconds>);
static_assert(std::is_same_v<decltype(std::declval<const math::UTCTime&>().getFractionalFemtoseconds()),
                             math::Femtoseconds>);
static_assert(std::is_signed_v<std::chrono::seconds::rep> && sizeof(std::chrono::seconds::rep) == 8);

// PointingType: five alternatives in published order.
static_assert(std::is_same_v<rfmel::PointingType,
                             std::variant<rfmel::ECEFPointing, rfmel::LLAPointing,
                                          rfmel::PlatformRelativePointing, rfmel::FaceRelativePointing,
                                          rfmel::BaselineRelativePointing>>);
static_assert(std::is_same_v<EcefPoint, boost::numeric::ublas::c_vector<double, 3>>);
static_assert(std::is_same_v<EcefVelocity, EcefPoint> && std::is_same_v<NedVelocity, EcefPoint>);
static_assert(std::is_same_v<decltype(AzEl::az), double> && std::is_same_v<decltype(AzEl::el), double>);
static_assert(std::is_same_v<decltype(LLAPoint::lla), double[3]>);
static_assert(std::is_same_v<rfmel::Conic, double>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::ECEFPointing&>().getTimeOfValidity()),
                             const math::UTCTime&>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::LLAPointing&>().getTimeOfValidity()),
                             math::UTCTime>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::PlatformRelativePointing&>().getLocation()),
                             const AzEl&>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::FaceRelativePointing&>().getLocation()),
                             const AzEl&>);
static_assert(std::is_same_v<decltype(std::declval<const rfmel::BaselineRelativePointing&>().getLocation()),
                             rfmel::Conic>);

// ReceiveEvent: polymorphic JobEvent subclass carrying nested containers and a
// map of RAW, non-owning Weights pointers (Weights is non-polymorphic).
static_assert(std::is_base_of_v<rfmel::JobEvent, rfmel::ReceiveEvent>);
static_assert(std::is_polymorphic_v<rfmel::JobEvent>);
static_assert(!std::is_polymorphic_v<rfmel::Weights>);
static_assert(std::is_copy_constructible_v<rfmel::ReceiveEvent>);
using EventRef = const rfmel::ReceiveEvent&;
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getStart()), math::Femtoseconds>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getDuration()), math::Femtoseconds>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getCenterFrequency()), rfmel::Frequency>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getSampleFrequency()), rfmel::Frequency>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getMaxExtensionDuration()), math::Femtoseconds>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getEventID()), rfmel::JobEventID>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getStabPointIndex()), std::size_t>);
// Published as `const auto getDirection() const`; the top-level const on a
// scalar prvalue is discarded, so the observed type is the plain enum.
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getDirection()), rfmel::JobEvent::Direction>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getPolarization()),
                             const std::vector<rfmel::StokesVector>&>);
static_assert(std::is_same_v<rfmel::StokesVector, std::array<double, 4>>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getWeights()),
                             const std::map<rfmel::WeightType, rfmel::Weights*>&>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getApplicableRxElementGroups()),
                             const std::vector<std::size_t>&>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getPulseDetectionSettings()),
                             const rfmel::PulseDetectionSettings&>);
static_assert(std::is_same_v<decltype(std::declval<EventRef>().getElementGroupLabel()), rfmel::ElementGroupLabel>);
static_assert(std::is_same_v<rfmel::ElementGroupLabel, std::string>);
static_assert(std::is_same_v<rfmel::Frequency, double>);
} // namespace

int rf_product_rx_header_compile_probe()
{
    return 0;
}
