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

/*!
 * \file
 *
 * bzip2 decompression filter, replacing boost::iostreams::bzip2_decompressor.
 */
#ifndef INNOEXTRACT_STREAM_BZIP2_HPP
#define INNOEXTRACT_STREAM_BZIP2_HPP

#include <ios>
#include <string>

#include "stream/symmetric_filter.hpp"

namespace stream {

//! Error thrown if there was an error in a bzip2 stream.
struct bzip2_error : public std::ios_base::failure {
	
	bzip2_error(const std::string & msg, int code)
		: std::ios_base::failure(msg), error_code(code) { }
	
	//! \return the bzip2 code for the error.
	int error() const { return error_code; }
	
private:
	
	int error_code;
	
};

class bzip2_decompressor_impl {
	
public:
	
	typedef char char_type;
	
	bzip2_decompressor_impl();
	~bzip2_decompressor_impl();
	
	bzip2_decompressor_impl(const bzip2_decompressor_impl &) = delete;
	bzip2_decompressor_impl & operator=(const bzip2_decompressor_impl &) = delete;
	
	bzip2_decompressor_impl(bzip2_decompressor_impl && o) noexcept : stream(o.stream) { o.stream = NULL; }
	bzip2_decompressor_impl & operator=(bzip2_decompressor_impl &&) = delete;
	
	bool filter(const char * & begin_in, const char * end_in,
	            char * & begin_out, char * end_out, bool flush);
	
	void close();
	
private:
	
	void * stream; //!< bz_stream *
	
};

/*!
 * A filter that decompresses bzip2 streams, to be used with \ref stream::io.
 */
typedef symmetric_filter<bzip2_decompressor_impl> bzip2_decompressor;

} // namespace stream

#endif // INNOEXTRACT_STREAM_BZIP2_HPP
