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

#include "realm_games.h"

#include "protocol/bnet_protocol.h"
#include "command.h"
#include "config/config.h"
#include "config/config_realm.h"
#include "file_util.h"
#include "game.h"
#include "includes.h"
#include "realm.h"
#include "util.h"

#include "aura.h"

using namespace std;

//
// GameSearchQuery
//

GameSearchQuery::GameSearchQuery()
 : m_Realm(nullptr)
 {
 }

GameSearchQuery::GameSearchQuery(shared_ptr<CRealm> realm)
 : m_Realm(realm)
{
}

GameSearchQuery::~GameSearchQuery()
{
}
