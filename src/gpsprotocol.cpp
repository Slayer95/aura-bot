/*

  Copyright [2024] [Leonardo Julca]

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

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
#include "game.h"
#include "gameprotocol.h"
#include "util.h"
#include "gpsprotocol.h"

#include <queue>

using namespace std;

//
// CGPSProtocol
//

CGPSProtocol::CGPSProtocol(CAura* nAura)
 : m_Aura(nAura)
{
}

CGPSProtocol::~CGPSProtocol() = default;

///////////////////////
// RECEIVE FUNCTIONS //
///////////////////////

////////////////////
// SEND FUNCTIONS //
////////////////////

vector<uint8_t> CGPSProtocol::SEND_GPSC_INIT(const uint32_t version) const
{
  vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_INIT, 8, 0};
  AppendByteArray(packet, version, false);
  return packet;
}

vector<uint8_t> CGPSProtocol::SEND_GPSC_RECONNECT(const uint8_t UID, const uint32_t reconnectKey, const uint32_t lastPacket) const
{
  vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_RECONNECT, 13, 0, UID};
  AppendByteArray(packet, reconnectKey, false);
  AppendByteArray(packet, lastPacket, false);
  return packet;
}

vector<uint8_t> CGPSProtocol::SEND_GPSC_ACK(const uint32_t lastPacket) const
{
  vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_ACK, 8, 0};
  AppendByteArray(packet, lastPacket, false);
  return packet;
}

vector<uint8_t> CGPSProtocol::SEND_GPSS_INIT(const uint16_t reconnectPort, const uint8_t UID, const uint32_t reconnectKey, const uint8_t numEmptyActions) const
{
  vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_INIT, 12, 0};
  AppendByteArray(packet, reconnectPort, false);
  packet.push_back(UID);
  AppendByteArray(packet, reconnectKey, false);
  packet.push_back(numEmptyActions);
  return packet;
}

vector<uint8_t> CGPSProtocol::SEND_GPSS_RECONNECT(const uint32_t lastPacket) const
{
  vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_RECONNECT, 8, 0};
  AppendByteArray(packet, lastPacket, false);
  return packet;
}

vector<uint8_t> CGPSProtocol::SEND_GPSS_ACK(const uint32_t lastPacket) const
{
  vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_ACK, 8, 0};
  AppendByteArray(packet, lastPacket, false);
  return packet;
}

vector<uint8_t> CGPSProtocol::SEND_GPSS_REJECT(const uint32_t reason) const
{
  vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_REJECT, 8, 0};
  AppendByteArray(packet, reason, false);
  return packet;
}

vector<uint8_t> CGPSProtocol::SEND_GPSS_SUPPORT_EXTENDED(const int64_t ticks) const
{
  vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPS_SUPPORT_EXTENDED, 8, 0};
  const uint32_t seconds = ticks / 1000;
  AppendByteArray(packet, seconds, false);
  return packet;
}

array<uint8_t, 2> CGPSProtocol::SEND_GPSS_DIMENSIONS() const
{
  return {192, 7};
}
