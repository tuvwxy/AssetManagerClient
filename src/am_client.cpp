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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <string>
#include <iostream>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include "asset_manager_client.hpp"

// global variables
std::string g_base_address = "";
bool g_use_udp = false;
std::string g_host_address = "127.0.0.1";
bool g_continue = true;

void ProcessArguments(int argc, const char* argv[]);
void ProcessInput(am::AssetManagerClient& am, const std::string& input);
void Signal(int what);

template <class T>
bool from_string(T& t,
    const std::string& s,
    std::ios_base& (*f)(std::ios_base&));

template <class T>
bool from_string(T& t,
    const std::string& s,
    std::ios_base& (*f)(std::ios_base&))
{
  std::istringstream iss(s);
  return !(iss >> f >> t).fail();
}

int main(int argc, const char* argv[])
{
  ProcessArguments(argc, argv);
  std::cout << "Using base address: " << g_base_address << "\n";
  std::cout << "Type \"help\" to list commands\n";

  am::AssetManagerClient am(g_base_address, g_host_address);
  if (g_use_udp) am.SetOption(am::AssetManagerClient::CORE_USE_UDP);
  am.SetOption(am::AssetManagerClient::SEND_UNLOAD_IN_DESTRUCTOR);

  signal(SIGINT, Signal);
#if !defined(_WIN32)
  signal(SIGQUIT, Signal);
#endif

  while (g_continue) {
    std::cout << "-> ";
    std::string input;
    std::getline(std::cin, input);
    if (std::cin.eof() || std::cin.fail()) break;
    ProcessInput(am, input);
  }

  return 0;
}

void ProcessArguments(int argc, const char* argv[])
{
  int i = 1;
  while (i < argc) {
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      goto print_usage;
    } else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--protocol")) {
      if (i++ < argc) {
        if (!strcmp(argv[i], "tcp") || !strcmp(argv[i], "TCP")) {
          g_use_udp = false;
        } else if (!strcmp(argv[i], "udp") || !strcmp(argv[i], "UDP")) {
          g_use_udp = true;
        } else {
          printf("Unrecognized protocol: %s\n", argv[i]);
          goto print_usage;
        }
      } else {
        printf("Not enough argument.\n");
        goto print_usage;
      }
    } else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--ip")) {
      if (i++ < argc) {
        g_host_address = argv[i];
      } else {
        printf("Not enough argument.\n");
        goto print_usage;
      }
    } else if (i == (argc - 1)) {
      g_base_address = argv[i];
    }
    i++;
  }

  if (g_base_address.empty()) {
    goto print_usage;
  }

  return;

print_usage:
  printf("Usage: am_client [ -i ip ] [ -p port ] base_address", argv[0]);
  printf("\nOptions:");
  printf("\n  -h,--help                "
      "Display this information.");
  printf("\n  -p,--protocol <tcp|udp>  "
      "Use TCP or UDP protocol. Default = tcp.");
  printf("\n  -i,--ip <ip>             "
      "Set Asset Manager's host address. Default = 127.0.0.1");
  printf("\n");
  exit(0);
}

void ProcessInput(am::AssetManagerClient& am, const std::string& input)
{
  boost::char_separator<char> sep(" ");
  boost::tokenizer< boost::char_separator<char> > tok(input, sep);
  boost::tokenizer< boost::char_separator<char> >::iterator it = tok.begin();
  for (; it != tok.end(); ++it) {
    if (!it->compare("load")) {
      am.Load();
    } else if (!it->compare("unload")) {
      am.Unload();
    } else if (!it->compare("system_mute")) {
      if (++it == tok.end()) {
        goto missing_argument;
      } else if (!it->compare("true") || !it->compare("1")) {
        am.SetSystemMute(true);
      } else if (!it->compare("false") || !it->compare("0")) {
        am.SetSystemMute(false);
      } else {
        goto bad_argument;
      }
    } else if (!it->compare("system_volume")) {
      float volume;
      if (++it == tok.end()) {
        goto missing_argument;
      } else if (from_string<float>(volume, *it, std::dec)) {
        am.SetSystemVolume(volume);
      } else {
        goto bad_argument;
      }
    } else if (!it->compare("mute")) {
      if (++it == tok.end()) {
        goto missing_argument;
      } else if (!it->compare("true") || !it->compare("1")) {
        am.SetMute(true);
      } else if (!it->compare("false") || !it->compare("0")) {
        am.SetMute(false);
      } else {
        goto bad_argument;
      }
    } else if (!it->compare("volume")) {
      float volume;
      if (++it == tok.end()) {
        goto missing_argument;
      } else if (from_string<float>(volume, *it, std::dec)) {
        am.SetVolume(volume);
      } else {
        goto bad_argument;
      }
    } else if (!it->compare("help")) {
      std::cout << "load" << std::endl;
      std::cout << "unload" << std::endl;
      std::cout << "system_mute    true|1|false|0" << std::endl;
      std::cout << "system_volume  0-1" << std::endl;
      std::cout << "mute           true|1|false|0" << std::endl;
      std::cout << "volume         0-1" << std::endl;
    } else {
      std::cerr << "Unknown command: " << *it << "\n";
      break;
    }
  }

  return;

bad_argument:
  std::cerr << "Bad argument: " << *it << "\n";
  return;
missing_argument:
  std::cerr << "Missing argument \n";
}

void Signal(int what)
{
  g_continue = false;
  fclose(stdin);
}

