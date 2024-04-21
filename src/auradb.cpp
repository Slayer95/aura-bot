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

/*

   Copyright [2010] [Josko Nikolic]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT

 */

#include "auradb.h"
#include "aura.h"
#include "util.h"
#include "config.h"
#include "sqlite3.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CQSLITE3 (wrapper class)
//

CSQLITE3::CSQLITE3(const filesystem::path& filename)
  : m_Ready(true)
{
  if (sqlite3_open_v2(PathToString(filename).c_str(), reinterpret_cast<sqlite3**>(&m_DB), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    m_Ready = false;
}

CSQLITE3::~CSQLITE3()
{
  sqlite3_close(static_cast<sqlite3*>(m_DB));
}

//
// CAuraDB
//

CAuraDB::CAuraDB(CConfig& CFG)
  : FromAddStmt(nullptr),
    FromCheckStmt(nullptr),
    AliasAddStmt(nullptr),
    AliasCheckStmt(nullptr),
    BanCheckStmt(nullptr),
    ModeratorCheckStmt(nullptr),
    m_FirstRun(false),
    m_HasError(false)
{
  m_File = CFG.GetPath("db.storage_file", CFG.GetHomeDir() / filesystem::path("aura.db"));
  Print("[SQLITE3] opening database [" + PathToString(m_File) + "]");
  m_DB = new CSQLITE3(m_File);

  if (!m_DB->GetReady()) {
    // setting m_HasError to true indicates there's been a critical error and we want Aura to shutdown
    // this is okay here because we're in the constructor so we're not dropping any games or players

    Print(string("[SQLITE3] error opening database [" + PathToString(m_File) + "] - ") + m_DB->GetError());
    m_HasError = true;
    m_Error    = "error opening database";
    return;
  }

  // find the schema number so we can determine whether we need to upgrade or not

  int64_t schemaNumber = 0;
  sqlite3_stmt* Statement;
  m_DB->Prepare(R"(SELECT value FROM config WHERE name="schema_number")", reinterpret_cast<void**>(&Statement));

  if (Statement) {
    int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW) {
      if (sqlite3_column_count(Statement) == 1) {
        schemaNumber = sqlite3_column_int64(Statement, 0);
      } else {
        m_HasError = true;
        m_Error    = "schema number missing - no columns found";
        return;
      }
    } else if (RC == SQLITE_ERROR) {
      m_HasError = true;
      m_Error    = m_DB->GetError();
      return;
    }

    m_DB->Finalize(Statement);

    if (schemaNumber == SCHEMA_NUMBER) {
      return; // Up to date
    }

    if (schemaNumber != 0) {
      // Josko's original schema number is 1, but text.
      // I am using 2 as int64.
      // Neither Josko's nor other legacy schemas are supported.
      Print("[SQLITE3] incompatible database file [aura.db] found");
      m_HasError = true;
      m_Error    = "incompatible database format";
      return;
    }
  }

  // Initialize database
  m_FirstRun = true;

  Print("[SQLITE3] initializing database");

  if (m_DB->Exec(R"(CREATE TABLE moderators ( id INTEGER PRIMARY KEY, name TEXT NOT NULL, server TEXT NOT NULL DEFAULT "" ))") != SQLITE_OK)
    Print("[SQLITE3] error creating moderators table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE bans ( id INTEGER PRIMARY KEY, server TEXT NOT NULL, name TEXT NOT NULL, date TEXT NOT NULL, moderators TEXT NOT NULL, reason TEXT )") != SQLITE_OK)
    Print("[SQLITE3] error creating bans table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE players ( id INTEGER PRIMARY KEY, name TEXT NOT NULL, games INTEGER, dotas INTEGER, loadingtime INTEGER, duration INTEGER, left INTEGER, wins INTEGER, losses INTEGER, kills INTEGER, deaths INTEGER, creepkills INTEGER, creepdenies INTEGER, assists INTEGER, neutralkills INTEGER, towerkills INTEGER, raxkills INTEGER, courierkills INTEGER )") != SQLITE_OK)
    Print("[SQLITE3] error creating players table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE config ( name TEXT NOT NULL PRIMARY KEY, value INTEGER )") != SQLITE_OK)
    Print("[SQLITE3] error creating config table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE iptocountry ( ip1 INTEGER NOT NULL, ip2 INTEGER NOT NULL, country TEXT NOT NULL, PRIMARY KEY ( ip1, ip2 ) )") != SQLITE_OK)
    Print("[SQLITE3] error creating iptocountry table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE aliases ( alias TEXT NOT NULL PRIMARY KEY, value TEXT NOT NULL )") != SQLITE_OK)
    Print("[SQLITE3] error creating aliases table - " + m_DB->GetError());

  // Insert schema number
  m_DB->Prepare(R"(INSERT INTO config VALUES ( "schema_number", ? ))", reinterpret_cast<void**>(&Statement));
  if (Statement) {
    sqlite3_bind_int64(Statement, 1, SCHEMA_NUMBER);
    const int32_t RC = m_DB->Step(Statement);
    if (RC == SQLITE_ERROR) {
      Print("[SQLITE3] error inserting schema number [1] - " + m_DB->GetError());
    }
    m_DB->Finalize(Statement);
  }
}

