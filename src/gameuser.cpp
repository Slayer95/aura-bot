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

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT

 */

#include <utility>

#include "config_bot.h"
#include "gameuser.h"
#include "aura.h"
#include "realm.h"
#include "map.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "vlanprotocol.h"
#include "game.h"
#include "socket.h"
#include "net.h"

using namespace std;

//
// CGameConnection
//

CGameConnection::CGameConnection(CGameProtocol* nProtocol, CAura* nAura, uint16_t nPort, CStreamIOSocket* nSocket)
  : m_Aura(nAura),
    m_Protocol(nProtocol),
    m_Port(nPort),
    m_Type(INCOMING_CONNECTION_TYPE_NONE),
    m_Socket(nSocket),
    m_IncomingJoinPlayer(nullptr),
    m_DeleteMe(false)
{
}

CGameConnection::~CGameConnection()
{
  delete m_Socket;
  m_Socket = nullptr;
  delete m_IncomingJoinPlayer;
}

void CGameConnection::SetTimeout(const int64_t delta)
{
  m_TimeoutTicks = GetTicks() + delta;
}

void CGameConnection::CloseConnection()
{
  m_Socket->Close();
}

uint8_t CGameConnection::Update(void* fd, void* send_fd, int64_t timeout)
{
  if (m_DeleteMe || !m_Socket || m_Socket->HasError()) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  const int64_t Ticks = GetTicks();

  if (m_TimeoutTicks.has_value() && m_TimeoutTicks.value() < Ticks) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  uint8_t result = PREPLAYER_CONNECTION_OK;
  bool Abort = false;
  if (m_Type == INCOMING_CONNECTION_TYPE_KICKED_PLAYER) {
    m_Socket->Discard(static_cast<fd_set*>(fd));
  } else if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {
    // extract as many packets as possible from the socket's receive buffer and process them
    string*              RecvBuffer         = m_Socket->GetBytes();
    std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
    uint32_t             LengthProcessed    = 0;

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while (Bytes.size() >= 4) {
      // bytes 2 and 3 contain the length of the packet
      const uint16_t             Length = ByteArrayToUInt16(Bytes, false, 2);
      if (Length < 4) {
        Abort = true;
        break;
      }
      if (Bytes.size() < Length) break;
      const std::vector<uint8_t> Data   = std::vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

      if (Bytes[0] == W3GS_HEADER_CONSTANT ||
        (Bytes[0] == GPS_HEADER_CONSTANT && m_Aura->m_Net->m_Config->m_ProxyReconnect > 0) ||
        Bytes[0] == VLAN_HEADER_CONSTANT
        ) {
        if (Length >= 8 && Bytes[0] == W3GS_HEADER_CONSTANT && Bytes[1] == CGameProtocol::W3GS_REQJOIN) {
          if (!m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror() || m_Aura->m_CurrentLobby->GetLobbyLoading() || m_Aura->m_CurrentLobby->GetExiting()) {
            // Game already started
            Abort = true;
            break;
          }
          if (GetIsVLAN()) {
            // VLAN uses separate TCP connections for game discovery than for joining games.
            Abort = true;
            break;
          }
          if (GetIsUDPTunnel()) {
            vector<uint8_t> packet = {GPS_HEADER_CONSTANT, CGPSProtocol::GPS_UDPFIN, 4, 0};
            m_Socket->PutBytes(packet);
          }

          if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
            Print("[AURA] Got REQJOIN " + ByteArrayToDecString(Bytes));
          }
          delete m_IncomingJoinPlayer;
          m_IncomingJoinPlayer = m_Protocol->RECEIVE_W3GS_REQJOIN(Data, m_Aura->m_CurrentLobby->m_Config->m_UnsafeNameHandler);
          if (!m_IncomingJoinPlayer) {
            // Invalid request.
          } else if (m_Aura->m_CurrentLobby->GetHostCounter() != (m_IncomingJoinPlayer->GetHostCounter() & 0x00FFFFFF)) {
            // Trying to join the wrong game.
          } else if (m_Aura->m_CurrentLobby->EventRequestJoin(this, m_IncomingJoinPlayer)) {
            result = PREPLAYER_CONNECTION_PROMOTED;
            m_Socket = nullptr;
          } else {
            // Join failed.
          }
          Abort = true;
          if (result == PREPLAYER_CONNECTION_PROMOTED) {
            m_Type = INCOMING_CONNECTION_TYPE_PROMOTED_PLAYER;
          }
        } else if (GetIsUDPTunnel() && Bytes[0] == W3GS_HEADER_CONSTANT && CGameProtocol::W3GS_SEARCHGAME <= Bytes[1] && Bytes[1] <= CGameProtocol::W3GS_DECREATEGAME) {
          if (Length <= 1024) {
            struct UDPPkt pkt;
            pkt.socket = m_Socket;
            pkt.sender = &(m_Socket->m_RemoteHost);
            memcpy(pkt.buf, Bytes.data(), Length);
            pkt.length = Length;
            m_Aura->m_Net->HandleUDP(&pkt);
          } 
        } else if (Length == 13 && Bytes[0] == GPS_HEADER_CONSTANT && Bytes[1] == CGPSProtocol::GPS_RECONNECT && m_Type == INCOMING_CONNECTION_TYPE_NONE) {
          const uint32_t ReconnectKey = ByteArrayToUInt32(Bytes, false, 5);
          const uint32_t LastPacket   = ByteArrayToUInt32(Bytes, false, 9);

          // look for a matching player in a running game

          CGameUser* Match = nullptr;

          for (auto& game : m_Aura->m_Games) {
            if (game->GetGameLoaded() && game->GetIsProxyReconnectable()) {
              CGameUser* Player = game->GetUserFromUID(Bytes[4]);
              if (Player && Player->GetGProxyAny() && Player->GetGProxyReconnectKey() == ReconnectKey) {
                Match = Player;
                break;
              }
            }
          }

          if (!Match || Match->GetDeleteMe() || Match->m_Game->GetIsGameOver()) {
            m_Socket->PutBytes(m_Aura->m_GPSProtocol->SEND_GPSS_REJECT(REJECTGPS_NOTFOUND));
            Abort = true;
          } else {
            // reconnect successful!
            Match->EventGProxyReconnect(m_Socket, LastPacket);
            m_Socket = nullptr;
            result = PREPLAYER_CONNECTION_RECONNECTED;
            Abort = true;
          }
        } else {
          bool anyExtensions = m_Aura->m_Net->m_Config->m_EnableTCPScanUDP || m_Aura->m_Net->m_Config->m_EnableTCPWrapUDP || m_Aura->m_Net->m_Config->m_VLANEnabled;
          if (Length == 6 && Bytes[0] == GPS_HEADER_CONSTANT && Bytes[1] == CGPSProtocol::GPS_UDPSCAN) {
            if (m_Aura->m_Net->m_Config->m_EnableTCPScanUDP) {
              if (m_Aura->m_CurrentLobby->GetIsLobby() && m_Aura->m_CurrentLobby->GetUDPEnabled()) {
                vector<uint8_t> packet = {GPS_HEADER_CONSTANT, CGPSProtocol::GPS_UDPSCAN, 6, 0};
                uint16_t port = m_Aura->m_CurrentLobby->GetDiscoveryPort(GetInnerIPVersion(&(m_Socket->m_RemoteHost)));
                AppendByteArray(packet, port, false);
                m_Socket->PutBytes(packet);
              }
            } else if (!anyExtensions) {
              Abort = true;
            }
          } else if (m_Type == INCOMING_CONNECTION_TYPE_NONE && Length == 4 && Bytes[0] == GPS_HEADER_CONSTANT && Bytes[1] == CGPSProtocol::GPS_UDPSYN) {
            if (m_Aura->m_Net->m_Config->m_EnableTCPWrapUDP) {
              vector<uint8_t> packet = {GPS_HEADER_CONSTANT, CGPSProtocol::GPS_UDPACK, 4, 0};
              m_Socket->PutBytes(packet);
              m_Type = INCOMING_CONNECTION_TYPE_UDP_TUNNEL;
            } else if (!anyExtensions) {
              Abort = true;
            }
          } else if (m_Type == INCOMING_CONNECTION_TYPE_NONE && Length == 4 && Bytes[0] == VLAN_HEADER_CONSTANT && Bytes[1] == 0xFF) { // TODO: VLAN
            if (m_Aura->m_Net->m_Config->m_VLANEnabled) {
              m_Type = INCOMING_CONNECTION_TYPE_VLAN;
            } else if (!anyExtensions) {
              Abort = true;
            }
          }
        }

        if (Abort) {
          // Process no more packets
          break;
        }

        LengthProcessed += Length;
        Bytes = std::vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
      }
    }

    if (Abort && result != PREPLAYER_CONNECTION_PROMOTED && result != PREPLAYER_CONNECTION_RECONNECTED) {
      result = PREPLAYER_CONNECTION_DESTROY;
      RecvBuffer->clear();
    } else if (LengthProcessed > 0) {
      *RecvBuffer = RecvBuffer->substr(LengthProcessed);
    }
  } else if (Ticks - m_Socket->GetLastRecv() >= timeout) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  if (Abort) {
    m_DeleteMe = true;
  }

  /*
  if (result == PREPLAYER_CONNECTION_PROMOTED || result == PREPLAYER_CONNECTION_RECONNECTED) {
    return result;
  }
  */

  // At this point, m_Socket may have been transferred to CGameUser
  if (m_DeleteMe || !m_Socket->GetConnected() || m_Socket->HasError() || m_Socket->HasFin()) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  m_Socket->DoSend(static_cast<fd_set*>(send_fd));

  if (m_Type == INCOMING_CONNECTION_TYPE_KICKED_PLAYER && !m_Socket->GetIsSendPending()) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  return PREPLAYER_CONNECTION_OK;
}

