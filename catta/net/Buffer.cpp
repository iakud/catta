#include <catta/net/Buffer.h>

#include <cstring>

using namespace catta;
Buffer::Buffer()
	: capacity_(kCapacity)
	, readIndex_(0)
	, writeIndex_(0)
	, next_(nullptr) {
	buffer_ = static_cast<char*>(std::malloc(capacity_));
}

Buffer::~Buffer() {
	if (buffer_) {
		std::free(buffer_);
	}
}

BufferPool::BufferPool(uint32_t capacity)
	: capacity_(capacity)
	, count_(0)
	, head_(nullptr)
	, tail_(nullptr) {
}

BufferPool::~BufferPool() {
	Buffer* buffer = take();
	while (buffer) {
		delete buffer;
		buffer = take();
	}
}

void BufferPool::put(Buffer* buffer) {
	if (buffer) {
		if (count_ < capacity_) {
			buffer->next_ = nullptr;
			if (count_ == 0) {
				head_ = tail_ = buffer;
			} else {
				tail_->next_ = buffer;
				tail_ = buffer;
			}
			++count_;
		} else {
			delete buffer;
		}
	}
}

Buffer* BufferPool::take() {
	Buffer* buffer;
	if (count_ > 0) {
		buffer = head_;
		if (head_ == tail_) {
			head_ = tail_ = nullptr;
		} else {
			head_ = head_->next_;
			buffer->next_ = nullptr;
		}
		--count_;
	} else {
		buffer = new Buffer();
	}
	return buffer;
}

SendBuffer::SendBuffer(BufferPool* pool)
	: pool_(pool)
	, size_(0)
	, iovcnt_(0) {
	head_ = tail_ = pool_->take();
	int count = kMaxSend / Buffer::kCapacity + (kMaxSend % Buffer::kCapacity > 2 ? 1 : 0) + 2;
	iov_ = new struct iovec[count];
}

SendBuffer::~SendBuffer() {
	while (head_) {
		Buffer* head = head_;
		head_ = head->next_;
		pool_->put(head);
	}
	if (iov_) {
		delete iov_;
	}
}

void SendBuffer::write(const void* buf, uint32_t count) {
	if (buf == nullptr || count == 0) {
		return;
	}

	uint32_t writableSize = std::min(tail_->capacity_ - tail_->count_, count);
	if (writableSize > 0) {
		// FIXME
		// if (tail_->readIndex_ < tail_->writeIndex_)

		// else
		uint32_t writeSize = tail_->capacity_ - tail_->writeIndex_;
		if (writableSize < writeSize) {
			std::memcpy(tail_->buffer_ + tail_->writeIndex_, buf, writableSize);
			tail_->writeIndex_ += count;
		} else {
			std::memcpy(tail_->buffer_ + tail_->writeIndex_, buf, writeSize);
			tail_->writeIndex_ = writableSize - writeSize;
			if (writableSize > writeSize) {
				std::memcpy(tail_->buffer_, static_cast<const char*>(buf) + writeSize, tail_->writeIndex_);
			}
		}
		tail_->count_ += writableSize;
	}
	uint32_t nwrote = writableSize;
	while (nwrote < count) {
		if (tail_->next_ == nullptr) {
			tail_->next_ = pool_->take();	
		}
		tail_ = tail_->next_;
		writableSize = std::min(tail_->capacity_, count - nwrote);
		std::memcpy(tail_->buffer_, static_cast<const char*>(buf) + nwrote, writableSize);
		if (writableSize < tail_->capacity_) {
			tail_->writeIndex_ = writableSize;
		}
		tail_->count_ = writableSize;
		nwrote += writableSize;
	}
	size_ += count;
}

void SendBuffer::prepareSend(struct iovec** iov, int* iovcnt) {
	int count = 0;
	uint32_t readableSize = head_->count_;
	if (readableSize > 0) {
		if (head_->readIndex_ < head_->writeIndex_) {
			iov_[count].iov_base = head_->buffer_ + head_->readIndex_;
			iov_[count].iov_len = readableSize;
			++count;
		} else {
			iov_[count].iov_base = head_->buffer_ + head_->readIndex_;
			iov_[count].iov_len = head_->capacity_ - head_->readIndex_;
			++count;
			if (head_->writeIndex_ > 0) {
				iov_[count].iov_base = head_->buffer_;
				iov_[count].iov_len = head_->writeIndex_;
				++count;
			}
		}
		uint32_t nread = readableSize;
		Buffer* buffer = head_;
		while (buffer->next_ && nread < kMaxSend) {
			buffer = buffer->next_;
			iov_[count].iov_base = buffer->buffer_;
			iov_[count].iov_len = buffer->count_;
			++count;
			nread += buffer->count_;
		}
	}
	*iov = iov_;
	*iovcnt = count;
}

