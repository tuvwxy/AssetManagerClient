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
#ifndef _ASSET_MANAGER_CLIENT_HPP_
#define _ASSET_MANAGER_CLIENT_HPP_

/// @file asset_manager_client.hpp
/// @author Toshiro Yamada (tnyamada@gmail.com)
/// @date September, 2011
/// @brief Main public interface for Asset Manager Client

#include <string>
#include <vector>

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif

namespace am {

class TCPClient;
class UDPClient;

/// @brief Simple interface for interacting with Asset Manager server.
///
/// AssetManagerClient can control basic parameters of Asset Manager server and
/// manage a project. The API is simple and uses asynchronous sockets; i.e.
/// function calls do not block. (Except for the destructor. See @a
/// ~AssetManagerClient for more details.) Interntally, AssetManagerClient uses
/// Boost ASIO library for handling asynchronous TCP and UDP networking.
class AssetManagerClient {
 public:

  /// Options to change internal behavior of the class.
  enum Option {
    /// Use UDP instead of TCP (default) for sending core messages.
    CORE_USE_UDP                        = 1 << 0,
  };

  /// @brief Constructor of @a AssetManagerClient.
  ///
  /// Create an AssetManagerClient with a specified Open Sound Control base
  /// address and address of the host computer. @a base_address is associated
  /// with the Asset Manager project and should be unique. (No other Asset
  /// Manager project should be associated with the same @a base_address.)
  /// Optionally, destination TCP and UDP ports can be specified if Asset
  /// Manager is running on a non-default ports.
  ///
  /// @param[in] base_address Base Open Sound Control address. Custom Open Sound
  ///                         Control messages sent through SendCustomTCP and
  ///                         SendCustomUDP will have the address prefixed with
  ///                         this base address.
  /// @param[in] host         Address of the computer running Asset Manager.
  /// @param[in] tcp_port     (Optional) Destination TCP port.
  /// @param[in] udp_port     (Optional) Destination UDP port.
  AssetManagerClient(const std::string& base_address,
      const std::string& host,
      long tcp_port=TCP_PORT,
      long udp_port=UDP_PORT);

  /// @brief Destructor of @a AssetManagerClient.
  ~AssetManagerClient();

  /// @brief Set option to change internal behavior.
  ///
  /// Multiple options can be set by using the bitwise OR (|). Once the option
  /// is set, it cannot be removed.
  ///
  /// @see @c enum @a Option
  void SetOption(Option option);

  /// @brief Mute or unmute Asset Manager.
  ///
  /// If muted, all projects will be effectively muted.
  ///
  /// @param[in] mute         @c true to mute, @c false to unmute.
  ///
  /// @see @a SetMute
  void SetSystemMute(bool mute);

  /// @brief Set overall volume of Asset Manager.
  ///
  /// This affects all projects output levels and is not recommended to change
  /// a specific project volume: use @a SetVolume instead. Value must be
  /// between 0 and 1. and out-of-range values are ignored.
  ///
  /// @param[in] volume       Value between 0. and 1., which 0. is silence and
  ///                         1. is full volume (= 0. dB attenuation).
  ///
  /// @see @a SetVolume
  void SetSystemVolume(float volume);

  /// @brief Load project in Asset Manager.
  ///
  /// This function should be called to start a proejct. Non-system calls
  /// will not take effect if a project is not loaded. If a project is large,
  /// loading maye take time (in Asset Manager) and non-system call may not
  /// take effect immediately. Moreover, if a project is set to "auto load" in
  /// Asset Manager, a call to @a Load is unecessary (and is ignored) because
  /// the project should already be loaded.
  ///
  /// @see @a Unload
  void Load();

  /// @brief Unload project in Asset Manager.
  ///
  /// This function should be called to exit a project. Any subsequent
  /// non-system calls are ignored upon project unload. As in case of @a Load,
  /// if a project is set to "auto load" in Asset Manager, a call to @a Unload
  /// will not remove the project and hence is ignored. In this case, for
  /// example, a custom message should be sent to reset the project to an
  /// initial state and @a SetMute should be called to reduce processing load.
  void Unload();

  /// @brief Mute or unmute project.
  ///
  /// Muting the project will stop the audio signal processing and effectively
  /// mute the proejct. This also reduces the processing load in Asset Manager
  /// and may be used to balance multiple projects simultaneously. This may be
  /// favorable if multiple large projects are hosted on Asset Manager and load
  /// time of the projects interferes with quickly switching between thep
  /// pojects.
  ///
  /// @param[in] mute         @c true to mute, @c false to unmute.
  ///
  /// @see @a SetSystemMute
  void SetMute(bool mute);

  /// @brief Set project volume.
  ///
  /// Value should be between 0 and 1.
  ///
  /// @param[in] volume       Value between 0. and 1., which 0. is silence and
  ///                         1. is full volume (= 0. dB attenuation).
  ///
  /// @see @a SetSystemVolume
  void SetVolume(float volume);

