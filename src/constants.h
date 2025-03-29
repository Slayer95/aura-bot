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

#ifndef AURA_CONSTANTS_H_
#define AURA_CONSTANTS_H_

// includes.h

#define LOG_LEVEL_EMERGENCY 1
#define LOG_LEVEL_ALERT 2
#define LOG_LEVEL_CRITICAL 3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_WARNING 5
#define LOG_LEVEL_NOTICE 6
#define LOG_LEVEL_INFO 7
#define LOG_LEVEL_DEBUG 8
#define LOG_LEVEL_TRACE 9
#define LOG_LEVEL_TRACE2 10
#define LOG_LEVEL_TRACE3 11

#define ANTI_SPOOF_NONE 0
#define ANTI_SPOOF_BASIC 1
#define ANTI_SPOOF_EXTENDED 2
#define ANTI_SPOOF_FULL 3

#define MAX_READ_FILE_SIZE 0x18000000

// aura.h

#define APP_ACTION_DONE 0u
#define APP_ACTION_ERROR 1u
#define APP_ACTION_WAIT 2u
#define APP_ACTION_TIMEOUT 3u

#define APP_ACTION_TYPE_UPNP 0u
#define APP_ACTION_TYPE_HOST 1u

#define APP_ACTION_MODE_TCP 0u
#define APP_ACTION_MODE_UDP 1u

#define SERVICE_TYPE_NONE 0
#define SERVICE_TYPE_GAME 1
#define SERVICE_TYPE_REALM 2
#define SERVICE_TYPE_IRC 3
#define SERVICE_TYPE_DISCORD 4
#define SERVICE_TYPE_INVALID 255

constexpr uint8_t LOG_C = 1u;
constexpr uint8_t LOG_P = 2u;
constexpr uint8_t LOG_R = 4u;
constexpr uint8_t LOG_ALL = LOG_C | LOG_P | LOG_R;

// map.h

constexpr uint8_t MAPSPEED_SLOW = 1u;
constexpr uint8_t MAPSPEED_NORMAL = 2u;
constexpr uint8_t MAPSPEED_FAST = 3u;

constexpr uint8_t MAPVIS_HIDETERRAIN = 1u;
constexpr uint8_t MAPVIS_EXPLORED = 2u;
constexpr uint8_t MAPVIS_ALWAYSVISIBLE = 3u;
constexpr uint8_t MAPVIS_DEFAULT = 4u;

constexpr uint8_t MAPOBS_NONE = 1u;
constexpr uint8_t MAPOBS_ONDEFEAT = 2u;
constexpr uint8_t MAPOBS_ALLOWED = 3u;
constexpr uint8_t MAPOBS_REFEREES = 4u;

constexpr uint8_t MAPFLAG_TEAMSTOGETHER = 1u;
constexpr uint8_t MAPFLAG_FIXEDTEAMS = 2u;
constexpr uint8_t MAPFLAG_UNITSHARE = 4u;
constexpr uint8_t MAPFLAG_RANDOMHERO = 8u;
constexpr uint8_t MAPFLAG_RANDOMRACES = 16u;

#define MAPOPT_HIDEMINIMAP (1 << 0)
#define MAPOPT_MODIFYALLYPRIORITIES (1 << 1)
#define MAPOPT_MELEE (1 << 2) // the bot cares about this one...
#define MAPOPT_REVEALTERRAIN (1 << 4)
#define MAPOPT_FIXEDPLAYERSETTINGS (1 << 5) // and this one...
#define MAPOPT_CUSTOMFORCES (1 << 6)        // and this one, the rest don't affect the bot's logic
#define MAPOPT_CUSTOMTECHTREE (1 << 7)
#define MAPOPT_CUSTOMABILITIES (1 << 8)
#define MAPOPT_CUSTOMUPGRADES (1 << 9)
#define MAPOPT_WATERWAVESONCLIFFSHORES (1 << 11)
#define MAPOPT_HASTERRAINFOG (1 << 12)
#define MAPOPT_REQUIRESEXPANSION (1 << 13)
#define MAPOPT_ITEMCLASSIFICATION (1 << 14)
#define MAPOPT_WATERTINTING (1 << 15)
#define MAPOPT_ACCURATERANDOM (1 << 16)
#define MAPOPT_ABILITYSKINS (1 << 17)

constexpr uint8_t MAPFILTER_MAKER_USER = 1;
constexpr uint8_t MAPFILTER_MAKER_BLIZZARD = 2;

constexpr uint8_t MAPFILTER_TYPE_MELEE = 1;
constexpr uint8_t MAPFILTER_TYPE_SCENARIO = 2;

constexpr uint8_t MAPFILTER_SIZE_SMALL = 1;
constexpr uint8_t MAPFILTER_SIZE_MEDIUM = 2;
constexpr uint8_t MAPFILTER_SIZE_LARGE = 4;

constexpr uint8_t MAPFILTER_OBS_FULL = 1;
constexpr uint8_t MAPFILTER_OBS_ONDEATH = 2;
constexpr uint8_t MAPFILTER_OBS_NONE = 4;