void SendBuffer::hasSent(uint32_t count) {
	uint32_t readableSize = head_->count_;
	if (readableSize > 0) {
		// FIXME
		if (count < readableSize) {
			//uint32_t readSize = head_->
			//head_->readIndex_ =
		}
	}
	/*
	uint32_t readable;
	uint32_t nsent = 0;
	while (nsent < count) {
		Buffer* buffer = listBuffer_.front();
		readable = std::min(buffer->readableBytes(), count - nsent);
		buffer->hasRead(readable);
		if (buffer->readableBytes() == 0) {
			pool_->put(listBuffer_.popFront());
		}
		nsent += readable;
	}
	size_ -= count;
	*/
}

/*
ssize_t SendBuffer::writeSocket(Socket& socket) {
	if (!head_->next_) {
		size_t readable = head_->write_ - head_->read_;
		const ssize_t nwrite = socket.write(head_->buffer_ + head_->read_, readable);
		if (nwrite > 0) {
			if (static_cast<size_t>(nwrite) < readable) {
				head_->read_ += nwrite;
			} else {
				head_->read_ = head_->write_ = 0;
			}
			count_ -= nwrite;
		}
		return nwrite;
	}

	if (head_->read_ == 0) {
		size_t readable = head_->write_ - head_->read_;
		const ssize_t nwrite = socket.write(head_->buffer_, readable);
		if (nwrite > 0) {
			if (static_cast<size_t>(nwrite) < readable) {
				head_->read_ += nwrite;
			} else {
				Buffer* buffer = head_;
				head_ = head_->next_;
				buffer->clear();
				pool_->put(buffer);
			}
			count_ -= nwrite;
		}
		return nwrite;
	}

	struct iovec iov[2];
	Buffer* next = head_->next_;
	size_t readable = head_->write_ - head_->read_;
	iov[0].iov_base = head_->buffer_ + head_->read_;
	iov[0].iov_len = readable;
	iov[1].iov_base = next->buffer_;
	iov[1].iov_len = next->write_ - head_->read_;

	const ssize_t nwrite = socket.writev(iov, 2);
	if (nwrite > 0) {
		if (static_cast<size_t>(nwrite) < readable) {
			head_->read_ += nwrite;
		} else {
			head_->read_ = head_->write_ = 0;
			head_->next_ = nullptr;
			pool_->put(head_);
			head_ = next;
			if (static_cast<size_t>(nwrite) > readable) {
				head_->read_ = nwrite - readable;
			}
		}
		count_ -= nwrite;
	}
	return nwrite;
}
*/

ReceiveBuffer::ReceiveBuffer(BufferPool* pool)
	: pool_(pool)
	, size_(0) {
	head_ = tail_ = pool_->take();
	int count = kMaxReceive / Buffer::kCapacity + (kMaxReceive % Buffer::kCapacity > 2 ? 1 : 0) + 2;
	iov_ = new struct iovec[count];
}

ReceiveBuffer::~ReceiveBuffer() {
	while (head_) {
		Buffer* head = head_;
		head_ = head->next_;
		pool_->put(head);
	}
	if (iov_) {
		delete iov_;
	}
}

void ReceiveBuffer::prepareReceive(struct iovec** iov, int* iovcnt) {
	int count = 0;
	uint32_t writableSize = tail_->capacity_ - tail_->count_;
	if (writableSize > 0) {
		if (tail_->writeIndex_ < tail_->readIndex_) {
			iov_[count].iov_base = tail_->buffer_ + tail_->writeIndex_;
			iov_[count].iov_len = writableSize;
			++count;
		} else {
			iov_[count].iov_base = tail_->buffer_ + tail_->writeIndex_;
			iov_[count].iov_len = tail_->capacity_ - tail_->writeIndex_;
			++count;
			if (tail_->readIndex_ > 0) {
				iov_[count].iov_base = tail_->buffer_;
				iov_[count].iov_len = tail_->readIndex_;
				++count;
			}
		}
	}
	uint32_t nwrote = writableSize;
	Buffer* buffer = tail_;
	while (nwrote < kMaxReceive) {
		if (buffer->next_ == nullptr) {
			buffer->next_ = pool_->take();	
		}
		buffer = buffer->next_;
		iov_[count].iov_base = buffer->buffer_;
		iov_[count].iov_len = buffer->capacity_;
		++count;
		nwrote += buffer->capacity_;
	}
	*iov = iov_;
	*iovcnt = count;
}

void ReceiveBuffer::hasReceived(uint32_t count) {
	uint32_t writableSize = std::min(tail_->capacity_ - tail_->count_, count);
	if (writableSize > 0) {
		// FIXME
		uint32_t writeSize = tail_->capacity_ - tail_->writeIndex_;
		if (writableSize < writeSize) {
			tail_->writeIndex_ += count;
		} else {
			tail_->writeIndex_ = writableSize - writeSize;
		}
		tail_->count_ += writableSize;
	}
	uint32_t nwrote = writableSize;
	while (nwrote < count) {
		tail_ = tail_->next_;
		writableSize = std::min(tail_->capacity_, count - nwrote);
		if (writableSize < tail_->capacity_) {
			tail_->writeIndex_ = writableSize;
		}
		tail_->count_ = writableSize;
		nwrote += writableSize;
	}
	size_ -= count;
}