void CGameConnection::Send(const std::vector<uint8_t>& data) const
{
  if (m_Socket && !m_Socket->HasError()) {
    m_Socket->PutBytes(data);
  }
}

//
// CGameUser
//

CGameUser::CGameUser(CGame* nGame, CGameConnection* connection, uint8_t nUID, uint32_t nJoinedRealmInternalId, string nJoinedRealm, string nName, std::array<uint8_t, 4> nInternalIP, bool nReserved)
  : m_Protocol(connection->m_Protocol),
    m_Game(nGame),
    m_Socket(connection->GetSocket()),
    m_IPv4Internal(std::move(nInternalIP)),
    m_RealmInternalId(nJoinedRealmInternalId),
    m_RealmHostName(std::move(nJoinedRealm)),
    m_Name(std::move(nName)),
    m_TotalPacketsSent(0),
    m_TotalPacketsReceived(1),
    m_LeftCode(PLAYERLEAVE_LOBBY),
    m_QuitGame(false),
    m_PongCounter(0),
    m_SyncCounterOffset(0),
    m_SyncCounter(0),
    m_JoinTicks(GetTicks()),
    m_LastMapPartSent(0),
    m_LastMapPartAcked(0),
    m_StartedDownloadingTicks(0),
    m_FinishedDownloadingTime(0),
    m_FinishedLoadingTicks(0),
    m_StartedLaggingTicks(0),
    m_LastGProxyWaitNoticeSentTime(0),
    m_GProxyReconnectKey(rand()),
    m_UID(nUID),
    m_OldUID(0xFF),
    m_PseudonymUID(0xFF),
    m_Verified(false),
    m_Owner(false),
    m_Reserved(nReserved),
    m_Observer(false),
    m_PowerObserver(false),
    m_WhoisShouldBeSent(false),
    m_WhoisSent(false),
    m_MapReady(false),
    m_MapKicked(false),
    m_PingKicked(false),
    m_SpoofKicked(false),
    m_Ready(false),
    m_HasHighPing(false),
    m_DownloadAllowed(false),
    m_DownloadStarted(false),
    m_DownloadFinished(false),
    m_FinishedLoading(false),
    m_Lagging(false),
    m_DropVote(false),
    m_KickVote(false),
    m_Muted(false),
    m_LeftMessageSent(false),
    m_StatusMessageSent(false),
    m_UsedAnyCommands(false),
    m_SentAutoCommandsHelp(false),
    m_SmartCommand(SMART_COMMAND_NONE),
    m_CheckStatusByTicks(GetTicks() + CHECK_STATUS_LATENCY),

    m_GProxy(false),
    m_GProxyPort(0),
    m_GProxyDisconnectNoticeSent(false),

    m_GProxyExtended(false),
    m_GProxyVersion(0),
    m_Disconnected(false),
    m_TotalDisconnectTicks(0),

    m_TeamCaptain(0),
    m_Saved(false),
    m_RemainingPauses(GAME_PAUSES_PER_PLAYER),
    m_DeleteMe(false)
{
  m_Socket->SetLogErrors(true);
}

