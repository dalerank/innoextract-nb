/*
 * Copyright (C) 2011-2020 Daniel Scharrer
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

#include "util/load.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

namespace util {

void binary_string::load(std::istream & is, std::string & target) {
	
	std::uint32_t length = util::load<std::uint32_t>(is);
	if(is.fail()) {
		return;
	}
	
	target.clear();
	
	while(length) {
		char buffer[10 * 1024];
		std::uint32_t buf_size = std::min(length, std::uint32_t(sizeof(buffer)));
		is.read(buffer, std::streamsize(buf_size));
		target.append(buffer, buf_size);
		length -= buf_size;
	}
}

void binary_string::skip(std::istream & is) {
	
	std::uint32_t length = util::load<std::uint32_t>(is);
	if(is.fail()) {
		return;
	}
	
	discard(is, length);
}

void encoded_string::load(std::istream & is, std::string & target, codepage_id codepage,
                          const std::bitset<256> * lead_bytes) {
	binary_string::load(is, target);
	to_utf8(target, codepage, lead_bytes);
}

unsigned to_unsigned(const char * chars, size_t count) {
	
	if(count == 0) {
		throw bad_number_cast();
	}
	
	unsigned long value = 0;
	for(size_t i = 0; i < count; i++) {
		unsigned char c = static_cast<unsigned char>(chars[i]);
		if(c < '0' || c > '9') {
			throw bad_number_cast();
		}
		value = value * 10 + static_cast<unsigned long>(c - '0');
		if(value > std::numeric_limits<unsigned>::max()) {
			throw bad_number_cast();
		}
	}
	
	return unsigned(value);
}

} // namespace util
