/*

  Copyright [2024-2025] [Leonardo Julca]

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

#include "config_db.h"

using namespace std;

//
// CDataBaseConfig
//

CDataBaseConfig::CDataBaseConfig(CConfig& CFG)
{
  const bool wasStrict = CFG.GetStrictMode();
  CFG.SetStrictMode(true);
  m_File = CFG.GetPath("db.storage_file", CFG.GetHomeDir() / filesystem::path("aura.db"));
  m_TWRPGFile = CFG.GetPath("game_data.twrpg_path", CFG.GetHomeDir() / filesystem::path("twrpg.json"));
  m_JournalMode = CFG.GetEnum<JournalMode>("db.journal_mode", TO_ARRAY("delete", "truncate", "persist", "memory", "wal", "off"), JournalMode::kDel);
  m_Synchronous = CFG.GetEnum<SynchronousMode>("db.synchronous", TO_ARRAY("off", "normal", "full", "extra"), SynchronousMode::kFull);
  m_WALInterval = CFG.GetUint16("db.wal_autocheckpoint", 100);
  CFG.SetStrictMode(wasStrict);
}

CDataBaseConfig::CDataBaseConfig(const CDataBaseConfig& other)
{
  m_File = other.m_File;
  m_TWRPGFile = other.m_TWRPGFile;
  m_JournalMode = other.m_JournalMode;
  m_Synchronous = other.m_Synchronous;
  m_WALInterval = other.m_WALInterval;
}

bool CDataBaseConfig::operator==(const CDataBaseConfig& other) const
{
  return (
    m_File == other.m_File && 
    m_TWRPGFile == other.m_TWRPGFile &&
    m_JournalMode == other.m_JournalMode &&
    m_Synchronous == other.m_Synchronous &&
    m_WALInterval == other.m_WALInterval
  );
}

CDataBaseConfig& CDataBaseConfig::operator=(const CDataBaseConfig& other)
{
  if (this != &other) {
    m_File = other.m_File;
    m_TWRPGFile = other.m_TWRPGFile;
    m_JournalMode = other.m_JournalMode;
    m_Synchronous = other.m_Synchronous;
    m_WALInterval = other.m_WALInterval;
  }

  return *this;
}

CDataBaseConfig::~CDataBaseConfig()
{
}
