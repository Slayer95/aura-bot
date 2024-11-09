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

#ifndef AURA_FORWARD_H_
#define AURA_FORWARD_H_

#include <queue>
#include <vector>

class CAura;
class CAuraDB;
class CBNCSUtilInterface;
class CCLI;
class CCommandContext;
class CConfig;
class CConnection;
class CDBBan;
class CDBDotAPlayer;
class CDBDotAPlayerSummary;
class CDBGamePlayer;
class CDBGamePlayerSummary;
class CDBGameSummary;
class CDiscord;
class CDotaStats;
class CGame;
class CGameSeeker;
class CGameSetup;
class CGameSlot;
class CIncomingAction;
class CIncomingChatEvent;
class CIncomingChatPlayer;
class CIncomingGameHost;
class CIncomingJoinRequest;
class CIncomingMapSize;
class CIncomingVLanGameInfo;
class CIncomingVLanSearchGame;
class CIRC;
class CMap;
class CNet;
class CPacked;
class CQueuedChatMessage;
class CRealm;
class CSaveGame;
class CSHA1;
class CSocket;
class CStreamIOSocket;
class CTCPClient;
class CTCPServer;
class CUDPServer;
class CUDPSocket;
class CW3MMD;

namespace GameUser
{
  class CGameUser;
};

struct CBotConfig;
struct CCommandConfig;
struct CDiscordConfig;
struct CGameConfig;
struct CIRCConfig;
struct CNetConfig;
struct CRealmConfig;

struct UDPPkt;

typedef std::vector<GameUser::CGameUser*>         UserList;
typedef std::vector<const GameUser::CGameUser*>   ImmutableUserList;
typedef std::queue<CIncomingAction*>              ActionQueue;

#endif // AURA_FORWARD_H_
