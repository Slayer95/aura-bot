#ifndef AURA_COMMAND_H_
#define AURA_COMMAND_H_

#include "aura.h"
#include "auradb.h"
#include "bnet.h"
#include "game.h"
#include "gameplayer.h"
#include "irc.h"
#include "map.h"

#include <optional>
#include <ostream>
#include <utility>

#define CHAT_SEND_ALL 1 << 0
#define CHAT_LOG_CONSOLE 1 << 1

#define PERM_GAME_PLAYER 1 << 0
#define PERM_GAME_OWNER 1 << 1
#define PERM_BNET_ADMIN 1 << 3
#define PERM_BNET_ROOTADMIN 1 << 4
#define PERM_BOT_SUDO_SPOOFABLE 1 << 6
#define PERM_BOT_SUDO_OK 1 << 7

//
// CCommandContext
//

class CCommandContext
{
protected:
  CAura*                        m_Aura;
  std::string                   m_FromName;
  bool                          m_FromWhisper;
  CGame*                        m_Game;
  uint8_t                       m_Permissions;

  CGamePlayer*                  m_Player;

  std::string                   m_HostName;
  CBNET*                        m_Realm;

  CIRC*                         m_IRC;
  std::string                   m_ChannelName;

  std::ostream*                 m_Output;

  std::optional<uint8_t>        m_OverrideVerified;
  std::optional<uint8_t>        m_OverridePermissions;

public:
  CCommandContext(CAura* nAura, CGame* game, CGamePlayer* player, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CGame* targetGame, CBNET* fromRealm, std::string& fromName, bool& isWhisper, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CGame* targetGame, CIRC* ircNetwork, std::string& channelName, std::string& userName, bool& isWhisper, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CGame* targetGame, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CBNET* fromRealm, std::string& fromName, bool& isWhisper, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CIRC* ircNetwork, std::string& channelName, std::string& userName, bool& isWhisper, std::ostream* outputStream);
  CCommandContext(CAura* nAura, std::ostream* outputStream);

  bool SetIdentity(const std::string& userName, const std::string& realmId);
  void SetAuthenticated(const bool& nAuthenticated);
  void SetPermissions(const uint8_t nPermissions);
  void UpdatePermissions();
  std::optional<std::pair<std::string, std::string>> CheckSudo(const std::string& message);

  void SendReply(const std::string& message);
  void SendReply(const std::string& message, const uint8_t ccFlags);
  void ErrorReply(const std::string& message);
  void SendAll(const std::string& message);
  void ErrorAll(const std::string& message);
  CGamePlayer* GetTargetPlayer(const std::string& target);
  CGamePlayer* GetTargetPlayerOrSelf(const std::string& target);
  void UseImplicitHostedGame();
  void Run(const std::string& command, const std::string& payload);

  ~CCommandContext();
};

#endif
