/*

  Copyright [2024-2025] [Leonardo Julca]

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

#include "auradb.h"
#include "cli.h"
#include "config/config_bot.h"
#include "config/config_game.h"
#include "config/config_net.h"
#include "net.h"
#include "file_util.h"
#include "os_util.h"
#include "protocol/game_protocol.h"
#include "game_setup.h"
#include "realm.h"
#include "optional.h"
#include "util.h"

#include "aura.h"

using namespace std;

//
// CCLI
//

CCLI::CCLI()
 : m_UseStandardPaths(false),
   m_InfoAction(CLIAction::kNone),
   m_Verbose(false),
   m_ExecAuth(CommandAuth::kAuto),
   m_ExecBroadcast(false),
   m_ExecOnline(true),
   m_RunTests(false)
{
}

CCLI::~CCLI() = default;

/*
CLI::Validator CCLI::GetIsFullyQualifiedUserValidator()
{
  return CLI::Validator(
    [](string &input) -> string {
      if (input.find('@') == string::npos) {
        return "Username must contain '@' to specify realm (trailing @ means no realm)";
      }
      return string{}; // Return an empty string for valid input
    },
    "Username must contain '@' to specify realm (trailing @ means no realm)",
    "IsFullyQualifiedUser"
  );
}
*/

CLIResult CCLI::Parse(const int argc, char** argv)
{
  CLI::App app{AURA_APP_NAME};
  //CLI::Validator IsFullyQualifiedUser = GetIsFullyQualifiedUserValidator();
  argv = app.ensure_utf8(argv);

  bool examples = false;
  bool about = false;
  optional<string> rawGameVersion;
  optional<string> rawWar3DataVersion;

  app.option_defaults()->ignore_case();

  app.add_option("MAPFILE", m_SearchTarget, 
    "Map, config or URI from which to host a game from the CLI. File names resolve from maps directory, unless --stdpaths."
  );
  app.add_option("GAMENAME", m_GameName, 
    "Name assigned to a game hosted from the CLI."
  );

  app.add_flag("--stdpaths", m_UseStandardPaths, 
    "Makes file names always resolve from CWD when input through the CLI. Commutative."
  );
#ifdef _WIN32
  app.add_flag("--init-system,--no-init-system{false}", m_InitSystem, 
    "Adds Aura to the PATH environment variable, as well as to Windows Explorer context menu."
  );
#else
  app.add_flag("--init-system,--no-init-system{false}", m_InitSystem,
    "Adds Aura to the PATH environment variable."
  );
#endif

  app.add_flag("--auto-port-forward,--no-auto-port-forward{false}", m_EnableUPnP, 
    "Enable automatic port-forwarding, using Universal Plug-and-Play."
#ifdef DISABLE_MINIUPNP
    " (This distribution of Aura does not support this feature.)"
#endif
  );
  app.add_flag("--lan,--no-lan{false}", m_LAN, 
    "Show hosted games on Local Area Network."
  );
  app.add_flag("--bnet,--no-bnet{false}", m_BNET, 
    "Switch to enable or disable every defined realm."
  );
  app.add_flag("--irc,--no-irc{false}", m_IRC, 
    "Switch to enable or disable IRC integration."
  );
  app.add_flag("--discord,--no-discord{false}", m_Discord, 
    "Switch to enable or disable Discord integration."
  );
  app.add_flag("--exit,--no-exit{false}", m_ExitOnStandby, 
    "Terminates the process when idle."
  );
  app.add_flag("--cache,--no-cache{false}", m_UseMapCFGCache, 
    "Caches loaded map files into the map configs folder."
  );

  app.add_flag("--cache-revalidate", m_MapCFGCacheRevalidation, 
    "Customizes the judgment of when to validate the cached map configs against their base maps."
  )->transform(
    CLI::CheckedTransformer(map<string, CacheRevalidationMethod>{
      {"never", CacheRevalidationMethod::kNever},
      {"always", CacheRevalidationMethod::kAlways},
      {"modified", CacheRevalidationMethod::kModified}
    })
  );

  app.add_flag("--about,--version", about,
    "Display software information."
  );
  app.add_flag("--example,--examples", examples,
    "Display CLI hosting examples."
  );
  app.add_flag("--verbose", m_Verbose, 
    "Outputs detailed information when running CLI actions."
  );
  app.add_flag("--check-jass,--no-check-jass{false}", m_CheckJASS, 
    "Checks map scripts for correctness."
 );
  app.add_flag("--extract-jass,--no-extract-jass{false}", m_ExtractJASS, 
    "Automatically extract files from the game install directory."
  );

#ifdef _WIN32
  app.add_option("--homedir", m_HomePath, 
    "Customizes Aura home dir (%AURA_HOME%)."
  );
#else
  app.add_option("--homedir", m_HomePath, 
    "Customizes Aura home dir ($AURA_HOME)."
  );
#endif
  app.add_option("--config", m_CFGPath, 
    "Customizes the main Aura config file. File names resolve from home dir, unless --stdpaths."
  );
  app.add_option("--config-adapter", m_CFGAdapterPath, 
    "Customizes an adapter file for migrating legacy Aura config files. File names resolve from home dir, unless --stdpaths."
  );
  app.add_option("--w3dir", m_War3Path, 
    "Customizes the game install directory."
  );
  app.add_option("--mapdir", m_MapPath, 
    "Customizes the maps directory."
  );
  app.add_option("--cfgdir", m_MapCFGPath, 
    "Customizes the map configs directory."
  );
  app.add_option("--cachedir", m_MapCachePath, 
    "Customizes the map cache directory."
  );
  app.add_option("--jassdir", m_JASSPath, 
    "Customizes the directory where extracted JASS files are stored."
  );
  app.add_option("--savedir", m_GameSavePath, 
    "Customizes the game save directory."
  );
  app.add_option("--data-version", rawWar3DataVersion, 
    "Customizes the game version to be used when reading the game install directory."
  );

  app.add_option("-s,--search-type", m_SearchType,
    "Restricts file searches when hosting from the CLI. "
    "Values: map, config, local, any"
  )->transform(
    CLI::CheckedTransformer(map<string, uint8_t>{
      {"map", SEARCH_TYPE_ONLY_MAP},
      {"config", SEARCH_TYPE_ONLY_CONFIG},
      {"local", SEARCH_TYPE_ONLY_FILE},
      {"any", SEARCH_TYPE_ANY}
    })
  );

  app.add_option("--bind-address", m_BindAddress,
    "Restricts connections to the game server, only allowing the input IPv4 address."
  )->check(CLI::ValidIPV4);
  app.add_option("--host-port", m_HostPort,
    "Customizes the game server to only listen in the specified port."
  );
  app.add_option("--udp-lan-mode", m_UDPDiscoveryMode,
    "Customizes the behavior of the game discovery service. "
    "Values: strict, lax, free."
  )->transform(
    CLI::CheckedTransformer(map<string, UDPDiscoveryMode>{
      {"strict", UDPDiscoveryMode::kStrict},
      {"lax", UDPDiscoveryMode::kLax},
      {"free", UDPDiscoveryMode::kFree}
    })
  );

#ifdef DEBUG
  app.add_option("--log-level", m_LogLevel,
    "Customizes how detailed Aura's output should be. "
    "Values: notice, info, debug, trace, trace2, trace3."
  )->transform(
    CLI::CheckedTransformer(map<string, LogLevel>{
      {"emergency", LogLevel::kEmergency},
      {"alert", LogLevel::kAlert},
      {"critical", LogLevel::kCritical},
      {"error", LogLevel::kError},
      {"warning", LogLevel::kWarning},
      {"notice", LogLevel::kNotice},
      {"info", LogLevel::kInfo},
      {"debug", LogLevel::kDebug},
      {"trace", LogLevel::kTrace},
      {"trace2", LogLevel::kTrace2},
      {"trace3", LogLevel::kTrace3},
    })
  );
#else
  app.add_option("--log-level", m_LogLevel,
  "Customizes how detailed Aura's output should be. "
  "Values: notice, info, debug."
  )->transform(
    CLI::CheckedTransformer(map<string, LogLevel>{
      {"emergency", LogLevel::kEmergency},
      {"alert", LogLevel::kAlert},
      {"critical", LogLevel::kCritical},
      {"error", LogLevel::kError},
      {"warning", LogLevel::kWarning},
      {"notice", LogLevel::kNotice},
      {"info", LogLevel::kInfo},
      {"debug", LogLevel::kDebug}
    })
  );
#endif

  // Game hosting
  app.add_option("--owner", m_GameOwner,
    "Customizes the game owner when hosting from the CLI."
  )/*->check(IsFullyQualifiedUser)*/;
  app.add_flag("--no-owner", m_GameOwnerLess,
    "Disables the game owner feature when hosting from the CLI."
  );
  app.add_flag("--lock-teams,--no-lock-teams{false}", m_GameTeamsLocked,
    "Toggles 'Lock Teams' setting when hosting from the CLI."
  );
  app.add_flag("--teams-together,--no-teams-together{false}", m_GameTeamsTogether,
    "Toggles 'Teams Together' setting when hosting from the CLI."
  );
  app.add_flag("--share-advanced,--no-share-advanced{false}", m_GameAdvancedSharedUnitControl,
    "Toggles 'Advanced Shared Unit Control' setting when hosting from the CLI."
  );
  app.add_flag("--random-races,--no-random-races{false}", m_GameRandomRaces,
    "Toggles 'Random Races' setting when hosting from the CLI."
  );
  app.add_flag("--random-heroes,--no-random-heroes{false}", m_GameRandomHeroes,
    "Toggles 'Random Heroes' when hosting from the CLI."
  );

  app.add_option("--observers", m_GameObservers,
    "Customizes observers when hosting from the CLI. "
    "Values: none, referees, on-defeat, full"
  )->transform(
    CLI::CheckedTransformer(map<string, GameObserversMode>{
      {"none", GameObserversMode::kNone},
      {"on-defeat", GameObserversMode::kOnDefeat},
      {"full", GameObserversMode::kStartOrOnDefeat},
      {"referees", GameObserversMode::kReferees}
    })
  );

  app.add_option("--visibility", m_GameVisibility,
    "Customizes in-game visibility and fog-of-war when hosting from the CLI. "
    "Values: default, hide, explored, visible"
  )->transform(
    CLI::CheckedTransformer(map<string, GameVisibilityMode>{
      {"default", GameVisibilityMode::kDefault},
      {"hide", GameVisibilityMode::kHideTerrain},
      {"explored", GameVisibilityMode::kExplored},
      {"visible", GameVisibilityMode::kAlwaysVisible}
    })
  );

  app.add_option("--speed", m_GameSpeed,
    "Customizes game speed when hosting from the CLI. "
    "Values: slow, normal, fast"
  )->transform(
    CLI::CheckedTransformer(map<string, GameSpeed>{
      {"slow", GameSpeed::kSlow},
      {"normal", GameSpeed::kNormal},
      {"fast", GameSpeed::kFast}
    })
  );

  app.add_option("--realm-visibility", m_GameDisplayMode,
    "Customizes whether the game is displayed in any realms. "
    "Values: public, private, none"
  )->transform(
    CLI::CheckedTransformer(map<string, uint8_t>{
      {"public", GAME_DISPLAY_PUBLIC},
      {"private", GAME_DISPLAY_PRIVATE},
      {"none", GAME_DISPLAY_NONE}
    })
  );

  app.add_option("--on-ipflood", m_GameIPFloodHandler,
    "Customizes how to deal with excessive game connections from the same IP."
    "Values: none, notify, deny"
  )->transform(
    CLI::CheckedTransformer(map<string, OnIPFloodHandler>{
      {"none", OnIPFloodHandler::kNone},
      {"notify", OnIPFloodHandler::kNotify},
      {"deny", OnIPFloodHandler::kDeny}
    })
  );

  app.add_option("--on-leave", m_GameLeaverHandler, 
    "Customizes how to deal with leaver players. "
    "Values: none, native, share"
  )->transform(
    CLI::CheckedTransformer(map<string, OnPlayerLeaveHandler>{
      {"none", OnPlayerLeaveHandler::kNone},
      {"native", OnPlayerLeaveHandler::kNative},
      {"share", OnPlayerLeaveHandler::kShareUnits}
    })
  );

  app.add_option("--on-share-units", m_GameShareUnitsHandler,
    "Customizes how to deal with attempts to share control of units with teammates. "
    "Values: native, kick, restrict"
  )->transform(
    CLI::CheckedTransformer(map<string, OnShareUnitsHandler>{
      {"native", OnShareUnitsHandler::kNative},
      {"kick", OnShareUnitsHandler::kKickSharer},
      {"restrict", OnShareUnitsHandler::kRestrictSharee}
    })
  );

  app.add_option("--on-unsafe-name", m_GameUnsafeNameHandler,
    "Customizes how to deal with users that try to join with confusing, or otherwise problematic names. "
    "Values: none, censor, deny"
  )->transform(
    CLI::CheckedTransformer(map<string, OnUnsafeNameHandler>{
      {"none", OnUnsafeNameHandler::kNone},
      {"censor", OnUnsafeNameHandler::kCensorMayDesync},
      {"deny", OnUnsafeNameHandler::kDeny}
    })
  );

  app.add_option("--on-broadcast-error", m_GameBroadcastErrorHandler,
    "Customizes the judgment of when to close a game that couldn't be announced in a realm. "
    "Values: ignore, exit-main-error, exit-empty-main-error, exit-any-error, exit-empty-any-error, exit-max-errors"
  )->transform(
    CLI::CheckedTransformer(map<string, OnRealmBroadcastErrorHandler>{
      {"ignore", OnRealmBroadcastErrorHandler::kIgnoreErrors},
      {"exit-main-error", OnRealmBroadcastErrorHandler::kExitOnMainError},
      {"exit-empty-main-error", OnRealmBroadcastErrorHandler::kExitOnMainErrorIfEmpty},
      {"exit-any-error", OnRealmBroadcastErrorHandler::kExitOnAnyError},
      {"exit-empty-any-error", OnRealmBroadcastErrorHandler::kExitOnAnyErrorIfEmpty},
      {"exit-max-errors", OnRealmBroadcastErrorHandler::kExitOnMaxErrors}
    })
  );

  app.add_option("--game-version", rawGameVersion,
    "Customizes the main version for the hosted lobby."
  );
  
  app.add_flag(  "--tft,--roc{false}", m_GameIsExpansion,
    "Customizes whether the hosted lobby will target Reign of Chaos or Frozen Throne clients."
  );

  app.add_option("--game-locale-mod", m_GameLocaleMod,
    "Customizes the targetted locale for the hosted lobby, for Frozen Throne v1.30+ or Reforged clients. "
    "This option uses Reforged-oriented localization. "
    "Values: enUS, deDE, esES, esMX, frFR, itIT, koKR, plPL, ptBR, ruRU, zhCN, zhTW"
  )->transform(
    CLI::CheckedTransformer(map<string, W3ModLocale>{
      {"enUS", W3ModLocale::kENUS},
      {"deDE", W3ModLocale::kDEDE},
      {"esES", W3ModLocale::kESES},
      {"esMX", W3ModLocale::kESMX},
      {"frFR", W3ModLocale::kFRFR},
      {"itIT", W3ModLocale::kITIT},
      {"koKR", W3ModLocale::kKOKR},
      {"plPL", W3ModLocale::kPLPL},
      {"ptBR", W3ModLocale::kPTBR},
      {"ruRU", W3ModLocale::kRURU},
      {"zhCN", W3ModLocale::kZHCN},
      {"zhTW", W3ModLocale::kZHTW}
    })
  );

  app.add_option("--game-locale-lang-id", m_GameLocaleLangID, 
    "Customizes the targetted locale for game clients. This option uses MPQ-based localization."
  );
  app.add_option("--crossplay", m_GameCrossPlayMode, 
    "Customizes the crossplay capabilities of the hosted lobby. "
    "Values: none, conservative, optimistic, force"
  )->transform(
    CLI::CheckedTransformer(map<string, CrossPlayMode>{
      {"none", CrossPlayMode::kNone},
      {"conservative", CrossPlayMode::kConservative},
      {"optimistic", CrossPlayMode::kOptimistic},
      {"force", CrossPlayMode::kForce}
    })
  );

  app.add_option("--alias", m_GameMapAlias, 
    "Registers an alias for the map used when hosting from the CLI."
  );
  app.add_option("--mirror", m_GameMirrorSource, 
    "Mirrors a game, listing it in the connected realms. Syntax: IP:PORT#ID."
  );
  app.add_option("--exclude", m_ExcludedRealms, 
    "Hides the game in the listed realm(s). Repeatable."
  );
  app.add_flag(  "--proxy", m_GameMirrorProxy, 
    "Proxies LAN connections towards the mirrored game."
  );

  app.add_option("--lobby-timeout-mode", m_GameLobbyTimeoutMode, 
    "Customizes under which circumstances should a game lobby timeout. "
    "Values: never, empty, ownerless, strict"
  )->transform(
    CLI::CheckedTransformer(map<string, LobbyTimeoutMode>{
      {"never", LobbyTimeoutMode::kNever},
      {"empty", LobbyTimeoutMode::kEmpty},
      {"no-owner", LobbyTimeoutMode::kOwnerMissing},
      {"strict", LobbyTimeoutMode::kStrict}
    })
  );

  app.add_option("--lobby-owner-timeout-mode", m_GameLobbyOwnerTimeoutMode, 
    "Customizes under which circumstances should game ownership expire. "
    "Values: never, absent, strict"
  )->transform(
    CLI::CheckedTransformer(map<string, LobbyOwnerTimeoutMode>{
      {"never", LobbyOwnerTimeoutMode::kNever},
      {"absent", LobbyOwnerTimeoutMode::kAbsent},
      {"strict", LobbyOwnerTimeoutMode::kStrict}
    })
  );

  app.add_option("--loading-timeout-mode", m_GameLoadingTimeoutMode, 
    "Customizes under which circumstances should players taking too long to load the game be kicked. "
    "Values: never, strict"
  )->transform(
    CLI::CheckedTransformer(map<string, GameLoadingTimeoutMode>{
      {"never", GameLoadingTimeoutMode::kNever},
      {"strict", GameLoadingTimeoutMode::kStrict}
    })
  );

  app.add_option("--playing-timeout-mode", m_GamePlayingTimeoutMode, 
    "Customizes under which circumstances should a started game expire. "
    "Values: never, dry, strict"
  )->transform(
    CLI::CheckedTransformer(map<string, GamePlayingTimeoutMode>{
      {"never", GamePlayingTimeoutMode::kNever},
      {"dry", GamePlayingTimeoutMode::kDry},
      {"strict", GamePlayingTimeoutMode::kStrict}
    })
  );

  app.add_option("--lobby-timeout", m_GameLobbyTimeout,
    "Sets the time limit for the game lobby (seconds.)"
  );
  app.add_option("--lobby-owner-timeout", m_GameLobbyOwnerTimeout,
    "Sets the time limit for an absent game owner to keep their power (seconds.)"
  );
  app.add_option("--loading-timeout", m_GameLoadingTimeout,
    "Sets the time limit for players to load a started game (seconds.)"
  );
  app.add_option("--playing-timeout", m_GamePlayingTimeout,
    "Sets the time limit for a started game (seconds.)"
  );

  app.add_option("--playing-timeout-warning-short-interval", m_GamePlayingTimeoutWarningShortInterval,
    "Sets the interval for the latest and most often game timeout warnings to be displayed."
  );
  app.add_option("--playing-timeout-warning-short-ticks", m_GamePlayingTimeoutWarningShortCountDown,
    "Sets the amount of ticks for the latest and most often game timeout warnings to be displayed."
 );
  app.add_option("--playing-timeout-warning-large-interval", m_GamePlayingTimeoutWarningLargeInterval,
    "Sets the interval for the earliest and rarest game timeout warnings to be displayed."
  );
  app.add_option("--playing-timeout-warning-large-ticks", m_GamePlayingTimeoutWarningLargeCountDown,
    "Sets the amount of ticks for the earliest and rarest timeout warnings to be displayed."
  );

  app.add_flag(  "--fast-expire-lan-owner,--no-fast-expire-lan-owner{false}", m_GameLobbyOwnerReleaseLANLeaver,
    "Allows to unsafely turn off the feature that removes game owners as soon as they leave a game lobby they joined from LAN.");

  app.add_option("--start-countdown-interval", m_GameLobbyCountDownInterval,
    "Sets the interval for the game start countdown to tick down.");
  app.add_option("--start-countdown-ticks", m_GameLobbyCountDownStartValue,
    "Sets the amount of ticks for the game start countdown.");

  app.add_option("--download-timeout", m_GameMapDownloadTimeout,
    "Sets the time limit for the map download (seconds.)");

  app.add_option("--players-ready", m_GamePlayersReadyMode,
    "Customizes when Aura will consider a player to be ready to start the game. "
    "Values: fast, race, explicit."
  )->transform(
    CLI::CheckedTransformer(map<string, PlayersReadyMode>{
      {"fast", PlayersReadyMode::kFast},
      {"race", PlayersReadyMode::kExpectRace},
      {"explicit", PlayersReadyMode::kExplicit}
    })
  );

  app.add_option("--auto-start-players", m_GameAutoStartPlayers,
    "Sets an amount of occupied slots for automatically starting the game."
  );
  app.add_option("--auto-start-time", m_GameAutoStartSeconds, 
    "Sets a time that should pass before automatically starting the game (seconds.)"
  );
  app.add_flag(  "--auto-start-balanced,--no-auto-start-balanced{false}", m_GameAutoStartRequiresBalance,
    "Whether to require balanced teams before automatically starting the game."
  );
  app.add_option("--auto-end-players", m_GameNumPlayersToStartGameOver,
    "Sets a low amount of players required for Aura to stop hosting a game."
  );
  app.add_option("--lobby-auto-kick-ping", m_GameAutoKickPing,
    "Customizes the maximum allowed ping in a game lobby."
  );
  app.add_option("--lobby-high-ping", m_GameWarnHighPing,
    "Customizes the ping at which Aura will issue high-ping warnings."
  );
  app.add_option("--lobby-safe-ping", m_GameSafeHighPing,
    "Customizes the ping required for Aura to consider a high-ping player as acceptable again."
  );
  app.add_flag(  "--lag-screen,--no-lag-screen{false}", m_GameEnableLagScreen,
    "Customizes whether to pause the game when players lag behind."
  );
  app.add_option("--latency", m_GameLatencyAverage,
    "Sets the refresh period for the game as a ping equalizer, in milliseconds."
  );
  app.add_option("--latency-max-frames", m_GameLatencyMaxFrames,
    "Sets a maximum amount of frames clients may fall behind. When exceeded, the lag screen shows up."
  );
  app.add_option("--latency-safe-frames", m_GameLatencySafeFrames,
    "Sets a frame difference clients must catch up to in order for the lag screen to go away."
  );
  app.add_flag(  "--latency-equalizer,--no-latency-equalizer{false}", m_GameLatencyEqualizerEnabled,
    "Enables a minimum delay for all actions sent by game players."
  );
  app.add_option("--latency-equalizer-frames", m_GameLatencyEqualizerFrames,
    "Sets the amount of frames to be used by the latency equalizer."
  );
  app.add_flag(  "--latency-normalize,--no-latency-normalize{false}", m_GameSyncNormalize,
    "Whether Aura tries to automatically fix some game-start lag issues."
  );
  app.add_option("--max-apm", m_GameMaxAPM,
    "Limits the actions each player may perform per minute (APM)."
  );
  app.add_option("--max-burst-apm", m_GameMaxBurstAPM,
    "Limits the actions each player may perform per minute (APM)."
  );

  app.add_option("--reconnection", m_GameReconnectionMode,
    "Customizes GProxy support for the hosted game. Values: disabled, basic, extended."
  )->transform(
    CLI::CheckedTransformer(map<string, uint8_t>{
      {"disabled", RECONNECT_DISABLED},
      {"basic", RECONNECT_ENABLED_GPROXY_BASIC},
      {"extended", RECONNECT_ENABLED_GPROXY_ALL}
    })
  );

  app.add_flag(  "--lobby-chat,--no-lobby-chat{false}", m_GameEnableLobbyChat,
    "Whether to allow players to engage in chat in the hosted lobby."
  );
  app.add_flag(  "--in-game-chat,--no-in-game-chat{false}", m_GameEnableInGameChat,
    "Whether to allow players to engage in chat in the hosted game."
  );
  app.add_option("--load", m_GameSavedPath,
    "Sets the saved game .w3z file path for the game lobby."
  );
  app.add_option("--reserve", m_GameReservations,
    "Adds a player to the reserved list of the game lobby."
  );
  app.add_flag(  "--check-joinable,--no-check-joinable{false}", m_GameCheckJoinable,
    "Reports whether the game is joinable over the Internet."
  );
  app.add_flag(  "--notify-joins,--no-notify-joins{false}", m_GameNotifyJoins,
    "Reports whether the game is joinable over the Internet."
  );
  app.add_flag(  "--check-reservation,--no-check-reservation{false}", m_GameCheckReservation,
    "Enforces only players in the reserved list be able to join the game."
  );
  app.add_option("--hcl", m_GameHCL,
    "Customizes a hosted game using the HCL standard."
  );
  app.add_flag(  "--ffa", m_GameFreeForAll,
    "Sets free-for-all game mode - every player is automatically assigned to a different team."
  );
  app.add_flag(  "--allow-save,--no-allow-save{false}", m_GameSaveAllowed,
    "Customizes whether saving the game is allowed."
  );

  app.add_option("--winners-source", m_GameResultSource,
    "Customizes how Aura will determine the winner(s) of the hosted lobby. Values: none, only-leave-code, only-mmd, prefer-leave-code, prefer-mmd"
  )->transform(
    CLI::CheckedTransformer(map<string, GameResultSourceSelect>{
      {"none", GameResultSourceSelect::kNone},
      {"only-leave-code", GameResultSourceSelect::kOnlyLeaveCode},
      {"only-mmd", GameResultSourceSelect::kOnlyMMD},
      {"prefer-leave-code", GameResultSourceSelect::kPreferLeaveCode},
      {"prefer-mmd", GameResultSourceSelect::kPreferMMD}
    })
  );

  app.add_option("--hide-ign-started", m_GameHideLoadedNames,
    "Whether to hide player names in various outputs (e.g. commands) after the game starts. Values: never, host, always, auto"
  )->transform(
    CLI::CheckedTransformer(map<string, HideIGNMode>{
      {"never", HideIGNMode::kNever},
      {"host", HideIGNMode::kHost},
      {"always", HideIGNMode::kAlways},
      {"auto", HideIGNMode::kAuto}
    })
  );

  app.add_flag(  "--hide-ign,--no-hide-ign{false}", m_GameHideLobbyNames,
    "Whether to hide player names in a hosted game lobby."
  );
  app.add_flag(  "--load-in-game,--no-load-in-game{false}", m_GameLoadInGame,
    "Whether to allow players chat in the game while waiting for others to finish loading."
  );

  app.add_option("--fake-users-shared-control", m_GameFakeUsersShareUnitsMode,
    "Whether to automatically let fake users share unit control with players. Values: never, auto, team, all"
  )->transform(
    CLI::CheckedTransformer(map<string, FakeUsersShareUnitsMode>{
      {"never", FakeUsersShareUnitsMode::kNever},
      {"auto", FakeUsersShareUnitsMode::kAuto},
      {"team", FakeUsersShareUnitsMode::kTeam},
      {"all", FakeUsersShareUnitsMode::kAll}
    })
  );

  app.add_flag(  "--join-in-progress-observers,--no-join-in-progress-observers{false}", m_GameEnableJoinObserversInProgress,
    "Whether to allow observers to watch the game after it has already started."
  );
  app.add_flag(  "--join-in-progress-players,--no-join-in-progress-players{false}", m_GameEnableJoinPlayersInProgress,
    "Whether to allow players to join the game after it has already started."
  );
  app.add_flag(  "--log-game-commands,--no-log-game-commands{false}", m_GameLogCommands,
    "Whether to log usage of chat triggers in a hosted game lobby."
  );

  app.add_flag(  "--replaceable,--no-replaceable{false}", m_GameLobbyReplaceable,
    "Whether users can use the !host command to replace the lobby."
  );
  app.add_flag(  "--auto-rehost,--no-auto-rehost{false}", m_GameLobbyAutoRehosted,
    "Registers the provided game setup, and rehosts it whenever there is no active lobby."
  );

  // Command execution
  app.add_option("--exec", m_ExecCommands,
    "Runs a command from the CLI. Repeatable."
  );
  app.add_option("--exec-as", m_ExecAs,
    "Customizes the user identity when running commands from the CLI."
  )/*->check(IsFullyQualifiedUser)*/;

  app.add_option("--exec-auth", m_ExecAuth,
    "Customizes the user permissions when running commands from the CLI."
  )->transform(
    CLI::CheckedTransformer(map<string, CommandAuth>{
      {"auto", CommandAuth::kAuto},
      {"spoofed", CommandAuth::kSpoofed},
      {"verified", CommandAuth::kVerified},
      {"admin", CommandAuth::kAdmin},
      {"rootadmin", CommandAuth::kRootAdmin},
      {"sudo", CommandAuth::kSudo}
    })
  );

  app.add_flag(  "--exec-broadcast", m_ExecBroadcast, 
    "Enables broadcasting the command execution to all users in the channel"
  );
  app.add_flag(  "--exec-online,--exec-offline{false}", m_ExecOnline, 
    "Customizes whether the bot should be connected to a service before running a CLI command using an identity from that service."
  );

  // Port-forwarding
  app.add_option("--port-forward-tcp", m_PortForwardTCP,
    "Enable port-forwarding on the given TCP ports. Repeatable."
#ifdef DISABLE_MINIUPNP
    " (This distribution of Aura does not support this feature.)"
#endif
  );
  app.add_option("--port-forward-udp", m_PortForwardUDP,
    "Enable port-forwarding on the given UDP ports. Repeatable."
#ifdef DISABLE_MINIUPNP
    " (This distribution of Aura does not support this feature.)"
#endif
  );

  app.add_flag(  "--test", m_RunTests,
    "Run tests to ensure Aura is working correctly."
  );

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    if (0 == app.exit(e)) {
      return CLIResult::kInfoAndQuit;
    }
    return CLIResult::kError;
  } catch (...) {
    Print("[AURA] CLI unhandled exception");
    return CLIResult::kError;
  }

  if (rawGameVersion.has_value()) {
    optional<Version> gameVersion = ParseGameVersion(rawGameVersion.value());
    if (!gameVersion.has_value()) {
      Print("[AURA] Invalid value for --game-version");
      return CLIResult::kError;
    }
    m_GameVersion.swap(gameVersion);
  }

  if (rawWar3DataVersion.has_value()) {
    optional<Version> war3DataVersion = ParseGameVersion(rawWar3DataVersion.value());
    if (!war3DataVersion.has_value()) {
      Print("[AURA] Invalid value for --game-version");
      return CLIResult::kError;
    }
    m_War3DataVersion.swap(war3DataVersion);
  }

  if (!m_ExecCommands.empty() && !m_ExecAs.has_value()) {
    Print("[AURA] Option --exec-as is required");
    return CLIResult::kError;
  }

  if (about || examples || m_RunTests) {
    if (about) {
      m_InfoAction = CLIAction::kAbout;
    } else if (examples) {
      m_InfoAction = CLIAction::kExamples;
    } else if (m_RunTests) {
      return CLIResult::kTest;
    }
    return CLIResult::kInfoAndQuit;
  }

  // Make sure directories have a trailing slash.
  // But m_CFGPath is just a file.
  if (m_HomePath.has_value()) NormalizeDirectory(m_HomePath.value());
  if (m_War3Path.has_value()) NormalizeDirectory(m_War3Path.value());
  if (m_MapPath.has_value()) NormalizeDirectory(m_MapPath.value());
  if (m_MapCFGPath.has_value()) NormalizeDirectory(m_MapCFGPath.value());
  if (m_MapCachePath.has_value()) NormalizeDirectory(m_MapCachePath.value());
  if (m_JASSPath.has_value()) NormalizeDirectory(m_JASSPath.value());
  if (m_GameSavePath.has_value()) NormalizeDirectory(m_GameSavePath.value());

  if (m_SearchTarget.has_value()) {
    if (!m_ExitOnStandby.has_value()) m_ExitOnStandby = true;
    if (!m_UseMapCFGCache.has_value()) m_UseMapCFGCache = !m_UseStandardPaths;
  }

  if (m_CFGAdapterPath.has_value()) {
    return CLIResult::kConfigAndQuit;
  }

  return CLIResult::kOk;
}

