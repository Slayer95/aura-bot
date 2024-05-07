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
#include "gameplayer.h"
#include "aura.h"
#include "realm.h"
#include "map.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
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
    m_IsUDPTunnel(false),
    m_Socket(nSocket),
    m_IncomingJoinPlayer(nullptr),
    m_DeleteMe(false)
{
}

CGameConnection::~CGameConnection()
{
  if (m_Socket)
    delete m_Socket;

  delete m_IncomingJoinPlayer;
}

uint8_t CGameConnection::Update(void* fd, void* send_fd)
{
  if (m_DeleteMe)
    return PREPLAYER_CONNECTION_DESTROY;

  if (!m_Socket)
    return PREPLAYER_CONNECTION_OK;

  const int64_t Time = GetTime();
  if (Time - m_Socket->GetLastRecv() >= 5) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  bool IsPromotedToPlayer = false;
  bool Abort = false;
  if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {
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

      if (Bytes[0] == W3GS_HEADER_CONSTANT || (Bytes[0] == GPS_HEADER_CONSTANT && m_Aura->m_Net->m_Config->m_ProxyReconnect > 0)) {
        if (Length >= 8 && Bytes[0] == W3GS_HEADER_CONSTANT && Bytes[1] == CGameProtocol::W3GS_REQJOIN &&
          m_Aura->m_CurrentLobby && !m_Aura->m_CurrentLobby->GetIsMirror() && !m_Aura->m_CurrentLobby->GetLobbyLoading()) {
          if (m_IsUDPTunnel) {
            m_IsUDPTunnel = false;
            vector<uint8_t> packet = {GPS_HEADER_CONSTANT, CGPSProtocol::GPS_UDPFIN, 4, 0};
            m_Socket->PutBytes(packet);
          }

          if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
            Print("[AURA] Got REQJOIN " + ByteArrayToDecString(Bytes));
          }
          delete m_IncomingJoinPlayer;
          m_IncomingJoinPlayer = m_Protocol->RECEIVE_W3GS_REQJOIN(Data);
          if (!m_IncomingJoinPlayer) {
            // Invalid request
            Abort = true;
          } else if (m_Aura->m_CurrentLobby->GetHostCounter() != (m_IncomingJoinPlayer->GetHostCounter() & 0x00FFFFFF)) {
            // Trying to join the wrong game
            Abort = true;
          } else if (m_Aura->m_CurrentLobby->EventRequestJoin(this, m_IncomingJoinPlayer)) {
            IsPromotedToPlayer = true;
          } else {
            // Join failed
            Abort = true;
          }
        } else if (m_IsUDPTunnel && Bytes[0] == W3GS_HEADER_CONSTANT && CGameProtocol::W3GS_SEARCHGAME <= Bytes[1] && Bytes[1] <= CGameProtocol::W3GS_DECREATEGAME) {
          if (Length <= 1024) {
            struct UDPPkt pkt;
            pkt.socket = m_Socket;
            pkt.sender = &(m_Socket->m_RemoteHost);
            memcpy(pkt.buf, Bytes.data(), Length);
            pkt.length = Length;
            m_Aura->m_Net->HandleUDP(&pkt);
          } 
        } else if (Length == 13 && Bytes[0] == GPS_HEADER_CONSTANT && Bytes[1] == CGPSProtocol::GPS_RECONNECT) {
          const uint32_t ReconnectKey = ByteArrayToUInt32(Bytes, false, 5);
          const uint32_t LastPacket   = ByteArrayToUInt32(Bytes, false, 9);

          // look for a matching player in a running game

          CGamePlayer* Match = nullptr;

          for (auto& game : m_Aura->m_Games) {
            if (game->GetGameLoaded() && game->GetIsProxyReconnectable()) {
              CGamePlayer* Player = game->GetPlayerFromPID(Bytes[4]);
              if (Player && Player->GetGProxyAny() && Player->GetGProxyReconnectKey() == ReconnectKey) {
                Match = Player;
                break;
              }
            }
          }

          if (!Match || Match->m_Game->GetIsGameOver()) {
            m_Socket->PutBytes(m_Aura->m_GPSProtocol->SEND_GPSS_REJECT(REJECTGPS_NOTFOUND));
            Abort = true;
          } else {
            // reconnect successful!
            Match->EventGProxyReconnect(m_Socket, LastPacket);
            IsPromotedToPlayer = true;
          }
        } else {
          bool anyExtensions = m_Aura->m_Net->m_Config->m_EnableTCPScanUDP || m_Aura->m_Net->m_Config->m_EnableTCPWrapUDP;
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
          } else if (Length == 4 && Bytes[0] == GPS_HEADER_CONSTANT && Bytes[1] == CGPSProtocol::GPS_UDPSYN) {
            if (m_Aura->m_Net->m_Config->m_EnableTCPWrapUDP) {
              vector<uint8_t> packet = {GPS_HEADER_CONSTANT, CGPSProtocol::GPS_UDPACK, 4, 0};
              m_Socket->PutBytes(packet);
              m_IsUDPTunnel = true;
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

    if (Abort) {
      RecvBuffer->clear();
    } else if (LengthProcessed > 0) {
      *RecvBuffer = RecvBuffer->substr(LengthProcessed);
    }
  }

  if (Abort)
    m_DeleteMe = true;

  if (m_DeleteMe || !m_Socket->GetConnected() || m_Socket->HasError()) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  if (IsPromotedToPlayer) {
    return PREPLAYER_CONNECTION_PROMOTED;
  }

  m_Socket->DoSend(static_cast<fd_set*>(send_fd));
  return PREPLAYER_CONNECTION_OK;
}

void CGameConnection::Send(const std::vector<uint8_t>& data) const
{
  if (m_Socket)
    m_Socket->PutBytes(data);
}

//
// CGamePlayer
//

CGamePlayer::CGamePlayer(CGame* nGame, CGameConnection* connection, uint8_t nPID, uint32_t nJoinedRealmInternalId, string nJoinedRealm, string nName, std::vector<uint8_t> nInternalIP, bool nReserved)
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
    m_SyncCounter(0),
    m_JoinTime(GetTime()),
    m_LastMapPartSent(0),
    m_LastMapPartAcked(0),
    m_StartedDownloadingTicks(0),
    m_FinishedDownloadingTime(0),
    m_FinishedLoadingTicks(0),
    m_StartedLaggingTicks(0),
    m_LastGProxyWaitNoticeSentTime(0),
    m_GProxyReconnectKey(rand()),
    m_KickByTime(0),
    m_LastGProxyAckTime(0),
    m_PID(nPID),
    m_Verified(false),
    m_Owner(false),
    m_Reserved(nReserved),
    m_Observer(false),
    m_PowerObserver(false),
    m_WhoisShouldBeSent(false),
    m_WhoisSent(false),
    m_HasMap(false),
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
    m_CheckStatusByTime(GetTime() + 5),

    m_GProxy(false),
    m_GProxyPort(0),
    m_GProxyDisconnectNoticeSent(false),

    m_GProxyExtended(false),
    m_GProxyVersion(0),
    m_Disconnected(false),
    m_TotalDisconnectTime(0),
    m_LastDisconnectTime(0),

    m_TeamCaptain(0),
    m_Saved(false),
    m_PauseCounter(0),
    m_DeleteMe(false)
{
}

CGamePlayer::~CGamePlayer()
{
  delete m_Socket;

  for (auto& ctx : m_Game->m_Aura->m_ActiveContexts) {
    if (ctx->m_Player == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_Player = nullptr;
    }
  }
}

uint32_t CGamePlayer::GetPing() const
{
  // just average all the pings in the vector, nothing fancy

  if (m_Pings.empty())
    return 0;

  uint32_t AvgPing = 0;

  for (const auto& ping : m_Pings)
    AvgPing += ping;

  AvgPing /= static_cast<uint32_t>(m_Pings.size());

  return AvgPing;
}

CRealm* CGamePlayer::GetRealm(bool mustVerify)
{
  if (m_RealmInternalId < 0x10)
    return nullptr;

  if (mustVerify && !m_Verified) {
    return nullptr;
  }

  return m_Game->m_Aura->GetRealmByInputId(m_Game->m_Aura->m_RealmsIdentifiers[m_RealmInternalId]);
}

string CGamePlayer::GetRealmDataBaseID(bool mustVerify)
{
  CRealm* Realm = GetRealm(mustVerify);
  if (Realm) return Realm->GetDataBaseID();
  return "@@LAN/VPN";
}

void CGamePlayer::ResetConnection()
{
  if (!m_Disconnected) {
    m_LastDisconnectTime = GetTime();
  }
  m_Disconnected = true;
  m_Socket->Reset();
}

bool CGamePlayer::Update(void* fd)
{
  const int64_t Time = GetTime();

  // wait 4 seconds after joining before sending the /whois or /w
  // if we send the /whois too early battle.net may not have caught up with where the player is and return erroneous results

  if (m_WhoisShouldBeSent && !m_Verified && !m_WhoisSent && !m_RealmHostName.empty() && Time - m_JoinTime >= 4) {
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

  // check for socket timeouts
  // if we don't receive anything from a player for 30 seconds we can assume they've dropped
  // this works because in the lobby we send pings every 5 seconds and expect a response to each one
  // and in the game the Warcraft 3 client sends keepalives frequently (at least once per second it looks like)

  if (m_Socket && Time - m_Socket->GetLastRecv() >= 30) {
    m_Game->EventPlayerDisconnectTimedOut(this);
    ResetConnection();
  }

  // GProxy++ acks

  if (m_GProxy && Time - m_LastGProxyAckTime >= 10) {
    m_Socket->PutBytes(m_Game->m_Aura->m_GPSProtocol->SEND_GPSS_ACK(m_TotalPacketsReceived));
    m_LastGProxyAckTime = Time;
  }

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
        m_Game->EventPlayerDisconnectGameProtocolError(this);
        ResetConnection();
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
            m_Game->EventPlayerLeft(this);
            Abort = true;
            break;

          case CGameProtocol::W3GS_GAMELOADED_SELF:
            if (m_Protocol->RECEIVE_W3GS_GAMELOADED_SELF(Data))
            {
              if (!m_FinishedLoading)
              {
                m_FinishedLoading      = true;
                m_FinishedLoadingTicks = GetTicks();
                m_Game->EventPlayerLoaded(this);
              }
            }

            break;

          case CGameProtocol::W3GS_OUTGOING_ACTION:
            Action = m_Protocol->RECEIVE_W3GS_OUTGOING_ACTION(Data, m_PID);

            if (Action)
              m_Game->EventPlayerAction(this, Action);

            // don't delete Action here because the game is going to store it in a queue and delete it later

            break;

          case CGameProtocol::W3GS_OUTGOING_KEEPALIVE:
            m_CheckSums.push(m_Protocol->RECEIVE_W3GS_OUTGOING_KEEPALIVE(Data));
            if (!m_Game->GetLagging() || m_Lagging)
              ++m_SyncCounter;
            m_Game->EventPlayerKeepAlive(this);
            break;

          case CGameProtocol::W3GS_CHAT_TO_HOST:
            ChatPlayer = m_Protocol->RECEIVE_W3GS_CHAT_TO_HOST(Data);

            if (ChatPlayer)
              m_Game->EventPlayerChatToHost(this, ChatPlayer);

            delete ChatPlayer;
            break;

          case CGameProtocol::W3GS_DROPREQ:
            if (!m_DropVote)
            {
              m_DropVote = true;
              m_Game->EventPlayerDropRequest(this);
            }

            break;

          case CGameProtocol::W3GS_MAPSIZE:
            MapSize = m_Protocol->RECEIVE_W3GS_MAPSIZE(Data);

            if (MapSize)
              m_Game->EventPlayerMapSize(this, MapSize);

            delete MapSize;
            break;

          case CGameProtocol::W3GS_PONG_TO_HOST:
            Pong = m_Protocol->RECEIVE_W3GS_PONG_TO_HOST(Data);

            // we discard pong values of 1
            // the client sends one of these when connecting plus we return 1 on error to kill two birds with one stone

            if (Pong != 1) {
              // we also discard pong values when we're downloading because they're almost certainly inaccurate
              // this statement also gives the player a 5 second grace period after downloading the map to allow queued (i.e. delayed) ping packets to be ignored

              if (!m_DownloadStarted || (m_DownloadFinished && GetTime() - m_FinishedDownloadingTime >= 5)) {
                // we also discard pong values when anyone else is downloading if we're configured to
                if (!(m_Game->m_Aura->m_Net->m_Config->m_HasBufferBloat && m_Game->IsDownloading())) {
                  m_Pings.push_back(m_Game->m_Aura->m_Config->m_RTTPings ? (static_cast<uint32_t>(GetTicks()) - Pong) : ((static_cast<uint32_t>(GetTicks()) - Pong) / 2));
                  if (m_Pings.size() > 10) {
                    m_Pings.erase(begin(m_Pings));
                  }
                }
              }
            }

            m_Game->EventPlayerPongToHost(this);
            break;
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
          m_Socket->PutBytes(m_Game->m_Aura->m_GPSProtocol->SEND_GPSS_INIT(m_GProxyPort, m_PID, m_GProxyReconnectKey, m_Game->GetGProxyEmptyActions()));
          if (m_GProxyVersion >= 2) {
            m_Socket->PutBytes(m_Game->m_Aura->m_GPSProtocol->SEND_GPSS_SUPPORT_EXTENDED(m_Game->m_Aura->m_Net->m_Config->m_ReconnectWaitTime * 60));
          }
          // the port to which the client directly connects
          // (proxy port if it uses a proxy; the hosted game port otherwise)
          Print("[GAME: " + m_Game->GetGameName() + "] player [" + m_Name + "] will reconnect at port " + to_string(m_GProxyPort) + " if disconnected");
        } else if (Bytes[1] == CGPSProtocol::GPS_SUPPORT_EXTENDED && Length >= 8) {
          //uint32_t seconds = ByteArrayToUInt32(Bytes, false, 4);
          if (m_Game->GetIsProxyReconnectableLong()) {
            m_GProxyExtended = true;
            Print("[GAME: " + m_Game->GetGameName() + "] player [" + m_Name + "] is using GProxy Extended");
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
  }

  // EventPlayerLeft sets the game in an state where this player is still in m_Players, but it has no associated slot.
  // It's therefore crucial to check the Abort flag that it sets to avoid modifying it further.
  // As soon as the CGamePlayer::Update() call returns, EventPlayerDeleted takes care of erasing from the m_Players vector.
  if (!Abort) {
    if (m_Socket) {
      // try to find out why we're requesting deletion
      // in cases other than the ones covered here m_LeftReason should have been set when m_DeleteMe was set
      if (m_Socket->HasError()) {
        m_Game->EventPlayerDisconnectSocketError(this);
        ResetConnection();
      } else if (!m_Socket->GetConnected()) {
        m_Game->EventPlayerDisconnectConnectionClosed(this);
        ResetConnection();
      }
    }

    if (GetKickQueued() && m_KickByTime < Time)
      m_Game->EventPlayerKickHandleQueued(this);

    if (!m_StatusMessageSent && m_CheckStatusByTime < Time)
      m_Game->EventPlayerCheckStatus(this);
  }

  if (!m_Verified && m_RealmInternalId >= 0x10 && Time - m_JoinTime >= 60 && m_Game->GetIsLobby()) {
    CRealm* Realm = GetRealm(false);
    if (Realm && Realm->GetUnverifiedAutoKickedFromLobby()) {
      m_DeleteMe = true;
      m_LeftReason = GetName() + " has been kicked because he is not verified by their realm";
      m_LeftCode = PLAYERLEAVE_DISCONNECT;
      m_Game->SendAllChat(GetName() + " has been kicked because he is not verified by " + m_RealmHostName);
    }
  }

  if (m_Disconnected && m_GProxyExtended && GetTotalDisconnectTime() > m_Game->m_Aura->m_Net->m_Config->m_ReconnectWaitTime * 60) {
    m_DeleteMe = true;
    m_LeftReason = GetName() + " has been kicked because he didn't reconnect in time";
    m_LeftCode = PLAYERLEAVE_DISCONNECT;
    m_Game->SendAllChat(GetName() + " has been kicked because he didn't reconnect in time." );
  }

  if (m_GProxy && m_Game->GetGameLoaded())
    return m_DeleteMe;

  if (m_DeleteMe)
    return true;

  if (m_Socket)
    return m_Socket->HasError() || !m_Socket->GetConnected();

  return false;
}

void CGamePlayer::Send(const std::vector<uint8_t>& data)
{
  // must start counting packet total from beginning of connection
  // but we can avoid buffering packets until we know the client is using GProxy++ since that'll be determined before the game starts
  // this prevents us from buffering packets for non-GProxy++ clients

  ++m_TotalPacketsSent;

  if (m_GProxy && m_Game->GetGameLoaded())
    m_GProxyBuffer.push(data);

  if (!m_Disconnected)
    m_Socket->PutBytes(data);
}

void CGamePlayer::EventGProxyReconnect(CStreamIOSocket* NewSocket, const uint32_t LastPacket)
{
  delete m_Socket;
  m_Socket = NewSocket;
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

  m_GProxyBuffer               = TempBuffer;
  m_GProxyDisconnectNoticeSent = false;
  m_Disconnected = false;
  if (m_LastDisconnectTime > 0)
    m_TotalDisconnectTime += GetTime() - m_LastDisconnectTime;

  m_Game->SendAllChat("Player [" + m_Name + "] reconnected with GProxy++!");
}

int64_t CGamePlayer::GetTotalDisconnectTime() const
{
  if (!m_Disconnected || !m_LastDisconnectTime) {
    return m_TotalDisconnectTime;
  } else {
    return m_TotalDisconnectTime + GetTime() - m_LastDisconnectTime;
  }
}

string CGamePlayer::GetDelayText() const
{
  string pingText, syncText;
  // Note: When someone is lagging, we actually clear their ping data.
  const bool anyPings = GetNumPings() > 0;
  if (!anyPings) {
    pingText = "?";
  } else if (GetNumPings() < 3) {
    pingText = "*" + to_string(GetPing());
  } else {
    pingText = to_string(GetPing());
  }
  if (!m_Game->GetGameLoaded() || GetSyncCounter() >= m_Game->GetSyncCounter()) {
    if (anyPings) return pingText + "ms";
    return pingText;
  }
  float rtt = static_cast<float>(GetPing());
  if (!m_Game->m_Aura->m_Config->m_RTTPings) rtt *= 2;
  float syncDelay = static_cast<float>(m_Game->GetLatency()) * static_cast<float>(m_Game->GetSyncCounter() - GetSyncCounter());

  // Expect clients to always be at least one RTT behind.
  // The "sync delay" is defined as the additional delay they got.
  syncDelay -= rtt;

  if (!anyPings) {
    return "+" + to_string(static_cast<uint32_t>(syncDelay)) + "ms";
  } else if (syncDelay <= 0) {
    return pingText + "ms";
  } else {
    return pingText + "+" + to_string(static_cast<uint32_t>(syncDelay)) + "ms";
  }
}

bool CGamePlayer::GetIsOwner(optional<bool> assumeVerified) const
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