CGameUser::~CGameUser()
{
  if (m_Socket) {
    if (!m_LeftMessageSent) {
      Send(m_Game->GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(GetUID(), GetLeftCode()));
    }
    m_Socket->Flush();
    UnrefConnection();
    delete m_Socket;
  }

  for (auto& ctx : m_Game->m_Aura->m_ActiveContexts) {
    if (ctx->m_GameUser == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_GameUser = nullptr;
    }
  }
}

uint32_t CGameUser::GetOperationalRTT() const
{
  // weighted average of stored pings (max 6 stored = 25-30 seconds)
  // 4:3:2:1:1:1 (more recent = more weight)
  //
  // note that this vector may have the bias of LC-style pings incorporated
  // this means that the output "operational RTT" may sometimes be half the actual RTT.

  uint32_t weightedSum = 0;
  uint8_t backDelta = 0;
  uint8_t i = static_cast<uint8_t>(m_RTTValues.size());
  uint32_t totalWeight = 0;
  while (i--) {
    const uint32_t weight = (backDelta >= MAX_PING_WEIGHT ? 1 : MAX_PING_WEIGHT - backDelta);
    weightedSum += m_RTTValues[i] * weight;
    totalWeight += weight;
    backDelta++;
  }

  if (totalWeight == 0) {
    return 0;
  }

  return weightedSum / totalWeight;
}

uint32_t CGameUser::GetDisplayRTT() const
{
  return GetOperationalRTT();
}

uint32_t CGameUser::GetRTT() const
{
  if (m_Game->m_Aura->m_Config->m_LiteralRTT) {
    return GetOperationalRTT();
  }
  return GetOperationalRTT() * 2;
}

string CGameUser::GetConnectionErrorString() const
{
  string errorString;
  if (m_Socket) {
    errorString = m_Socket->GetErrorString();
  }
  if (errorString.empty()) {
    errorString = "EUNKNOWN";
  }
  return errorString;
}

string CGameUser::GetLowerName() const
{
  return ToLowerCase(m_Name);
}

