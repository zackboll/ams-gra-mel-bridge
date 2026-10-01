#pragma once

#include <mel/library/ErrorOr.h>

#include <rfmel/rfmeltypes/RFMELTypes.h>

namespace ams::iface::rfmel
{

	/// @brief Indicates the reason for a job cancellation error.
	/// @Required This enumeration provides data definition associated with CancelError
	/// and must be included as-is in all RF MEL implementations.
	enum class CancelError : int
	{
		None = 0, /// An error-free cancellation.
		max = None
	};

	/// @brief Represents the result of a job cancellation operation, indicating success or failure and potential error reasons.
	/// @Required This class provides required RF MEL functionality for supporting cancelJob
	/// and must be included as-is in all RF MEL implementations.
	class CancelStatus
	{
	public:
		/// @brief The successful case with no errors
		CancelStatus() : hasError(false), errorCode(CancelError::None)
		{
		}

		/// @brief The error case containing the appropriate error code.
		explicit CancelStatus(CancelError error) : hasError(true), errorCode(error)
		{
		}

		~CancelStatus() = default;
		CancelStatus(const CancelStatus &other) = default;
		CancelStatus &operator=(const CancelStatus &other) = default;
		CancelStatus(const CancelStatus &&other) noexcept : hasError(other.hasError), errorCode(other.errorCode)
		{
		}

		CancelStatus &operator=(CancelStatus &&other) noexcept
		{
			hasError = other.hasError;
			errorCode = other.errorCode;
			return *this;
		}

		/// @brief Gets an error if available, otherwise will return None.
		[[nodiscard]] CancelError getError() const
		{
			return errorCode;
		}

		/// @brief Checks the cancellation status and returns true on success (no errors) or false on failure (error present).
		explicit operator bool() const
		{
			return !hasError;
		}

	private:
		bool hasError;
		CancelError errorCode;
	};
} // namespace ams::iface::rfmel
