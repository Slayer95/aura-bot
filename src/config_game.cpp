#include "config_game.h"
#include "util.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CGameConfig
//

CGameConfig::CGameConfig(CConfig* CFG)
{
  m_VoteKickPercentage        = CFG->GetInt("bot_votekickpercentage", 70);
  m_NumPlayersToStartGameOver = CFG->GetInt("bot_gameoverplayernumber", 1);
  m_SyncLimit                 = CFG->GetFloat("bot_synclimit", 10.0f);
  m_SyncLimitSafe             = CFG->GetFloat("bot_synclimitsafe", 3.0f);
  m_SyncFactor                = CFG->GetFloat("bot_syncfactor", 1.0f);
  m_AutoKickPing              = CFG->GetInt("bot_autokickping", 300);
  m_WarnHighPing              = CFG->GetInt("bot_warnhighping", 200);
  m_LobbyTimeLimit            = CFG->GetInt("bot_lobbytimelimit", 10);
  m_LobbyNoOwnerTime          = CFG->GetInt("bot_lobbyownerlesstime", 2);
  m_Latency                   = static_cast<uint16_t>(CFG->GetInt("bot_latency", 100));
  m_PerfThreshold             = CFG->GetInt("bot_perflimit", 150);
  m_LacksMapKickDelay         = CFG->GetInt("bot_nomapkickdelay", 60);

  string BotCommandTrigger    = CFG->GetString("bot_commandtrigger", "!");
  m_CommandTrigger            = BotCommandTrigger[0];
  m_IndexVirtualHostName      = CFG->GetString("bot_indexvirtualhostname", 1, 15, "Aura Bot");
  m_LobbyVirtualHostName      = CFG->GetString("bot_lobbyvirtualhostname", 1, 15, "|cFF4080C0Aura");

  m_NotifyJoins               = CFG->GetBool("bot_notifyjoins", false);
  m_IgnoredNotifyJoinPlayers  = CFG->GetSet("bot_notifyjoinsexcept", ',', {});
  m_LANEnabled                = CFG->GetBool("bot_enablelan", true);

  if (m_VoteKickPercentage > 100)
    m_VoteKickPercentage = 100;

  if (m_SyncFactor < 0.1f)
    m_SyncFactor = 1.0f;
}

CGameConfig::~CGameConfig() = default;