string CGameUser::GetDisplayName() const
{
  if (m_Game->GetIsHiddenPlayerNames() && !(m_Observer && m_Game->GetGameLoaded())) {
    if (m_PseudonymUID == 0xFF) {
      return "Player " + ToDecString(m_UID);
    } else {
      // After CGame::RunPlayerObfuscation()
      return "Player " + ToDecString(m_PseudonymUID) + "?";
    }
  }
  return m_Name;
}

CRealm* CGameUser::GetRealm(bool mustVerify) const
{
  if (m_RealmInternalId < 0x10)
    return nullptr;

  if (mustVerify && !m_Verified) {
    return nullptr;
  }

  return m_Game->m_Aura->GetRealmByInputId(m_Game->m_Aura->m_RealmsIdentifiers[m_RealmInternalId]);
}

string CGameUser::GetRealmDataBaseID(bool mustVerify) const
{
  CRealm* Realm = GetRealm(mustVerify);
  if (Realm) return Realm->GetDataBaseID();
  return string();
}

void CGameUser::CloseConnection()
{
  if (m_Disconnected) return;
  m_LastDisconnectTicks = GetTicks();
  m_Disconnected = true;
  m_Socket->Close();
}

void CGameUser::UnrefConnection(bool deferred)
{
  m_Game->m_Aura->m_Net->OnUserKicked(this, deferred);

  if (!m_Disconnected) {
    m_LastDisconnectTicks = GetTicks();
    m_Disconnected = true;
  }
}

void CGameUser::RefreshUID()
{
  m_OldUID = m_UID;
  m_UID = m_Game->GetNewUID();
}

