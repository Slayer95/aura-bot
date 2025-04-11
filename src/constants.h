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

#include <string>

// includes.h

constexpr uint8_t LOG_LEVEL_EMERGENCY = 1;
constexpr uint8_t LOG_LEVEL_ALERT = 2;
constexpr uint8_t LOG_LEVEL_CRITICAL = 3;
constexpr uint8_t LOG_LEVEL_ERROR = 4;
constexpr uint8_t LOG_LEVEL_WARNING = 5;
constexpr uint8_t LOG_LEVEL_NOTICE = 6;
constexpr uint8_t LOG_LEVEL_INFO = 7;
constexpr uint8_t LOG_LEVEL_DEBUG = 8;
constexpr uint8_t LOG_LEVEL_TRACE = 9;
constexpr uint8_t LOG_LEVEL_TRACE2 = 10;
constexpr uint8_t LOG_LEVEL_TRACE3 = 11;

constexpr uint8_t ANTI_SPOOF_NONE = 0;
constexpr uint8_t ANTI_SPOOF_BASIC = 1;
constexpr uint8_t ANTI_SPOOF_EXTENDED = 2;
constexpr uint8_t ANTI_SPOOF_FULL = 3;

constexpr size_t MAX_READ_FILE_SIZE = 0x18000000;

constexpr double PERCENT_FACTOR = 100.;

constexpr unsigned char ProductID_ROC[4] = {51, 82, 65, 87};
constexpr unsigned char ProductID_TFT[4] = {80, 88, 51, 87};

// aura.h

constexpr uint8_t APP_ACTION_DONE = 0u;
constexpr uint8_t APP_ACTION_ERROR = 1u;
constexpr uint8_t APP_ACTION_WAIT = 2u;
constexpr uint8_t APP_ACTION_TIMEOUT = 3u;

constexpr uint8_t APP_ACTION_TYPE_UPNP = 0u;
constexpr uint8_t APP_ACTION_TYPE_HOST = 1u;

constexpr uint8_t APP_ACTION_MODE_TCP = 0u;
constexpr uint8_t APP_ACTION_MODE_UDP = 1u;

constexpr uint8_t SERVICE_TYPE_NONE = 0;
constexpr uint8_t SERVICE_TYPE_GAME = 1;
constexpr uint8_t SERVICE_TYPE_REALM = 2;
constexpr uint8_t SERVICE_TYPE_IRC = 3;
constexpr uint8_t SERVICE_TYPE_DISCORD = 4;
constexpr uint8_t SERVICE_TYPE_INVALID = 255;

constexpr uint8_t TASK_TYPE_GAME_FRAME = 0;
constexpr uint8_t TASK_TYPE_CHECK_JOINABLE = 1;
constexpr uint8_t TASK_TYPE_MAP_DOWNLOAD = 2;
constexpr uint8_t TASK_TYPE_HMC_HTTP = 3;

constexpr uint8_t LOG_C = 1u;
constexpr uint8_t LOG_P = 2u;
constexpr uint8_t LOG_R = 4u;
constexpr uint8_t LOG_ALL = LOG_C | LOG_P | LOG_R;

// fileutil.h

constexpr size_t FILE_SEARCH_FUZZY_MAX_RESULTS = 5;
constexpr std::string::size_type FILE_SEARCH_FUZZY_MAX_DISTANCE = 10;

// map.h

constexpr uint32_t MAX_MAP_SIZE_1_23 = 0x400000;
constexpr uint32_t MAX_MAP_SIZE_1_26 = 0x800000;
constexpr uint32_t MAX_MAP_SIZE_1_28 = 0x8000000;
constexpr uint32_t MAX_MAP_SIZE_RF = 0x20000000;

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

constexpr uint32_t MAPOPT_HIDEMINIMAP = (1 << 0);
constexpr uint32_t MAPOPT_MODIFYALLYPRIORITIES = (1 << 1);
constexpr uint32_t MAPOPT_MELEE = (1 << 2); // the bot cares about this one...
constexpr uint32_t MAPOPT_REVEALTERRAIN = (1 << 4);
constexpr uint32_t MAPOPT_FIXEDPLAYERSETTINGS = (1 << 5); // and this one...
constexpr uint32_t MAPOPT_CUSTOMFORCES = (1 << 6);        // and this one, the rest don't affect the bot's logic
constexpr uint32_t MAPOPT_CUSTOMTECHTREE = (1 << 7);
constexpr uint32_t MAPOPT_CUSTOMABILITIES = (1 << 8);
constexpr uint32_t MAPOPT_CUSTOMUPGRADES = (1 << 9);
constexpr uint32_t MAPOPT_WATERWAVESONCLIFFSHORES = (1 << 11);
constexpr uint32_t MAPOPT_HASTERRAINFOG = (1 << 12);
constexpr uint32_t MAPOPT_REQUIRESEXPANSION = (1 << 13);
constexpr uint32_t MAPOPT_ITEMCLASSIFICATION = (1 << 14);
constexpr uint32_t MAPOPT_WATERTINTING = (1 << 15);
constexpr uint32_t MAPOPT_ACCURATERANDOM = (1 << 16);
constexpr uint32_t MAPOPT_ABILITYSKINS = (1 << 17);

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

