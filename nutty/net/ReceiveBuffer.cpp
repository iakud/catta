#include <nutty/net/ReceiveBuffer.h>

#include <nutty/net/Buffer.h>

#include <cstring>
#include <arpa/inet.h>

using namespace nutty;

ReceiveBuffer::ReceiveBuffer()
	: size_(0) {
}

ReceiveBuffer::~ReceiveBuffer() {
	for (Buffer*& buffer : buffers_) {
		delete buffer;
	}
	for (Buffer*& buffer : extendBuffers_) {
		delete buffer;
	}
}

void ReceiveBuffer::read(void* buf, uint32_t count) {
	uint32_t nread = 0;
	while (nread < count && !buffers_.empty()) {
		Buffer* buffer = buffers_.front();
		uint32_t readableSize = std::min(buffer->readableSize(), count - nread);
		std::memcpy(static_cast<char*>(buf) + nread, buffer->dataRead(), readableSize);
		buffer->hasRead(readableSize);
		nread += readableSize;
		if (buffer->empty()) {
			buffers_.pop_front();
			buffer->reset();
			extendBuffers_.push_back(buffer);
		}
	}
	size_ -= nread;
}

void ReceiveBuffer::peek(void* buf, uint32_t count) const {
	uint32_t nread = 0;
	for (const Buffer* buffer : buffers_) {
		if (nread >= count) break;
		uint32_t readableSize = std::min(buffer->readableSize(), count - nread);
		std::memcpy(static_cast<char*>(buf) + nread, buffer->dataRead(), readableSize);
		nread += readableSize;
	}
}

void ReceiveBuffer::peek(void* buf, uint32_t offset, uint32_t count) const {
	uint32_t nread = 0;
	for (const Buffer* buffer : buffers_) {
		if (nread >= count) break;
		uint32_t readableSize = std::min(buffer->readableSize(), count - nread);
		if (offset > 0) {
			if (offset < readableSize) {
				readableSize -= offset;
				std::memcpy(static_cast<char*>(buf) + nread, buffer->dataRead() + offset, readableSize);
				offset = 0;
				nread += readableSize;
			} else {
				offset -= readableSize;
			}
		} else {
			std::memcpy(static_cast<char*>(buf) + nread, buffer->dataRead() + offset, readableSize);
			nread += readableSize;
		}
	}
}

void ReceiveBuffer::retrieve(uint32_t count) {
	uint32_t nread = 0;
	while (nread < count && !buffers_.empty()) {
		Buffer* buffer = buffers_.front();
		uint32_t readableSize = std::min(buffer->readableSize(), count - nread);
		buffer->hasRead(readableSize);
		nread += readableSize;
		if (buffer->empty()) {
			buffers_.pop_front();
			buffer->reset();
			extendBuffers_.push_back(buffer);
		}
	}
	size_ -= nread;
}

void ReceiveBuffer::retrieveAll() {
	while (!buffers_.empty()) {
		Buffer* buffer = buffers_.front();
		buffers_.pop_front();
		buffer->reset();
		extendBuffers_.push_back(buffer);
	}
	size_ = 0;
}

void ReceiveBuffer::prepareReceive(struct iovec* iov, int& iovcnt) {
	iovcnt = 0;
	uint32_t nwrote = 0;
	
	if (!buffers_.empty()) {
		Buffer*& buffer = buffers_.back();
		uint32_t writableSize = buffer->writableSize();
		if (writableSize > 0) {
			iov[iovcnt].iov_base = buffer->dataWrite();
			iov[iovcnt].iov_len = writableSize;
			++iovcnt;
			nwrote += writableSize;
		}
	}
	if (nwrote < kReceiveSize) {
		if (extendBuffers_.empty()) {
			Buffer* buffer = new Buffer(kReceiveSize * 2);
			extendBuffers_.push_back(buffer);
		}
		Buffer*& buffer = extendBuffers_.front();
		uint32_t writableSize = buffer->capacity();
		iov[iovcnt].iov_base = buffer->data();
		iov[iovcnt].iov_len = writableSize;
		++iovcnt;
		nwrote += writableSize;
	}
}

void ReceiveBuffer::hasReceived(uint32_t count) {
	uint32_t nwrote = 0;
	if (!buffers_.empty()) {
		Buffer* buffer = buffers_.back();
		uint32_t writableSize = std::min(buffer->writableSize(), count);
		if (writableSize > 0) {
			buffer->hasWritten(writableSize);
			nwrote += writableSize;
		}
	}
	if (nwrote < count && !extendBuffers_.empty()) {
		Buffer* buffer = extendBuffers_.front();
		extendBuffers_.pop_front();
		uint32_t writableSize = std::min(buffer->writableSize(), count - nwrote);
		buffer->hasWritten(writableSize);
		nwrote += writableSize;
		buffers_.push_back(buffer);
	}
	size_ += nwrote;
}

void ReceiveBuffer::prepareSend(struct iovec* iov, int& iovcnt) const {
	iovcnt = 0;
	uint32_t nread = 0;
	for (const Buffer* buffer : buffers_) {
		//if (nread >= kSendSize) break;
		uint32_t readableSize = buffer->readableSize();
		iov[iovcnt].iov_base = buffer->dataRead();
		iov[iovcnt].iov_len = readableSize;
		++iovcnt;
		nread += readableSize;
	}
}