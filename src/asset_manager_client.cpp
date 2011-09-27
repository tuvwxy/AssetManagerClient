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
#include "asset_manager_client.hpp"

#include <vector>
#include <iterator>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include "tnyosc.hpp"
#include "tcp_client.hpp"
#include "udp_client.hpp"

#include "oscpack.h"

namespace asio = boost::asio;
using namespace am;

//-----------------------------------------------------------------------------
AssetManagerClient::AssetManagerClient(const std::string& base_address,
    const std::string& host,
    long tcp_port,
    long udp_port)
: base_address_(base_address)
, options_(0)
, start_bundle_(false)
{
  tcp_client_ = new TCPClient(host, tcp_port);
  udp_client_ = new UDPClient(host, udp_port);
}

AssetManagerClient::~AssetManagerClient()
{
  if (options_ & SEND_UNLOAD_IN_DESTRUCTOR) {
    Unload();
  }
  if (!(options_ & DONOT_CLEAR_QUEUE_IN_DESTRUCTOR)) {
    tcp_client_->BlockUntilQueueIsEmpty();
    udp_client_->BlockUntilQueueIsEmpty();
  }
  delete tcp_client_;
  delete udp_client_;
}

void AssetManagerClient::SetOption(Option option)
{
  options_ ^= option;
}

void AssetManagerClient::SetSystemMute(bool mute)
{
  tnyosc::Message msg("/AM/Mute");
  msg.append(mute ? 1 : 0);
  SendCoreMessage(msg.byte_array());
}

void AssetManagerClient::SetSystemVolume(float volume)
{
  if (0.0f <= volume && volume <= 1.0f) {
    tnyosc::Message msg("/AM/Volume");
    msg.append(20*log10(volume));
    SendCoreMessage(msg.byte_array());
  }
}

void AssetManagerClient::Load()
{
  tnyosc::Message msg("/AM/Load");
  msg.append(base_address_);
  SendCoreMessage(msg.byte_array());
}

void AssetManagerClient::Unload()
{
  tnyosc::Message msg("/AM/Unload");
  msg.append(base_address_);
  SendCoreMessage(msg.byte_array());
}


void AssetManagerClient::SetMute(bool mute)
{
  tnyosc::Message msg("/AM/Project/Mute");
  msg.append(base_address_);
  msg.append(mute ? 1 : 0);
  SendCoreMessage(msg.byte_array());
}

void AssetManagerClient::SetVolume(float volume)
{
  if (0.0f <= volume && volume <= 1.0f) {
    tnyosc::Message msg("/AM/Project/Volume");
    msg.append(base_address_);
    msg.append(20*log10(volume));
    SendCoreMessage(msg.byte_array());
  }
}

void AssetManagerClient::SendCoreMessage(const std::vector<char>& msg)
{
  if (options_ & CORE_USE_UDP) {
    if (start_bundle_)
      AppendBundle(udp_bundle_, msg);
    else
      udp_client_->Send(msg);
  } else {
    tcp_client_->Send(msg);
  }
}

void AssetManagerClient::SendCustomTCP(const std::string& url,
    const char* format, ...)
{
  std::vector<char> buf(MAX_MESSAGE_SIZE);
  std::string address(base_address_);
  address.append(url);
  va_list ap;
  va_start(ap, format);
  int32_t size = voscpack((uint8_t*)&buf[0], address.c_str(), format, ap);
  va_end(ap);
  if (size > 0) {
    buf.resize(size);
    tcp_client_->Send(buf);
  }
}

void AssetManagerClient::SendCustomUDP(const std::string& url,
    const char* format, ...)
{
  std::vector<char> buf(MAX_MESSAGE_SIZE);
  std::string address(base_address_);
  address.append(url);
  va_list ap;
  va_start(ap, format);
  int32_t size = voscpack((uint8_t*)&buf[0], address.c_str(), format, ap);
  va_end(ap);
  if (size > 0) {
    buf.resize(size);
    if (start_bundle_)
      AppendBundle(udp_bundle_, buf);
    else
      udp_client_->Send(buf);
  }
}

void AssetManagerClient::StartBundle()
{
  if (start_bundle_) EndBundle();
  NewBundle(udp_bundle_);
  start_bundle_ = true;
}

void AssetManagerClient::EndBundle()
{
  if (udp_bundle_.size() > 16) {
    udp_client_->Send(udp_bundle_);
    udp_bundle_.clear();
  }
  start_bundle_ = false;
}

void AssetManagerClient::NewBundle(std::vector<char>& bundle)
{
  static std::string id = "#bundle";
  bundle.resize(16);
  std::copy(id.begin(), id.end(), bundle.begin());
  bundle[15] = 1;
}

void AssetManagerClient::AppendBundle(std::vector<char>& bundle,
    const std::vector<char>& message)
{
  if (bundle.size() + message.size() > MAX_MESSAGE_SIZE) {
    udp_client_->Send(bundle);
    NewBundle(bundle);
  }
  if (message.size()) {
    int32_t a = htonl(message.size());
    bundle.insert(bundle.end(), (char*)&a, (char*)&a+4);
    bundle.insert(bundle.end(), message.begin(), message.end());
  }
}

