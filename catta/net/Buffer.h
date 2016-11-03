#ifndef CATTA_NET_BUFFER_H
#define CATTA_NET_BUFFER_H

#include <catta/base/noncopyable.h>

#include <cstdint>
#include <memory>
#include <unistd.h>
#include <sys/uio.h>

namespace catta {

class Buffer : noncopyable {
public:
	Buffer();
	~Buffer();

	static const uint32_t kCapacity = 8 * 1024;

	char* buffer_;
	uint32_t capacity_;
	uint32_t count_;
	uint32_t readIndex_;
	uint32_t writeIndex_;
	Buffer* next_;
}; // end class Buffer

class BufferPool : noncopyable {
public:
	BufferPool(uint32_t capacity);
	~BufferPool();

	inline void put(Buffer* buffer);
	inline Buffer* take();

private:
	const uint32_t capacity_;
	uint32_t count_;
	Buffer* head_;
	Buffer* tail_;
}; // end class BufferPool

class SendBuffer : noncopyable {
public:
	SendBuffer(BufferPool* pool);
	~SendBuffer();

	void write(const void* buf, uint32_t count);
	uint32_t size() { return size_; }

private:
	void prepareSend(struct iovec** iov, int* iovcnt);
	void hasSent(uint32_t count);

private:
	static const uint32_t kMaxSend = 64 * 1024;

	BufferPool* pool_;
	uint32_t size_;
	Buffer* head_;
	Buffer* tail_;
	struct iovec* iov_;
	int iovcnt_;

	friend class TcpConnection;
}; // end class SendBuffer

class ReceiveBuffer : noncopyable {
public:
	ReceiveBuffer(BufferPool* pool);
	~ReceiveBuffer();

	void peek(void* buf, uint32_t count);
	void read(void* buf, uint32_t count);
	void retrieve(uint32_t count);
	uint32_t size() { return size_; }

private:
	void prepareReceive(struct iovec** iov, int* iovcnt);
	void hasReceived(uint32_t count);

private:
	static const uint32_t kMaxReceive = 8 * 1024;

	BufferPool* pool_;
	uint32_t size_;
	Buffer* head_;
	Buffer* tail_;
	struct iovec* iov_;

	friend class TcpConnection;
}; // end class ReceiveBuffer

} // end namespace catta

#endif // CATTA_NET_BUFFER_H