bool CGameUser::Update(void* fd, int64_t timeout)
{
  if (m_Disconnected) {
    if (m_GProxyExtended && GetTotalDisconnectTicks() > m_Game->m_Aura->m_Net->m_Config->m_ReconnectWaitTicks) {
      m_Game->EventUserKickGProxyExtendedTimeout(this);
    }
    return m_DeleteMe;
  }

  if (m_Socket->HasError()) {
    m_Game->EventUserDisconnectSocketError(this);
    return m_DeleteMe;
  }

  if (m_DeleteMe) {
    m_Socket->ClearRecvBuffer(); // in case there are pending bytes from a previous recv
    m_Socket->Discard(static_cast<fd_set*>(fd));
    return m_DeleteMe;
  }

  const int64_t Ticks = GetTicks();

  bool Abort = false;
  if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {
    // extract as many packets as possible from the socket's receive buffer and process them

    string*              RecvBuffer         = m_Socket->GetBytes();
    std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
    uint32_t             LengthProcessed    = 0;

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    CIncomingAction*     Action;
    CIncomingChatPlayer* ChatPlayer;
    CIncomingMapSize*    MapSize;
    uint32_t             Pong;

    while (Bytes.size() >= 4)
    {
      // bytes 2 and 3 contain the length of the packet
      const uint16_t             Length = ByteArrayToUInt16(Bytes, false, 2);
      if (Length < 4) {
        m_Game->EventUserDisconnectGameProtocolError(this, true);
        Abort = true;
        break;
      }
      if (Bytes.size() < Length) break;
      const std::vector<uint8_t> Data   = std::vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

      if (Bytes[0] == W3GS_HEADER_CONSTANT)
      {
        ++m_TotalPacketsReceived;

        // byte 1 contains the packet ID

        switch (Bytes[1])
        {
          case CGameProtocol::W3GS_LEAVEGAME:
            m_Game->EventUserLeft(this);
            Abort = true;
            break;

          case CGameProtocol::W3GS_GAMELOADED_SELF:
            if (m_Protocol->RECEIVE_W3GS_GAMELOADED_SELF(Data)) {
              if (m_Game->GetGameLoading() && !m_FinishedLoading) {
                m_FinishedLoading      = true;
                m_FinishedLoadingTicks = GetTicks();
                m_Game->EventUserLoaded(this);
              }
            }

            break;

          case CGameProtocol::W3GS_OUTGOING_ACTION:
            Action = m_Protocol->RECEIVE_W3GS_OUTGOING_ACTION(Data, m_UID);

            if (Action) {
              if (!m_Game->EventUserAction(this, Action)) {
                m_Game->EventUserDisconnectGameProtocolError(this, false);
                Abort = true;
              }
            }

            // don't delete Action here because the game is going to store it in a queue and delete it later

            break;

          case CGameProtocol::W3GS_OUTGOING_KEEPALIVE:
            m_CheckSums.push(m_Protocol->RECEIVE_W3GS_OUTGOING_KEEPALIVE(Data));
            ++m_SyncCounter;
            m_Game->EventUserKeepAlive(this);
            break;

          case CGameProtocol::W3GS_CHAT_TO_HOST:
            ChatPlayer = m_Protocol->RECEIVE_W3GS_CHAT_TO_HOST(Data);

            if (ChatPlayer)
              m_Game->EventUserChatToHost(this, ChatPlayer);

            delete ChatPlayer;
            break;

          case CGameProtocol::W3GS_DROPREQ:
            if (m_Game->GetLagging() && !m_DropVote) {
              m_DropVote = true;
              m_Game->EventUserDropRequest(this);
            }

            break;

          case CGameProtocol::W3GS_MAPSIZE:
            if (m_MapReady) {
              // Protection against rogue clients
              break;
            }

            MapSize = m_Protocol->RECEIVE_W3GS_MAPSIZE(Data);

            if (MapSize)
              m_Game->EventUserMapSize(this, MapSize);

            delete MapSize;
            break;

          case CGameProtocol::W3GS_PONG_TO_HOST: {
            Pong = m_Protocol->RECEIVE_W3GS_PONG_TO_HOST(Data);

            // we discard pong values of 1
            // the client sends one of these when connecting plus we return 1 on error to kill two birds with one stone

            // we also discard pong values when anyone else is downloading if we're configured to
            const bool bufferBloatForbidden = m_Game->m_Aura->m_Net->m_Config->m_HasBufferBloat && m_Game->IsDownloading();

            if (Pong != 1 && !bufferBloatForbidden) {
              // we also discard pong values when we're downloading because they're almost certainly inaccurate
              // this statement also gives the player a 8 second grace period after downloading the map to allow queued (i.e. delayed) ping packets to be ignored
              if (!m_DownloadStarted || (m_DownloadFinished && GetTime() - m_FinishedDownloadingTime >= 8)) {
                m_RTTValues.push_back(m_Game->m_Aura->m_Config->m_LiteralRTT ? (static_cast<uint32_t>(GetTicks()) - Pong) : ((static_cast<uint32_t>(GetTicks()) - Pong) / 2));
                if (m_RTTValues.size() > MAXIMUM_PINGS_COUNT) {
                  m_RTTValues.erase(begin(m_RTTValues));
                }
                m_Game->EventUserPongToHost(this);
              }
            }
            if (!bufferBloatForbidden && m_RTTValues.size() < CONSISTENT_PINGS_COUNT) {
              // Measure player's ping as fast as possible, by chaining new pings to pongs received.
              Send(m_Game->GetProtocol()->SEND_W3GS_PING_FROM_HOST());
            }
            ++m_PongCounter;
            break;
          }
        }
      }
      else if (Bytes[0] == GPS_HEADER_CONSTANT && m_Game->GetIsProxyReconnectable()) {
        if (Bytes[1] == CGPSProtocol::GPS_ACK && Length == 8) {
          const size_t LastPacket               = ByteArrayToUInt32(Data, false, 4);
          const size_t PacketsAlreadyUnqueued   = m_TotalPacketsSent - m_GProxyBuffer.size();

          if (LastPacket > PacketsAlreadyUnqueued)
          {
            size_t PacketsToUnqueue = LastPacket - PacketsAlreadyUnqueued;

            if (PacketsToUnqueue > m_GProxyBuffer.size())
              PacketsToUnqueue = m_GProxyBuffer.size();

            while (PacketsToUnqueue > 0)
            {
              m_GProxyBuffer.pop();
              --PacketsToUnqueue;
            }
          }
        } else if (Bytes[1] == CGPSProtocol::GPS_INIT) {
          CRealm* MyRealm = GetRealm(false);
          if (MyRealm) {
            m_GProxyPort = MyRealm->GetUsesCustomPort() ? MyRealm->GetPublicHostPort() : m_Game->GetHostPort();
          } else if (m_RealmInternalId == 0) {
            m_GProxyPort = m_Game->m_Aura->m_Net->m_Config->m_UDPEnableCustomPortTCP4 ? m_Game->m_Aura->m_Net->m_Config->m_UDPCustomPortTCP4 : m_Game->GetHostPort();
          } else {
            m_GProxyPort = 6112;
          }
          m_GProxy = true;
          if (Length >= 8) {
            m_GProxyVersion = ByteArrayToUInt32(Bytes, false, 4);
          }
          m_Socket->PutBytes(m_Game->m_Aura->m_GPSProtocol->SEND_GPSS_INIT(m_GProxyPort, m_UID, m_GProxyReconnectKey, m_Game->GetGProxyEmptyActions()));
          if (m_GProxyVersion >= 2) {
            m_Socket->PutBytes(m_Game->m_Aura->m_GPSProtocol->SEND_GPSS_SUPPORT_EXTENDED(m_Game->m_Aura->m_Net->m_Config->m_ReconnectWaitTicks));
          }
          // the port to which the client directly connects
          // (proxy port if it uses a proxy; the hosted game port otherwise)
          Print(m_Game->GetLogPrefix() + "player [" + m_Name + "] will reconnect at port " + to_string(m_GProxyPort) + " if disconnected");
        } else if (Bytes[1] == CGPSProtocol::GPS_SUPPORT_EXTENDED && Length >= 8) {
          //uint32_t seconds = ByteArrayToUInt32(Bytes, false, 4);
          if (m_GProxy && m_Game->GetIsProxyReconnectableLong()) {
            m_GProxyExtended = true;
            Print(m_Game->GetLogPrefix() + "player [" + m_Name + "] is using GProxy Extended");
          }
        }
      }

      if (Abort) {
        // Process no more packets
        break;
      }
      
      LengthProcessed += Length;
      Bytes = std::vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
    }

    if (Abort) {
      RecvBuffer->clear();
    } else if (LengthProcessed > 0) {
      *RecvBuffer = RecvBuffer->substr(LengthProcessed);
    }
  } else if (Ticks - m_Socket->GetLastRecv() >= timeout) {
    // check for socket timeouts
    // if we don't receive anything from a player for 30 seconds we can assume they've dropped
    // this works because in the lobby we send pings every 5 seconds and expect a response to each one
    // and in the game the Warcraft 3 client sends keepalives frequently (at least once per second it looks like)
    m_Game->EventUserDisconnectTimedOut(this);
    if (m_Disconnected) {
      if (m_DeleteMe) {
        m_Socket->Discard(static_cast<fd_set*>(fd));
      }
      return m_DeleteMe;
    }
  }

  // EventUserLeft sets the game in a state where this player is still in m_Users, but it has no associated slot.
  // It's therefore crucial to check the Abort flag that it sets to avoid modifying it further.
  // As soon as the CGameUser::Update() call returns, EventUserDeleted takes care of erasing from the m_Users vector.
  if (!Abort) {
    // try to find out why we're requesting deletion
    // in cases other than the ones covered here m_LeftReason should have been set when m_DeleteMe was set
    if (m_Socket->HasError()) {
      m_Game->EventUserDisconnectSocketError(this);
    } else if (m_Socket->HasFin() || !m_Socket->GetConnected()) {
      m_Game->EventUserDisconnectConnectionClosed(this);
    } else if (m_KickByTicks.has_value() && m_KickByTicks.value() < Ticks) {
      m_Game->EventUserKickHandleQueued(this);
    } else if (!m_Verified && m_RealmInternalId >= 0x10 && Ticks - m_JoinTicks >= GAME_USER_UNVERIFIED_KICK_TICKS && m_Game->GetIsLobby()) {
      CRealm* Realm = GetRealm(false);
      if (Realm && Realm->GetUnverifiedAutoKickedFromLobby()) {
        m_Game->EventUserKickUnverified(this);
      }
    }

    if (!m_StatusMessageSent && m_CheckStatusByTicks < Ticks) {
      m_Game->EventUserCheckStatus(this);
    }
  }

  if (!m_DeleteMe) {
    // GProxy++ acks
    if (m_GProxy && (!m_LastGProxyAckTicks.has_value() || Ticks - m_LastGProxyAckTicks.value() >= GPS_ACK_PERIOD)) {
      m_Socket->PutBytes(m_Game->m_Aura->m_GPSProtocol->SEND_GPSS_ACK(m_TotalPacketsReceived));
      m_LastGProxyAckTicks = Ticks;
    }

    // wait 5 seconds after joining before sending the /whois or /w
    // if we send the /whois too early battle.net may not have caught up with where the player is and return erroneous results
    if (m_WhoisShouldBeSent && !m_Verified && !m_WhoisSent && !m_RealmHostName.empty() && Ticks - m_JoinTicks >= AUTO_REALM_VERIFY_LATENCY) {
      CRealm* Realm = GetRealm(false);
      if (Realm) {
        if (m_Game->GetDisplayMode() == GAME_PUBLIC || Realm->GetPvPGN()) {
          if (m_Game->GetSentPriorityWhois()) {
            Realm->QueuePriorityWhois("/whois " + m_Name);
            m_Game->SetSentPriorityWhois(true);
          } else {
            Realm->QueueCommand("/whois " + m_Name);
          }
        } else if (m_Game->GetDisplayMode() == GAME_PRIVATE) {
          Realm->QueueWhisper(R"(Spoof check by replying to this message with "sc" [ /r sc ])", m_Name);
        }
      }

      m_WhoisSent = true;
    }
  }

  /*
  if (m_GProxy && m_Game->GetGameLoaded()) {
    return m_DeleteMe;
  }
  */

  if (m_DeleteMe) {
    return m_DeleteMe;
  }
  if (m_Socket) {
    if (m_Socket->HasError()) {
      m_Game->EventUserDisconnectSocketError(this);
    } else if (m_Socket->HasFin() || !m_Socket->GetConnected()) {
      m_Game->EventUserDisconnectConnectionClosed(this);
    }
    return m_DeleteMe;
  }

  return false;
}

