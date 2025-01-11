/*
  MbedUdp.h - UDP implementation using mbed Sockets
  Copyright (c) 2021 Arduino SA.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Arduino.h"
#include "SocketWrapper.h"
#include "api/Udp.h"
#include "sys/socket.h"
#include "zephyr/net/net_ip.h"
#include "zephyr/net/net_if.h"

#include <list>
#include <deque>
#include <vector>
#include <memory>

class ZephyrUDP : public arduino::UDP {
private:
  int _socket;

public:
  ZephyrUDP() : _socket(-1) {}  // Constructor
  ~ZephyrUDP() {
    stop();
  }

  // initialize, start listening on specified port. Returns 1 if successful, 0 if there are no sockets available to use
  virtual uint8_t begin(uint16_t port) {

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    zsock_ioctl(_socket, ZFD_IOCTL_FIONBIO);

    if (::bind(_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(_socket);
        _socket = -1;
        return false;
    }

    return true;
  }

  // initialize, start listening on specified multicast IP address and port. Returns 1 if successful, 0 if there are no sockets available to use
  virtual uint8_t beginMulticast(IPAddress ip, uint16_t port) {
    bool ret = begin(port);
    if (ret == false) {
      return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;

    net_if_ipv4_maddr_join(net_if_get_by_index(1), net_if_ipv4_maddr_add(net_if_get_by_index(1), (struct in_addr*)&addr));
    return true;
  }

  // Finish with the UDP socket
  virtual void stop() {
    if (_socket != -1) {
      ::close(_socket);
      _socket = -1;
    }
  }

  // Sending UDP packets

  // Start building up a packet to send to the remote host specific in ip and port
  // Returns 1 if successful, 0 if there was a problem with the supplied IP address or port
  virtual int beginPacket(IPAddress ip, uint16_t port) {
    _send_to_ip = ip;
    _send_to_port = port;

    /* Make sure that the transmit data buffer is empty. */
    _tx_data.clear();
    return true;
  }

  // Start building up a packet to send to the remote host specific in host and port
  // Returns 1 if successful, 0 if there was a problem resolving the hostname or port
  virtual int beginPacket(const char* host, uint16_t port) {
    // Resolve address
    struct addrinfo hints;
	  struct addrinfo *res;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int resolve_attempts = 100;
    int ret;

    while (resolve_attempts--) {
      ret = getaddrinfo(host, String(port).c_str(), &hints, &res);

      if (ret == 0) {
        break;
      } else {
        k_sleep(K_MSEC(1));
      }
    }

    if (ret != 0) {
      return false;
    }

    return beginPacket(IPAddress(((sockaddr_in*)(res->ai_addr))->sin_addr.s_addr), port);
  }
  
  // Finish off this packet and send it
  // Returns 1 if the packet was sent successfully, 0 if there was an error
  virtual int endPacket() {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_send_to_port);
    addr.sin_addr.s_addr = _send_to_ip;
    return ::sendto(_socket, _tx_data.data(), _tx_data.size(), 0, (sockaddr*)&addr, sizeof(addr));
  }

  // Write a single byte into the packet
  virtual size_t write(uint8_t data) {
    _tx_data.push_back(data);
    return 1;
  }

  // Write size bytes from buffer into the packet
  virtual size_t write(uint8_t* buffer, size_t size) {
    std::copy(buffer, buffer + size, std::back_inserter(_tx_data));
    return size;
  }

  // Write size bytes from buffer into the packet
  virtual size_t write(const uint8_t* buffer, size_t size) {
    std::copy(buffer, buffer + size, std::back_inserter(_tx_data));
    return size;
  }

  using Print::write;

  int parsePacket()
  {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    uint8_t tmp_buf[512];

    int ret = ::recvfrom(_socket, tmp_buf, sizeof(tmp_buf), 0, (sockaddr*)&addr, &addrlen);
    if (ret > 0)
    {
      auto pkt = std::make_shared<UdpRxPacket>(
          IPAddress(addr.sin_addr.s_addr),
          ntohs(addr.sin_port), tmp_buf, ret);

      _rx_pkt_list.push_back(pkt);

      // drop the oldest packet if the list is full
      if(_rx_pkt_list.size() > _rx_pkt_list_size) {
          _rx_pkt_list.pop_front();
      }
    }

    if (_rx_pkt_list.size())
    {
      /* Discard UdpRxPacket object previously held by _rx_pkt
      * and replace it with the new one.
      */
      _rx_pkt = _rx_pkt_list.front();
      _rx_pkt_list.pop_front();
      return _rx_pkt->totalSize();
    }
    else
    {
      /* Otherwise ensure that _rx_pkt definitely
      * does not hold any UdpRxPacket object anymore.
      */
      _rx_pkt.reset();
      return 0;
    }
  }

  int available()
  {
    if (_rx_pkt)
      return _rx_pkt->available();
    else
      return 0;
  }

  int read()
  {
    if (_rx_pkt)
    return _rx_pkt->read();
    else
      return -1;
  }

  int read(unsigned char* buffer, size_t len)
  {
    if (_rx_pkt)
      return _rx_pkt->read(buffer, len);
    else
      return -1;
  }

  int read(char* buffer, size_t len)
  {
    if (_rx_pkt)
      return _rx_pkt->read(buffer, len);
    else
      return -1;
  }

  int peek()
  {
    if (_rx_pkt)
      return _rx_pkt->peek();
    else
      return -1;
  }

  void flush()
  {
    /* Delete UdpRxPacket object held by _rx_pkt. */
    if (_rx_pkt)
      _rx_pkt.reset();
  }

  virtual IPAddress remoteIP() {
    if (_rx_pkt)
      return _rx_pkt->remoteIP();
    else
      return IPAddress();
  }

  virtual uint16_t remotePort() {
    if (_rx_pkt)
      return _rx_pkt->remotePort();
    else
      return 0;
  }