bool CCLI::CheckGameParameters() const
{
  if (m_GameOwnerLess.value_or(false) && m_GameOwner.has_value()) {
    Print("[AURA] Conflicting --owner and --no-owner flags.");
    return false;
  }
  return true;
}

bool CCLI::RunGameLoadParameters(shared_ptr<CGameSetup> gameSetup) const
{
  if (!gameSetup->RestoreFromSaveFile()) {
    Print("[AURA] Invalid save file [" + PathToString(gameSetup->m_SaveFile) + "]");
    return false;
  } else if (m_GameCheckReservation.has_value() && !m_GameCheckReservation.value()) {
    Print("[AURA] Resuming a loaded game must always check reservations.");
    return false;
  } else if (m_GameLobbyAutoRehosted.value_or(false)) {
    // Do not allow automatically rehosting loads of the same savefile,
    // Because that would mean keeping the CSaveGame around.
    // Also, what's this? The Battle for Wesnoth?
    Print("[AURA] A loaded game cannot be auto rehosted.");
    return false;
  }
  return true;
}

void CCLI::RunInfoActions() const
{
  switch (m_InfoAction) {
    case CLIAction::kAbout: {
      Print("Aura " + string(AURA_VERSION));
      Print("Aura is a permissive-licensed open source project.");
      Print("Say hi at <" + string(AURA_ISSUES_URL) + ">");
      break;
    }
    case CLIAction::kExamples: {
      Print("Usage: aura [MAP NAME] [GAME NAME]");
      Print(R"(Example: aura monolith "creep invasion")");
      Print(R"(Example: aura "lost temple" "2v2")");
      Print("See additional options at CLI.md");
      break;
    }
    default:
      break;
  }
}

