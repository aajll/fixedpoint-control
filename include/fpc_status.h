/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fpc_status.h
 *
 * @brief
 *    Shared status codes for fixedpoint-control APIs.
 */

#ifndef FPC_STATUS_H_
#define FPC_STATUS_H_

/**
 * @name Version
 * @brief Release of this header, for diagnostics and @c #if checks.
 *
 * @details
 *    @c FPC_VERSION_NUMBER is @c MAJOR*10000 @c + @c MINOR*100
 *    @c + @c PATCH with type @c long, so it cannot overflow where @c int is
 *    16 bits. The Meson build fails to configure when these values differ
 *    from the project version.
 * @{
 */
/* cppcheck-suppress-begin misra-c2012-2.5 ; @deviation Public version macros
 * for consumer diagnostics and #if checks; the library does not use them. */
#define FPC_VERSION_MAJOR 0
#define FPC_VERSION_MINOR 4
#define FPC_VERSION_PATCH 0
#define FPC_VERSION       "0.4.0"
#define FPC_VERSION_NUMBER                                                     \
        ((FPC_VERSION_MAJOR * 10000L) + (FPC_VERSION_MINOR * 100L)             \
         + FPC_VERSION_PATCH)
/* cppcheck-suppress-end misra-c2012-2.5 ; @deviation As above. */
/** @} */

/**
 * @enum fpc_status
 * @brief Common return codes for public APIs.
 *
 * Negative values indicate errors. Positive values indicate successful
 * operations with a noteworthy condition, such as saturation.
 *
 * @note The function contract specifies that:
 *   - FPC_STATUS_OK (0): Operation completed successfully
 *   - FPC_STATUS_NULL_PTR (-1): Null pointer passed where valid pointer
 * required
 *   - FPC_STATUS_INVALID_PARAM (-2): Configuration parameters invalid
 *   - FPC_STATUS_NOT_INITIALIZED (-3): Object not initialised
 *   - FPC_STATUS_SATURATED (1): Operation succeeded but clamping/saturation
 * occurred
 */
enum fpc_status {
        FPC_STATUS_OK = 0,
        FPC_STATUS_SATURATED = 1,
        FPC_STATUS_NULL_PTR = -1,
        FPC_STATUS_INVALID_PARAM = -2,
        FPC_STATUS_NOT_INITIALIZED = -3
};

#endif /* FPC_STATUS_H_ */
