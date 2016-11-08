#ifndef BASE_H
#define BASE_H

#include <stdio.h>
#include<stdlib.h>
#include <signal.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <memory>
#include <list>
#include <vector>
#include <map>
#include <array>
#include <cstdint>
#include <stdint.h>
#include <random>

// third party library

#include <enet/enet.h>
#include <spdlog/spdlog.h>
#include <pool/pool.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#define ASIO_STANDALONE

#ifdef _MSC_VER
#define ASIO_MSVC _MSC_VER
#define _WEBSOCKETPP_CPP11_STL_
#define _WEBSOCKETPP_CPP11_THREAD_
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_MEMORY_
#endif

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>



#endif