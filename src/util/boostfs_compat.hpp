/*
 * Copyright (C) 2012-2020 Daniel Scharrer
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author(s) be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/*!
 * \file
 *
 * Helper functions to convert between std::filesystem::path and UTF-8
 * std::string. All strings used internally (setup data, command-line
 * arguments) are UTF-8. On most platforms std::filesystem::path's narrow
 * encoding already matches this, but on Windows std::filesystem::path is
 * natively UTF-16 and converts to/from std::string using the current ANSI
 * codepage, which would silently mangle non-ASCII characters. These helpers
 * make sure UTF-8 is used consistently regardless of platform.
 */
#ifndef INNOEXTRACT_UTIL_BOOSTFS_COMPAT_HPP
#define INNOEXTRACT_UTIL_BOOSTFS_COMPAT_HPP

#include <filesystem>
#include <string>

namespace util {

inline const std::string & as_string(const std::string & path) {
	return path;
}

#if defined(_WIN32)

//! Convert a std::filesystem::path to a UTF-8 string.
std::string as_string(const std::filesystem::path & path);

//! Construct a std::filesystem::path from a UTF-8 string.
std::filesystem::path u8path(const std::string & utf8);

#else

inline std::string as_string(const std::filesystem::path & path) {
	return path.string();
}

inline std::filesystem::path u8path(const std::string & utf8) {
	return std::filesystem::path(utf8);
}

#endif

} // namespace util

#endif // INNOEXTRACT_UTIL_BOOSTFS_COMPAT_HPP