#define MAPGAMETYPE_UNKNOWN0 (1) // always set except for saved games?
#define MAPGAMETYPE_BLIZZARD (1 << 3)
#define MAPGAMETYPE_MELEE (1 << 5)
#define MAPGAMETYPE_SAVEDGAME (1 << 9)
#define MAPGAMETYPE_PRIVATEGAME (1 << 11)
#define MAPGAMETYPE_MAKERUSER (1 << 13)
#define MAPGAMETYPE_MAKERBLIZZARD (1 << 14)
#define MAPGAMETYPE_TYPEMELEE (1 << 15)
#define MAPGAMETYPE_TYPESCENARIO (1 << 16)
#define MAPGAMETYPE_SIZESMALL (1 << 17)
#define MAPGAMETYPE_SIZEMEDIUM (1 << 18)
#define MAPGAMETYPE_SIZELARGE (1 << 19)
#define MAPGAMETYPE_OBSFULL (1 << 20)
#define MAPGAMETYPE_OBSONDEATH (1 << 21)
#define MAPGAMETYPE_OBSNONE (1 << 22)

constexpr uint8_t MAPLAYOUT_ANY = 0u;
constexpr uint8_t MAPLAYOUT_CUSTOM_FORCES = 1u;
constexpr uint8_t MAPLAYOUT_FIXED_PLAYERS = 3u;

constexpr uint8_t MAP_DATASET_DEFAULT = 0u;
constexpr uint8_t MAP_DATASET_CUSTOM = 1u;
constexpr uint8_t MAP_DATASET_MELEE = 2u;

constexpr uint8_t CACHE_REVALIDATION_NEVER = 0;
constexpr uint8_t CACHE_REVALIDATION_ALWAYS = 1u;
constexpr uint8_t CACHE_REVALIDATION_MODIFIED = 2u;

constexpr uint8_t MAP_FEATURE_TOGGLE_DISABLED = 0u;
constexpr uint8_t MAP_FEATURE_TOGGLE_OPTIONAL = 1u;
constexpr uint8_t MAP_FEATURE_TOGGLE_REQUIRED = 2u;

// Load map fragments in memory max 8 MB at a time.
constexpr uint32_t MAP_FILE_MAX_CHUNK_SIZE = 0x800000;
// May also choose a chunk size different from the max cache chunk size.
constexpr uint32_t MAP_FILE_PROCESSING_CHUNK_SIZE = 0x800000;

constexpr uint8_t MAP_CONFIG_SCHEMA_NUMBER = 3;

constexpr uint8_t MAP_FILE_SOURCE_CATEGORY_NONE = 0u;
constexpr uint8_t MAP_FILE_SOURCE_CATEGORY_MPQ = 1u;
constexpr uint8_t MAP_FILE_SOURCE_CATEGORY_FS = 2u;

constexpr uint8_t MAP_ALLOW_LUA_NEVER = 0u;
constexpr uint8_t MAP_ALLOW_LUA_ALWAYS = 1u;
constexpr uint8_t MAP_ALLOW_LUA_AUTO = 2u;

constexpr const char* HCL_CHARSET_STANDARD = "abcdefghijklmnopqrstuvwxyz0123456789 -=,."; // 41 characters
constexpr const char* HCL_CHARSET_SMALL = "0123456789abcdef-\" \\"; // 20 characters

constexpr uint8_t MMD_TYPE_STANDARD = 0u;
constexpr uint8_t MMD_TYPE_DOTA = 1u;

constexpr const char* AHCL_DEFAULT_CHARSET = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZабвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ -=,.!?*_+=|:;"; // 142 characters 

constexpr uint8_t MAP_TRANSFER_NONE = 0u;
constexpr uint8_t MAP_TRANSFER_DONE = 1u;
constexpr uint8_t MAP_TRANSFER_IN_PROGRESS = 2u;
constexpr uint8_t MAP_TRANSFER_RATE_LIMITED = 3u;
constexpr uint8_t MAP_TRANSFER_MISSING = 4u;
constexpr uint8_t MAP_TRANSFER_INVALID = 5u;

constexpr uint8_t MAP_TRANSFER_CHECK_ALLOWED = 0u;
constexpr uint8_t MAP_TRANSFER_CHECK_INVALID = 1u;
constexpr uint8_t MAP_TRANSFER_CHECK_MISSING = 2u;
constexpr uint8_t MAP_TRANSFER_CHECK_DISABLED = 3u;
constexpr uint8_t MAP_TRANSFER_CHECK_TOO_LARGE = 4u;
constexpr uint8_t MAP_TRANSFER_CHECK_BUFFERBLOAT = 5u;

// game.h
#define SLOTS_ALIGNMENT_CHANGED (1 << 0)
#define SLOTS_DOWNLOAD_PROGRESS_CHANGED (1 << 1)
#define SLOTS_HCL_INJECTED (1 << 2)

#define SAVE_ON_LEAVE_NEVER 0u
#define SAVE_ON_LEAVE_AUTO 1u
#define SAVE_ON_LEAVE_ALWAYS 2u

