
#ifndef AURA_BONJOUR_H
#define AURA_BONJOUR_H

#include "includes.h"
#include "game.h"
#include <bonjour/dns_sd.h>
#include <vector>
#include <tuple>

//
// CBonjour
//

class CBonjour
{
public:
  DNSServiceRef               m_Service;
  DNSRecordRef                m_Record;
  uint8_t                     m_InterfaceType;
  uint16_t                    m_Port;
  bool                        m_GameIsExpansion;
  Version                     m_GameVersion;

  CBonjour(CAura* nAura, std::shared_ptr<CGame> nGame, uint8_t interfaceType, uint16_t port, const Version& version);
  ~CBonjour();

#ifdef DISABLE_BONJOUR
  inline DNSServiceRef static CreateManager() { return nullptr; }
  inline void static Destroy(void* /*ptr*/) {}
#else
  DNSServiceRef static CreateManager();
  void static Destroy(DNSServiceRef service);
#endif

#ifndef DISABLE_BONJOUR
  void PushRecord(std::shared_ptr<CGame> nGame);
  std::vector<uint8_t> GetGameBroadcastData(std::shared_ptr<CGame> nGame, const std::string& gameName);
#endif

  bool static CheckLibrary();
  std::string GetRegisterType();
  std::string static ErrorCodeToString(DNSServiceErrorType errCode); // DNSServiceErrorType is simply int32_t
};

#endif // AURA_BONJOUR_H