void CCLI::OverrideConfig(CAura* nAura) const
{
  ReadOpt(m_War3DataVersion) >> nAura->m_Config.m_Warcraft3DataVersion;
  ReadOpt(m_War3Path) >> nAura->m_Config.m_Warcraft3Path;
  ReadOpt(m_MapPath) >> nAura->m_Config.m_MapPath;
  ReadOpt(m_MapCFGPath) >> nAura->m_Config.m_MapCFGPath;
  ReadOpt(m_MapCachePath) >> nAura->m_Config.m_MapCachePath;
  ReadOpt(m_JASSPath) >> nAura->m_Config.m_JASSPath;
  ReadOpt(m_GameSavePath) >> nAura->m_Config.m_GameSavePath;

  ReadOpt(m_CheckJASS) >> nAura->m_Config.m_ValidateJASS;
  ReadOpt(m_ExtractJASS) >> nAura->m_Config.m_ExtractJASS;
  ReadOpt(m_ExitOnStandby) >> nAura->m_Config.m_ExitOnStandby;
  ReadOpt(m_Discord) >> nAura->m_Discord.m_Config.m_Enabled;
  ReadOpt(m_LAN) >> nAura->m_GameDefaultConfig->m_UDPEnabled;
  ReadOpt(m_UseMapCFGCache) >> nAura->m_Config.m_EnableCFGCache;
  ReadOpt(m_MapCFGCacheRevalidation) >> nAura->m_Config.m_CFGCacheRevalidateAlgorithm;
  ReadOpt(m_LogLevel) >> nAura->m_Config.m_LogLevel;

  if (m_UDPDiscoveryMode.has_value()) {
    const bool isMainServerEnabled = m_UDPDiscoveryMode.value() != UDPDiscoveryMode::kFree;
    nAura->m_Net.m_Config.m_UDPMainServerEnabled = isMainServerEnabled;
    if (!isMainServerEnabled) {
      nAura->m_Net.m_Config.m_UDPBroadcastStrictMode = m_UDPDiscoveryMode.value() == UDPDiscoveryMode::kStrict;
    }
  }

  if (m_BindAddress.has_value()) {
    optional<sockaddr_storage> address = CNet::ParseAddress(m_BindAddress.value(), ACCEPT_IPV4);
    if (address.has_value()) {
      nAura->m_Net.m_Config.m_BindAddress4 = address.value();
    }
  }

  auto hostPortReader = ReadOpt(m_HostPort);
  hostPortReader >> nAura->m_Net.m_Config.m_MinHostPort;
  hostPortReader >> nAura->m_Net.m_Config.m_MaxHostPort;

#ifndef DISABLE_MINIUPNP
  ReadOpt(m_EnableUPnP) >> nAura->m_Net.m_Config.m_EnableUPnP;
#endif
}