#define CUSTOM_LAYOUT_NONE 0u
#define CUSTOM_LAYOUT_ONE_VS_ALL 1u
#define CUSTOM_LAYOUT_HUMANS_VS_AI 2u
#define CUSTOM_LAYOUT_FFA 4u
#define CUSTOM_LAYOUT_DRAFT 8u
#define CUSTOM_LAYOUT_LOCKTEAMS 15u
#define CUSTOM_LAYOUT_COMPACT 16u
#define CUSTOM_LAYOUT_ISOPLAYERS 32u

constexpr uint16_t REFRESH_PERIOD_MIN_SUGGESTED = 30;
constexpr uint16_t REFRESH_PERIOD_MAX_SUGGESTED = 300;
#define GAME_BANNABLE_MAX_HISTORY_SIZE 32

#define GAME_PAUSES_PER_PLAYER 3u
#define GAME_SAVES_PER_PLAYER 1u
#define GAME_SAVES_PER_REFEREE_ANTIABUSE 3u
#define GAME_SAVES_PER_REFEREE_DEFAULT 255u

#define GAME_ONGOING 0u
#define GAME_OVER_TRUSTED 1u
#define GAME_OVER_MMD 2u

#define HIDDEN_PLAYERS_NONE 0u
#define HIDDEN_PLAYERS_LOBBY 1u
#define HIDDEN_PLAYERS_GAME 2u
#define HIDDEN_PLAYERS_ALL 3u

#define HIGH_PING_KICK_DELAY 10000u
#define MAX_SCOPE_BANS 100u
#define REALM_HOST_COOLDOWN_TICKS 30000u
#define AUTO_REHOST_COOLDOWN_TICKS 180000u

#define ON_SEND_ACTIONS_NONE 0u
#define ON_SEND_ACTIONS_PAUSE 1u
#define ON_SEND_ACTIONS_RESUME 2u

#define PING_EQUALIZER_PERIOD_TICKS 10000u
#define PING_EQUALIZER_DEFAULT_FRAMES 7u

// 10 players x 150 APM x (1 min / 60000 ms) x (100 ms latency) = 2.5
#define DEFAULT_ACTIONS_PER_FRAME 3u

#define SYNCHRONIZATION_CHECK_MIN_FRAMES 5u

constexpr uint8_t BUFFERING_ENABLED_NONE = 0u;
constexpr uint8_t BUFFERING_ENABLED_PLAYING = 1u;
constexpr uint8_t BUFFERING_ENABLED_LOADING = 2u;
constexpr uint8_t BUFFERING_ENABLED_SLOTS = 4u;
constexpr uint8_t BUFFERING_ENABLED_LOBBY = 8u;
constexpr uint8_t BUFFERING_ENABLED_ALL = 15u;

constexpr uint8_t GAME_FRAME_TYPE_ACTIONS = 0u;
constexpr uint8_t GAME_FRAME_TYPE_PAUSED = 1u;
constexpr uint8_t GAME_FRAME_TYPE_LEAVER = 2u;
constexpr uint8_t GAME_FRAME_TYPE_CHAT = 3u;
constexpr uint8_t GAME_FRAME_TYPE_LATENCY = 4u;
constexpr uint8_t GAME_FRAME_TYPE_GPROXY = 5u;

constexpr uint8_t GAME_RESULT_SOURCE_NONE = 0u;
constexpr uint8_t GAME_RESULT_SOURCE_LEAVECODE = 1u;
constexpr uint8_t GAME_RESULT_SOURCE_MMD = 2u;

constexpr uint8_t GAME_RESULT_CONSTRAINTS_NONE = 0u;
constexpr uint8_t GAME_RESULT_CONSTRAINTS_NODRAW = 1u;
constexpr uint8_t GAME_RESULT_CONSTRAINTS_SAME_TEAM_WINNERS = 2u;
constexpr uint8_t GAME_RESULT_CONSTRAINTS_ALL = GAME_RESULT_CONSTRAINTS_NODRAW | GAME_RESULT_CONSTRAINTS_SAME_TEAM_WINNERS;

constexpr uint8_t GAME_RESULT_LOSER = 0u;
constexpr uint8_t GAME_RESULT_DRAWER = 1u;
constexpr uint8_t GAME_RESULT_WINNER = 2u;
constexpr uint8_t GAME_RESULT_UNDECIDED = 3u;

// game_slot.h

constexpr uint8_t UID_ZERO = 0;

constexpr uint8_t SLOTSTATUS_OPEN = 0u;
constexpr uint8_t SLOTSTATUS_CLOSED = 1u;
constexpr uint8_t SLOTSTATUS_OCCUPIED = 2u;
constexpr uint8_t SLOTSTATUS_VALID = 3u;
constexpr uint8_t SLOTSTATUS_VALID_INITIAL_NON_COMPUTER = 1u;

constexpr uint8_t SLOTRACE_HUMAN = 1u;
constexpr uint8_t SLOTRACE_ORC = 2u;
constexpr uint8_t SLOTRACE_NIGHTELF = 4u;
constexpr uint8_t SLOTRACE_UNDEAD = 8u;
constexpr uint8_t SLOTRACE_RANDOM = 32u;
constexpr uint8_t SLOTRACE_SELECTABLE = 64u;
constexpr uint8_t SLOTRACE_PICKRANDOM = 128u;
constexpr uint8_t SLOTRACE_INVALID = 255u;