CAuraDB::~CAuraDB()
{
  Print("[SQLITE3] closing database [" + PathToString(m_File.filename()) + "]");

  if (FromAddStmt)
    m_DB->Finalize(FromAddStmt);

  if (FromCheckStmt)
    m_DB->Finalize(FromCheckStmt);

  if (AliasAddStmt)
    m_DB->Finalize(AliasAddStmt);

  if (AliasCheckStmt)
    m_DB->Finalize(AliasCheckStmt);

  if (BanCheckStmt)
    m_DB->Finalize(BanCheckStmt);

  if (ModeratorCheckStmt)
    m_DB->Finalize(ModeratorCheckStmt);

  delete m_DB;
}

uint32_t CAuraDB::ModeratorCount(const string& server)
{
  uint32_t      Count = 0;
  sqlite3_stmt* Statement;
  m_DB->Prepare("SELECT COUNT(*) FROM moderators WHERE server=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW)
      Count = sqlite3_column_int(Statement, 0);
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error counting moderators [" + server + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error counting moderators [" + server + "] - " + m_DB->GetError());

  return Count;
}

bool CAuraDB::ModeratorCheck(const string& server, string user)
{
  bool IsAdmin = false;
  transform(begin(user), end(user), begin(user), [](char c) { return static_cast<char>(std::tolower(c)); });

  if (!ModeratorCheckStmt)
    m_DB->Prepare("SELECT * FROM moderators WHERE server=? AND name=?", &ModeratorCheckStmt);

  if (ModeratorCheckStmt)
  {
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(ModeratorCheckStmt), 1, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(ModeratorCheckStmt), 2, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(ModeratorCheckStmt);

    // we're just checking to see if the query returned a row, we don't need to check the row data itself

    if (RC == SQLITE_ROW)
      IsAdmin = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error checking moderators [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Reset(ModeratorCheckStmt);
  }
  else
    Print("[SQLITE3] prepare error checking moderators [" + server + " : " + user + "] - " + m_DB->GetError());

  return IsAdmin;
}

bool CAuraDB::ModeratorCheck(string user)
{
  bool IsAdmin = false;
  transform(begin(user), end(user), begin(user), [](char c) { return static_cast<char>(std::tolower(c)); });

  sqlite3_stmt* Statement;
  m_DB->Prepare("SELECT * FROM moderators WHERE name=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    // we're just checking to see if the query returned a row, we don't need to check the row data itself

    if (RC == SQLITE_ROW)
      IsAdmin = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error checking moderators [" + user + "] - " + m_DB->GetError());

    m_DB->Reset(Statement);
  }
  else
    Print("[SQLITE3] prepare error checking moderators [" + user + "] - " + m_DB->GetError());

  return IsAdmin;
}

bool CAuraDB::ModeratorAdd(const string& server, string user)
{
  bool Success = false;
  transform(begin(user), end(user), begin(user), [](char c) { return static_cast<char>(std::tolower(c)); });

  sqlite3_stmt* Statement;
  m_DB->Prepare("INSERT INTO moderators ( server, name ) VALUES ( ?, ? )", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_DONE)
      Success = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error adding moderators [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error adding moderators [" + server + " : " + user + "] - " + m_DB->GetError());

  return Success;
}

bool CAuraDB::ModeratorRemove(const string& server, string user)
{
  bool          Success = false;
  sqlite3_stmt* Statement;
  transform(begin(user), end(user), begin(user), [](char c) { return static_cast<char>(std::tolower(c)); });
  m_DB->Prepare("DELETE FROM moderators WHERE server=? AND name=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_DONE)
      Success = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error removing moderators [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error removing moderators [" + server + " : " + user + "] - " + m_DB->GetError());

  return Success;
}

vector<string> CAuraDB::ListModerators(const string& server)
{
  vector<string> admins;

  sqlite3_stmt* Statement;
  m_DB->Prepare("SELECT * FROM moderators WHERE server=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);

    int32_t RC;
    while ((RC = m_DB->Step(Statement)) == SQLITE_ROW) {
      const unsigned char* user = m_DB->Column(Statement, 1);
      const string userWrap = string(reinterpret_cast<const char*>(user));
      Print("Found moderator: " + userWrap);
      admins.push_back(userWrap);
    }
    m_DB->Reset(Statement);
  }
  else
    Print("[SQLITE3] prepare error listing moderators [" + server + "] - " + m_DB->GetError());

  return admins;
}

uint32_t CAuraDB::BanCount(const string& server)
{
  uint32_t      Count = 0;
  sqlite3_stmt* Statement;
  m_DB->Prepare("SELECT COUNT(*) FROM bans WHERE server=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW)
      Count = sqlite3_column_int(Statement, 0);
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error counting bans [" + server + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error counting bans [" + server + "] - " + m_DB->GetError());

  return Count;
}

CDBBan* CAuraDB::BanCheck(const string& server, string user)
{
  CDBBan* Ban = nullptr;
  transform(begin(user), end(user), begin(user), [](char c) { return static_cast<char>(std::tolower(c)); });

  if (!BanCheckStmt)
    m_DB->Prepare("SELECT name, date, moderators, reason FROM bans WHERE server=? AND name=?", &BanCheckStmt);

  if (BanCheckStmt)
  {
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(BanCheckStmt), 1, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(BanCheckStmt), 2, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(BanCheckStmt);

    if (RC == SQLITE_ROW)
    {
      if (sqlite3_column_count(static_cast<sqlite3_stmt*>(BanCheckStmt)) == 4)
      {
        string Name   = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(BanCheckStmt), 0));
        string Date   = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(BanCheckStmt), 1));
        string Admin  = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(BanCheckStmt), 2));
        string Reason = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(BanCheckStmt), 3));

        Ban = new CDBBan(server, Name, Date, Admin, Reason);
      }
      else
        Print("[SQLITE3] error checking ban [" + server + " : " + user + "] - row doesn't have 4 columns");
    }
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error checking ban [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Reset(BanCheckStmt);
  }
  else
    Print("[SQLITE3] prepare error checking ban [" + server + " : " + user + "] - " + m_DB->GetError());

  return Ban;
}

