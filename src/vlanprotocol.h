#ifndef VLANPROTOCOL_H
#define VLANPROTOCOL_H

//
// CVirtualLanProtocol
//

#define VLAN_HEADER_CONSTANT 250

class CIncomingVLanSearchGame;
class CIncomingVLanGameInfo;

namespace VLANProtocol
{
  namespace Magic
  {
    constexpr uint8_t SEARCHGAME                                     = 47;  // 0x2F (UDP/LAN)
    constexpr uint8_t GAMEINFO                                       = 48;  // 0x30 (UDP/LAN)
    constexpr uint8_t CREATEGAME                                     = 49;  // 0x31 (UDP/LAN)
    constexpr uint8_t REFRESHGAME                                    = 50;  // 0x32 (UDP/LAN)
    constexpr uint8_t DECREATEGAME                                   = 51;  // 0x33 (UDP/LAN)
  };

  // receive functions

  CIncomingVLanSearchGame* RECEIVE_VLAN_SEARCHGAME(const std::vector<uint8_t>& data);
  CIncomingVLanGameInfo* RECEIVE_VLAN_GAMEINFO(const std::vector<uint8_t>& data);

  // send functions

  std::vector<uint8_t> SEND_VLAN_SEARCHGAME(bool TFT, uint32_t war3Version );
  std::vector<uint8_t> SEND_VLAN_GAMEINFO(bool TFT, uint32_t war3Version, uint32_t mapGameType, uint32_t mapFlags, uint16_t mapWidth, uint16_t mapHeight, std::string gameName, std::string hostName, uint32_t elapsedTime, std::string mapPath, std::vector<uint8_t> mapCRC, uint32_t slotsTotal, uint32_t slotsOpen, std::vector<uint8_t> ip, uint16_t port, uint32_t hostCounter, uint32_t entryKey);
  std::vector<uint8_t> SEND_VLAN_CREATEGAME(bool TFT, uint32_t war3Version, uint32_t hostCounter, std::vector<uint8_t> ip, uint16_t port);
  std::vector<uint8_t> SEND_VLAN_REFRESHGAME(uint32_t hostCounter, uint32_t players, uint32_t playerSlots, std::vector<uint8_t> ip, uint16_t port);
  std::vector<uint8_t> SEND_VLAN_DECREATEGAME(uint32_t hostCounter, std::vector<uint8_t> ip, uint16_t port);

  // other functions

  bool ValidateLength(const std::vector<uint8_t>& content);
};

//
// CIncomingVLanSearchGame
//

class CIncomingVLanSearchGame
{
private:
  bool m_TFT;
  uint32_t m_Version;

public:
  CIncomingVLanSearchGame(bool nTFT, uint32_t nVersion);
  ~CIncomingVLanSearchGame();

  bool GetTFT()                                         { return m_TFT; }
  uint32_t GetVersion()                                 { return m_Version; }
};

//
// CIncomingVLanGameInfo
//
class CIncomingVLanGameInfo
{
private:
  bool m_TFT;
  uint32_t m_Version;
  uint32_t m_MapGameType;
  std::string m_GameName;
  std::vector<uint8_t> m_StatString;
  uint32_t m_ReceivedTime;
  uint32_t m_ElapsedTime;
  uint32_t m_SlotsTotal;
  uint32_t m_SlotsOpen;
  std::vector<uint8_t> m_IP;
  uint16_t m_Port;
  uint32_t m_HostCounter;
  uint32_t m_EntryKey;

  // decoded from stat string:

  uint32_t m_MapFlags;
  uint16_t m_MapWidth;
  uint16_t m_MapHeight;
  std::vector<uint8_t> m_MapCRC;
  std::string m_MapPath;
  std::string m_HostName;

public:
  CIncomingVLanGameInfo(bool nTFT, uint32_t nVersion, uint32_t nMapGameType, std::string nGameName, uint32_t nElapsedTime, uint32_t nSlotsTotal, uint32_t nSlotsOpen, std::vector<uint8_t> &nIP, uint16_t nPort, uint32_t nHostCounter, uint32_t nEntryKey, std::vector<uint8_t> &nStatString);
  ~CIncomingVLanGameInfo();

  bool GetTFT( )                                        { return m_TFT; }
  uint32_t GetVersion( )                                { return m_Version; }
  uint32_t GetMapGameType( )                            { return m_MapGameType; }
  uint32_t GetMapFlags( )                               { return m_MapFlags; }
  uint16_t GetMapWidth( )                               { return m_MapWidth; }
  uint16_t GetMapHeight( )                              { return m_MapHeight; }
  std::string GetGameName( )                            { return m_GameName; }
  std::vector<uint8_t> GetStatString( )                 { return m_StatString; }
  std::string GetHostName( )                            { return m_HostName; }
  uint32_t GetReceivedTime( )                           { return m_ReceivedTime; }
  uint32_t GetElapsedTime( )                            { return m_ElapsedTime; }
  std::string GetMapPath( )                             { return m_MapPath; }
  std::vector<uint8_t> GetMapCRC( )                     { return m_MapCRC; }
  uint32_t GetSlotsTotal( )                             { return m_SlotsTotal; }
  uint32_t GetSlotsOpen( )                              { return m_SlotsOpen; }
  std::vector<uint8_t> GetIP( )                         { return m_IP; }
  uint16_t GetPort( )                                   { return m_Port; }
  uint32_t GetHostCounter( )                            { return m_HostCounter; }
  uint32_t GetEntryKey( )                               { return m_EntryKey; }

  void SetElapsedTime( uint32_t nElapsedTime )          { m_ElapsedTime = nElapsedTime; }
  void SetSlotsTotal( uint32_t nSlotsTotal )            { m_SlotsTotal = nSlotsTotal; }
  void SetSlotsOpen( uint32_t nSlotsOpen )              { m_SlotsOpen = nSlotsOpen; }
};
#endif