constexpr uint32_t MAPGAMETYPE_UNKNOWN0 = (1); // always set except for saved games?
constexpr uint32_t MAPGAMETYPE_BLIZZARD = (1 << 3);
constexpr uint32_t MAPGAMETYPE_MELEE = (1 << 5);
constexpr uint32_t MAPGAMETYPE_SAVEDGAME = (1 << 9);
constexpr uint32_t MAPGAMETYPE_PRIVATEGAME = (1 << 11);
constexpr uint32_t MAPGAMETYPE_MAKERUSER = (1 << 13);
constexpr uint32_t MAPGAMETYPE_MAKERBLIZZARD = (1 << 14);
constexpr uint32_t MAPGAMETYPE_TYPEMELEE = (1 << 15);
constexpr uint32_t MAPGAMETYPE_TYPESCENARIO = (1 << 16);
constexpr uint32_t MAPGAMETYPE_SIZESMALL = (1 << 17);
constexpr uint32_t MAPGAMETYPE_SIZEMEDIUM = (1 << 18);
constexpr uint32_t MAPGAMETYPE_SIZELARGE = (1 << 19);
constexpr uint32_t MAPGAMETYPE_OBSFULL = (1 << 20);
constexpr uint32_t MAPGAMETYPE_OBSONDEATH = (1 << 21);
constexpr uint32_t MAPGAMETYPE_OBSNONE = (1 << 22);

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

constexpr uint8_t MAP_CONFIG_SCHEMA_NUMBER = 4;

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

constexpr uint8_t AI_TYPE_NONE = 0u;
constexpr uint8_t AI_TYPE_MELEE = 1u;
constexpr uint8_t AI_TYPE_AMAI = 2u;
constexpr uint8_t AI_TYPE_CUSTOM = 3u;

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
constexpr uint8_t MAP_TRANSFER_CHECK_TOO_LARGE_VERSION = 4u;
constexpr uint8_t MAP_TRANSFER_CHECK_TOO_LARGE_CONFIG = 5u;
constexpr uint8_t MAP_TRANSFER_CHECK_BUFFERBLOAT = 6u;

// game.h
constexpr uint8_t SLOTS_ALIGNMENT_CHANGED = (1 << 0);
constexpr uint8_t SLOTS_DOWNLOAD_PROGRESS_CHANGED = (1 << 1);
constexpr uint8_t SLOTS_HCL_INJECTED = (1 << 2);

constexpr uint8_t SAVE_ON_LEAVE_NEVER = 0u;
constexpr uint8_t SAVE_ON_LEAVE_AUTO = 1u;
constexpr uint8_t SAVE_ON_LEAVE_ALWAYS = 2u;

constexpr uint8_t CUSTOM_LAYOUT_NONE = 0u;
constexpr uint8_t CUSTOM_LAYOUT_ONE_VS_ALL = 1u;
constexpr uint8_t CUSTOM_LAYOUT_HUMANS_VS_AI = 2u;
constexpr uint8_t CUSTOM_LAYOUT_FFA = 4u;
constexpr uint8_t CUSTOM_LAYOUT_DRAFT = 8u;
constexpr uint8_t CUSTOM_LAYOUT_LOCKTEAMS = 15u;
constexpr uint8_t CUSTOM_LAYOUT_COMPACT = 16u;
constexpr uint8_t CUSTOM_LAYOUT_ISOPLAYERS = 32u;

constexpr uint16_t REFRESH_PERIOD_MIN_SUGGESTED = 30;
constexpr uint16_t REFRESH_PERIOD_MAX_SUGGESTED = 300;
constexpr uint8_t GAME_BANNABLE_MAX_HISTORY_SIZE = 32;

constexpr size_t MAX_GAME_VERSION_ERROR_USERS_STORED = 500;