bool CAuraDB::BanAdd(const string& server, string user, const string& moderators, const string& reason)
{
  bool          Success = false;
  sqlite3_stmt* Statement;
  transform(begin(user), end(user), begin(user), [](char c) { return static_cast<char>(std::tolower(c)); });
  m_DB->Prepare("INSERT INTO bans ( server, name, date, moderators, reason ) VALUES ( ?, ?, date('now'), ?, ? )", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 3, moderators.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 4, reason.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_DONE)
      Success = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error adding ban [" + server + " : " + user + " : " + moderators + " : " + reason + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error adding ban [" + server + " : " + user + " : " + moderators + " : " + reason + "] - " + m_DB->GetError());

  return Success;
}

bool CAuraDB::BanRemove(const string& server, string user)
{
  bool          Success = false;
  sqlite3_stmt* Statement;
  transform(begin(user), end(user), begin(user), [](char c) { return static_cast<char>(std::tolower(c)); });
  m_DB->Prepare("DELETE FROM bans WHERE server=? AND name=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_DONE)
      Success = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error removing ban [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error removing ban [" + server + " : " + user + "] - " + m_DB->GetError());

  return Success;
}

vector<string> CAuraDB::ListBans(const string& server)
{
  vector<string> bans;

  sqlite3_stmt* Statement;
  m_DB->Prepare("SELECT * FROM bans WHERE server=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);

    int32_t RC;
    while ((RC = m_DB->Step(Statement)) == SQLITE_ROW) {
      const unsigned char* user = m_DB->Column(Statement, 1);
      bans.push_back(string(reinterpret_cast<const char*>(user)));
    }
    m_DB->Reset(Statement);
  }
  else
    Print("[SQLITE3] prepare error listing bans [" + server + "] - " + m_DB->GetError());

  return bans;
}

bool CAuraDB::BanRemove(string user)
{
  bool          Success = false;
  sqlite3_stmt* Statement;
  transform(begin(user), end(user), begin(user), [](char c) { return static_cast<char>(std::tolower(c)); });
  m_DB->Prepare("DELETE FROM bans WHERE name=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_DONE)
      Success = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error removing ban [" + user + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error removing ban [" + user + "] - " + m_DB->GetError());

  return Success;
}

void CAuraDB::GamePlayerAdd(string name, uint64_t loadingtime, uint64_t duration, uint64_t left)
{
  sqlite3_stmt* Statement;
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });

  // check if entry exists

  int32_t  RC;
  uint32_t Games = 0;

  m_DB->Prepare("SELECT games, loadingtime, duration, left FROM players WHERE name=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW)
    {
      Games += 1 + sqlite3_column_int(Statement, 0);
      loadingtime += sqlite3_column_int64(Statement, 1);
      duration += sqlite3_column_int64(Statement, 2);
      left += sqlite3_column_int64(Statement, 3);
    }

    m_DB->Finalize(Statement);
  }
  else
  {
    Print("[SQLITE3] prepare error adding gameplayer [" + name + "] - " + m_DB->GetError());
    return;
  }

  if (Games == 0)
  {
    // insert new entry

    m_DB->Prepare("INSERT INTO players ( name, games, loadingtime, duration, left ) VALUES ( ?, ?, ?, ?, ? )", reinterpret_cast<void**>(&Statement));

    if (Statement == nullptr)
    {
      Print("[SQLITE3] prepare error inserting gameplayer [" + name + "] - " + m_DB->GetError());
      return;
    }

    sqlite3_bind_text(Statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(Statement, 2, 1);
    sqlite3_bind_int64(Statement, 3, loadingtime);
    sqlite3_bind_int64(Statement, 4, duration);
    sqlite3_bind_int64(Statement, 5, left);
  }
  else
  {
    // update existing entry

    m_DB->Prepare("UPDATE players SET games=?, loadingtime=?, duration=?, left=? WHERE name=?", reinterpret_cast<void**>(&Statement));

    if (Statement == nullptr)
    {
      Print("[SQLITE3] prepare error updating gameplayer [" + name + "] - " + m_DB->GetError());
      return;
    }

    sqlite3_bind_int(Statement, 1, Games);
    sqlite3_bind_int64(Statement, 2, loadingtime);
    sqlite3_bind_int64(Statement, 3, duration);
    sqlite3_bind_int64(Statement, 4, left);
    sqlite3_bind_text(Statement, 5, name.c_str(), -1, SQLITE_TRANSIENT);
  }

  RC = m_DB->Step(Statement);

  if (RC != SQLITE_DONE)
    Print("[SQLITE3] error adding gameplayer [" + name + "] - " + m_DB->GetError());

  m_DB->Finalize(Statement);
}

CDBGamePlayerSummary* CAuraDB::GamePlayerSummaryCheck(string name)
{
  sqlite3_stmt*         Statement;
  CDBGamePlayerSummary* GamePlayerSummary = nullptr;
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });
  m_DB->Prepare("SELECT games, loadingtime, duration, left FROM players WHERE name=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW)
    {
      if (sqlite3_column_count(Statement) == 4)
      {
        const uint32_t TotalGames  = sqlite3_column_int(Statement, 0);
        const uint64_t LoadingTime = sqlite3_column_int64(Statement, 1);
        const uint64_t Left        = sqlite3_column_int64(Statement, 2);
        const uint64_t Duration    = sqlite3_column_int64(Statement, 3);

        float AvgLoadingTime = static_cast<float>(static_cast<double>(LoadingTime) / TotalGames / 1000);
        uint32_t AvgLeftPercent = static_cast<uint32_t>(static_cast<double>(Duration) / Left * 100);

        GamePlayerSummary = new CDBGamePlayerSummary(TotalGames, AvgLoadingTime, AvgLeftPercent);
      }
      else
        Print("[SQLITE3] error checking gameplayersummary [" + name + "] - row doesn't have 4 columns");
    }
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error checking gameplayersummary [" + name + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error checking gameplayersummary [" + name + "] - " + m_DB->GetError());

  return GamePlayerSummary;
}

void CAuraDB::DotAPlayerAdd(string name, uint32_t winner, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t neutralkills, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills)
{
  bool          Success = false;
  sqlite3_stmt* Statement;
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });
  m_DB->Prepare("SELECT dotas, wins, losses, kills, deaths, creepkills, creepdenies, assists, neutralkills, towerkills, raxkills, courierkills FROM players WHERE name=?", reinterpret_cast<void**>(&Statement));

  int32_t  RC;
  uint32_t Dotas  = 1;
  uint32_t Wins   = 0;
  uint32_t Losses = 0;

  if (winner == 1)
    ++Wins;
  else if (winner == 2)
    ++Losses;

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW)
    {
      Dotas += sqlite3_column_int(Statement, 0);
      Wins += sqlite3_column_int(Statement, 1);
      Losses += sqlite3_column_int(Statement, 2);
      kills += sqlite3_column_int(Statement, 3);
      deaths += sqlite3_column_int(Statement, 4);
      creepkills += sqlite3_column_int(Statement, 5);
      creepdenies += sqlite3_column_int(Statement, 6);
      assists += sqlite3_column_int(Statement, 7);
      neutralkills += sqlite3_column_int(Statement, 8);
      towerkills += sqlite3_column_int(Statement, 9);
      raxkills += sqlite3_column_int(Statement, 10);
      courierkills += sqlite3_column_int(Statement, 11);

      Success = true;
    }

    m_DB->Finalize(Statement);
  }
  else
  {
    Print("[SQLITE3] prepare error adding dotaplayer [" + name + "] - " + m_DB->GetError());
    return;
  }

  // there must be a row already because we add one, if not present, in GamePlayerAdd( ) before the call to DotAPlayerAdd( )

  if (Success == false)
  {
    Print("[SQLITE3] error adding dotaplayer [" + name + "] - no existing row");
    return;
  }

  m_DB->Prepare("UPDATE players SET dotas=?, wins=?, losses=?, kills=?, deaths=?, creepkills=?, creepdenies=?, assists=?, neutralkills=?, towerkills=?, raxkills=?, courierkills=? WHERE name=?", reinterpret_cast<void**>(&Statement));

  if (Statement == nullptr)
  {
    Print("[SQLITE3] prepare error updating dotalayer [" + name + "] - " + m_DB->GetError());
    return;
  }

  sqlite3_bind_int(Statement, 1, Dotas);
  sqlite3_bind_int(Statement, 2, Wins);
  sqlite3_bind_int(Statement, 3, Losses);
  sqlite3_bind_int(Statement, 4, kills);
  sqlite3_bind_int(Statement, 5, deaths);
  sqlite3_bind_int(Statement, 6, creepkills);
  sqlite3_bind_int(Statement, 7, creepdenies);
  sqlite3_bind_int(Statement, 8, assists);
  sqlite3_bind_int(Statement, 9, neutralkills);
  sqlite3_bind_int(Statement, 10, towerkills);
  sqlite3_bind_int(Statement, 11, raxkills);
  sqlite3_bind_int(Statement, 12, courierkills);
  sqlite3_bind_text(Statement, 13, name.c_str(), -1, SQLITE_TRANSIENT);

  RC = m_DB->Step(Statement);

  if (RC != SQLITE_DONE)
    Print("[SQLITE3] error adding dotaplayer [" + name + "] - " + m_DB->GetError());

  m_DB->Finalize(Statement);
}

