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

#ifndef AURA_GPSPROTOCOL_H_
#define AURA_GPSPROTOCOL_H_

//
// CGameProtocol
//

#define GPS_HEADER_CONSTANT 248

#define REJECTGPS_INVALID 1
#define REJECTGPS_NOTFOUND 2

class CGPSProtocol
{
public:
  enum Protocol
  {
    GPS_INIT              = 1,
    GPS_RECONNECT         = 2,
    GPS_ACK               = 3,
    GPS_REJECT            = 4,
    GPS_UDPSCAN           = 10,
    GPS_UDPBRIDGE         = 11,
    GPS_SUPPORT_EXTENDED	= 50,
  };

  CGPSProtocol();
  ~CGPSProtocol();

  // receive functions

  // send functions

  std::vector<uint8_t> SEND_GPSC_INIT(const uint32_t version) const;
  std::vector<uint8_t> SEND_GPSC_RECONNECT(uint8_t PID, const uint32_t reconnectKey, const uint32_t lastPacket) const;
  std::vector<uint8_t> SEND_GPSC_ACK(const uint32_t lastPacket) const;
  std::vector<uint8_t> SEND_GPSS_INIT(const uint16_t reconnectPort, const uint8_t PID, const uint32_t reconnectKey, const uint8_t numEmptyActions) const;
  std::vector<uint8_t> SEND_GPSS_RECONNECT(const uint32_t lastPacket) const;
  std::vector<uint8_t> SEND_GPSS_ACK(const uint32_t lastPacket) const;
  std::vector<uint8_t> SEND_GPSS_REJECT(const uint32_t reason) const;
  std::vector<uint8_t> SEND_GPSS_SUPPORT_EXTENDED(const uint32_t seconds) const;
  std::vector<uint8_t> SEND_GPSS_DIMENSIONS() const;
};

#endif // AURA_GPSPROTOCOL_H_
