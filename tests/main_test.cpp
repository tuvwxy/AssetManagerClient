#include "asset_manager_client.hpp"

#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#include <Windows.h>
void usleep(DWORD microseconds)
{
  Sleep(microseconds/1000);
}
#else
#include <unistd.h>
#endif
#include <iostream>

int main(int argc, const char* argv[])
{
  if (argc != 2) {
    std::cerr << argv[0] << " ip\n";
    exit(1);
  }

  std::string base_address = "/sleep_walk_expeirment";
  std::string host = argv[1];

  // Allocate AssetManagerClient on stack or heap
  am::AssetManagerClient am(base_address, host);

  // Set options:
  // Setting CORE_USE_UDP will make all "core" messages, such as all the
  // "Set" functions, Load, and Unload, to be sent over UDP instead of the
  // default TCP. This should only be set if there's a problem with TCP.
  //am.SetOption(am::AssetManagerClient::CORE_USE_UDP);

  // Unmute Asset Manager and set volume to nominal
  am.SetSystemMute(false);
  am.SetSystemVolume(1.0f);

  // Load project, make sure it's unmuted and set volume to nominal.
  am.Load();
  am.SetMute(false);
  am.SetVolume(1.0f);

  // Send a custom message over TCP
  am.SendCustomTCP("/object/cue", "i", 1);

  // Sleep a little so the TCP message gets sent before the UDP messages.
  usleep(1000);

  // Start a new bundle so UDP packets are bundled as many as possible.
  am.StartBundle();

  // Send custom messages over UDP. Note that the order of packet arrival is NOT
  // gauranteed when using UDP or mixing messages with TCP! For example, UDP
  // messages below may arrive sooner than all the previous TCP messages.
  for (int i = 0; i < 10; i++) {
    am.SendCustomUDP("/object/pos", "fff", (float)i, i * 1.23f, i * 3.0f);
  }

  // Send the bundle over the network.
  am.EndBundle();

  // Sleep a little so the UDP messages gets sent before unload.
  usleep(1000);

  // Unload project to release resources on Asset Manager
  am.Unload();

  // Block until all messages are processed to make sure that they are sent
  // before exiting the program.
  am.BlockUntilQueuesAreEmpty();

  return 0;
}