CDBDotAPlayerSummary* CAuraDB::DotAPlayerSummaryCheck(string name)
{
  sqlite3_stmt*         Statement;
  CDBDotAPlayerSummary* DotAPlayerSummary = nullptr;
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });
  m_DB->Prepare("SELECT dotas, wins, losses, kills, deaths, creepkills, creepdenies, assists, neutralkills, towerkills, raxkills, courierkills FROM players WHERE name=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW)
    {
      if (sqlite3_column_count(Statement) == 12)
      {
        const uint32_t TotalGames = sqlite3_column_int(Statement, 0);

        if (TotalGames != 0)
        {
          const uint32_t TotalWins         = sqlite3_column_int(Statement, 1);
          const uint32_t TotalLosses       = sqlite3_column_int(Statement, 2);
          const uint32_t TotalKills        = sqlite3_column_int(Statement, 3);
          const uint32_t TotalDeaths       = sqlite3_column_int(Statement, 4);
          const uint32_t TotalCreepKills   = sqlite3_column_int(Statement, 5);
          const uint32_t TotalCreepDenies  = sqlite3_column_int(Statement, 6);
          const uint32_t TotalAssists      = sqlite3_column_int(Statement, 7);
          const uint32_t TotalNeutralKills = sqlite3_column_int(Statement, 8);
          const uint32_t TotalTowerKills   = sqlite3_column_int(Statement, 9);
          const uint32_t TotalRaxKills     = sqlite3_column_int(Statement, 10);
          const uint32_t TotalCourierKills = sqlite3_column_int(Statement, 11);

          DotAPlayerSummary = new CDBDotAPlayerSummary(TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills, TotalTowerKills, TotalRaxKills, TotalCourierKills);
        }
      }
      else
        Print("[SQLITE3] error checking dotaplayersummary [" + name + "] - row doesn't have 12 columns");
    }

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error checking dotaplayersummary [" + name + "] - " + m_DB->GetError());

  return DotAPlayerSummary;
}

