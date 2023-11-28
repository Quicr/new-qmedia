#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace channel {

template<typename T>
struct Channel
{
  Channel(size_t capacity)
    : _capacity(capacity)
  {
  }

  std::optional<T> receive()
  {
    auto lock = std::unique_lock<std::mutex>(_mutex);
    _cv.wait(lock, [this] { return !is_empty() || !is_open(); });

    if (!is_open()) {
      return std::nullopt;
    }

    T val = std::move(_data.front());
    _data.pop_front();
    _cv.notify_all();
    return val;
  };

  std::optional<T> receive(std::chrono::milliseconds wait_time)
  {
    auto lock = std::unique_lock<std::mutex>(_mutex);
    const auto success = _cv.wait_for(
      lock, wait_time, [this] { return !is_empty() || !is_open(); });

    if (!success || !is_open()) {
      return std::nullopt;
    }

    T val = std::move(_data.front());
    _data.pop_front();
    _cv.notify_all();
    return val;
  };

  bool send(T&& val)
  {
    auto lock = std::unique_lock<std::mutex>(_mutex);
    _cv.wait(lock, [this] { return is_open() && has_capacity(); });

    if (!is_open()) {
      return false;
    }

    _data.emplace_back(val);
    _cv.notify_all();
    return true;
  };

  bool send(T&& val, std::chrono::milliseconds wait_time)
  {
    auto lock = std::unique_lock<std::mutex>(_mutex);
    const auto success = _cv.wait_for(
      lock, wait_time, [this] { return is_open() && has_capacity(); });

    if (!success || !is_open()) {
      return false;
    }

    _data.emplace_back(val);
    _cv.notify_all();
    return true;
  };

  // Helper functions
  bool is_open() const { return _open; }
  bool is_empty() const { return _data.empty(); }
  bool has_capacity() const { return (_data.size() < _capacity); }
  size_t capacity() const { return _capacity; }
  size_t size() const { return _data.size(); }

protected:
  std::mutex _mutex;
  std::condition_variable _cv;
  std::deque<T> _data;
  size_t _capacity;
  bool _open = true;
};

template<typename T>
struct Sender;

template<typename T>
struct Receiver;

template<typename T>
struct ChannelView
{
  ChannelView(size_t capacity)
    : channel(std::make_shared<Channel<T>>(capacity))
  {
  }

  ChannelView(std::shared_ptr<Channel<T>> channel_in)
    : channel(channel_in)
  {
  }

  Sender<T> make_sender() const { return { channel }; }

  Receiver<T> make_receiver() const { return { channel }; }

  bool is_open() const { return channel->is_open(); }
  bool is_empty() const { return channel->is_empty(); }
  bool has_capacity() const { return channel->has_capacity(); }
  size_t capacity() const { return channel->capacity(); }
  size_t size() const { return channel->size(); }

protected:
  std::shared_ptr<Channel<T>> channel;
};

template<typename T>
struct Sender : ChannelView<T>
{
  using parent = ChannelView<T>;
  using parent::channel;
  using parent::parent;

  auto send(T&& val) const { return channel->send(std::move(val)); }
  auto send(T&& val, std::chrono::milliseconds wait_time) const
  {
    return channel->send(std::move(val), wait_time);
  }
};

template<typename T>
struct Receiver : ChannelView<T>
{
  using parent = ChannelView<T>;
  using parent::channel;
  using parent::parent;

  auto receive() const { return channel->receive(); }
  auto receive(std::chrono::milliseconds wait_time) const
  {
    return channel->receive(wait_time);
  }
};

template<typename T>
std::tuple<Sender<T>, Receiver<T>>
create(size_t capacity)
{
  auto chan = std::make_shared<Channel<T>>(capacity);
  return { chan, chan };
}

} // namespace channel
