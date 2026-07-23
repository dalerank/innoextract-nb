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

#include "util/boostfs_compat.hpp"

#if defined(_WIN32)

#include "util/encoding.hpp"

namespace util {

std::string as_string(const std::filesystem::path & path) {
	
	const std::wstring & wide = path.native();
	std::string utf16le(reinterpret_cast<const char *>(wide.data()), wide.size() * sizeof(wchar_t));
	
	std::string result;
	utf16le_to_wtf8(utf16le, result);
	return result;
	
}

std::filesystem::path u8path(const std::string & utf8) {
	
	std::string utf16le;
	wtf8_to_utf16le(utf8, utf16le);
	
	std::wstring wide(reinterpret_cast<const wchar_t *>(utf16le.data()), utf16le.size() / sizeof(wchar_t));
	
	return std::filesystem::path(wide);
	
}

} // namespace util

#endif // defined(_WIN32)