string CAuraDB::FromCheck(uint32_t ip)
{
  // a big thank you to tjado for help with the iptocountry feature

  string From = "??";

  if (!FromCheckStmt)
    m_DB->Prepare("SELECT country FROM iptocountry WHERE ip1<=? AND ip2>=?", &FromCheckStmt);

  if (!FromCheckStmt) {
    Print("[SQLITE3] prepare error checking iptocountry [" + to_string(ip) + "] - " + m_DB->GetError());
    return From;
  }

  sqlite3_bind_int(static_cast<sqlite3_stmt*>(FromCheckStmt), 1, unsigned_to_signed_32(ip));
  sqlite3_bind_int(static_cast<sqlite3_stmt*>(FromCheckStmt), 2, unsigned_to_signed_32(ip));

  const int32_t RC = m_DB->Step(FromCheckStmt);

  if (RC == SQLITE_ROW)
  {
    if (sqlite3_column_count(static_cast<sqlite3_stmt*>(FromCheckStmt)) == 1)
      From = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(FromCheckStmt), 0));
    else
      Print("[SQLITE3] error checking iptocountry [" + to_string(ip) + "] - row doesn't have 1 column");
  }
  else if (RC == SQLITE_ERROR)
    Print("[SQLITE3] error checking iptocountry [" + to_string(ip) + "] - " + m_DB->GetError());

  m_DB->Reset(FromCheckStmt);

  return From;
}

