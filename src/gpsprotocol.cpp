/*

   Copyright [2010] [Josko Nikolic]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

 */

#include "aura.h"
#include "util.h"
#include "gpsprotocol.h"

//
// CGPSProtocol
//

CGPSProtocol::CGPSProtocol() = default;

CGPSProtocol::~CGPSProtocol() = default;

///////////////////////
// RECEIVE FUNCTIONS //
///////////////////////

////////////////////
// SEND FUNCTIONS //
////////////////////

std::vector<uint8_t> CGPSProtocol::SEND_GPSC_INIT(const uint32_t version) const
{
  std::vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_INIT, 8, 0};
  AppendByteArray(packet, version, false);
  return packet;
}

std::vector<uint8_t> CGPSProtocol::SEND_GPSC_RECONNECT(const uint8_t PID, const uint32_t reconnectKey, const uint32_t lastPacket) const
{
  std::vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_RECONNECT, 13, 0, PID};
  AppendByteArray(packet, reconnectKey, false);
  AppendByteArray(packet, lastPacket, false);
  return packet;
}

std::vector<uint8_t> CGPSProtocol::SEND_GPSC_ACK(const uint32_t lastPacket) const
{
  std::vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_ACK, 8, 0};
  AppendByteArray(packet, lastPacket, false);
  return packet;
}

std::vector<uint8_t> CGPSProtocol::SEND_GPSS_INIT(const uint16_t reconnectPort, const uint8_t PID, const uint32_t reconnectKey, const uint8_t numEmptyActions) const
{
  std::vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_INIT, 12, 0};
  AppendByteArray(packet, reconnectPort, false);
  packet.push_back(PID);
  AppendByteArray(packet, reconnectKey, false);
  packet.push_back(numEmptyActions);
  return packet;
}

std::vector<uint8_t> CGPSProtocol::SEND_GPSS_RECONNECT(const uint32_t lastPacket) const
{
  std::vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_RECONNECT, 8, 0};
  AppendByteArray(packet, lastPacket, false);
  return packet;
}

std::vector<uint8_t> CGPSProtocol::SEND_GPSS_ACK(const uint32_t lastPacket) const
{
  std::vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_ACK, 8, 0};
  AppendByteArray(packet, lastPacket, false);
  return packet;
}

std::vector<uint8_t> CGPSProtocol::SEND_GPSS_REJECT(const uint32_t reason) const
{
  std::vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_REJECT, 8, 0};
  AppendByteArray(packet, reason, false);
  return packet;
}

std::vector<uint8_t> CGPSProtocol::SEND_GPSS_SUPPORT_EXTENDED(const uint32_t seconds) const
{
  std::vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_SUPPORT_EXTENDED, 8, 0};
  AppendByteArray(packet, seconds, false);
  return packet;
}

std::vector<uint8_t> CGPSProtocol::SEND_GPSS_DIMENSIONS() const
{
  std::vector<uint8_t> packet = {192, 7};
  return packet;
}
