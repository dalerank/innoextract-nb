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
 * Minimal replacement for the parts of boost::iostreams used by innoextract:
 * a pull-based "Source" concept plus a chain of filters built on top of it.
 *
 * A Source is anything with a member function
 * \code std::streamsize read(char * buffer, std::streamsize n) \endcode
 * that returns the number of bytes read, \c 0 only if \c n was \c 0, or \c -1 at
 * end of stream (mirroring the behavior of the old boost::iostreams Source concept).
 *
 * A Filter is anything with a member function template
 * \code template <typename Source> std::streamsize read(Source & src, char * dest, std::streamsize n) \endcode
 * that pulls from the given upstream Source and returns data the same way a Source does.
 *
 * \ref chain composes a sequence of Filters on top of one terminal device (a Source),
 * lazily wiring them together (filters are registered via \ref chain::push in the same
 * order they used to be pushed onto a boost::iostreams::chain, while the terminal device
 * is registered via \ref chain::push_device) and exposes the result as a single Source.
 *
 * \ref filtering_istream wraps a \ref chain in a real std::istream, so callers can keep
 * using read()/gcount()/eof()/exceptions() as before.
 */
#ifndef INNOEXTRACT_STREAM_IO_HPP
#define INNOEXTRACT_STREAM_IO_HPP

#include <cstdio>
#include <istream>
#include <memory>
#include <streambuf>
#include <vector>

namespace stream {

namespace io {

//! Type-erased Source: something that can be read from a fixed number of bytes at a time.
class source {
	
public:
	
	virtual ~source() = default;
	
	//! \return the number of bytes read, or \c -1 at end of stream.
	virtual std::streamsize read(char * buffer, std::streamsize n) = 0;
	
};

//! Sentinel kept only for parity with boost::iostreams::WOULD_BLOCK - never actually
//! returned by any of our (purely synchronous) sources or filters.
const int WOULD_BLOCK = -2;

//! Read from a Source - trivial forwarding function, kept for parity with the
//! boost::iostreams::read() free function that the ported filter code used to call.
template <typename Source>
std::streamsize read(Source & src, char * buffer, std::streamsize n) {
	return src.read(buffer, n);
}

//! Read a single byte from a Source.
//! \return the byte value (0-255), or \c EOF at the end of the stream.
template <typename Source>
int get(Source & src) {
	char c;
	std::streamsize n = src.read(&c, 1);
	if(n <= 0) {
		return EOF;
	}
	return int(static_cast<unsigned char>(c));
}

namespace detail {

//! Wraps a device (a concrete Source-like object) by value as a type-erased \ref source.
template <typename Device>
class device_source : public source {
	
	Device device_;
	
public:
	
	explicit device_source(Device device) : device_(std::move(device)) { }
	
	std::streamsize read(char * buffer, std::streamsize n) override {
		return device_.read(buffer, n);
	}
	
};

//! Wraps a Filter plus its (type-erased) upstream Source as a new type-erased \ref source.
template <typename Filter>
class filter_source : public source {
	
	Filter filter_;
	std::unique_ptr<io::source> upstream_;
	
public:
	
	filter_source(Filter filter, std::unique_ptr<io::source> upstream)
		: filter_(std::move(filter)), upstream_(std::move(upstream)) { }
	
	std::streamsize read(char * buffer, std::streamsize n) override {
		return filter_.read(*upstream_, buffer, n);
	}
	
};

/*!
 * Move-only factory that wraps a (possibly non-copyable) Filter and, once given an
 * upstream \ref source, produces a new \ref filter_source on top of it.
 *
 * Used instead of std::function (which requires its target to be copy-constructible)
 * to store pending \ref chain::push() filters, since our decompressor filters own
 * non-copyable state (zlib/bzip2/lzma streams).
 */
class filter_factory {
	
public:
	
	virtual ~filter_factory() = default;
	
	virtual std::unique_ptr<source> build(std::unique_ptr<source> upstream) = 0;
	
};

template <typename Filter>
class filter_factory_impl : public filter_factory {
	
	Filter filter_;
	
public:
	
	explicit filter_factory_impl(Filter filter) : filter_(std::move(filter)) { }
	