bool CAuraDB::FromAdd(uint32_t ip1, uint32_t ip2, const string& country)
{
  // a big thank you to tjado for help with the iptocountry feature

  bool Success = false;

  if (!FromAddStmt)
    m_DB->Prepare("INSERT INTO iptocountry VALUES ( ?, ?, ? )", &FromAddStmt);

  if (!FromAddStmt) {
    Print("[SQLITE3] prepare error adding iptocountry [" + to_string(ip1) + " : " + to_string(ip2) + " : " + country + "] - " + m_DB->GetError());
    return false;
  }

  // Losslessly converting IPs to signed 32-bits integers rather than to same-value 64-bits integers
  // This saves ~400 KB in initial database size, down from 3.6 MB to just about 3.17 MB
  // (for reference, ip-to-country.csv is 5.92 MB)
  sqlite3_bind_int(static_cast<sqlite3_stmt*>(FromAddStmt), 1, unsigned_to_signed_32(ip1));
  sqlite3_bind_int(static_cast<sqlite3_stmt*>(FromAddStmt), 2, unsigned_to_signed_32(ip2));
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(FromAddStmt), 3, country.c_str(), -1, SQLITE_TRANSIENT);

  int32_t RC = m_DB->Step(FromAddStmt);

  if (RC == SQLITE_DONE)
    Success = true;
  else if (RC == SQLITE_ERROR)
    Print("[SQLITE3] error adding iptocountry [" + to_string(ip1) + " : " + to_string(ip2) + " : " + country + "] - " + m_DB->GetError());

  m_DB->Reset(FromAddStmt);

  return Success;
}