  /// @brief Send custom TCP message to the project.
  ///
  /// This function may be used to send custom Open Sound Control messages to
  /// the project. OSC address is prefixed with base_address set when
  /// constructing @a AssetManagerClient. TCP guarantees delivery (if
  /// a connection is valid) and therefore this function should be preferred
  /// over @a SendCustomUDP when such guarantee is desirable. For
  /// time-sensitive and frequently updated messages (such as sound object
  /// locations), use @a SendCustomUDP.
  ///
  /// Note that when mixing calls to @a SendCustomTCP and @a SendCustomUDP,
  /// messages order is guaranteed and should not be expected. For example, if
  /// @a SendCustomUDP is called right after @a SendCustomTCP, the UDP message
  /// is likely to arrive before the TCP message (although that also depends).
  /// Therefore, if order of arrival is important, use only @a SendCustomTCP.
  ///
  /// The arguments are similar to printf-like functions. @a format specifies
  /// the number of arguments and their types, and the appropriate number of
  /// arguments are specified right afterwrds.
  ///
  /// Following types allowed:
  ///   - i: 32-bit integer number
  ///   - h: 64-bit integer number
  ///   - f: 32-bit floating point number
  ///   - d: 64-bit floating point number
  ///   - s: string (@c char[], NOT @c std::string)
  ///   - c: ASCII character
  ///   - T: True (No argument required)
  ///   - F: False (No argument required)
  ///   - N: Nil (No argument required)
  ///   - I: Infinitum (No argument required)
  ///
  /// @param[in] url          OSC's URL address of the message.
  /// @param[in] format       A character string representing OSC argument types.
  /// @param[in] ...          Arguments that match with the type specified in
  ///                         @a format.
  ///
  /// @code
  ///   AssetManagerClient am("/test", "127.0.0.1");
  ///   am.SendCustomTCP("/cue", "i", 12); // send an integer value
  ///   am.SendCustomTCP("/bang", "I"); // send an infinitum
  ///
  ///   float x, y, z;
  ///   ...
  ///   am.SendCustomTCP("/object/pos", "fff", x, y, z); // send three floats
  /// @endcode
  ///
  /// @see @a SendCustomUDP
  void SendCustomTCP(const std::string& url, const char* format, ...);

  /// @brief Send custom UDP message to the project.
  ///
  /// UDP variant of @a SendCustomTCP. See @a SendCustomTCP for more details.
  ///
  /// @param[in] url          OSC's URL address of the message.
  /// @param[in] format       A character string representing OSC argument types.
  /// @param[in] ...          Arguments that match with the type specified in
  ///                         @a format.
  ///
  /// @see @a SendCustomTCP
  void SendCustomUDP(const std::string& url, const char* format, ...);

  /// @brief Mark the start of a new bundle.
  ///
  /// Bundle groups UDP messages into a single packet so the number of packets
  /// sent can be minimized. The maximum packet size is limited to 1500 bytes
  /// and a bundle cannot be larger than this value. If maximum packet size is
  /// reached, the full bundle is sent and a new bundle is created. If @a
  /// StartBundle is called consecutively without a call to @a EndBundle, end of
  /// bundle is implied and the bundle is sent over the network before a new
  /// bundle is created.
  ///
  /// Note that bundles have no effect to TCP (stream socket) type and
  /// therefore calling @a StartBundle and @a EndBundle around TCP message
  /// calls have no effect.
  ///
  /// @see @a EndBundle
  void StartBundle();

  /// @brief Mark the end of the bundle and send it over the network.
  ///
  /// Consecutive call to @a EndBundle without a call to @a Start Bundle has
  /// no effect. See @a StartBundle for more information.
  ///
  /// @see @a StartBundle
  void EndBundle();

  /// @brief Block and process all messages in the TCP and UDP queues
  ///
  /// The function blocks and process all the messages that are in the TCP and
  /// UDP queues. This may be used to ensure that messages are sent before
  /// exiting the program.
  void BlockUntilQueuesAreEmpty();

 private:
  DISALLOW_COPY_AND_ASSIGN(AssetManagerClient);

  enum {
    TCP_PORT = 15002,
    UDP_PORT = 15003,
    MAX_MESSAGE_SIZE = 1500
  };

  void SendCoreMessage(const std::vector<char>& msg);
  void NewBundle(std::vector<char>& bundle);
  void AppendBundle(std::vector<char>& bundle, const std::vector<char>& message);

  std::string base_address_;
  int options_;
  TCPClient* tcp_client_;
  UDPClient* udp_client_;
  bool start_bundle_;
  std::vector<char> tcp_bundle_;
  std::vector<char> udp_bundle_;
};

} // namespace am
#endif // _ASSET_MANAGER_CLIENT_HPP_

