#include "Watcher.h"

#include "EventLoop.h"

using namespace catta;

Watcher::Watcher(const int fd, EventLoop* loop)
	: fd_(fd)
	, loop_(loop)
	, events_(WatcherEvents::kEventNone)
	, revents_(WatcherEvents::kEventNone)
	, started_(false)
	, activeIndex_(kInvalidActiveIndex) {
}

Watcher::~Watcher() {
}

void Watcher::setEvents(WatcherEvents events) {
	events_ = events | WatcherEvents::kEventError | WatcherEvents::kEventClose;
	update();
}

void Watcher::activeEvents(WatcherEvents revents) {
	if (started_) {
		revents_ |= revents;
		if (activeIndex_ == kInvalidActiveIndex) {
			activeIndex_ = static_cast<int>(loop_->activeWatchers_.size());
			loop_->activeWatchers_.push_back(this);
		}
	}
}

void Watcher::start() {
	if (!started_) {
		loop_->addWatcher(this);
		started_ = true;
	}
}

void Watcher::update() {
	if (started_) {
		loop_->updateWatcher(this);
	}
}

void Watcher::stop() {
	if (started_) {
		if (activeIndex_ != kInvalidActiveIndex) {
			loop_->activeWatchers_[activeIndex_] = nullptr;
			activeIndex_ = kInvalidActiveIndex;
		}
		loop_->removeWatcher(this);
		started_ = false;
	}
}

void Watcher::handleEvents() {
	WatcherEvents revents = revents_ & events_;
	revents_ = WatcherEvents::kEventNone;
	activeIndex_ = kInvalidActiveIndex;
	if (revents & kEventClose && closeCallback_) {
		closeCallback_();
	}
	if (revents & kEventError && errorCallback_) {
		errorCallback_();
	}
	if (revents & kEventRead && readCallback_) {
		readCallback_();
	}
	if (revents & kEventWrite && writeCallback_) {
		writeCallback_();
	}
}