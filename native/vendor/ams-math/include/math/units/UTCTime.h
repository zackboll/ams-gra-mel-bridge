#pragma once

#include <chrono>
#include <cstdint>

namespace ams::util::math
{
	using Picoseconds = std::chrono::duration<int64_t, std::pico>;	 // max 106.75 days
	using Femtoseconds = std::chrono::duration<int64_t, std::femto>; // max 2.5 hrs

	inline Picoseconds operator"" _ps(unsigned long long x)
	{
		return Picoseconds{x};
	}

	inline Femtoseconds operator"" _fs(unsigned long long x)
	{
		return Femtoseconds{x};
	}

	/// @class Represents current time since the UTC epoch in femtoseconds.
	/// Internally, the time is stored as an integer number of seconds
	/// and an integer number of femtoseconds.
	class UTCTime
	{
	public:
		/// @brief Default constructor yields time [0 sec / 0 fsec] from the epoch.
		UTCTime() = default;
		~UTCTime() = default;

		/// @brief Constructor to convert from any std::chrono::duration type.
		/// @param other A std::chrono::duration since the epoch
		template <class Rep, class Period>
		explicit UTCTime(std::chrono::duration<Rep, Period> other)
			: integral(std::chrono::duration_cast<std::chrono::seconds>(other)),
			  fractional(std::chrono::duration_cast<Femtoseconds>(other - integral))
		{
		}

		UTCTime(std::chrono::seconds integral_in, Femtoseconds fractional_in) : integral(integral_in), fractional(fractional_in)
		{
			normalize();
		}

		/// @brief Constructor to convert from any std::chrono::time_point type.
		/// @param other A std::chrono::time_point
		template <class Clock, class Duration = typename Clock::duration>
		explicit UTCTime(std::chrono::time_point<Clock, Duration> other)
			: UTCTime(other.time_since_epoch()) // re-use the duration conversion constructor
		{
		}

		/// @brief Allow default copy/move constructors/operators.
		UTCTime(const UTCTime &) = default;
		UTCTime(UTCTime &&) = default;
		UTCTime &operator=(const UTCTime &) = default;
		UTCTime &operator=(UTCTime &&) = default;

		/// @brief Get the integral seconds component of the duration since the epoch
		[[nodiscard]] std::chrono::seconds getIntegralSeconds() const
		{
			return integral;
		}

		/// @brief Get the fractional femtoseconds component of the duration since the epoch
		[[nodiscard]] Femtoseconds getFractionalFemtoseconds() const
		{
			return fractional;
		}

		/// @brief Convert to std::chrono::duration since the epoch in the desired units.
		template <typename Duration>
		Duration toDuration() const
		{
			return std::chrono::duration_cast<Duration>(integral) + std::chrono::duration_cast<Duration>(fractional);
		}

		/// @brief Convert to a std::chrono::time_point with the desired clock resolution.
		template <typename TimePoint>
		TimePoint toTimePoint() const
		{
			return TimePoint{toDuration<typename TimePoint::duration>()};
		}

		/// @brief Get the current time as a UTCTime.
		static UTCTime now()
		{
			return UTCTime{std::chrono::system_clock::now()};
		}

		/// @brief Compare with another UTCTime.
		bool operator==(const UTCTime &other) const
		{
			return (this->integral == other.integral) && (this->fractional == other.fractional);
		}

		/// @brief Compare with another UTCTime.
		bool operator<(const UTCTime &other) const
		{
			return ((this->integral < other.integral) || ((this->integral == other.integral) && (this->fractional < other.fractional)));
		}

		/// @brief Compare with another UTCTime.
		bool operator>(const UTCTime &other) const
		{
			return other < *this;
		}

		/// @brief Compare with another UTCTime.
		bool operator>=(const UTCTime &other) const
		{
			return !(*this < other);
		}

		/// @brief Compare with another UTCTime.
		bool operator<=(const UTCTime &other) const
		{
			return !(*this > other);
		}

		/// @brief Returns a new UTC time, 'dur' time into the future.
		/// @param dur A positive or negative duration, in some desired units.
		template <class Rep, class Period>
		UTCTime operator+(const std::chrono::duration<Rep, Period> &dur) const
		{
			// There may be more efficient ways to do this, but this method
			// prevents 'dur' from overflowing the fractional component.
			UTCTime sum{dur}; // input gets normalized
			sum.integral += this->integral;
			sum.fractional += this->fractional;
			sum.normalize();
			return sum;
		}

		/// @brief Returns a new UTC time, 'dur' time into the past.
		/// @param dur A positive or negative duration, in some desired units.
		template <class Rep, class Period>
		UTCTime operator-(const std::chrono::duration<Rep, Period> &dur) const
		{
			return *this + (-dur);
		}

		/// @brief Moves this UTC time 'dur' time into the future.
		/// @param dur A positive or negative duration, in some desired units.
		template <class Rep, class Period>
		UTCTime &operator+=(const std::chrono::duration<Rep, Period> &dur)
		{
			*this = *this + dur;
			return *this;
		}

		/// @brief Moves this UTC time 'dur' time into the past.
		/// @param dur A positive or negative duration, in some desired units.
		template <class Rep, class Period>
		UTCTime &operator-=(const std::chrono::duration<Rep, Period> &dur)
		{
			*this = *this - dur;
			return *this;
		}

	private:
		/// Seconds since the UTC epoch
		std::chrono::seconds integral{0};

		/// Fractional seconds since the UTC epoch
		Femtoseconds fractional{0};

		/// @brief Normalize the representation of seconds and femtoseconds,
		/// such that the number of femtoseconds is less than one second.
		void normalize()
		{
			long long f_count = fractional.count();
			auto overflow = f_count / std::femto::den;
			auto remainder = f_count % std::femto::den;

			// If the remainder is negative (e.g., -0.5s),
			// we shift 1 full second from the integral part to the fractional part.
			if (remainder < 0)
			{
				overflow -= 1;
				remainder += std::femto::den;
			}

			integral += std::chrono::seconds{overflow};
			fractional = Femtoseconds{remainder};
		}
	};
} // namespace ams::util::math