	std::unique_ptr<source> build(std::unique_ptr<source> upstream) override {
		return std::unique_ptr<source>(new filter_source<Filter>(std::move(filter_), std::move(upstream)));
	}
	
};

} // namespace detail

/*!
 * A chain of filters built on top of one terminal device (Source).
 *
 * Filters are registered outer-to-inner via \ref push(), i.e. in the same order they
 * used to be pushed onto a boost::iostreams::chain: the first filter pushed ends up
 * closest to the consumer (it is applied last / read from first), while the device
 * pushed via \ref push_device() is the ultimate source of raw bytes.
 *
 * The actual pipeline is only wired together lazily, on the first \ref read() call,
 * so the push-order does not matter relative to when the device is registered.
 */
class chain : public source {
	
	// Filter factories, stored in push() order (outer to inner).
	std::vector< std::unique_ptr<detail::filter_factory> > filters_;
	std::unique_ptr<source> device_;
	std::unique_ptr<source> built_;
	
public:
	
	chain() = default;
	
	chain(const chain &) = delete;
	chain & operator=(const chain &) = delete;
	chain(chain &&) = default;
	chain & operator=(chain &&) = default;
	
	//! Register a filter. Must be called before the first \ref read().
	template <typename Filter>
	void push(Filter filter, size_t /* buffer_size */ = 0) {
		filters_.push_back(
			std::unique_ptr<detail::filter_factory>(
				new detail::filter_factory_impl<Filter>(std::move(filter))
			)
		);
	}
	
	//! Register the terminal device (the actual source of raw bytes).
	//! Must be called exactly once, before the first \ref read().
	template <typename Device>
	void push_device(Device device) {
		device_.reset(new detail::device_source<Device>(std::move(device)));
	}
	
	std::streamsize read(char * buffer, std::streamsize n) override {
		if(!built_) {
			std::unique_ptr<source> cur = std::move(device_);
			for(auto it = filters_.rbegin(); it != filters_.rend(); ++it) {
				cur = (*it)->build(std::move(cur));
			}
			built_ = std::move(cur);
		}
		return built_->read(buffer, n);
	}
	
};

namespace detail {

//! std::streambuf that pulls its data from a \ref chain.
class chain_streambuf : public std::streambuf {
	
	chain & chain_;
	std::vector<char> buffer_;
	
public:
	
	explicit chain_streambuf(chain & c, size_t buffer_size = 8192) : chain_(c), buffer_(buffer_size) { }
	
protected:
	
	int_type underflow() override {
		if(gptr() < egptr()) {
			return traits_type::to_int_type(*gptr());
		}
		std::streamsize n = chain_.read(buffer_.data(), std::streamsize(buffer_.size()));
		if(n <= 0) {
			return traits_type::eof();
		}
		setg(buffer_.data(), buffer_.data(), buffer_.data() + n);
		return traits_type::to_int_type(*gptr());
	}
	
};

} // namespace detail

//! A std::istream backed by a \ref chain of filters, mirroring the (small) subset of
//! boost::iostreams::filtering_istream that innoextract used.
class filtering_istream : public std::istream {
	
	io::chain chain_;
	detail::chain_streambuf buf_;
	
public:
	
	filtering_istream() : std::istream(nullptr), chain_(), buf_(chain_) {
		this->rdbuf(&buf_);
	}
	
	filtering_istream(const filtering_istream &) = delete;
	filtering_istream & operator=(const filtering_istream &) = delete;
	
	//! Register a filter, closest to the consumer first (same order as boost's chain::push).
	template <typename Filter>
	void push(Filter filter, size_t buffer_size = 0) {
		chain_.push(std::move(filter), buffer_size);
	}
	
	//! Register the terminal device.
	template <typename Device>
	void push_device(Device device) {
		chain_.push_device(std::move(device));
	}
	
};

//! Adapts a std::istream to the Source concept (read() returning std::streamsize).
class istream_source {
	
	std::istream * is_;
	
public:
	
	explicit istream_source(std::istream & is) : is_(&is) { }
	
	std::streamsize read(char * buffer, std::streamsize n) {
		
		is_->read(buffer, n);
		std::streamsize nread = is_->gcount();
		
		if(is_->eof() && !is_->bad()) {
			// Reading past the end of the stream is not an error for our Source concept.
			is_->clear(is_->rdstate() & ~std::ios_base::failbit);
		}
		
		return (nread == 0) ? -1 : nread;
	}
	
};

} // namespace io

} // namespace stream

#endif // INNOEXTRACT_STREAM_IO_HPP
