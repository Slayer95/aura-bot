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

#ifndef AURA_COMMAND_STRUCTS_H_
#define AURA_COMMAND_STRUCTS_H_

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
};

//
// RealmUserSearchResult
//

struct RealmUserSearchResult
{
  bool success;
  std::string userName;
  std::string hostName;
  std::shared_ptr<CRealm> realm;

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
};

//
// ServiceUserSearchResult
//

struct ServiceUserSearchResult
{
  uint8_t status;
  ServiceUserSearchResult()
   : status(0)
  {}

  ~ServiceUserSearchResult() = default;
};

#endif