constexpr uint8_t SLOTCOMP_EASY = 0u;
constexpr uint8_t SLOTCOMP_NORMAL = 1u;
constexpr uint8_t SLOTCOMP_HARD = 2u;
constexpr uint8_t SLOTCOMP_VALID = 3u;
constexpr uint8_t SLOTCOMP_INVALID = 255u;

constexpr uint8_t SLOTCOMP_NO = 0u;
constexpr uint8_t SLOTCOMP_YES = 1u;

constexpr uint8_t SLOTTYPE_NONE = 0u;
constexpr uint8_t SLOTTYPE_USER = 1u;
constexpr uint8_t SLOTTYPE_COMP = 2u;
constexpr uint8_t SLOTTYPE_NEUTRAL = 3u;
constexpr uint8_t SLOTTYPE_RESCUEABLE = 4u;
constexpr uint8_t SLOTTYPE_AUTO = 255u;

constexpr uint8_t SLOTPROG_NEW = 0u;
constexpr uint8_t SLOTPROG_RDY = 100u;
constexpr uint8_t SLOTPROG_RST = 255u;

constexpr int MAX_SLOTS_MODERN = 24;
constexpr int MAX_SLOTS_LEGACY = 12;

// connection.h

constexpr uint8_t JOIN_RESULT_FAIL = 0u;
constexpr uint8_t JOIN_RESULT_PLAYER = 1u;
constexpr uint8_t JOIN_RESULT_OBSERVER = 2u;

constexpr uint8_t INCON_UPDATE_OK = 0u;
constexpr uint8_t INCON_UPDATE_DESTROY = 1u;
constexpr uint8_t INCON_UPDATE_PROMOTED = 2u;
constexpr uint8_t INCON_UPDATE_PROMOTED_PASSTHROUGH = 3u;
constexpr uint8_t INCON_UPDATE_RECONNECTED = 4u;

constexpr uint8_t INCON_TYPE_NONE = 0u;
constexpr uint8_t INCON_TYPE_UDP_TUNNEL = 1u;
constexpr uint8_t INCON_TYPE_PLAYER = 2u;
constexpr uint8_t INCON_TYPE_KICKED_PLAYER = 3u;
constexpr uint8_t INCON_TYPE_VLAN = 4u;
constexpr uint8_t INCON_TYPE_OBSERVER = 5u;

// game_user.h

#define CONSISTENT_PINGS_COUNT 3u
#define MAXIMUM_PINGS_COUNT 6u
#define MAX_PING_WEIGHT 4u

#define SMART_COMMAND_NONE 0u
#define SMART_COMMAND_GO 1u

#define GAME_USER_UNVERIFIED_KICK_TICKS 60000u
#define AUTO_REALM_VERIFY_LATENCY 5000u
#define CHECK_STATUS_LATENCY 5000u
#define READY_REMINDER_PERIOD 20000u

#define SYSTEM_RTT_POLLING_PERIOD 10000u

#define USERSTATUS_LOBBY 0u
#define USERSTATUS_LOADING_SCREEN 1u
#define USERSTATUS_PLAYING 2u
#define USERSTATUS_ENDING 3u
#define USERSTATUS_ENDED 4u

// game_async_observer.h

constexpr uint8_t ASYNC_OBSERVER_GOAL_OBSERVER = 0u;
constexpr uint8_t ASYNC_OBSERVER_GOAL_PLAYER = 1u;

constexpr uint8_t ASYNC_OBSERVER_OK = 0u;
constexpr uint8_t ASYNC_OBSERVER_DESTROY = 1u;
constexpr uint8_t ASYNC_OBSERVER_PROMOTED = 2u;

// game_setup.h

#define SEARCH_TYPE_ONLY_MAP 1
#define SEARCH_TYPE_ONLY_CONFIG 2
#define SEARCH_TYPE_ONLY_FILE 3
#define SEARCH_TYPE_ANY 7

#define MATCH_TYPE_NONE 0u
#define MATCH_TYPE_MAP 1u
#define MATCH_TYPE_CONFIG 2u
#define MATCH_TYPE_INVALID 4u
#define MATCH_TYPE_FORBIDDEN 8u

#define RESOLUTION_OK 0
#define RESOLUTION_ERR 1
#define RESOLUTION_BAD_NAME 2

#define SETUP_USE_STANDARD_PATHS true
#define SETUP_PROTECT_ARBITRARY_TRAVERSAL false

#ifdef _WIN32
#define FILE_EXTENSIONS_MAP {L".w3x", L".w3m"}
#define FILE_EXTENSIONS_CONFIG {L".ini"}
#else
#define FILE_EXTENSIONS_MAP {".w3x", ".w3m"}
#define FILE_EXTENSIONS_CONFIG {".ini"}
#endif

#define GAMESETUP_STEP_MAIN 0
#define GAMESETUP_STEP_RESOLUTION 1
#define GAMESETUP_STEP_SUGGESTIONS 2
#define GAMESETUP_STEP_DOWNLOAD 3