constexpr uint8_t GAME_PAUSES_PER_PLAYER = 3u;
constexpr uint8_t GAME_SAVES_PER_PLAYER = 1u;
constexpr uint8_t GAME_SAVES_PER_REFEREE_ANTIABUSE = 3u;
constexpr uint8_t GAME_SAVES_PER_REFEREE_DEFAULT = 255u;

constexpr uint8_t GAME_ONGOING = 0u;
constexpr uint8_t GAME_OVER_TRUSTED = 1u;
constexpr uint8_t GAME_OVER_MMD = 2u;

constexpr uint8_t HIDDEN_PLAYERS_NONE = 0u;
constexpr uint8_t HIDDEN_PLAYERS_LOBBY = 1u;
constexpr uint8_t HIDDEN_PLAYERS_GAME = 2u;
constexpr uint8_t HIDDEN_PLAYERS_ALL = 3u;

constexpr int64_t HIGH_PING_KICK_DELAY = 10000;
constexpr uint8_t MAX_SCOPE_BANS = 100u;
constexpr int64_t REALM_HOST_COOLDOWN_TICKS = 30000;
constexpr int64_t AUTO_REHOST_COOLDOWN_TICKS = 180000;

constexpr uint8_t ON_SEND_ACTIONS_NONE = 0u;
constexpr uint8_t ON_SEND_ACTIONS_PAUSE = 1u;
constexpr uint8_t ON_SEND_ACTIONS_RESUME = 2u;

constexpr int64_t PING_EQUALIZER_PERIOD_TICKS = 10000;
constexpr uint8_t PING_EQUALIZER_DEFAULT_FRAMES = 7u;

// 10 players x 150 APM x (1 min / 60000 ms) x (100 ms latency) = 2.5
constexpr size_t DEFAULT_ACTIONS_PER_FRAME = 3u;

constexpr uint8_t SYNCHRONIZATION_CHECK_MIN_FRAMES = 5u;

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

constexpr uint8_t GAME_RESULT_SOURCE_SELECT_NONE = 0u;
constexpr uint8_t GAME_RESULT_SOURCE_SELECT_ONLY_LEAVECODE = 1u;
constexpr uint8_t GAME_RESULT_SOURCE_SELECT_ONLY_MMD = 2u;
constexpr uint8_t GAME_RESULT_SOURCE_SELECT_PREFER_LEAVECODE = 3u;
constexpr uint8_t GAME_RESULT_SOURCE_SELECT_PREFER_MMD = 4u;

constexpr uint8_t GAME_RESULT_CONSTRAINTS_NONE = 0u;
constexpr uint8_t GAME_RESULT_CONSTRAINTS_NODRAW = 1u;
constexpr uint8_t GAME_RESULT_CONSTRAINTS_SAME_TEAM_WINNERS = 2u;
constexpr uint8_t GAME_RESULT_CONSTRAINTS_ALL = GAME_RESULT_CONSTRAINTS_NODRAW | GAME_RESULT_CONSTRAINTS_SAME_TEAM_WINNERS;

constexpr uint8_t GAME_RESULT_CONFLICT_HANDLER_VOID = 0u; // vulnerable to griefing
constexpr uint8_t GAME_RESULT_CONFLICT_HANDLER_PESSIMISTIC = 1u; // protects against self-boosting, but vulnerable to griefing
constexpr uint8_t GAME_RESULT_CONFLICT_HANDLER_OPTIMISTIC = 2u; // protects against griefing; together with max winner teams = 1, self-boosting results in void

// majority handlers assume that democracy is good
// but cheaters may complot, and ties may happen
// ties are resolved with the _OR_SOMETHING part
constexpr uint8_t GAME_RESULT_CONFLICT_HANDLER_MAJORITY_OR_VOID = 3u;
constexpr uint8_t GAME_RESULT_CONFLICT_HANDLER_MAJORITY_OR_PESSIMISTIC = 4u;
constexpr uint8_t GAME_RESULT_CONFLICT_HANDLER_MAJORITY_OR_OPTIMISTIC = 5u;

constexpr uint8_t GAME_RESULT_VIRTUAL_UNDECIDED_HANDLER_NONE = 0u;
constexpr uint8_t GAME_RESULT_VIRTUAL_UNDECIDED_HANDLER_LOSER_SELF = 1u;
constexpr uint8_t GAME_RESULT_VIRTUAL_UNDECIDED_HANDLER_AUTO = 2u;

