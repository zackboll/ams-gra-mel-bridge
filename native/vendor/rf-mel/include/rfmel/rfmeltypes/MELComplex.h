/// @file include/rfmel/rfmeltypes/MELComplex.h
/// @brief This header defines the type used for RF sample data by the Jobs Interface of the RF MEL.

#pragma once

#include <utility>
#include <cstdint>

namespace ams::iface::rfmel
{
	/// @Required This class provides data definition in support of data endpoints and must be included as-is in all RF MEL implementations.
	/// @brief Provides a way for the MEL define a complex cartesian signed or unsigned integer data type similar to std::complex. It must be fully
	/// interleaved, and properly aligned in memory, like the C-compatible std::complex is, meaning that each MELComplex<int8_t> inside of a vector
	/// would only use up 16 bits of memory, and not pad the rest for word alignment (4 or 8 byte).
	/// @note std::complex could not be used because it only supports float, double, and long double template types. Also, unlike us, std::complex
	/// does not need to use 'alignas' because neither of its types when combined are smaller than one word on a typical 32 or 64 bit system.
	template <typename T, typename U = T>
	class alignas(sizeof(T) + sizeof(U)) MELComplex
	{
		/*
		 * alignas requires to be 0 or power of 2 (1, 2, 4, 8, 16) bytes
		 * so:
		 *
		 * 2 * sizeof(int8_t)  = (2 * 1) = 2 bytes
		 * 2 * sizeof(int16_t) = (2 * 2) = 4 bytes
		 * 2 * sizeof(int32_t) = (2 * 4) = 8 bytes

		 * 2 * sizeof(uint8_t)  = (2 * 1) = 2 bytes
		 * 2 * sizeof(uint16_t) = (2 * 2) = 4 bytes
		 * 2 * sizeof(uint32_t) = (2 * 4) = 8 bytes
		 *
		 * therefore, it should be possible for all commonly used interleaved complex cartesian
		 * integer types should to be properly aligned using this method
		 */

	public:
		explicit MELComplex(const T& re = T(), const U& im = U()) : mReal(re), mImag(im)
		{
		}

		[[nodiscard]] T real() const
		{
			return this->mReal;
		}

		[[nodiscard]] U imag() const
		{
			return this->mImag;
		}

		void real(const T& aReal)
		{
			this->mReal = aReal;
		}

		void imag(const U& aImag)
		{
			this->mImag = aImag;
		}

		MELComplex(MELComplex&& other) noexcept : MELComplex()
		{
			*this = std::move(other);
		}

		MELComplex& operator=(MELComplex&& other) noexcept
		{
			if(this != &other)
			{
				mReal = std::move(other.mReal);
				mImag = std::move(other.mImag);
			}
			return *this;
		}

		~MELComplex() = default;

		MELComplex(const MELComplex&) = default;

		MELComplex& operator=(const MELComplex&) = default;

	private:
		T mReal;
		U mImag;
	};
} // namespace ams::iface::rfmel
