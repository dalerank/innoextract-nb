/*
 * Copyright (C) 2013-2019 Daniel Scharrer
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
 * Wrapper class for a Source (see \ref stream::io) that can be used to restrict
 * sources to appear smaller than they really are.
 */
#ifndef INNOEXTRACT_STREAM_RESTRICT_HPP
#define INNOEXTRACT_STREAM_RESTRICT_HPP

#include <algorithm>
#include <cstdint>
#include <utility>

#include "stream/io.hpp"

namespace stream {

//! Restricts a Source to a specific size from the current position, always using a
//! 64-bit counter. The BaseSource is stored by value - use \ref ref() to wrap sources
//! whose lifetime is managed elsewhere and that should not be copied/moved.
template <typename BaseSource>
class restricted_source {
	
	BaseSource    base;      //!< The base source to read from.
	std::uint64_t remaining; //!< Number of bytes remaining in the restricted source.
	
public:
	
	restricted_source(BaseSource source, std::uint64_t size)
		: base(std::move(source)), remaining(size) { }
	
	std::streamsize read(char * buffer, std::streamsize bytes) {
		
		if(bytes <= 0) {
			return 0;
		}
		
		// Restrict the number of bytes to read
		bytes = std::streamsize(std::min(std::uint64_t(bytes), remaining));
		if(bytes == 0) {
			return -1; // End of the restricted source reached
		}
		
		std::streamsize nread = io::read(base, buffer, bytes);
		
		// Remember how many bytes were read so far
		if(nread > 0) {
			remaining -= std::min(std::uint64_t(nread), remaining);
		}
		
		return nread;
	}
	
};

/*!
 * Restricts a source to a specific size from the current position and makes
 * it non-seekable.
 *
 * \c source is stored by value in the result - pass a lightweight adapter
 * (e.g. \ref ref_source, via \ref ref()) if the underlying object should not be
 * copied or moved.
 */
template <typename BaseSource>
restricted_source<BaseSource> restrict(BaseSource source, std::uint64_t size) {
	return restricted_source<BaseSource>(std::move(source), size);
}

//! Lightweight Source that forwards read() to a referenced object whose lifetime is
//! managed elsewhere. Useful for wrapping non-copyable sources (like slice_reader or
//! io::chain) so they can be used with \ref restrict() and \ref io::chain::push_device.
template <typename T>
class ref_source {
	
	T * ptr;
	
public:
	
	explicit ref_source(T & object) : ptr(&object) { }
	
	std::streamsize read(char * buffer, std::streamsize n) {
		return ptr->read(buffer, n);
	}
	
};

//! \return a \ref ref_source wrapping \c object by reference.
template <typename T>
ref_source<T> ref(T & object) {
	return ref_source<T>(object);
}

} // namespace stream

#endif // INNOEXTRACT_STREAM_RESTRICT_HPP
