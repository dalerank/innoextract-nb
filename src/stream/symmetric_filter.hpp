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
 * Minimal replacement for boost::iostreams::symmetric_filter.
 *
 * Wraps an Impl type providing
 * \code
 * bool filter(const char * & begin_in, const char * end_in,
 *              char * & begin_out, char * end_out, bool flush);
 * void close();
 * \endcode
 * (exactly the interface used by our LZMA/zlib/bzip2 decompressor implementations) and
 * turns it into a pull-based \ref stream::io "io" Filter with an internal input buffer.
 *
 * \c filter() is expected to consume as much of [begin_in, end_in) and produce as much
 * of [begin_out, end_out) as possible, advancing \c begin_in and \c begin_out to reflect
 * how much was consumed/produced. It returns \c true if more output may still be
 * produced (i.e. the end of the compressed stream has not been reached yet) and \c false
 * once the stream is finished. \c flush is \c true once there is no more input data
 * available, so implementations can use it to detect truncated streams.
 */
#ifndef INNOEXTRACT_STREAM_SYMMETRIC_FILTER_HPP
#define INNOEXTRACT_STREAM_SYMMETRIC_FILTER_HPP

#include <algorithm>
#include <cstddef>
#include <ios>
#include <vector>

#include "stream/io.hpp"

namespace stream {

//! Default buffer size used by \ref symmetric_filter, matching
//! boost::iostreams::default_device_buffer_size.
const size_t default_filter_buffer_size = 4096;

template <class Impl>
class symmetric_filter : public Impl {
	
	std::vector<char> inbuf_;
	const char * in_next_;
	const char * in_end_;
	bool input_eof_;
	bool eof_;
	
public:
	
	explicit symmetric_filter(size_t buffer_size = default_filter_buffer_size)
		: inbuf_(buffer_size)
		, in_next_(inbuf_.data())
		, in_end_(inbuf_.data())
		, input_eof_(false)
		, eof_(false)
	{ }
	
	template <typename Source>
	std::streamsize read(Source & src, char * dest, std::streamsize n) {
		
		if(n == 0) {
			return 0;
		}
		if(eof_) {
			return -1;
		}
		
		char * out_next = dest;
		char * out_end = dest + n;
		
		while(out_next < out_end) {
			
			if(in_next_ == in_end_ && !input_eof_) {
				std::streamsize nread = io::read(src, inbuf_.data(), std::streamsize(inbuf_.size()));
				if(nread <= 0) {
					input_eof_ = true;
					in_next_ = in_end_ = inbuf_.data();
				} else {
					in_next_ = inbuf_.data();
					in_end_ = inbuf_.data() + nread;
				}
			}
			
			bool more = this->filter(in_next_, in_end_, out_next, out_end, input_eof_);
			if(!more) {
				eof_ = true;
				this->close();
				break;
			}
			
			if(in_next_ == in_end_ && input_eof_) {
				// No more input will ever be available - whatever the filter produced
				// (possibly nothing) this call is all we can return for now.
				break;
			}
			
		}
		
		std::streamsize result = out_next - dest;
		return (result == 0) ? -1 : result;
	}
	
};

} // namespace stream

#endif // INNOEXTRACT_STREAM_SYMMETRIC_FILTER_HPP