private:

  /* UDP TRANSMISSION */
  IPAddress _send_to_ip;
  uint16_t _send_to_port;
  std::vector<uint8_t> _tx_data;
  int _rx_pkt_list_size = 10;
  /* UDP RECEPTION */
  class UdpRxPacket
  {
  private:
    IPAddress const _remote_ip;
    uint16_t const _remote_port;
    size_t const _rx_data_len;
    std::deque<uint8_t> _rx_data;

  public:
    UdpRxPacket(
      IPAddress const remote_ip,
      uint16_t const remote_port,
      uint8_t const * p_data,
      size_t const data_len)
      : _remote_ip(remote_ip)
      , _remote_port(remote_port)
      , _rx_data_len(data_len)
      , _rx_data(p_data, p_data + data_len)
    {
    }

    typedef std::shared_ptr<UdpRxPacket> SharedPtr;

    IPAddress remoteIP() const { return _remote_ip; }
    uint16_t remotePort() const { return _remote_port; }
    size_t totalSize() const { return _rx_data_len; }

    int available()
    {
      return _rx_data.size();
    }

    int read()
    {
      uint8_t const data = _rx_data.front();
      _rx_data.pop_front();
      return data;
    }

    int read(unsigned char* buffer, size_t len)
    {
      size_t bytes_read = 0;
      for (; bytes_read < len && !_rx_data.empty(); bytes_read++)
      {
        buffer[bytes_read] = _rx_data.front();
        _rx_data.pop_front();
      }
      return bytes_read;
    }

    int read(char* buffer, size_t len)
    {
      return read((unsigned char*)buffer, len);
    }

    int peek()
    {
      return _rx_data.front();
    }
  };
  std::list<UdpRxPacket::SharedPtr> _rx_pkt_list;
  UdpRxPacket::SharedPtr _rx_pkt;
};