void CGameUser::Send(const std::vector<uint8_t>& data)
{
  // must start counting packet total from beginning of connection
  // but we can avoid buffering packets until we know the client is using GProxy++ since that'll be determined before the game starts
  // this prevents us from buffering packets for non-GProxy++ clients

  ++m_TotalPacketsSent;

  if (m_GProxy && m_Game->GetGameLoaded())
    m_GProxyBuffer.push(data);

  if (!m_Disconnected && !m_Socket->HasError()) {
    if (m_Game->m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
      if (!m_Socket) {
        Print(m_Game->GetLogPrefix() + " trying to put bytes into nullptr socket");
      } else if (m_Socket->HasError()) {
        Print(m_Game->GetLogPrefix() + " trying to put bytes into errored socket [" + m_Socket->GetName() + "] (" + to_string(m_Socket->GetError()) + ")");
      } else if (m_Socket->m_Socket == INVALID_SOCKET) {
        Print(m_Game->GetLogPrefix() + " trying to put bytes into invalid socket [" + m_Socket->GetName() + "]");
      } else {
        Print(m_Game->GetLogPrefix() + " trying to put " + to_string(data.size())  + " bytes into socket");
      }
    }
    m_Socket->PutBytes(data);
    if (m_Game->m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
      if (!m_Socket) {
        Print(m_Game->GetLogPrefix() + " successfully put " + to_string(data.size()) + " bytes into nullptr socket");
      } else if (m_Socket->HasError()) {
        Print(m_Game->GetLogPrefix() + " successfully put " + to_string(data.size()) + " bytes into errored socket [" + m_Socket->GetName() + "] (" + to_string(m_Socket->GetError()) + ")");
      } else if (m_Socket->m_Socket == INVALID_SOCKET) {
        Print(m_Game->GetLogPrefix() + " successfully put " + to_string(data.size()) + " bytes into invalid socket [" + m_Socket->GetName() + "]");
      } else {
        Print(m_Game->GetLogPrefix() + " successfully put " + to_string(data.size()) + " bytes into socket [" + m_Socket->GetName() + "]");
      }
    }
  }
}