bool CAuraDB::AliasAdd(const string& alias, const string& target)
{
  bool Success = false;

  if (!AliasAddStmt)
    m_DB->Prepare("INSERT INTO aliases VALUES ( ?, ? )", &AliasAddStmt);

  if (!AliasAddStmt) {
    Print("[SQLITE3] prepare error adding alias [" + alias + ": " + target + "] - " + m_DB->GetError());
    return false;
  }

  sqlite3_bind_text(static_cast<sqlite3_stmt*>(AliasAddStmt), 1, alias.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(AliasAddStmt), 2, target.c_str(), -1, SQLITE_TRANSIENT);

  int32_t RC = m_DB->Step(AliasAddStmt);

  if (RC == SQLITE_DONE)
    Success = true;
  else if (RC == SQLITE_ERROR)
    Print("[SQLITE3] error adding alias [" + alias + ": " + target + "] - " + m_DB->GetError());

  m_DB->Reset(AliasAddStmt);

  return Success;
}

string CAuraDB::AliasCheck(const string& alias)
{
  string value;

  if (!AliasCheckStmt)
    m_DB->Prepare("SELECT value FROM aliases WHERE alias=?", &AliasCheckStmt);

  if (!AliasCheckStmt) {
    Print("[SQLITE3] prepare error checking alias [" + alias + "] - " + m_DB->GetError());
    return value;
  }

  sqlite3_bind_text(static_cast<sqlite3_stmt*>(AliasCheckStmt), 1, alias.c_str(), -1, SQLITE_TRANSIENT);

  const int32_t RC = m_DB->Step(AliasCheckStmt);

  if (RC == SQLITE_ERROR) {
    Print("[SQLITE3] error checking alias [" + alias + "] - " + m_DB->GetError());
    return value;
  }

  if (RC == SQLITE_ROW) {
    if (sqlite3_column_count(static_cast<sqlite3_stmt*>(AliasCheckStmt)) == 1) {
      Print("AliasCheck ok");
      value = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(AliasCheckStmt), 0));
    } else {
      Print("[SQLITE3] error checking alias [" + alias + "] - row doesn't have 1 column");
    }
  }

  m_DB->Reset(AliasCheckStmt);

  return value;
}

