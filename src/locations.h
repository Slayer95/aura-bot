/*

  Copyright [2025] [Leonardo Julca]

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

#ifndef AURA_LOCATIONS_H_
#define AURA_LOCATIONS_H_

#include "includes.h"

//
// GameControllerSearchResult
//

struct GameControllerSearchResult
{
  bool success;
  uint8_t SID;
  GameUser::CGameUser* user;

  GameControllerSearchResult()
   : success(false),
     SID(0),
     user(nullptr)
  {}

  GameControllerSearchResult(uint8_t nSID, GameUser::CGameUser* nUser)
   : success(true),
     SID(nSID),
     user(nUser)
  {}

  ~GameControllerSearchResult() = default;

  [[nodiscard]] inline bool GetSuccess() const { return success; }
};

//
// RealmUserSearchResult
//

struct RealmUserSearchResult
{
  bool success;
  std::string userName;
  std::string hostName;
  std::weak_ptr<CRealm> realm;

  RealmUserSearchResult()
   : success(false)
  {}

  RealmUserSearchResult(const std::string& nUserName, const std::string& nHostName)
   : success(false),
     userName(nUserName),
     hostName(nHostName)
  {}

  RealmUserSearchResult(const std::string& nUserName, const std::string& nHostName, std::shared_ptr<CRealm> nRealm)
   : success(true),
     userName(nUserName),
     hostName(nHostName),
     realm(nRealm)
  {}

  ~RealmUserSearchResult() = default;

  [[nodiscard]] inline bool GetSuccess() const { return success; }
  [[nodiscard]] inline std::string GetUser() const { return userName; }
  [[nodiscard]] inline std::string GetHostName() const { return hostName; }
  [[nodiscard]] inline std::shared_ptr<CRealm> GetRealm() const { return realm.lock(); }
};

//
// ServiceUser
//

struct ServiceUser
{
  uint8_t serviceType;
  std::weak_ptr<void> servicePtr;
  std::string userName;

  ServiceUser();
  ServiceUser(const ServiceUser& otherService);
  ServiceUser(uint8_t serviceType, std::string nUserName);
  ServiceUser(uint8_t serviceType, std::string nUserName, std::shared_ptr<void> nServicePtr);
  ~ServiceUser();

  template <typename T>
  [[nodiscard]] inline std::shared_ptr<T> GetService() const
  {
    return static_pointer_cast<T>(servicePtr.lock());
  }
  inline bool GetIsExpired() const { return servicePtr.expired(); }
  inline uint8_t GetServiceType() const { return serviceType; }
  inline std::string GetUser() const { return userName; }
  void Reset();
};

#endif