void CGameUser::EventGProxyReconnect(CStreamIOSocket* NewSocket, const uint32_t LastPacket)
{
  // prevent potential session hijackers from stealing sudo access
  SudoModeEnd();

  // Runs from the CGameConnection iterator, so appending to CNet::m_IncomingConnections needs to wait
  // UnrefConnection(deferred = true) takes care of this
  // a new CGameConnection for the old CStreamIOSocket is created, and is pushed to CNet::m_StaleConnections 
  UnrefConnection(true);

  m_Socket = NewSocket;
  m_Socket->SetLogErrors(true);
  m_Socket->PutBytes(m_Game->m_Aura->m_GPSProtocol->SEND_GPSS_RECONNECT(m_TotalPacketsReceived));

  const size_t PacketsAlreadyUnqueued = m_TotalPacketsSent - m_GProxyBuffer.size();

  if (LastPacket > PacketsAlreadyUnqueued)
  {
    size_t PacketsToUnqueue = LastPacket - PacketsAlreadyUnqueued;

    if (PacketsToUnqueue > m_GProxyBuffer.size())
      PacketsToUnqueue = m_GProxyBuffer.size();

    while (PacketsToUnqueue > 0)
    {
      m_GProxyBuffer.pop();
      --PacketsToUnqueue;
    }
  }

  // send remaining packets from buffer, preserve buffer

  queue<std::vector<uint8_t>> TempBuffer;

  while (!m_GProxyBuffer.empty())
  {
    m_Socket->PutBytes(m_GProxyBuffer.front());
    TempBuffer.push(m_GProxyBuffer.front());
    m_GProxyBuffer.pop();
  }

  m_Disconnected = false;
  m_GProxyBuffer = TempBuffer;
  m_GProxyDisconnectNoticeSent = false;
  if (m_LastDisconnectTicks.has_value()) {
    m_TotalDisconnectTicks += GetTicks() - m_LastDisconnectTicks.value();
  }
  m_Game->SendAllChat("Player [" + GetDisplayName() + "] reconnected with GProxy++!");
  if (m_Game->m_Aura->MatchLogLevel(LOG_LEVEL_NOTICE)) {
    Print("user reconnected: [" + GetName() + "@" + GetRealmHostName() + "#" + ToDecString(GetUID()) + "] from [" + GetIPString() + "] (" + m_Socket->GetName() + ")");
  }
}

int64_t CGameUser::GetTotalDisconnectTicks() const
{
  if (!m_Disconnected || !m_LastDisconnectTicks.has_value()) {
    return m_TotalDisconnectTicks;
  } else {
    return m_TotalDisconnectTicks + GetTicks() - m_LastDisconnectTicks.value();
  }
}

string CGameUser::GetDelayText(bool displaySync) const
{
  string pingText, syncText;
  // Note: When someone is lagging, we actually clear their ping data.
  const bool anyPings = GetStoredRTTCount() > 0;
  if (!anyPings) {
    pingText = "?";
  } else if (GetStoredRTTCount() < 3) {
    pingText = "*" + to_string(GetOperationalRTT());
  } else {
    pingText = to_string(GetOperationalRTT());
  }
  if (!displaySync || !m_Game->GetGameLoaded() || GetNormalSyncCounter() >= m_Game->GetSyncCounter()) {
    if (anyPings) return pingText + "ms";
    return pingText;
  }
  float syncDelay = static_cast<float>(m_Game->GetLatency()) * static_cast<float>(m_Game->GetSyncCounter() - GetNormalSyncCounter());

  if (m_SyncCounterOffset == 0) {
    // Expect clients to always be at least one RTT behind.
    // The "sync delay" is defined as the additional delay they got.
    syncDelay -= static_cast<float>(GetRTT());
  }

  if (!anyPings) {
    return "+" + to_string(static_cast<uint32_t>(syncDelay)) + "ms";
  } else if (syncDelay <= 0) {
    return pingText + "ms";
  } else {
    return pingText + "+" + to_string(static_cast<uint32_t>(syncDelay)) + "ms";
  }
}

