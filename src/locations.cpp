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

#include "locations.h"
#include "game.h"
#include "game_user.h"
#include "async_observer.h"

using namespace std;

//
// ServiceUser
//

ServiceUser::ServiceUser()
 : serviceType(SERVICE_TYPE_NONE)
{
};

ServiceUser::ServiceUser(const ServiceUser& otherService)
 : serviceType(otherService.serviceType),
   servicePtr(otherService.servicePtr.lock()),
   userName(otherService.userName)
{
};

ServiceUser::ServiceUser(uint8_t nServiceType, string nUserName)
 : serviceType(nServiceType),
   userName(nUserName)
{
};

ServiceUser::ServiceUser(uint8_t nServiceType, int64_t nUserIdentifier, string nUserName)
 : serviceType(nServiceType),
   userIdentifier(nUserIdentifier),
   userName(nUserName)
{
};

ServiceUser::ServiceUser(uint8_t nServiceType, string nUserName, shared_ptr<void> nServicePtr)
 : serviceType(nServiceType),
   servicePtr(nServicePtr),
   userName(nUserName)
{
};

void ServiceUser::Reset()
{
  serviceType = SERVICE_TYPE_NONE;
  servicePtr.reset();
  userIdentifier.reset();
  userName.clear();
}

ServiceUser::~ServiceUser()
{
}

bool ServiceUser::operator==(const ServiceUser& other) const {
  return (
    (serviceType == other.serviceType) &&
    (servicePtr.lock() == other.servicePtr.lock()) &&
    (userIdentifier.has_value() == other.userIdentifier.has_value()) &&
    (!userIdentifier.has_value() || (*userIdentifier == *other.userIdentifier)) &&
    (userName == other.userName) &&
    (subLocation == other.subLocation)
  );
}

//
// GameSource
//


GameSource::GameSource()
 : userType(COMMAND_SOURCE_GAME_NONE),
   user(nullptr)
{
}

GameSource::GameSource(const GameSource& otherSource)
 : userType(otherSource.userType),
   game(otherSource.GetGame()),
   user(otherSource.user)
{
}

GameSource::GameSource(GameUser::CGameUser* nUser)
 : userType(COMMAND_SOURCE_GAME_USER),
   game(nUser->GetGame()),
   user(nUser)
{
}

GameSource::GameSource(CAsyncObserver* nSpectator)
 : userType(COMMAND_SOURCE_GAME_ASYNC_OBSERVER),
   game(nSpectator->GetGame()),
   spectator(nSpectator)
{
}

GameSource::~GameSource()
{
}

CConnection* GameSource::GetUserOrSpectator() const
{
  return static_cast<CConnection*>(user);
}

void GameSource::Expire()
{
  switch (userType) {
    case COMMAND_SOURCE_GAME_USER:
      Reset();
      break;
    case COMMAND_SOURCE_GAME_ASYNC_OBSERVER:
      userType = COMMAND_SOURCE_GAME_REPLAY;
      game.reset();
      break;
    case COMMAND_SOURCE_GAME_REPLAY:
    case COMMAND_SOURCE_GAME_NONE:
      break;
  }
}

void GameSource::Reset()
{
  userType = COMMAND_SOURCE_GAME_NONE;
  game.reset();
  user = nullptr;
}

bool GameSource::operator==(const GameSource& other) const
{
  return (
    (userType == other.userType) &&
    (game.lock() == other.game.lock()) &&
    reinterpret_cast<const void*>(user) == reinterpret_cast<const void*>(other.user)
  );
}
