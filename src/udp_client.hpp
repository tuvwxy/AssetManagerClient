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
#ifndef _UDP_CLIENT_HPP_
#define _UDP_CLIENT_HPP_

#include <string>
#include <deque>
#include <vector>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include "disallow_copy_and_assign.hpp"

namespace am {

/// Wrapper for using boost::asio::udp
class UDPClient {
 public:
  UDPClient(const std::string& host, int port);
  ~UDPClient();

  /// Send a message.
  void Send(const std::vector<char>& msg);

  /// Can be used before exiting the program to make sure all messages are sent
  /// or at least processed before abruptly exiting the program. This is
  /// necessary because the client runs on a separate thread and quiting after
  /// a call to Send does not gaurantee delivery.
  void BlockUntilQueueIsEmpty();

 private:
  DISALLOW_COPY_AND_ASSIGN(UDPClient);

  /// Asynchronous UDP client class. AsyncUDPClient actually process the
  /// network IO events.
  class AsyncUDPClient {
   public:
    AsyncUDPClient(boost::asio::io_service& io_service,
        boost::condition_variable& cond, boost::mutex& mut);
    ~AsyncUDPClient();

    void SetEndpoint(boost::asio::ip::udp::endpoint& endpoint);
    void Send(const std::vector<char>& msg);
    bool WriteInProgress() const { return write_in_progress_; }

   private:
    void DoSend(const std::vector<char> msg);
    void HandlerWrite(const boost::system::error_code& error,
         std::size_t bytes_transferred);

    bool write_in_progress_;
    boost::asio::io_service& io_service_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint endpoint_;
    std::deque< std::vector<char> > write_msgs_;
    boost::condition_variable& write_progress_cond_;
    boost::mutex& write_progress_mut_;
  };


  /// Thread is lazily created when Send funciton is called.
  bool RunThread();

  /// Thread to run AsyncUDPClient. This function sets the endpoint and calls
  /// io_services's run to start processing the AsyncUDPClient service.
  void Run();

  std::string host_;
  int port_;
  boost::asio::io_service io_service_;
  AsyncUDPClient client_;
  bool service_is_ready_;
  bool thread_is_running_;
  boost::thread thread_;
  boost::condition_variable write_progress_cond_;
  boost::mutex write_progress_mut_;
};

} // namespace am

#endif // _UDP_CLIENT_HPP_

