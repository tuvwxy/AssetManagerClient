// Copyright (C) 2011 by Toshiro Yamada
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#include "tcp_client.hpp"

#include <iostream>

#if defined(_WIN32)
#include <boost/cstdint.hpp> // int32_t for Windows
using boost::int32_t;
#endif

using namespace am;

namespace asio = boost::asio;

//-----------------------------------------------------------------------------
TCPClient::AsyncTCPClient::AsyncTCPClient(asio::io_service& io_service,
    boost::condition_variable& cond, boost::mutex& mut)
: io_service_(io_service)
, socket_(io_service)
, connected_(false)
, connecting_(false)
, write_in_progress_(false)
, write_progress_cond_(cond)
, write_progress_mut_(mut)
{
}

TCPClient::AsyncTCPClient::~AsyncTCPClient()
{
  socket_.close();
}

void TCPClient::AsyncTCPClient::Connect(
    asio::ip::tcp::resolver::iterator endpoint_iterator)
{
  if (!connected_ && !connecting_) {
    connecting_ = true;
    asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
    socket_.async_connect(endpoint,
        boost::bind(&AsyncTCPClient::HandleConnect, this,
          asio::placeholders::error, ++endpoint_iterator));
  }
}

void TCPClient::AsyncTCPClient::Send(const std::vector<char>& msg)
{
  io_service_.post(boost::bind(&AsyncTCPClient::DoSend, this, msg));
}

void TCPClient::AsyncTCPClient::Close()
{
  io_service_.post(boost::bind(&AsyncTCPClient::DoClose, this));
}

void TCPClient::AsyncTCPClient::HandleConnect(
    const boost::system::error_code& error,
    asio::ip::tcp::resolver::iterator endpoint_iterator)
{
  if (error) {
    socket_.close();
    if (endpoint_iterator != asio::ip::tcp::resolver::iterator()) {
      asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
      socket_.async_connect(endpoint,
          boost::bind(&AsyncTCPClient::HandleConnect, this,
            asio::placeholders::error, ++endpoint_iterator));
    } else {
      connected_ = false;
      {
        boost::lock_guard<boost::mutex> lock(write_progress_mut_);
        connecting_ = false;
      }
      write_progress_cond_.notify_all();
    }
  } else if (connecting_) {
    connected_ = true;
    connecting_ = false;
    if (!write_in_progress_ && !write_msgs_.empty()) {
      write_in_progress_ = true;
      asio::async_write(socket_,
          asio::buffer(write_msgs_.front()),
          boost::bind(&AsyncTCPClient::HandleWrite, this,
            asio::placeholders::error));
    }
  } else {
    std::cerr << "TCPClient::AsyncTCPClient::HandleConnect(): error\n";
    {
      boost::lock_guard<boost::mutex> lock(write_progress_mut_);
      connecting_ = false;
    }
    write_progress_cond_.notify_all();
  }
}

void TCPClient::AsyncTCPClient::DoSend(const std::vector<char> msg)
{
  if (!connected_ && !connecting_) return;

  // construct a message with prefixed length
  std::vector<char> prefixed_msg(msg.size() + 4);
  int32_t frame_size = htonl(msg.size());
  memcpy(&prefixed_msg[0], &frame_size, 4);
  std::copy(msg.begin(), msg.end(), prefixed_msg.begin() + 4);

  write_msgs_.push_back(prefixed_msg);
  if (!write_in_progress_ && !connecting_) {
    write_in_progress_ = true;
    asio::async_write(socket_,
        asio::buffer(write_msgs_.front()),
        boost::bind(&AsyncTCPClient::HandleWrite, this,
          asio::placeholders::error));
  }
}

void TCPClient::AsyncTCPClient::HandleWrite(
    const boost::system::error_code& error)
{
  if (!error) {
    write_msgs_.pop_front();
    if (!write_msgs_.empty()) {
      asio::async_write(socket_,
          asio::buffer(write_msgs_.front()),
          boost::bind(&AsyncTCPClient::HandleWrite, this,
            asio::placeholders::error));
    } else {
      {
        boost::lock_guard<boost::mutex> lock(write_progress_mut_);
        write_in_progress_ = false;
      }
      write_progress_cond_.notify_all();
    }
  } else {
    {
      boost::lock_guard<boost::mutex> lock(write_progress_mut_);
      write_in_progress_ = false;
    }
    write_progress_cond_.notify_all();
    DoClose();
  }
}

void TCPClient::AsyncTCPClient::DoClose()
{
  connected_ = false;
  connecting_ = false;
  write_in_progress_ = false;
  socket_.close();
  write_msgs_.clear();
}

//-----------------------------------------------------------------------------
TCPClient::TCPClient(const std::string& host, int port)
: host_(host)
, port_(port)
, client_(io_service_, write_progress_cond_, write_progress_mut_)
, service_is_ready_(false)
, thread_is_running_(false)
{
}

TCPClient::~TCPClient()
{
  if (thread_is_running_) {
    if (service_is_ready_) {
      client_.Close();
      io_service_.stop();
      thread_.join();
    } else {
      thread_.timed_join(boost::posix_time::seconds(0));
    }
  }
}

void TCPClient::Send(const std::vector<char>& msg)
{
  if (!thread_is_running_ && !RunThread()) return;
  client_.Send(msg);
}

void TCPClient::BlockUntilQueueIsEmpty()
{
  // Block until IO thread reached a ready state
  while (thread_is_running_ && !service_is_ready_);

  boost::unique_lock<boost::mutex> lock(write_progress_mut_);
  while (client_.WriteInProgress() || client_.Connecting()) {
    write_progress_cond_.wait(lock);
  }
}

bool TCPClient::RunThread()
{
  bool success = true;
  thread_is_running_ = true;
  try {
    thread_ = boost::thread(boost::bind(&TCPClient::Run, this));
  } catch (std::exception& e) {
    std::cerr << "TCPClient::RunThread(): exception -> " << e.what() << "\n";
    success = false;
    thread_is_running_ = false;
  }

  return success;
}

void TCPClient::Run()
{
  std::stringstream port_string;
  port_string << port_;

  using asio::ip::tcp;
  try {
    tcp::resolver resolver(io_service_);
    tcp::resolver::query query(host_, port_string.str());
    asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
    client_.Connect(iterator);
    asio::io_service::work work(io_service_);
    service_is_ready_ = true;
    io_service_.run();
  } catch (std::exception& e) {
    std::cerr << "TCPClient::Run(): exception -> " << e.what() << "\n";
  }
  io_service_.reset();
  service_is_ready_ = false;
  thread_is_running_ = false;
}

