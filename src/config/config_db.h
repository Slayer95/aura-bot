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

#ifndef AURA_CONFIG_DB_H_
#define AURA_CONFIG_DB_H_

#include "../includes.h"
#include "config.h"

//
// CDataBaseConfig
//

struct CDataBaseConfig
{
  enum class JournalMode : uint8_t
  {
    kDel                         = 0,
    kTruncate                    = 1,
    kPersist                     = 2,
    kMemory                      = 3,
    kWal                         = 4,
    kOff                         = 5,
    LAST                         = 6,
  };

  enum class SynchronousMode : uint8_t
  {
    kOff                         = 0,
    kNormal                      = 1,
    kFull                        = 2,
    kExtra                       = 3,
    LAST                         = 4,
  };

  std::filesystem::path                   m_File;
  std::filesystem::path                   m_TWRPGFile;
  CDataBaseConfig::JournalMode            m_JournalMode;
  CDataBaseConfig::SynchronousMode        m_Synchronous;
  uint16_t                                m_WALInterval;

  explicit CDataBaseConfig(CConfig& CFG);
  bool operator==(const CDataBaseConfig& other) const;
  CDataBaseConfig& operator=(const CDataBaseConfig& other);
  ~CDataBaseConfig();
};

#endif
