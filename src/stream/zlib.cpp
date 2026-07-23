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

#include "stream/zlib.hpp"

#include <cstring>

#include <zlib.h>

namespace stream {

zlib_decompressor_impl::zlib_decompressor_impl() : stream(NULL) {
	
	z_stream * strm = new z_stream;
	std::memset(strm, 0, sizeof(*strm));
	
	int ret = inflateInit(strm);
	if(ret != Z_OK) {
		std::string msg = strm->msg ? strm->msg : "zlib init error";
		delete strm;
		throw zlib_error(msg, ret);
	}
	
	stream = strm;
}

zlib_decompressor_impl::~zlib_decompressor_impl() {
	close();
}

bool zlib_decompressor_impl::filter(const char * & begin_in, const char * end_in,
                                    char * & begin_out, char * end_out, bool flush) {
	
	z_stream * strm = static_cast<z_stream *>(stream);
	
	strm->next_in = reinterpret_cast<Bytef *>(const_cast<char *>(begin_in));
	strm->avail_in = uInt(end_in - begin_in);
	
	strm->next_out = reinterpret_cast<Bytef *>(begin_out);
	strm->avail_out = uInt(end_out - begin_out);
	
	int ret = inflate(strm, Z_NO_FLUSH);
	
	if(flush && ret == Z_BUF_ERROR && strm->avail_out > 0 && strm->avail_in == 0) {
		throw zlib_error("truncated zlib stream", ret);
	}
	
	begin_in = reinterpret_cast<const char *>(strm->next_in);
	begin_out = reinterpret_cast<char *>(strm->next_out);
	
	if(ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
		std::string msg = strm->msg ? strm->msg : "zlib decompression error";
		throw zlib_error(msg, ret);
	}
	
	return (ret != Z_STREAM_END);
}

void zlib_decompressor_impl::close() {
	
	if(stream) {
		z_stream * strm = static_cast<z_stream *>(stream);
		inflateEnd(strm);
		delete strm;
		stream = NULL;
	}
}

} // namespace stream