#define MAP_ONREADY_SET_ACTIVE 1
#define MAP_ONREADY_HOST 2
#define MAP_ONREADY_ALIAS 3

#define SUGGESTIONS_TIMEOUT 3000u
#define GAMESETUP_STALE_TICKS 180000u

// game_protocol.h

#define GAME_NONE 0 // this case isn't part of the protocol, it's for internal use only
#define GAME_FULL 2
#define GAME_PUBLIC 16
#define GAME_PRIVATE 17

#define GAMETYPE_CUSTOM 1
#define GAMETYPE_BLIZZARD 9

constexpr uint8_t PLAYERLEAVE_DISCONNECT = 1u;
constexpr uint8_t PLAYERLEAVE_LOST = 7u;
constexpr uint8_t PLAYERLEAVE_LOSTBUILDINGS = 8u;
constexpr uint8_t PLAYERLEAVE_WON = 9u;
constexpr uint8_t PLAYERLEAVE_DRAW = 10u;
constexpr uint8_t PLAYERLEAVE_OBSERVER = 11u;
constexpr uint8_t PLAYERLEAVE_LOBBY = 13u;
constexpr uint8_t PLAYERLEAVE_GPROXY = 100u;

constexpr uint8_t REJECTJOIN_FULL = 9;
constexpr uint8_t REJECTJOIN_STARTED = 10;
constexpr uint8_t REJECTJOIN_WRONGPASSWORD = 27;

constexpr uint8_t ACTION_PAUSE = 1u;
constexpr uint8_t ACTION_RESUME = 2u;
constexpr uint8_t ACTION_SAVE = 6u;
constexpr uint8_t ACTION_SAVE_ENDED = 7u;
constexpr uint8_t ACTION_SHARE_UNITS = 80u;
constexpr uint8_t ACTION_TRANSFER_RESOURCES = 81u;
constexpr uint8_t ACTION_CHAT_TRIGGER = 96u;
constexpr uint8_t ACTION_MODAL_BTN = 106u;
constexpr uint8_t ACTION_SYNC_INT = 107u;

constexpr uint32_t ALLIANCE_SETTINGS_ALLY = 0x1Fu;
constexpr uint32_t ALLIANCE_SETTINGS_SHARED_VISION = 0x20u;
constexpr uint32_t ALLIANCE_SETTINGS_SHARED_CONTROL = 0x40u;
constexpr uint32_t ALLIANCE_SETTINGS_SHARED_VICTORY = 0x400u;

#define MAX_PLAYER_NAME_SIZE 15

constexpr uint8_t CHAT_RECV_ALL = 0;
constexpr uint8_t CHAT_RECV_ALLY = 1;
constexpr uint8_t CHAT_RECV_OBS = 2;

// game_virtual_user.h

constexpr uint8_t VIRTUAL_USER_ALLOW_ACTIONS_NONE = 0u;
constexpr uint8_t VIRTUAL_USER_ALLOW_ACTIONS_PAUSE = 1u;
constexpr uint8_t VIRTUAL_USER_ALLOW_ACTIONS_RESUME = 2u;
constexpr uint8_t VIRTUAL_USER_ALLOW_ACTIONS_SAVE = 4u;
constexpr uint8_t VIRTUAL_USER_ALLOW_ACTIONS_CHAT_TRIGGER = 8u;
constexpr uint8_t VIRTUAL_USER_ALLOW_ACTIONS_SYNC_INTEGER = 16u;
constexpr uint8_t VIRTUAL_USER_ALLOW_ACTIONS_ANY = 31u;

constexpr uint8_t VIRTUAL_USER_ALLOW_CONNECTIONS_NONE = 0u;
constexpr uint8_t VIRTUAL_USER_ALLOW_CONNECTIONS_OBSERVER = 1u;
constexpr uint8_t VIRTUAL_USER_ALLOW_CONNECTIONS_PLAYER = 2u;
constexpr uint8_t VIRTUAL_USER_ALLOW_CONNECTIONS_ANY = VIRTUAL_USER_ALLOW_CONNECTIONS_OBSERVER | VIRTUAL_USER_ALLOW_CONNECTIONS_PLAYER;

// chat.h

#define FROM_GAME (1 << 0)
#define FROM_BNET (1 << 1)
#define FROM_IRC (1 << 2)
#define FROM_DISCORD (1 << 3)
#define FROM_OTHER (1 << 7)

#define CHAT_SEND_SOURCE_ALL (1 << 0)
#define CHAT_SEND_TARGET_ALL (1 << 1)
#define CHAT_LOG_INCIDENT (1 << 2)
#define CHAT_TYPE_INFO (1 << 3)
#define CHAT_TYPE_DONE (1 << 4)
#define CHAT_TYPE_ERROR (1 << 5)
#define CHAT_WRITE_TARGETS (CHAT_SEND_SOURCE_ALL | CHAT_SEND_TARGET_ALL | CHAT_LOG_INCIDENT)
#define CHAT_RESPONSE_TYPES (CHAT_TYPE_INFO | CHAT_TYPE_DONE | CHAT_TYPE_ERROR)

