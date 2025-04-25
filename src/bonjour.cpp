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

	string players_num = ToDecString(slotsOff); // slots taken ???
	string players_max = to_string(slotsTotal);
	string secret = to_string(game->GetEntryKey());
	string time = to_string(game->GetUptime()); // creation time uint32_t ???

  vector<uint8_t> statInfo;
  AppendByteArray(statInfo, game->GetGameFlags(), false);
  statInfo.push_back(0);
  AppendByteArrayFast(statInfo, game->GetAnnounceWidth());
  AppendByteArrayFast(statInfo, game->GetAnnounceHeight());
  AppendByteArrayFast(statInfo, game->GetSourceFileHashBlizz(gameVersion));
  AppendByteArray(statInfo, game->GetSourceFilePath(), true);
  AppendByteArray(statInfo, game->GetIndexHostName(), true);
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
    AppendByteArray(game_data_d, gameName, true);
    game_data_d.push_back(0);
    AppendByteArrayFast(game_data_d, encodedStatString);
    game_data_d.push_back(0);
    AppendByteArray(game_data_d, (uint32_t)slotsTotal, false);
    AppendByteArrayFast(game_data_d, magicNumber);
    AppendByteArray(game_data_d, hostPort, false);
	} else {
    const uint32_t One = 1;
    AppendByteArray(game_data_d, One, false);
    AppendByteArray(game_data_d, gameName, true);
    AppendByteArrayFast(game_data_d, encodedStatString);
    game_data_d.push_back(0);
    AppendByteArray(game_data_d, (uint32_t)slotsTotal, false);
    AppendByteArray(game_data_d, gameName, true);
    game_data_d.push_back(0);
    AppendByteArray(game_data_d, hostPort, false);
  }

	string game_data = Base64::Encode(reinterpret_cast<unsigned char*>(game_data_d.data()), game_data_d.size(), false);

	string w66;
	if (gameVersion.second != 30) {
		w66	= string("\x0A") + ((char)gameName.size()) + gameName + string("\x10\0", 2) +
		"\x1A" + (char)(15 + players_num.size()) + "\x0A\x0Bplayers_num\x12" + (char)players_num.size() + players_num +
		"\x1A" + (char)(9 + gameName.size()) + "\x0A\x05_name\x12" + (char)gameName.size() + gameName +
		"\x1A" + (char)(15 + players_max.size()) + "\x0A\x0Bplayers_max\x12" + (char)players_max.size() + players_max + 
		"\x1A" + (char)(20 + time.size()) + "\x0A\x10game_create_time\x12" + (char)time.size() + time +
		"\x1A\x0A\x0A\x05_type\x12\x01\x31" + "\x1A\x0D\x0A\x08_subtype\x12\x01\x30" +
		"\x1A" + (char)(15 + secret.size()) + "\x0A\x0Bgame_secret\x12" + (char)secret.size() + secret +
		"\x1A" + (char)(14 + game_data.size()) + "\x01\x0A\x09game_data\x12" + (char)game_data.size() + "\x01" + game_data +
		"\x1A\x0C\x0A\x07game_id\x12\x01" + ((gameVersion.second >= 32) ? "1": "2") +
		"\x1A\x0B\x0A\x06_flags\x12\x01\x30";
	} else {
		w66 = string("\x0A") + ((char)gameName.size()) + gameName + string("\x10\0", 2) +
		"\x1A" + (char)(15 + secret.size()) + "\x0A\x0Bgame_secret\x12" + (char)secret.size() + secret +
		"\x1A\x0A\x0A\x05_type\x12\x01\x31" + "\x1A\x0D\x0A\x08_subtype\x12\x01\x30" +
		"\x1A\x0C\x0A\x07game_id\x12\x01" + "1" +
		"\x1A" + (char)(9 + gameName.size()) + "\x0A\x05_name\x12" + (char)gameName.size() + gameName +
		"\x1A" + (char)(15 + players_max.size()) + "\x0A\x0Bplayers_max\x12" + (char)players_max.size() + players_max +
		"\x1A\x0B\x0A\x06_flags\x12\x01\x30" +
		"\x1A" + (char)(15 + players_num.size()) + "\x0A\x0Bplayers_num\x12" + (char)players_num.size() + players_num +
		"\x1A" + (char)(20 + time.size()) + "\x0A\x10game_create_time\x12" + (char)time.size() + time +
		"\x1A" + (char)(14 + game_data.size()) + "\x01\x0A\x09game_data\x12" + (char)game_data.size() + "\x01" + game_data;
  }

  {
    int err = 0;
    if (isNew) {
      err = DNSServiceAddRecord(service, &record, 0, 66, (uint16_t)w66.size(), w66.c_str(), 0);
    } else {
      err = DNSServiceUpdateRecord(service, record, kDNSServiceFlagsForce, (uint16_t)w66.size(), w66.c_str(), 0);
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
#ifdef _WIN32
  return CheckDynamicLibrary(PLATFORM_STRING("dnssd.dll"));
#else
  return CheckDynamicLibrary(PLATFORM_STRING("dnssd.so"));
#endif
}

