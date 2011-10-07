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
#include "udp_client.hpp"

#include <iostream>

using namespace am;

namespace asio = boost::asio;

//-----------------------------------------------------------------------------
UDPClient::AsyncUDPClient::AsyncUDPClient(asio::io_service& io_service,
    boost::condition_variable& cond, boost::mutex& mut)
: write_in_progress_(false)
, msg_to_send_(false)
, io_service_(io_service)
, socket_(io_service_)
, write_progress_cond_(cond)
, write_progress_mut_(mut)
{
  socket_.open(asio::ip::udp::v4());
}

UDPClient::AsyncUDPClient::~AsyncUDPClient()
{
  socket_.close();
}

void UDPClient::AsyncUDPClient::SetEndpoint(asio::ip::udp::endpoint& endpoint)
{
  endpoint_ = endpoint;
}

void UDPClient::AsyncUDPClient::Send(const std::vector<char>& msg)
{
  msg_to_send_ = true;
  io_service_.post(boost::bind(&AsyncUDPClient::DoSend, this, msg));
}

void UDPClient::AsyncUDPClient::DoSend(const std::vector<char> msg)
{
  write_in_progress_ = !write_msgs_.empty();
  write_msgs_.push_back(msg);
  if (!write_in_progress_) {
    socket_.async_send_to(asio::buffer(write_msgs_.front()), endpoint_,
        boost::bind(&AsyncUDPClient::HandlerWrite, this,
          asio::placeholders::error,
          asio::placeholders::bytes_transferred));
  }
  msg_to_send_ = false;
}

void UDPClient::AsyncUDPClient::HandlerWrite(
    const boost::system::error_code& error,
    std::size_t bytes_transferred)
{
  if (!error) {
    write_msgs_.pop_front();
    if (!write_msgs_.empty()) {
      socket_.async_send_to(asio::buffer(write_msgs_.front()), endpoint_,
          boost::bind(&AsyncUDPClient::HandlerWrite, this,
            asio::placeholders::error,
            asio::placeholders::bytes_transferred));
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
    socket_.close();
  }
}

//-----------------------------------------------------------------------------
UDPClient::UDPClient(const std::string& host, int port)
: host_(host)
, port_(port)
, client_(io_service_, write_progress_cond_, write_progress_mut_)
, service_is_ready_(false)
, thread_is_running_(false)
{
}

UDPClient::~UDPClient()
{
  if (thread_is_running_) {
    if (service_is_ready_) {
      io_service_.stop();
      thread_.join();
    } else {
      thread_.timed_join(boost::posix_time::seconds(0));
    }
  }
}

void UDPClient::Send(const std::vector<char>& msg)
{
  if (!thread_is_running_ && !RunThread()) return;
  client_.Send(msg);
}

void UDPClient::BlockUntilQueueIsEmpty()
{
  // Block until IO thread reached a ready state
  while (thread_is_running_ && !service_is_ready_);

  boost::unique_lock<boost::mutex> lock(write_progress_mut_);
  while (client_.WriteInProgress() || client_.HaveMsgToSend()) {
    write_progress_cond_.wait(lock);
  }
}

bool UDPClient::RunThread()
{
  bool success = true;;
  thread_is_running_ = true;
  try {
    thread_ = boost::thread(boost::bind(&UDPClient::Run, this));
  } catch (std::exception& e) {
    std::cerr << "UDPClient::RunThread(): exception -> " << e.what() << "\n";
    success = false;
    thread_is_running_ = false;
  }

  return success;
}

void UDPClient::Run()
{
  std::stringstream port_string;
  port_string << port_;

  using asio::ip::udp;
  try {
    udp::resolver resolver(io_service_);
    udp::resolver::query query(host_, port_string.str());
    udp::endpoint endpoint = *resolver.resolve(query);
    client_.SetEndpoint(endpoint);
    asio::io_service::work work(io_service_);
    service_is_ready_ = true;
    io_service_.run();
  } catch (std::exception& e) {
    std::cerr << "UDPClient::Run(): exception -> " << e.what() << "\n";
  }
  io_service_.reset();
  service_is_ready_ = false;
  thread_is_running_ = false;
}

