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

#include "config_discord.h"
#include "discord.h"
#include "command.h"
#include "aura.h"
#include "socket.h"
#include "util.h"
#include "bnetprotocol.h"
#include "realm.h"
#include "net.h"

#include <utility>
#include <algorithm>
#include <variant>

class CDiscordConfig;

using namespace std;

//////////////////
//// CDiscord ////
//////////////////

CDiscord::CDiscord(CAura* nAura)
  : m_Aura(nAura),
    m_Client(nullptr),
    m_Config(nullptr),
    m_NickName(string()),
    m_LastPacketTime(GetTime()),
    m_LastAntiIdleTime(GetTime()),
    m_Exiting(false),
    m_WaitingToConnect(true)
{
}

CDiscord::~CDiscord()
{
  delete m_Config;

  for (auto& ctx : m_Aura->m_ActiveContexts) {
    if (ctx->m_DiscordAPI) {
      ctx->m_DiscordAPI = nullptr;
      ctx->SetPartiallyDestroyed();
    }
  }
}

bool CDiscord::Init()
{
#ifndef DISABLE_DPP
  m_Client = new dpp::cluster(m_Config->m_Token);
  m_Client->on_log(dpp::utility::cout_logger());
 
  m_Client->on_slashcommand([this](const dpp::slashcommand_t& event) {
    event.thinking(false); // Publicly visible
    m_CommandQueue.push(new dpp::slashcommand_t(event));
  });

  m_Client->on_ready([this](const dpp::ready_t& event) {
    if (dpp::run_once<struct register_bot_commands>()) {
      dpp::slashcommand nameSpace("aura", "Run any of Aura's commands.", m_Client->me.id);
      nameSpace.add_option(
        dpp::command_option(dpp::co_string, "command", "The command to be executed.", true)
      );
      nameSpace.add_option(
        dpp::command_option(dpp::co_string, "payload", "Any comma-separated parameters for the command.", false)
      );

      dpp::slashcommand hostShortcut("host", "Let Aura host a Warcraft 3 game.", m_Client->me.id);
      hostShortcut.add_option(
        dpp::command_option(dpp::co_string, "map", "Map to be hosted.", true)
      );
      hostShortcut.add_option(
        dpp::command_option(dpp::co_string, "title", "Display title for the hosted game lobby.", true)
      );
      m_Client->global_command_create(hostShortcut);
      m_Client->global_bulk_command_create({hostShortcut, nameSpace});
    }
  });

  m_Client->start(dpp::st_return);  
#endif

  return true;
}

bool CDiscord::Update()
{
  if (m_Config->m_Enabled == (m_Client == nullptr)) {
    if (m_Config->m_Enabled) {
      if (!Init()){
        return m_Exiting;
      }
    } else {
      delete m_Client;
      return m_Exiting;
    }
  }

  if (!m_Config->m_Enabled) {
    return m_Exiting;
  }

#ifndef DISABLE_DPP
  while (!m_CommandQueue.empty()) {
    string cmdToken, command, payload;
    cmdToken = "/aura ";
    dpp::slashcommand_t* event = m_CommandQueue.front();
    if (event->command.get_command_name() == "aura") {
      dpp::command_value maybePayload = event->get_parameter("payload");
      if (holds_alternative<string>(maybePayload)) {
        payload = get<string>(maybePayload);
      }
      command = get<string>(event->get_parameter("command"));
      event->edit_original_response(dpp::message("Command queued!"));
    } else if (event->command.get_command_name() == "host") {
      string mapName = get<string>(event->get_parameter("map"));
      string gameName = get<string>(event->get_parameter("title"));
      command = "host";
      payload = mapName + ", " + gameName;
      event->edit_original_response(dpp::message("Hosting your game briefly!"));
    }
    CCommandContext* ctx = new CCommandContext(m_Aura, m_Aura->m_CommandDefaultConfig, event, &std::cout);
    ctx->Run(cmdToken, command, payload);
    m_Aura->UnholdContext(ctx);
    m_CommandQueue.pop();
  }
#endif
  return m_Exiting;
}

void CDiscord::SendUser(const string& message, const uint64_t target)
{
  m_Client->direct_message_create(target, dpp::message(message), [this](const dpp::confirmation_callback_t& callback){
    if (callback.is_error()) {
      if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
        Print("[DISCORD] Failed to send direct message.");
      }
      return;
    } else {
      if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
        Print("[DISCORD] Direct message sent OK.");
      }
    }
  });
}

bool CDiscord::GetIsModerator(const string& nHostName)
{
  return m_Config->m_Admins.find(nHostName) != m_Config->m_Admins.end();
}

bool CDiscord::GetIsSudoer(const uint64_t nIdentifier)
{
  return m_Config->m_SudoUsers.find(nIdentifier) != m_Config->m_SudoUsers.end();
}