#define USER_PERMISSIONS_GAME_PLAYER (1 << 0)
#define USER_PERMISSIONS_GAME_OWNER (1 << 1)
#define USER_PERMISSIONS_CHANNEL_VERIFIED (1 << 2)
#define USER_PERMISSIONS_CHANNEL_ADMIN (1 << 3)
#define USER_PERMISSIONS_CHANNEL_ROOTADMIN (1 << 4)
#define USER_PERMISSIONS_BOT_SUDO_SPOOFABLE (1 << 6)
#define USER_PERMISSIONS_BOT_SUDO_OK (1 << 7)

#define SET_USER_PERMISSIONS_ALL (0xFFFF)

#define COMMAND_TOKEN_MATCH_NONE 0
#define COMMAND_TOKEN_MATCH_PRIVATE 1
#define COMMAND_TOKEN_MATCH_BROADCAST 2

// config_bot.h
constexpr uint8_t LOG_GAME_CHAT_NEVER = 0u;
constexpr uint8_t LOG_GAME_CHAT_ALLOWED = 1u;
constexpr uint8_t LOG_GAME_CHAT_ALWAYS = 2u;

constexpr uint8_t LOG_REMOTE_MODE_NONE = 0u;
constexpr uint8_t LOG_REMOTE_MODE_FILE = 1u;
constexpr uint8_t LOG_REMOTE_MODE_NETWORK = 2u;
constexpr uint8_t LOG_REMOTE_MODE_MIXED = 3u;

// config_realm.h

constexpr uint8_t REALM_AUTH_PVPGN = 0u;
constexpr uint8_t REALM_AUTH_BATTLENET = 1u;

constexpr uint8_t COMMANDS_ALLOWED_NONE = 0u;
constexpr uint8_t COMMANDS_ALLOWED_UNVERIFIED = 1u;
constexpr uint8_t COMMANDS_ALLOWED_VERIFIED = 2u;

constexpr uint8_t REALM_OBSERVER_DISPLAY_NONE = 0u;
constexpr uint8_t REALM_OBSERVER_DISPLAY_LOW_PRIORITY = 1u;
constexpr uint8_t REALM_OBSERVER_DISPLAY_ALWAYS = 2u;

constexpr uint8_t PVPGN_LOCALE_EN_US = 0u;
constexpr uint8_t PVPGN_LOCALE_CS_CZ = 1u;
constexpr uint8_t PVPGN_LOCALE_DE_DE = 2u;
constexpr uint8_t PVPGN_LOCALE_ES_ES = 3u;
constexpr uint8_t PVPGN_LOCALE_FR_FR = 4u;
constexpr uint8_t PVPGN_LOCALE_IT_IT = 5u;
constexpr uint8_t PVPGN_LOCALE_JA_JA = 6u;
constexpr uint8_t PVPGN_LOCALE_KO_KR = 7u;
constexpr uint8_t PVPGN_LOCALE_PL_PL = 8u;
constexpr uint8_t PVPGN_LOCALE_RU_RU = 9u;
constexpr uint8_t PVPGN_LOCALE_ZH_CN = 10u;
constexpr uint8_t PVPGN_LOCALE_ZH_TW = 11u;

// config_net.h 

constexpr uint8_t MAP_TRANSFERS_NEVER = 0u;
constexpr uint8_t MAP_TRANSFERS_AUTOMATIC = 1u;
constexpr uint8_t MAP_TRANSFERS_MANUAL = 2u;

// config_game.h

constexpr uint8_t ON_DESYNC_NONE = 0u;
constexpr uint8_t ON_DESYNC_NOTIFY = 1u;
constexpr uint8_t ON_DESYNC_DROP = 2u;

constexpr uint8_t ON_IPFLOOD_NONE = 0u;
constexpr uint8_t ON_IPFLOOD_NOTIFY = 1u;
constexpr uint8_t ON_IPFLOOD_DENY = 2u;

constexpr uint8_t ON_UNSAFE_NAME_NONE = 0u;
constexpr uint8_t ON_UNSAFE_NAME_CENSOR_MAY_DESYNC = 1u;
constexpr uint8_t ON_UNSAFE_NAME_DENY = 2u;

constexpr uint8_t ON_PLAYER_LEAVE_NONE = 0u;
constexpr uint8_t ON_PLAYER_LEAVE_NATIVE = 1u;
constexpr uint8_t ON_PLAYER_LEAVE_SHARE_UNITS = 2u;

constexpr uint8_t CROSSPLAY_MODE_NONE = 0u;
constexpr uint8_t CROSSPLAY_MODE_CONSERVATIVE = 1u;
constexpr uint8_t CROSSPLAY_MODE_OPTIMISTIC = 2u;
constexpr uint8_t CROSSPLAY_MODE_FORCE = 3u;

constexpr uint8_t READY_MODE_FAST = 0u;
constexpr uint8_t READY_MODE_EXPECT_RACE = 1u;
constexpr uint8_t READY_MODE_EXPLICIT = 2u;

constexpr uint8_t HIDE_IGN_NEVER = 0u;
constexpr uint8_t HIDE_IGN_HOST = 1u;
constexpr uint8_t HIDE_IGN_ALWAYS = 2u;
constexpr uint8_t HIDE_IGN_AUTO = 3u;