constexpr uint8_t GAME_RESULT_USER_UNDECIDED_HANDLER_NONE = 0u;
constexpr uint8_t GAME_RESULT_USER_UNDECIDED_HANDLER_LOSER_SELF = 1u;
constexpr uint8_t GAME_RESULT_USER_UNDECIDED_HANDLER_LOSER_SELF_AND_ALLIES = 2u; // causes allied computers/virtual users to lose

constexpr uint8_t GAME_RESULT_COMPUTER_UNDECIDED_HANDLER_NONE = 0u;
constexpr uint8_t GAME_RESULT_COMPUTER_UNDECIDED_HANDLER_LOSER_SELF = 1u;
constexpr uint8_t GAME_RESULT_COMPUTER_UNDECIDED_HANDLER_AUTO = 2u; // can win!

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

// game_controller_data.h

enum class GameControllerType : uint8_t
{
  kVirtual = 0u,
  kUser = 1u,
  kComputer = 2u,
};

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

constexpr uint8_t CONSISTENT_PINGS_COUNT = 3u;
constexpr uint8_t MAXIMUM_PINGS_COUNT = 6u;
constexpr uint32_t MAX_PING_WEIGHT = 4u;

constexpr uint8_t SMART_COMMAND_NONE = 0u;
constexpr uint8_t SMART_COMMAND_GO = 1u;

constexpr int64_t GAME_USER_UNVERIFIED_KICK_TICKS = 60000;
constexpr int64_t AUTO_REALM_VERIFY_LATENCY = 5000;
constexpr int64_t CHECK_STATUS_LATENCY = 5000;
constexpr int64_t READY_REMINDER_PERIOD = 20000;

constexpr int64_t SYSTEM_RTT_POLLING_PERIOD = 10000;

constexpr uint8_t USERSTATUS_LOBBY = 0u;
constexpr uint8_t USERSTATUS_LOADING_SCREEN = 1u;
constexpr uint8_t USERSTATUS_PLAYING = 2u;
constexpr uint8_t USERSTATUS_ENDING = 3u;
constexpr uint8_t USERSTATUS_ENDED = 4u;

// game_async_observer.h

constexpr uint8_t ASYNC_OBSERVER_GOAL_OBSERVER = 0u;
constexpr uint8_t ASYNC_OBSERVER_GOAL_PLAYER = 1u;

constexpr uint8_t ASYNC_OBSERVER_OK = 0u;
constexpr uint8_t ASYNC_OBSERVER_DESTROY = 1u;
constexpr uint8_t ASYNC_OBSERVER_PROMOTED = 2u;

// game_setup.h

constexpr uint8_t SEARCH_TYPE_ONLY_MAP = 1;
constexpr uint8_t SEARCH_TYPE_ONLY_CONFIG = 2;
constexpr uint8_t SEARCH_TYPE_ONLY_FILE = 3;
constexpr uint8_t SEARCH_TYPE_ANY = 7;

constexpr uint8_t MATCH_TYPE_NONE = 0u;
constexpr uint8_t MATCH_TYPE_MAP = 1u;
constexpr uint8_t MATCH_TYPE_CONFIG = 2u;
constexpr uint8_t MATCH_TYPE_INVALID = 4u;
constexpr uint8_t MATCH_TYPE_FORBIDDEN = 8u;

constexpr uint8_t RESOLUTION_OK = 0;
constexpr uint8_t RESOLUTION_ERR = 1;
constexpr uint8_t RESOLUTION_BAD_NAME = 2;

constexpr bool SETUP_USE_STANDARD_PATHS = true;
constexpr bool SETUP_PROTECT_ARBITRARY_TRAVERSAL = false;

#ifdef _WIN32
#define FILE_EXTENSIONS_MAP {L".w3x", L".w3m"}
#define FILE_EXTENSIONS_CONFIG {L".ini"}
#else
#define FILE_EXTENSIONS_MAP {".w3x", ".w3m"}
#define FILE_EXTENSIONS_CONFIG {".ini"}
#endif

constexpr uint8_t GAMESETUP_STEP_MAIN = 0;
constexpr uint8_t GAMESETUP_STEP_RESOLUTION = 1;
constexpr uint8_t GAMESETUP_STEP_SUGGESTIONS = 2;
constexpr uint8_t GAMESETUP_STEP_DOWNLOAD = 3;

constexpr uint8_t MAP_ONREADY_SET_ACTIVE = 1;
constexpr uint8_t MAP_ONREADY_HOST = 2;
constexpr uint8_t MAP_ONREADY_ALIAS = 3;

