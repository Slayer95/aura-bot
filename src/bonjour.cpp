#include <base64/base64.h>

#include "bonjour.h"
#include "game.h"
#include "os_util.h"

using namespace std;

CBonjour::CBonjour(string interfs)
 : m_Client(nullptr),
   m_Admin(nullptr)
{
  //DNSServiceRegister(&m_Client, 0, kDNSServiceInterfaceIndexAny, "_blizzard", "_udp.local", "", NULL, (uint16_t)6112, 0, "", nullptr, NULL);
  interf = interfs;
#ifndef DISABLE_BONJOUR  
  int err = DNSServiceCreateConnection(&m_Admin);
  if (err) Print("[MDNS] DNSServiceCreateConnection 1 failed: " + to_string(err) + "\n");
  err = DNSServiceCreateConnection(&m_Client);
  if (err) Print("[MDNS] DNSServiceCreateConnection 0 failed: " + to_string(err) + "\n");

  Print("[MDNS] DNS registration finished");
#endif
}

CBonjour::~CBonjour()
{
#ifndef DISABLE_BONJOUR
  if (m_Client) DNSServiceRefDeallocate(m_Client);
  if (m_Admin) DNSServiceRefDeallocate(m_Admin);
#endif
}

#ifndef DISABLE_BONJOUR
optional<pair<DNSServiceRef, DNSRecordRef>> CBonjour::FindGame(shared_ptr<CGame> game)
{
  optional<pair<DNSServiceRef, DNSRecordRef>> result;
  for (auto it = games.begin(); it != games.end(); it++) {
    if (get<1>(*it).lock() == game) {
      get<2>(*it) = GetTime();
      result = make_pair(get<0>(*it), get<3>(*it));
    }
  }
  return result;
}

void CBonjour::BroadcastGameInner(shared_ptr<CGame> game, const string& gameName, const Version& gameVersion, DNSServiceRef service, DNSRecordRef record, bool isNew)
{
  uint8_t slotsOff = static_cast<uint32_t>(game->GetNumSlots() == game->GetSlotsOpen() ? game->GetNumSlots() : game->GetSlotsOpen() + 1);
  uint32_t slotsTotal = game->GetNumSlots();
  uint16_t hostPort = game->GetHostPortForDiscoveryInfo(AF_INET);

  //string players_num = ToDecString(slotsOff); // slots taken ???
  string secret = to_string(game->GetEntryKey());

  vector<uint8_t> statInfo;
  AppendByteArray(statInfo, game->GetGameFlags(), false);
  statInfo.push_back(0);
  AppendByteArrayFast(statInfo, game->GetAnnounceWidth());
  AppendByteArrayFast(statInfo, game->GetAnnounceHeight());
  AppendByteArrayFast(statInfo, game->GetSourceFileHashBlizz(gameVersion));
  AppendByteArrayString(statInfo, game->GetSourceFilePath(), true);
  AppendByteArrayString(statInfo, game->GetIndexHostName(), true);
  statInfo.push_back(0);
  AppendByteArrayFast(statInfo, game->GetMap()->GetMapSHA1());

  if (statInfo.size() >= 0xFFFF) {
    return;
  }

  vector<uint8_t> encodedStatString = EncodeStatString(statInfo);

  vector<uint8_t> game_data_d;
  if (gameVersion.second >= 32) {
    // try this for games with short names on pre32?
    const std::array<uint8_t, 4> magicNumber = {0x01, 0x20, 0x43, 0x00}; // 1.32.6700 ??
    AppendByteArrayString(game_data_d, gameName, true);
    game_data_d.push_back(0);
    AppendByteArrayFast(game_data_d, encodedStatString);
    game_data_d.push_back(0);
    AppendByteArray(game_data_d, (uint32_t)slotsTotal, false);
    AppendByteArrayFast(game_data_d, magicNumber);
    AppendByteArray(game_data_d, hostPort, false);
  } else {
    const uint32_t One = 1;
    AppendByteArray(game_data_d, One, false);
    AppendByteArrayString(game_data_d, gameName, true);
    AppendByteArrayFast(game_data_d, encodedStatString);
    game_data_d.push_back(0);
    AppendByteArray(game_data_d, (uint32_t)slotsTotal, false);
    AppendByteArrayString(game_data_d, gameName, true);
    game_data_d.push_back(0);
    AppendByteArray(game_data_d, hostPort, false);
  }

  string game_data = Base64::Encode(reinterpret_cast<unsigned char*>(game_data_d.data()), game_data_d.size(), false);

  vector<uint8_t> w66;
  w66.reserve(512);

  // Game name
  w66.push_back(0x0A);
  w66.push_back((uint8_t)gameName.size());
  AppendByteArrayString(w66, gameName, false);
  w66.push_back(0x10);
  w66.push_back(0);

  if (gameVersion.second != 30) {
    AppendProtoBufferFromLengthDelimitedS2S(w66, "players_num", ToDecString(slotsOff)); // slots taken??
    AppendProtoBufferFromLengthDelimitedS2S(w66, "_name", gameName);
    AppendProtoBufferFromLengthDelimitedS2S(w66, "players_max", to_string(slotsTotal));
    AppendProtoBufferFromLengthDelimitedS2S(w66, "game_create_time", to_string(game->GetUptime())); // creation time uint32_t ???
    AppendProtoBufferFromLengthDelimitedS2C(w66, "_type", 0x31);
    AppendProtoBufferFromLengthDelimitedS2C(w66, "_subtype", 0x30);
    AppendProtoBufferFromLengthDelimitedS2S(w66, "game_secret", secret);

    // game_data
    w66.push_back(0x1A);
    w66.push_back((uint8_t)(14 + game_data.size()));
    w66.push_back(0x01); // Extra 0x01 ?!
    w66.push_back(0x0A);
    w66.push_back(0x09);
    AppendByteArrayString(w66, "game_data", false);
    w66.push_back(0x12);
    w66.push_back((uint8_t)game_data.size());
    w66.push_back(0x01); // Extra 0x01 ?!
    AppendByteArrayString(w66, game_data, false);

    AppendProtoBufferFromLengthDelimitedS2C(w66, "game_id", ((gameVersion.second >= 32) ? 0x31: 0x32));
    AppendProtoBufferFromLengthDelimitedS2C(w66, "_flags", 0x30);
  } else {
    AppendProtoBufferFromLengthDelimitedS2S(w66, "game_secret", secret);
    AppendProtoBufferFromLengthDelimitedS2C(w66, "_type", 0x31);
    AppendProtoBufferFromLengthDelimitedS2C(w66, "_subtype", 0x30);
    AppendProtoBufferFromLengthDelimitedS2C(w66, "game_id", 0x31);
    AppendProtoBufferFromLengthDelimitedS2S(w66, "_name", gameName);
    AppendProtoBufferFromLengthDelimitedS2S(w66, "players_max", to_string(slotsTotal));
    AppendProtoBufferFromLengthDelimitedS2C(w66, "_flags", 0x30);
    AppendProtoBufferFromLengthDelimitedS2S(w66, "players_num", ToDecString(slotsOff)); // slots taken??
    AppendProtoBufferFromLengthDelimitedS2S(w66, "game_create_time", to_string(game->GetUptime())); // creation time uint32_t ???

    // game_data
    w66.push_back(0x1A);
    w66.push_back((uint8_t)(14 + game_data.size()));
    w66.push_back(0x01); // Extra 0x01 ?!
    w66.push_back(0x0A);
    w66.push_back(0x09);
    AppendByteArrayString(w66, "game_data", false);
    w66.push_back(0x12);
    w66.push_back((uint8_t)game_data.size());
    w66.push_back(0x01); // Extra 0x01 ?!
    AppendByteArrayString(w66, game_data, false);
  }

  {
    int err = 0;
    if (isNew) {
      err = DNSServiceAddRecord(service, &record, 0, 66, (uint16_t)w66.size(), reinterpret_cast<const char*>(w66.data()), 0);
    } else {
      err = DNSServiceUpdateRecord(service, record, kDNSServiceFlagsForce, (uint16_t)w66.size(), reinterpret_cast<const char*>(w66.data()), 0);
    }
    if (err) {
      Print("[MDNS] DNSServiceAddRecord failed: " + to_string(err) + "\n");
      return;
    }
    games.emplace_back(service, game, GetTime(), record);
  }
}

