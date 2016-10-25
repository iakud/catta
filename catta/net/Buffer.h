#ifndef CATTA_NET_BUFFER_H
#define CATTA_NET_BUFFER_H

#include <catta/base/noncopyable.h>

#include <cstdint>

#include <unistd.h>

namespace catta {

class Socket;

class Buffer {
public:
	Buffer();
	~Buffer();

private:
	static const size_t kCapacity = 1024 * 8;

	char buffer_[kCapacity];
	size_t capacity_;
	size_t write_;
	size_t read_;
	Buffer* next_;

	friend class ListBuffer;
}; // end class Buffer

class BufferPool : noncopyable {
public:
	BufferPool(const uint32_t size)
		: size_(size)
		, count_(0) {
	}
	~BufferPool() {
	}

	void put(Buffer* buffer) {
		if (buffer) {
			delete buffer;
		}
	}

	Buffer* take() {
		Buffer* buffer = new Buffer();
		return buffer;
	}

	Buffer* next() {
		if (!next_) {
			next_ = take();
		}
		return next_;
	}

private:
	const uint32_t size_;
	uint32_t count_;
	Buffer* head_;		// head buffer
	Buffer* tail_;		// tail buffer
	Buffer* next_;
}; // end class BufferPool

class ListBuffer {
public:
	ListBuffer(BufferPool* pool);
	~ListBuffer();

	void write(const char* buf, size_t count);
	void read(char* data, size_t count);
	ssize_t write(Socket& socket);
	ssize_t read(Socket& socket);

private:
	BufferPool* pool_;
	size_t count_;
	Buffer* head_;
	Buffer* tail_;
}; // end class ListBuffer

} // end namespace catta

#endif // CATTA_NET_BUFFER_H