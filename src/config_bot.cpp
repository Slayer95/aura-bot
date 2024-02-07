#ifndef AURA_CONFIG_BOT_H_
#define AURA_CONFIG_BOT_

#include "config_bot.h"
#include "util.h"

#include <utility>
#include <algorithm>
#include <fstream>

using namespace std;

//
// CBotConfig
//

CBotConfig::CBotConfig(CConfig* CFG)
{
  m_Enabled                = CFG->GetBool("bot_enabled", true);
  m_ProxyReconnectEnabled  = CFG->GetBool("bot_enablegproxy", true);
  m_War3Version            = CFG->GetInt("bot_war3version", 27);
  m_Warcraft3Path          = CFG->GetPath("bot_war3path", filesystem::path(R"(C:\Program Files\Warcraft III\)"));
  m_MapCFGPath             = CFG->GetPath("bot_mapcfgpath", filesystem::path());
  m_MapPath                = CFG->GetPath("bot_mappath", filesystem::path());

  m_BindAddress            = CFG->GetString("bot_bindaddress", string());
  m_MinHostPort            = CFG->GetInt("bot_minhostport", CFG->GetInt("bot_hostport", 6112));
  m_MaxHostPort            = CFG->GetInt("bot_maxhostport", m_MinHostPort);
  m_EnableLANBalancer      = CFG->GetBool("bot_enablelanbalancer", false);
  m_LANHostPort            = CFG->GetInt("bot_lanhostport", 6112);

  /* Make absolute, lexically normal */
  m_GreetingPath           = CFG->GetPath("bot_greetingpath", filesystem::path());
  if (!m_GreetingPath.empty()) {
    ifstream in;
    in.open(m_GreetingPath.string(), ios::in);
    if (!in.fail()) {
      while (!in.eof()) {
        string Line;
        getline(in, Line);
        if (Line.empty()) {
          if (!in.eof())
            m_Greeting.push_back(" ");
        } else {
          m_Greeting.push_back(Line);
        }
      }
      in.close( );
    }
  }

  m_UDPInfoStrictMode      = CFG->GetBool("udp_infostrictmode", true);
  m_UDPForwardTraffic      = CFG->GetBool("udp_redirenabled", false);
  m_UDPForwardAddress      = CFG->GetString("udp_rediraddress", string());
  m_UDPForwardPort         = CFG->GetInt("udp_redirport", 6110);
  m_UDPForwardGameLists    = CFG->GetBool("udp_redirgamelists", false);
  m_UDPBlockedIPs          = CFG->GetSet("udp_blocklist", ',', {});
  m_UDPSupportGameRanger   = CFG->GetBool("udp_enablegameranger", false);
  m_UDPGameRangerAddress   = CFG->GetIPv4("udp_gamerangeraddress", {255, 255, 255, 255});
  m_UDPGameRangerPort      = CFG->GetInt("udp_gamerangerport_but_its_hardcoded", 6112);

  m_AllowDownloads         = CFG->GetBool("bot_allowdownloads", false);
  m_AllowUploads           = CFG->GetInt("bot_allowuploads", 0); // no|yes|conditional
  m_MaxDownloaders         = CFG->GetInt("bot_maxdownloaders", 3);
  m_MaxUploadSize          = CFG->GetInt("bot_maxuploadsize", 8192);
  m_MaxUploadSpeed         = CFG->GetInt("bot_maxuploadspeed", 1024);
  m_MaxParallelMapPackets  = CFG->GetInt("bot_maxparallelmappackets", 1000);
  m_RTTPings               = CFG->GetBool("bot_rttpings", false);
  m_HasBufferBloat         = CFG->GetBool("bot_hasbufferbloat", false);
  m_ReconnectWaitTime      = CFG->GetInt("bot_reconnectwaittime", 3);

  m_MinHostCounter         = CFG->GetInt("bot_firstgameid", 100);
  m_MaxGames               = CFG->GetInt("bot_maxgames", 20);
  m_MaxSavedMapSize        = CFG->GetInt("bot_maxpersistentsize", 0xFFFFFFFF);

  m_StrictPaths            = CFG->GetBool("bot_mapstrictpaths", false);
  m_EnableCFGCache         = CFG->GetBool("bot_mapenablecache", true);
  m_ExitOnStandby          = CFG->GetBool("bot_exitonstandby", false);

  // Master switch mainly intended for CLI. CFG key provided for completeness.
  m_EnableBNET             = CFG->GetMaybeBool("bot_enablebnet");
}

CBotConfig::~CBotConfig() = default;
#endif