constexpr int64_t SUGGESTIONS_TIMEOUT = 3000;
constexpr int64_t GAMESETUP_STALE_TICKS = 180000;

// game_protocol.h

constexpr int W3GS_UDP_MIN_PACKET_SIZE = 4;
constexpr size_t W3GS_ACTION_MAX_PACKET_SIZE = 1024;

constexpr uint8_t GAME_NONE = 0; // this case isn't part of the protocol, it's for internal use only
constexpr uint8_t GAME_FULL = 2;
constexpr uint8_t GAME_PUBLIC = 16;
constexpr uint8_t GAME_PRIVATE = 17;

constexpr uint8_t PLAYERLEAVE_DISCONNECT = 1u;
constexpr uint8_t PLAYERLEAVE_LOST = 7u;
constexpr uint8_t PLAYERLEAVE_LOSTBUILDINGS = 8u;
constexpr uint8_t PLAYERLEAVE_WON = 9u;
constexpr uint8_t PLAYERLEAVE_DRAW = 10u;
constexpr uint8_t PLAYERLEAVE_OBSERVER = 11u;
constexpr uint8_t PLAYERLEAVE_BADSAVE = 12u;
constexpr uint8_t PLAYERLEAVE_LOBBY = 13u;
constexpr uint8_t PLAYERLEAVE_GPROXY = 100u;

constexpr uint8_t REJECTJOIN_FULL = 9;
constexpr uint8_t REJECTJOIN_STARTED = 10;
constexpr uint8_t REJECTJOIN_WRONGPASSWORD = 27;

constexpr uint8_t ACTION_PAUSE = 0x01;
constexpr uint8_t ACTION_RESUME = 0x02;
constexpr uint8_t ACTION_SAVE = 0x06;
constexpr uint8_t ACTION_SAVE_ENDED = 0x07;
constexpr uint8_t ACTION_ABILITY_TARGET_NONE = 0x10;
constexpr uint8_t ACTION_ABILITY_TARGET_POINT = 0x11;
constexpr uint8_t ACTION_ABILITY_TARGET_OBJECT = 0x12;
constexpr uint8_t ACTION_GIVE_OR_DROP_ITEM = 0x13;
constexpr uint8_t ACTION_ABILITY_TARGET_DOUBLE = 0x14;
constexpr uint8_t ACTION_SELECTION = 0x16;
constexpr uint8_t ACTION_GROUP_HOTKEY_ASSIGN = 0x17;
constexpr uint8_t ACTION_GROUP_HOTKEY_SELECT = 0x18;
constexpr uint8_t ACTION_SELECTION_SUBGROUP = 0x19;
constexpr uint8_t ACTION_SELECTION_BEFORE_SUBGROUP = 0x1A;
constexpr uint8_t ACTION_UNKNOWN_0x1B = 0x1B;
constexpr uint8_t ACTION_SELECTION_GROUND_ITEM = 0x1C;
constexpr uint8_t ACTION_CANCEL_REVIVAL = 0x1D;
constexpr uint8_t ACTION_CANCEL_TRAINING = 0x1E;
constexpr uint8_t ACTION_UNKNOWN_0x21 = 0x21;
constexpr uint8_t ACTION_ALLIANCE_SETTINGS = 0x50;
constexpr uint8_t ACTION_TRANSFER_RESOURCES = 0x51;
constexpr uint8_t ACTION_CHAT_TRIGGER = 0x60;
constexpr uint8_t ACTION_ESCAPE = 0x61;
constexpr uint8_t ACTION_SCENARIO_TRIGGER = 0x62;
constexpr uint8_t ACTION_SKILLTREE = 0x66;
constexpr uint8_t ACTION_BUILDMENU = 0x67;
constexpr uint8_t ACTION_MINIMAPSIGNAL = 0x68;
constexpr uint8_t ACTION_ONEMORETURN_BLOCK_A = 0x69;
constexpr uint8_t ACTION_ONEMORETURN_BLOCK_B = 0x6A;
constexpr uint8_t ACTION_UNKNOWN_0x75 = 0x75;
constexpr uint8_t ACTION_MODAL_BTN = 0x6A;
constexpr uint8_t ACTION_GAME_CACHE = 0x6B;

constexpr uint8_t ACTION_SELECTION_MODE_ADD = 1;
constexpr uint8_t ACTION_SELECTION_MODE_REMOVE = 2;

