#pragma once

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <algorithm>
#include <array>
#include <string>

using namespace std;

constexpr size_t DEFAULT_NORMAL_MTU = 1500;
constexpr size_t DEFAULT_MIN_MTU = 576;
constexpr size_t DEFAULT_MAX_PACKET_SIZE = 65535;
constexpr size_t DEFAULT_HEADER_SIZE = sizeof ( AdvancedPacketHeader );
constexpr int DEFAULT_TIMEOUT = 5000;
constexpr size_t DEFAULT_MAX_RETRY = 3;

struct AdvancedPacketHeader
{
  uint8_t magic_code;
  uint8_t version;
  uint8_t type;
  uint8_t reserved;
  uint16_t flags;
  uint16_t sequence;
  uint32_t timestamp;
  uint32_t total_size;
  uint32_t fragment_size;
  uint32_t fragment_offset;
  uint32_t checksum;
};

enum class AdvancedPacketType : uint8_t
{
  DATA = 0x01,
  ACK = 0x06,
  NACK = 0x15,
  PING = 0x30
};

enum class AdvancedPacketFlags : uint16_t
{
  NONE = 0x0000,
  FRAGMENTED = 0x0002,
  REQUIRES_ACK = 0x0003
};

struct AdvancedSocketConfig
{
  uint16_t port = 8080;
  string host = "0.0.0.0";
  size_t recv_buffer = 65536;
  size_t send_buffer = 65536;
  bool nodelay = true;
  bool reuse_addr = true;
  int timeout = DEFAULT_TIMEOUT;
  size_t max_retry_count = DEFAULT_MAX_RETRY;
};
