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

#include "stream/bzip2.hpp"

#include <cstring>

#include <bzlib.h>

namespace stream {

bzip2_decompressor_impl::bzip2_decompressor_impl() : stream(NULL) {
	
	bz_stream * strm = new bz_stream;
	std::memset(strm, 0, sizeof(*strm));
	
	int ret = BZ2_bzDecompressInit(strm, 0 /* verbosity */, 0 /* small */);
	if(ret != BZ_OK) {
		delete strm;
		throw bzip2_error("bzip2 init error", ret);
	}
	
	stream = strm;
}

bzip2_decompressor_impl::~bzip2_decompressor_impl() {
	close();
}

bool bzip2_decompressor_impl::filter(const char * & begin_in, const char * end_in,
                                     char * & begin_out, char * end_out, bool flush) {
	
	bz_stream * strm = static_cast<bz_stream *>(stream);
	
	strm->next_in = const_cast<char *>(begin_in);
	strm->avail_in = unsigned(end_in - begin_in);
	
	strm->next_out = begin_out;
	strm->avail_out = unsigned(end_out - begin_out);
	
	int ret = BZ2_bzDecompress(strm);
	
	if(flush && ret == BZ_OK && strm->avail_out > 0 && strm->avail_in == 0) {
		throw bzip2_error("truncated bzip2 stream", ret);
	}
	
	begin_in = strm->next_in;
	begin_out = strm->next_out;
	
	if(ret != BZ_OK && ret != BZ_STREAM_END) {
		throw bzip2_error("bzip2 decompression error", ret);
	}
	
	return (ret != BZ_STREAM_END);
}

void bzip2_decompressor_impl::close() {
	
	if(stream) {
		bz_stream * strm = static_cast<bz_stream *>(stream);
		BZ2_bzDecompressEnd(strm);
		delete strm;
		stream = NULL;
	}
}

} // namespace stream