void CBonjour::BroadcastGame(shared_ptr<CGame> game, const Version& gameVersion)
{
  optional<pair<DNSServiceRef, DNSRecordRef>> storedGame = FindGame(game);

  DNSServiceRef service1 = nullptr;
  DNSRecordRef record = nullptr;
  string gameName = game->GetDiscoveryNameLAN();

  bool isNew = !storedGame.has_value();
  if (!isNew) {
    service1 = storedGame->first;
    record = storedGame->second;
  } else {
    string tmp = string("_blizzard._udp") + (game->GetIsExpansion() ? ",_w3xp" : ",_war3") + ToHexString(gameVersion.second + 10000);
    const char* temp = tmp.c_str();
    
    service1 = m_Client;
    int err = DNSServiceRegister(
      &(storedGame->first), kDNSServiceFlagsShareConnection, kDNSServiceInterfaceIndexAny, gameName.c_str(),
      temp, "local", NULL, ntohs(8152), 0, NULL, nullptr, NULL
    );
    if (err) {
      Print("[MDNS] DNSServiceRegister 1 failed: " + to_string(err) + "\n");
      return;
    }
  }

  BroadcastGameInner(game, gameName, gameVersion, service1, record, isNew);
}

void CBonjour::StopBroadcastGame(shared_ptr<CGame> game)
{
  for (auto it = games.begin(); it != games.end(); it++) {
    if (get<1>(*it).lock() == game) {
      DNSServiceRefDeallocate(get<0>(*it));
      it = games.erase(it);
    }
  }
}

#endif

bool CBonjour::CheckLibrary()
{
  PLATFORM_STRING_TYPE serviceName = PLATFORM_STRING("Bonjour");
  return CheckDynamicLibrary(PLATFORM_STRING("dnssd"), serviceName);
}
