#ifndef AURA_BONJOUR_H
#define AURA_BONJOUR_H

#include "includes.h"
#include "game.h"
#ifndef DISABLE_BONJOUR
#include <bonjour/dns_sd.h>
#endif
#include <vector>
#include <tuple>

//
// CBonjour
//

class CBonjour
{
#ifndef DISABLE_BONJOUR  
	std::vector<std::tuple<DNSServiceRef, std::weak_ptr<CGame>, uint32_t, DNSRecordRef>> games;
#endif
public:
#ifndef DISABLE_BONJOUR  
	DNSServiceRef               m_Client;
	DNSServiceRef               m_Admin;
#else
	void*                       m_Client;
	void*                       m_Admin;
#endif
	std::string interf;
	CBonjour(std::string interfs);
	~CBonjour();

#ifndef DISABLE_BONJOUR
  std::optional<std::pair<DNSServiceRef, DNSRecordRef>> FindGame(std::shared_ptr<CGame> nGame);
	void BroadcastGame(std::shared_ptr<CGame> nGame, const Version& gameVersion);
  void BroadcastGameInner(std::shared_ptr<CGame> nGame, const std::string& gameName, const Version& gameVersion, DNSServiceRef service, DNSRecordRef record, bool isNew);
  void StopBroadcastGame(std::shared_ptr<CGame> nGame);
#endif

  bool static CheckLibrary();
};

#endif // AURA_BONJOUR_H