constexpr uint32_t ALLIANCE_SETTINGS_ALLY = 0x1Fu;
constexpr uint32_t ALLIANCE_SETTINGS_SHARED_VISION = 0x20u;
constexpr uint32_t ALLIANCE_SETTINGS_SHARED_CONTROL = 0x40u;
constexpr uint32_t ALLIANCE_SETTINGS_SHARED_CONTROL_FAMILY = ALLIANCE_SETTINGS_ALLY | ALLIANCE_SETTINGS_SHARED_VISION | ALLIANCE_SETTINGS_SHARED_CONTROL;
constexpr uint32_t ALLIANCE_SETTINGS_SHARED_VICTORY = 0x400u;

constexpr size_t MAX_PLAYER_NAME_SIZE = 15;

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

// gps_protocol.h

constexpr uint32_t REJECTGPS_INVALID = 1;
constexpr uint32_t REJECTGPS_NOTFOUND = 2;
constexpr int64_t GPS_ACK_PERIOD = 10000u;

// chat.h

constexpr uint8_t FROM_GAME = (1 << 0);
constexpr uint8_t FROM_BNET = (1 << 1);
constexpr uint8_t FROM_IRC = (1 << 2);
constexpr uint8_t FROM_DISCORD = (1 << 3);
constexpr uint8_t FROM_OTHER = (1 << 7);

constexpr uint8_t CHAT_SEND_SOURCE_ALL = (1 << 0);
constexpr uint8_t CHAT_SEND_TARGET_ALL = (1 << 1);
constexpr uint8_t CHAT_LOG_INCIDENT = (1 << 2);
constexpr uint8_t CHAT_TYPE_INFO = (1 << 3);
constexpr uint8_t CHAT_TYPE_DONE = (1 << 4);
constexpr uint8_t CHAT_TYPE_ERROR = (1 << 5);
constexpr uint8_t CHAT_WRITE_TARGETS = (CHAT_SEND_SOURCE_ALL | CHAT_SEND_TARGET_ALL | CHAT_LOG_INCIDENT);
constexpr uint8_t CHAT_RESPONSE_TYPES = (CHAT_TYPE_INFO | CHAT_TYPE_DONE | CHAT_TYPE_ERROR);

constexpr uint16_t USER_PERMISSIONS_GAME_PLAYER = (1 << 0);
constexpr uint16_t USER_PERMISSIONS_GAME_OWNER = (1 << 1);
constexpr uint16_t USER_PERMISSIONS_CHANNEL_VERIFIED = (1 << 2);
constexpr uint16_t USER_PERMISSIONS_CHANNEL_ADMIN = (1 << 3);
constexpr uint16_t USER_PERMISSIONS_CHANNEL_ROOTADMIN = (1 << 4);
constexpr uint16_t USER_PERMISSIONS_BOT_SUDO_SPOOFABLE = (1 << 6);
constexpr uint16_t USER_PERMISSIONS_BOT_SUDO_OK = (1 << 7);

constexpr uint16_t SET_USER_PERMISSIONS_ALL = (0xFFFF);

constexpr uint8_t COMMAND_TOKEN_MATCH_NONE = 0;
constexpr uint8_t COMMAND_TOKEN_MATCH_PRIVATE = 1;
constexpr uint8_t COMMAND_TOKEN_MATCH_BROADCAST = 2;

// config_bot.h
constexpr uint8_t LOG_GAME_CHAT_NEVER = 0u;
constexpr uint8_t LOG_GAME_CHAT_ALLOWED = 1u;
constexpr uint8_t LOG_GAME_CHAT_ALWAYS = 2u;

constexpr uint8_t LOG_REMOTE_MODE_NONE = 0u;
constexpr uint8_t LOG_REMOTE_MODE_FILE = 1u;
constexpr uint8_t LOG_REMOTE_MODE_NETWORK = 2u;
constexpr uint8_t LOG_REMOTE_MODE_MIXED = 3u;

// config_realm.h

constexpr uint8_t REALM_TYPE_PVPGN = 0u;
constexpr uint8_t REALM_TYPE_BATTLENET_CLASSIC = 1u;

constexpr uint8_t REALM_AUTH_PVPGN = 0u;
constexpr uint8_t REALM_AUTH_BATTLENET = 1u;

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

constexpr uint8_t ON_SHARE_UNITS_NATIVE = 0u;
constexpr uint8_t ON_SHARE_UNITS_KICK = 1u;
constexpr uint8_t ON_SHARE_UNITS_RESTRICT = 2u;