//
// CDBBan
//

CDBBan::CDBBan(string nServer, string nName, string nDate, string nModerator, string nReason)
  : m_Server(std::move(nServer)),
    m_Name(std::move(nName)),
    m_Date(std::move(nDate)),
    m_Moderator(std::move(nModerator)),
    m_Reason(std::move(nReason))
{
}

CDBBan::~CDBBan() = default;

//
// CDBGamePlayer
//

CDBGamePlayer::CDBGamePlayer(string nName, uint64_t nLoadingTime, uint64_t nLeft, uint32_t nColor)
  : m_Name(std::move(nName)),
    m_LoadingTime(nLoadingTime),
    m_Left(nLeft),
    m_Color(nColor)
{
}

CDBGamePlayer::~CDBGamePlayer() = default;

//
// CDBGamePlayerSummary
//

CDBGamePlayerSummary::CDBGamePlayerSummary(uint32_t nTotalGames, float nAvgLoadingTime, uint32_t nAvgLeftPercent)
  : m_TotalGames(nTotalGames),
    m_AvgLoadingTime(nAvgLoadingTime),
    m_AvgLeftPercent(nAvgLeftPercent)
{
}

CDBGamePlayerSummary::~CDBGamePlayerSummary() = default;

//
// CDBDotAPlayer
//

CDBDotAPlayer::CDBDotAPlayer()
  : m_Color(0),
    m_NewColor(0),
    m_Kills(0),
    m_Deaths(0),
    m_CreepKills(0),
    m_CreepDenies(0),
    m_Assists(0),
    m_NeutralKills(0),
    m_TowerKills(0),
    m_RaxKills(0),
    m_CourierKills(0)
{
}

CDBDotAPlayer::CDBDotAPlayer(uint32_t nKills, uint32_t nDeaths, uint32_t nCreepKills, uint32_t nCreepDenies, uint32_t nAssists, uint32_t nNeutralKills, uint32_t nTowerKills, uint32_t nRaxKills, uint32_t nCourierKills)
  : m_Color(0),
    m_NewColor(0),
    m_Kills(nKills),
    m_Deaths(nDeaths),
    m_CreepKills(nCreepKills),
    m_CreepDenies(nCreepDenies),
    m_Assists(nAssists),
    m_NeutralKills(nNeutralKills),
    m_TowerKills(nTowerKills),
    m_RaxKills(nRaxKills),
    m_CourierKills(nCourierKills)
{
}

CDBDotAPlayer::~CDBDotAPlayer() = default;

//
// CDBDotAPlayerSummary
//

CDBDotAPlayerSummary::CDBDotAPlayerSummary(uint32_t nTotalGames, uint32_t nTotalWins, uint32_t nTotalLosses, uint32_t nTotalKills, uint32_t nTotalDeaths, uint32_t nTotalCreepKills, uint32_t nTotalCreepDenies, uint32_t nTotalAssists, uint32_t nTotalNeutralKills, uint32_t nTotalTowerKills, uint32_t nTotalRaxKills, uint32_t nTotalCourierKills)
  : m_TotalGames(nTotalGames),
    m_TotalWins(nTotalWins),
    m_TotalLosses(nTotalLosses),
    m_TotalKills(nTotalKills),
    m_TotalDeaths(nTotalDeaths),
    m_TotalCreepKills(nTotalCreepKills),
    m_TotalCreepDenies(nTotalCreepDenies),
    m_TotalAssists(nTotalAssists),
    m_TotalNeutralKills(nTotalNeutralKills),
    m_TotalTowerKills(nTotalTowerKills),
    m_TotalRaxKills(nTotalRaxKills),
    m_TotalCourierKills(nTotalCourierKills)
{
}

CDBDotAPlayerSummary::~CDBDotAPlayerSummary() = default;