string CGameUser::GetSyncText() const
{
  if (!m_Game->GetGameLoaded() || GetSyncCounter() >= m_Game->GetSyncCounter()) {
    return string();
  }
  bool isNormalized = m_SyncCounterOffset > 0;
  string behindTimeText;
  if (GetNormalSyncCounter() < m_Game->GetSyncCounter()) {
    float normalSyncDelay = static_cast<float>(m_Game->GetLatency()) * static_cast<float>(m_Game->GetSyncCounter() - GetNormalSyncCounter());
    behindTimeText = ToFormattedString(normalSyncDelay / 1000) + "s behind";
  }
  if (isNormalized && GetSyncCounter() < m_Game->GetSyncCounter()) {
    float totalSyncDelay = static_cast<float>(m_Game->GetLatency()) * static_cast<float>(m_Game->GetSyncCounter() - GetSyncCounter());
    if (behindTimeText.empty()) {
      behindTimeText += ToFormattedString(totalSyncDelay / 1000) + "s behind unnormalized";
    } else {
      behindTimeText += " (" + ToFormattedString(totalSyncDelay / 1000) + "s unnormalized)";
    }
  }
  return behindTimeText;
}

bool CGameUser::GetIsSudoMode() const
{
  if (!m_SudoMode.has_value()) return false;
  return GetTime() < m_SudoMode.value();
}

bool CGameUser::CheckSudoMode()
{
  if (GetIsSudoMode()) return true;
  if (m_SudoMode.has_value()) {
    m_SudoMode = nullopt;
    if (m_Game->m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
      Print(m_Game->GetLogPrefix() + "sudo session expired for [" + m_Name + "]");
    }
  }
  return false;
}

void CGameUser::SudoModeStart()
{
  if (m_Game->m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
    Print(m_Game->GetLogPrefix() + "sudo session started by [" + m_Name + "]");
  }
  m_SudoMode = GetTime() + 600;
}

void CGameUser::SudoModeEnd()
{
  if (!GetIsSudoMode()) {
    return;
  }
  if (m_Game->m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
    Print(m_Game->GetLogPrefix() + "sudo session ended by [" + m_Name + "]");
  }
  m_SudoMode = nullopt;
}

bool CGameUser::GetIsNativeReferee() const
{
  return m_Observer && m_Game->GetMap()->GetMapObservers() == MAPOBS_REFEREES;
}

bool CGameUser::GetCanUsePublicChat() const
{
  if (!m_Observer || m_PowerObserver || (!m_Game->GetGameLoading() && !m_Game->GetGameLoaded())) return true;
  return !m_Game->GetUsesCustomReferees() && m_Game->GetMap()->GetMapObservers() == MAPOBS_REFEREES;
}

bool CGameUser::GetIsOwner(optional<bool> assumeVerified) const
{
  if (m_Owner) return true;
  bool isVerified = false;
  if (assumeVerified.has_value()) {
    isVerified = assumeVerified.value();
  } else {
    isVerified = IsRealmVerified();
  }
  return m_Game->MatchOwnerName(m_Name) && m_RealmHostName == m_Game->GetOwnerRealm() && (
    isVerified || m_RealmHostName.empty()
  );
}

bool CGameUser::UpdateReady()
{
  if (m_UserReady.has_value()) {
    m_Ready = m_UserReady.value();
    return m_Ready;
  }
  if (!m_MapReady) {
    return m_Ready;
  }
  switch (m_Game->GetPlayersReadyMode()) {
    case READY_MODE_FAST:
      m_Ready = true;
      break;
    case READY_MODE_EXPECT_RACE:
      if (m_Game->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        m_Ready = true;
      } else if (m_Game->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
        m_Ready = true;
      } else {
        const CGameSlot* slot = m_Game->InspectSlot(m_Game->GetSIDFromUID(GetUID()));
        if (slot) {
          m_Ready = slot->GetRaceFixed() != SLOTRACE_RANDOM;
        } else {
          m_Ready = false;
        }
      }
      break;
    case READY_MODE_EXPLICIT:
    default: {
      m_Ready = false;
    }
  }
  return m_Ready;
}

bool CGameUser::GetReadyReminderIsDue() const
{
  return !m_ReadyReminderLastTicks.has_value() || m_ReadyReminderLastTicks.value() + READY_REMINDER_PERIOD < GetTicks();
}

void CGameUser::SetReadyReminded()
{
  m_ReadyReminderLastTicks = GetTicks();
}