constexpr uint8_t ON_ADV_ERROR_IGNORE_ERRORS = 0u;
constexpr uint8_t ON_ADV_ERROR_EXIT_ON_MAIN_ERROR = 1u;
constexpr uint8_t ON_ADV_ERROR_EXIT_ON_MAIN_ERROR_IF_EMPTY = 2u;
constexpr uint8_t ON_ADV_ERROR_EXIT_ON_ANY_ERROR = 3u;
constexpr uint8_t ON_ADV_ERROR_EXIT_ON_ANY_ERROR_IF_EMPTY = 4u;
constexpr uint8_t ON_ADV_ERROR_EXIT_ON_MAX_ERRORS = 5u;

constexpr uint8_t LOBBY_TIMEOUT_NEVER = 0u;
constexpr uint8_t LOBBY_TIMEOUT_EMPTY = 1u;
constexpr uint8_t LOBBY_TIMEOUT_OWNERLESS = 2u;
constexpr uint8_t LOBBY_TIMEOUT_STRICT = 3u;

constexpr uint8_t LOBBY_OWNER_TIMEOUT_NEVER = 0u;
constexpr uint8_t LOBBY_OWNER_TIMEOUT_ABSENT = 1u;
constexpr uint8_t LOBBY_OWNER_TIMEOUT_STRICT = 2u;

constexpr uint8_t GAME_LOADING_TIMEOUT_NEVER = 0u;
constexpr uint8_t GAME_LOADING_TIMEOUT_STRICT = 1u;

constexpr uint8_t GAME_PLAYING_TIMEOUT_NEVER = 0u;
constexpr uint8_t GAME_PLAYING_TIMEOUT_DRY = 1u;
constexpr uint8_t GAME_PLAYING_TIMEOUT_STRICT = 2u;

constexpr uint8_t LOG_CHAT_TYPE_ASCII = 1u;
constexpr uint8_t LOG_CHAT_TYPE_NON_ASCII = 2u;
constexpr uint8_t LOG_CHAT_TYPE_COMMANDS = 4u;

constexpr uint8_t MAX_GAME_VERSION_OVERRIDES = 24;

// config_discord.h

#define FILTER_ALLOW_ALL 0
#define FILTER_DENY_ALL 1
#define FILTER_ALLOW_LIST 2
#define FILTER_DENY_LIST 3

// config_commands.h

#define REALM_AUTH_PVPGN 0
#define REALM_AUTH_BATTLENET 1

#define COMMAND_PERMISSIONS_DISABLED 0
#define COMMAND_PERMISSIONS_SUDO 1
#define COMMAND_PERMISSIONS_SUDO_UNSAFE 2
#define COMMAND_PERMISSIONS_ROOTADMIN 3
#define COMMAND_PERMISSIONS_ADMIN 4
#define COMMAND_PERMISSIONS_VERIFIED_OWNER 5
#define COMMAND_PERMISSIONS_OWNER 6
#define COMMAND_PERMISSIONS_VERIFIED 7
#define COMMAND_PERMISSIONS_AUTO 8
#define COMMAND_PERMISSIONS_POTENTIAL_OWNER 9
#define COMMAND_PERMISSIONS_START_GAME 10
#define COMMAND_PERMISSIONS_UNVERIFIED 11

#define COMMANDS_ALLOWED_NONE 0
#define COMMANDS_ALLOWED_UNVERIFIED 1
#define COMMANDS_ALLOWED_VERIFIED 2

// net.h

#define HEALTH_STANDBY 0
#define HEALTH_PROGRESS 1
#define HEALTH_OKAY 2
#define HEALTH_ERROR 3

#define CONNECTION_TYPE_DEFAULT 0
#define CONNECTION_TYPE_LOOPBACK (1 << 0)
#define CONNECTION_TYPE_CUSTOM_PORT (1 << 1)
#define CONNECTION_TYPE_CUSTOM_IP_ADDRESS (1 << 2)
#define CONNECTION_TYPE_VPN (1 << 3)
#define CONNECTION_TYPE_IPV6 (1 << 4)

#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE 0
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL 1
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_API 2
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID 3

#define ACCEPT_IPV4 (1 << 0)
#define ACCEPT_IPV6 (1 << 1)
#define ACCEPT_ANY (ACCEPT_IPV4 | ACCEPT_IPV6)

#define HEALTH_CHECK_PUBLIC_IPV4 (1 << 0)
#define HEALTH_CHECK_PUBLIC_IPV6 (1 << 1)
#define HEALTH_CHECK_LOOPBACK_IPV4 (1 << 2)
#define HEALTH_CHECK_LOOPBACK_IPV6 (1 << 3)
#define HEALTH_CHECK_REALM (1 << 4)
#define HEALTH_CHECK_ALL (HEALTH_CHECK_PUBLIC_IPV4 | HEALTH_CHECK_PUBLIC_IPV6 | HEALTH_CHECK_LOOPBACK_IPV4 | HEALTH_CHECK_LOOPBACK_IPV6 | HEALTH_CHECK_REALM)
#define HEALTH_CHECK_VERBOSE (1 << 5)