constexpr uint8_t SELECT_EXPANSION_ROC = 0u;
constexpr uint8_t SELECT_EXPANSION_TFT = 1u;

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

constexpr uint16_t APM_RATE_LIMITER_MIN = 30;
constexpr uint16_t APM_RATE_LIMITER_MAX = 800;

constexpr int64_t APM_RATE_LIMITER_TICK_INTERVAL = 5000;
constexpr double APM_RATE_LIMITER_TICK_SCALING_FACTOR = APM_RATE_LIMITER_TICK_INTERVAL / 60000.;
constexpr double APM_RATE_LIMITER_BURST_ACTIONS = 100.;

constexpr size_t GAME_ACTION_HOLD_QUEUE_MAX_SIZE = 100;
constexpr size_t GAME_ACTION_HOLD_QUEUE_MIN_WARNING_SIZE = 50;
constexpr size_t GAME_ACTION_HOLD_QUEUE_MOD_WARNING_SIZE = 8;

// config_discord.h

constexpr uint8_t FILTER_ALLOW_ALL = 0;
constexpr uint8_t FILTER_DENY_ALL = 1;
constexpr uint8_t FILTER_ALLOW_LIST = 2;
constexpr uint8_t FILTER_DENY_LIST = 3;

// config_commands.h

constexpr uint8_t COMMAND_PERMISSIONS_DISABLED = 0;
constexpr uint8_t COMMAND_PERMISSIONS_SUDO = 1;
constexpr uint8_t COMMAND_PERMISSIONS_SUDO_UNSAFE = 2;
constexpr uint8_t COMMAND_PERMISSIONS_ROOTADMIN = 3;
constexpr uint8_t COMMAND_PERMISSIONS_ADMIN = 4;
constexpr uint8_t COMMAND_PERMISSIONS_VERIFIED_OWNER = 5;
constexpr uint8_t COMMAND_PERMISSIONS_OWNER = 6;
constexpr uint8_t COMMAND_PERMISSIONS_VERIFIED = 7;
constexpr uint8_t COMMAND_PERMISSIONS_AUTO = 8;
constexpr uint8_t COMMAND_PERMISSIONS_POTENTIAL_OWNER = 9;
constexpr uint8_t COMMAND_PERMISSIONS_START_GAME = 10;
constexpr uint8_t COMMAND_PERMISSIONS_UNVERIFIED = 11;

constexpr uint8_t COMMANDS_ALLOWED_NONE = 0;
constexpr uint8_t COMMANDS_ALLOWED_UNVERIFIED = 1;
constexpr uint8_t COMMANDS_ALLOWED_VERIFIED = 2;

// net.h

constexpr uint8_t HEALTH_STANDBY = 0;
constexpr uint8_t HEALTH_PROGRESS = 1;
constexpr uint8_t HEALTH_OKAY = 2;
constexpr uint8_t HEALTH_ERROR = 3;

constexpr uint8_t CONNECTION_TYPE_DEFAULT = 0;
constexpr uint8_t CONNECTION_TYPE_LOOPBACK = (1 << 0);
constexpr uint8_t CONNECTION_TYPE_CUSTOM_PORT = (1 << 1);
constexpr uint8_t CONNECTION_TYPE_CUSTOM_IP_ADDRESS = (1 << 2);
constexpr uint8_t CONNECTION_TYPE_VPN = (1 << 3);
constexpr uint8_t CONNECTION_TYPE_IPV6 = (1 << 4);

constexpr uint8_t NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE = 0;
constexpr uint8_t NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL = 1;
constexpr uint8_t NET_PUBLIC_IP_ADDRESS_ALGORITHM_API = 2;
constexpr uint8_t NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID = 3;

constexpr uint8_t ACCEPT_IPV4 = (1 << 0);
constexpr uint8_t ACCEPT_IPV6 = (1 << 1);
constexpr uint8_t ACCEPT_ANY = (ACCEPT_IPV4 | ACCEPT_IPV6);

constexpr uint8_t HEALTH_CHECK_PUBLIC_IPV4 = (1 << 0);
constexpr uint8_t HEALTH_CHECK_PUBLIC_IPV6 = (1 << 1);
constexpr uint8_t HEALTH_CHECK_LOOPBACK_IPV4 = (1 << 2);
constexpr uint8_t HEALTH_CHECK_LOOPBACK_IPV6 = (1 << 3);
constexpr uint8_t HEALTH_CHECK_REALM = (1 << 4);
constexpr uint8_t HEALTH_CHECK_ALL = (HEALTH_CHECK_PUBLIC_IPV4 | HEALTH_CHECK_PUBLIC_IPV6 | HEALTH_CHECK_LOOPBACK_IPV4 | HEALTH_CHECK_LOOPBACK_IPV6 | HEALTH_CHECK_REALM);
constexpr uint8_t HEALTH_CHECK_VERBOSE = (1 << 5);