bool CCLI::QueueActions(CAura* nAura) const
{
  for (const auto& port : m_PortForwardTCP) {
    AppAction upnpAction = AppAction(APP_ACTION_TYPE_UPNP, APP_ACTION_MODE_TCP, port, port);
    nAura->m_PendingActions.push(upnpAction);
  }

  for (const auto& port : m_PortForwardUDP) {
    AppAction upnpAction = AppAction(APP_ACTION_TYPE_UPNP, APP_ACTION_MODE_UDP, port, port);
    nAura->m_PendingActions.push(upnpAction);
  }

  if (m_SearchTarget.has_value()) {
    CGameExtraOptions options;
    options.AcquireCLI(this);
    if (!CheckGameParameters()) {
      return false;
    }

    const uint8_t searchType = m_SearchType.value_or(m_UseStandardPaths ? SEARCH_TYPE_ONLY_FILE : SEARCH_TYPE_ANY);

    optional<string> userName = GetUserMultiPlayerName();
    shared_ptr<CCommandContext> ctx = nullptr;
    try {
      ctx = make_shared<CCommandContext>(ServiceType::kCLI, nAura, userName.value_or(string()), false, &cout);
    } catch (...) {
      return false;
    }
    shared_ptr<CGameSetup> gameSetup = nullptr;
    try {
      gameSetup = make_shared<CGameSetup>(nAura, ctx, m_SearchTarget.value(), searchType, true, m_UseStandardPaths, true /* lucky mode */);
    } catch (...) {
      return false;
    }
    gameSetup->AcquireCLIEarly(this);
    if (!gameSetup->LoadMapSync()) {
      if (searchType == SEARCH_TYPE_ANY) {
        ctx->ErrorReply("Input does not refer to a valid map, config, or URL.");
      } else if (searchType == SEARCH_TYPE_ONLY_FILE) {
        ctx->ErrorReply("Input does not refer to a valid file");
      } else if (searchType == SEARCH_TYPE_ONLY_MAP) {
        ctx->ErrorReply("Input does not refer to a valid map (.w3x, .w3m)");
      } else if (searchType == SEARCH_TYPE_ONLY_CONFIG) {
        ctx->ErrorReply("Input does not refer to a valid map config file (.ini)");
      }
      return false;
    }
    if (!gameSetup->ApplyMapModifiers(&options)) {
      ctx->ErrorReply("Invalid map options. Map has fixed player settings.");
      return false;
    }
    if (!gameSetup->m_SaveFile.empty()) {
      if (!RunGameLoadParameters(gameSetup)) {
        return false;
      }
    }
    for (const auto& id : m_ExcludedRealms) {
      shared_ptr<CRealm> excludedRealm = nAura->GetRealmByInputId(id);
      if (excludedRealm) {
        gameSetup->AddIgnoredRealm(excludedRealm);
      } else {
        Print("[AURA] Unrecognized realm [" + id + "] ignored by --exclude");
      }
    }
    if (m_GameMapAlias.has_value()) {
      string normalizedAlias = GetNormalizedAlias(m_GameMapAlias.value());
      string mapFileName = gameSetup->GetMap()->GetServerFileName();
      if (nAura->m_DB->AliasAdd(normalizedAlias, mapFileName)) {
        Print("[AURA] Alias <<" + m_GameMapAlias.value() + ">> added for [" + mapFileName + "]");
      } else {
        Print("Failed to add alias.");
      }
    }
    if (m_GameMirrorSource.has_value()) {
      if (!gameSetup->SetMirrorSource(m_GameMirrorSource.value())) {
        Print("[AURA] Invalid mirror source [" + m_GameMirrorSource.value() + "]. Ensure it has the form IP:PORT#ID");
        return false;
      }
      if (m_GameMirrorProxy.has_value()) {
        gameSetup->SetMirrorProxy(m_GameMirrorProxy.value());
      }
    }
    gameSetup->AcquireHost(this, userName);
    gameSetup->AcquireCLISimple(this);
    gameSetup->SetActive();
    AppAction hostAction = AppAction(APP_ACTION_TYPE_HOST, 0);
    nAura->m_PendingActions.push(hostAction);
  }

  for (const auto& execEntry : m_ExecCommands) {
    bool padding = false;
    string cmdToken, command, target;
    if (ExtractMessageTokens(execEntry, cmdToken, padding, command, target)) {
      pair<string, string> identity = SplitAddress(m_ExecAs.value());
      LazyCommandContext lazyCommand = LazyCommandContext(m_ExecBroadcast, m_ExecOnline, command, target, ToLowerCase(identity.first), ToLowerCase(identity.second), m_ExecAuth);
      nAura->m_PendingActions.push(lazyCommand);
    }
  }

  return true;
}