#define GAME_TEST_TIMEOUT 3000
#define IP_ADDRESS_API_TIMEOUT 3000

#define RECONNECT_DISABLED 0
#define RECONNECT_ENABLED_GPROXY_BASIC 1
#define RECONNECT_ENABLED_GPROXY_EXTENDED 2

#define MAX_INCOMING_CONNECTIONS 255
#define GAME_USER_CONNECTION_MAX_TIMEOUT 5000u
#define GAME_USER_CONNECTION_MIN_TIMEOUT 500u
#define GAME_USER_TIMEOUT_VANILLA 70000u
#define GAME_USER_TIMEOUT_RECONNECTABLE 20000u

#define GAME_DEFAULT_UDP_PORT 6112u
#define UDP_DISCOVERY_MAX_EXTRA_ADDRESSES 30u

#define NET_PROTOCOL_TCP 0u
#define NET_PROTOCOL_UDP 1u

// Exponential backoff starts at twice this value (45x2=90 seconds)
constexpr int64_t NET_BASE_RECONNECT_DELAY = 45;
constexpr uint8_t NET_RECONNECT_MAX_BACKOFF = 12;

// realm_chat.h

constexpr uint8_t RECV_SELECTOR_SYSTEM = 1;
constexpr uint8_t RECV_SELECTOR_ONLY_WHISPER = 2;
constexpr uint8_t RECV_SELECTOR_ONLY_PUBLIC = 3;
constexpr uint8_t RECV_SELECTOR_ONLY_PUBLIC_OR_DROP = 4;
constexpr uint8_t RECV_SELECTOR_PREFER_PUBLIC = 5;

constexpr uint8_t CHAT_RECV_SELECTED_NONE = 0;
constexpr uint8_t CHAT_RECV_SELECTED_SYSTEM = 1;
constexpr uint8_t CHAT_RECV_SELECTED_PUBLIC = 2;
constexpr uint8_t CHAT_RECV_SELECTED_WHISPER = 3;
constexpr uint8_t CHAT_RECV_SELECTED_DROP = 4;

constexpr uint8_t CHAT_CALLBACK_NONE = 0;
constexpr uint8_t CHAT_CALLBACK_REFRESH_GAME = 1;
constexpr uint8_t CHAT_CALLBACK_RESET = 2;

constexpr uint8_t CHAT_VALIDATOR_NONE = 0;
constexpr uint8_t CHAT_VALIDATOR_LOBBY_JOINABLE = 1;

// pjass.h

constexpr uint8_t PJASS_PERMISSIVE = 0u;
constexpr uint8_t PJASS_STRICT = 1u;

constexpr uint8_t PJASS_OPTIONS_NOSYNTAXERROR = 0u;
constexpr uint8_t PJASS_OPTIONS_NOSEMANTICERROR = 1u;
constexpr uint8_t PJASS_OPTIONS_NORUNTIMEERROR = 2u;
constexpr uint8_t PJASS_OPTIONS_RB = 3u;
constexpr uint8_t PJASS_OPTIONS_NOMODULOOPERATOR = 4u;
constexpr uint8_t PJASS_OPTIONS_SHADOW = 5u;
constexpr uint8_t PJASS_OPTIONS_CHECKLONGNAMES = 6u;
constexpr uint8_t PJASS_OPTIONS_FILTER = 7u;
constexpr uint8_t PJASS_OPTIONS_CHECKGLOBALSINIT = 8u;
constexpr uint8_t PJASS_OPTIONS_CHECKSTRINGHASH = 9u;
constexpr uint8_t PJASS_OPTIONS_CHECKNUMBERLITERALS = 10u;

// w3mmd.h

#define MMD_ACTION_TYPE_VAR 0u
#define MMD_ACTION_TYPE_FLAG 1u
#define MMD_ACTION_TYPE_EVENT 2u
#define MMD_ACTION_TYPE_BLANK 3u
#define MMD_ACTION_TYPE_CUSTOM 4u

#define MMD_DEFINITION_TYPE_INIT 0u
#define MMD_DEFINITION_TYPE_VAR 1u
#define MMD_DEFINITION_TYPE_EVENT 2u

#define MMD_INIT_TYPE_VERSION 0u
#define MMD_INIT_TYPE_PLAYER 1u

#define MMD_VALUE_TYPE_INT 0u
#define MMD_VALUE_TYPE_REAL 1u
#define MMD_VALUE_TYPE_STRING 2u

#define MMD_OPERATOR_SET 0u
#define MMD_OPERATOR_ADD 1u
#define MMD_OPERATOR_SUBTRACT 2u

#define MMD_FLAG_LOSER 0u
#define MMD_FLAG_DRAWER 1u
#define MMD_FLAG_WINNER 2u
#define MMD_FLAG_LEAVER 3u
#define MMD_FLAG_PRACTICE 4u

#define MMD_PROCESSING_INITIAL_DELAY 60000
#define MMD_PROCESSING_STREAM_DEF_DELAY 60000
#define MMD_PROCESSING_STREAM_ACTION_DELAY 180000

constexpr uint32_t MMD_MAX_ARITY = 64u;

#endif // AURA_CONSTANTS_H_