constexpr int64_t GAME_TEST_TIMEOUT = 3000;
constexpr int64_t IP_ADDRESS_API_TIMEOUT = 3000;

constexpr uint8_t RECONNECT_DISABLED = 0;
constexpr uint8_t RECONNECT_ENABLED_GPROXY_BASIC = 1;
constexpr uint8_t RECONNECT_ENABLED_GPROXY_EXTENDED = 2;

constexpr size_t MAX_INCOMING_CONNECTIONS = 255;
constexpr float GAME_USER_CONNECTION_MAX_TIMEOUT = 5000.;
constexpr float GAME_USER_CONNECTION_MIN_TIMEOUT = 500.;
constexpr int64_t GAME_USER_TIMEOUT_VANILLA = 70000;
constexpr int64_t GAME_USER_TIMEOUT_RECONNECTABLE = 20000;

constexpr uint16_t GAME_DEFAULT_UDP_PORT = 6112u;
constexpr uint8_t UDP_DISCOVERY_MAX_EXTRA_ADDRESSES = 30u;

constexpr uint8_t NET_PROTOCOL_TCP = 0u;
constexpr uint8_t NET_PROTOCOL_UDP = 1u;

// Exponential backoff starts at twice this value (45x2=90 seconds)
constexpr int64_t NET_BASE_RECONNECT_DELAY = 45;
constexpr uint8_t NET_RECONNECT_MAX_BACKOFF = 12;

// realm.h

constexpr uint32_t REALM_TCP_KEEPALIVE_IDLE_TIME = 900;
constexpr int64_t REALM_APP_KEEPALIVE_IDLE_TIME = 180;
constexpr int64_t REALM_APP_KEEPALIVE_INTERVAL = 30;
constexpr int64_t REALM_APP_KEEPALIVE_MAX_MISSED = 4;

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

// dota.h

constexpr uint8_t DOTA_WINNER_UNDECIDED = 0u;
constexpr uint8_t DOTA_WINNER_SENTINEL = 1u;
constexpr uint8_t DOTA_WINNER_SCOURGE = 2u;

// w3mmd.h

constexpr uint8_t MMD_ACTION_TYPE_VAR = 0u;
constexpr uint8_t MMD_ACTION_TYPE_FLAG = 1u;
constexpr uint8_t MMD_ACTION_TYPE_EVENT = 2u;
constexpr uint8_t MMD_ACTION_TYPE_BLANK = 3u;
constexpr uint8_t MMD_ACTION_TYPE_CUSTOM = 4u;

constexpr uint8_t MMD_DEFINITION_TYPE_INIT = 0u;
constexpr uint8_t MMD_DEFINITION_TYPE_VAR = 1u;
constexpr uint8_t MMD_DEFINITION_TYPE_EVENT = 2u;

constexpr uint8_t MMD_INIT_TYPE_VERSION = 0u;
constexpr uint8_t MMD_INIT_TYPE_PLAYER = 1u;

constexpr uint8_t MMD_VALUE_TYPE_INT = 0u;
constexpr uint8_t MMD_VALUE_TYPE_REAL = 1u;
constexpr uint8_t MMD_VALUE_TYPE_STRING = 2u;

constexpr uint8_t MMD_OPERATOR_SET = 0u;
constexpr uint8_t MMD_OPERATOR_ADD = 1u;
constexpr uint8_t MMD_OPERATOR_SUBTRACT = 2u;

constexpr uint8_t MMD_FLAG_LOSER = 0u;
constexpr uint8_t MMD_FLAG_DRAWER = 1u;
constexpr uint8_t MMD_FLAG_WINNER = 2u;
constexpr uint8_t MMD_FLAG_LEAVER = 3u;
constexpr uint8_t MMD_FLAG_PRACTICE = 4u;

constexpr int64_t MMD_PROCESSING_INITIAL_DELAY = 60000;
constexpr int64_t MMD_PROCESSING_STREAM_DEF_DELAY = 60000;
constexpr int64_t MMD_PROCESSING_STREAM_ACTION_DELAY = 180000;

constexpr uint32_t MMD_MAX_ARITY = 64u;

#endif // AURA_CONSTANTS_H_
