
#ifndef AURA_MDNS_H
#define AURA_MDNS_H

#include "includes.h"
#include "game.h"
#include <mdns/dns_sd.h>
#include <vector>
#include <tuple>

//
// CMDNS
//

class CMDNS
{
public:
  CAura*                      m_Aura;
  DNSServiceRef               m_Service;
  DNSRecordRef                m_Record;
  uint8_t                     m_InterfaceType;
  uint16_t                    m_Port;
  bool                        m_GameIsExpansion;
  Version                     m_GameVersion;

  CMDNS(CAura* nAura, const CGame* nGame, uint8_t interfaceType, uint16_t port, const Version& version);
  ~CMDNS();

#ifdef DISABLE_MDNS
  inline DNSServiceRef static CreateManager() { return nullptr; }
  inline void static Destroy(void* /*ptr*/) {}
#else
  DNSServiceRef static CreateManager();
  void static Destroy(DNSServiceRef service);
#endif

#ifndef DISABLE_MDNS
  void PushRecord(std::shared_ptr<const CGame> nGame);
  std::vector<uint8_t> GetGameBroadcastData(std::shared_ptr<const CGame> nGame, const std::string& gameName);
#endif

  bool static CheckLibrary();
  std::string GetRegisterType();
  std::string static ErrorCodeToString(DNSServiceErrorType errCode); // DNSServiceErrorType is simply int32_t
};

#endif // AURA_MDNS_H
