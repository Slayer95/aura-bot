/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#include "aura.h"
#include "util.h"
#include "packed.h"
#include "savegame.h"

using namespace std;

//
// CSaveGame
//

CSaveGame::CSaveGame(CAura* nAura, const std::filesystem::path& fromPath)
  : CPacked(nAura),
    m_NumSlots(0),
    m_RandomSeed(0)
{
  m_ServerPath = fromPath;
  m_ClientPath = "Save\\Multiplayer\\" + PathToString(fromPath.filename());
}

CSaveGame::~CSaveGame()
{
}

bool CSaveGame::Parse()
{
	if (!m_Valid || m_Flags != 0) {
    if (m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
      Print("[SAVEGAME] invalid file type (flags mismatch)");
    }
		m_Valid = false;
		return false;
	}

	istringstream ISS(m_Decompressed);

	// savegame format figured out by Varlock:
	// string		-> map path
	// 0 (string?)	-> ??? (no idea what this is)
	// string		-> game name
	// 0 (string?)	-> ??? (maybe original game password)
	// string		-> stat string
	// 4 bytes		-> ??? (seems to be # of slots)
	// 4 bytes		-> ??? (seems to be 0x01 0x28 0x49 0x00 on both of the savegames examined)
	// 2 bytes		-> ??? (no idea what this is)
	// slot structure
	// 4 bytes		-> magic number

	uint8_t Garbage1;
	uint16_t Garbage2;
	uint32_t Garbage4;
	string GarbageString;
	uint32_t SaveHash;

	getline(ISS, m_MapPath, '\0');				// map path
	getline(ISS, GarbageString, '\0');			// ???
	getline(ISS, m_GameName, '\0');				// game name
	getline(ISS, GarbageString, '\0');			// ???
	getline(ISS, GarbageString, '\0');			// stat string
	ISS.read(reinterpret_cast<char*>(&Garbage4), 4);				// ???
	ISS.read(reinterpret_cast<char*>(&Garbage4), 4);				// ???
	ISS.read(reinterpret_cast<char*>(&Garbage2), 2);				// ???
	ISS.read(reinterpret_cast<char*>(&m_NumSlots), 1);			// number of slots

	if (m_NumSlots > m_Aura->m_MaxSlots) {
    if (m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
      Print("[SAVEGAME] invalid savegame (too many slots)");
    }
		m_Valid = false;
		return false;
	}

	for (uint8_t i = 0; i < m_NumSlots; i++) {
		uint8_t SlotData[9];
		ISS.read(reinterpret_cast<char*>(SlotData), 9);			// slot data
		m_Slots.emplace_back(SlotData[0], SlotData[1], SlotData[2], SlotData[3], SlotData[4], SlotData[5], SlotData[6], SlotData[7], SlotData[8]);
	}

	ISS.read(reinterpret_cast<char*>(&m_RandomSeed), 4);			// random seed
	ISS.read(reinterpret_cast<char*>(&Garbage1), 1);				// GameType
	ISS.read(reinterpret_cast<char*>(&Garbage1), 1);				// number of player slots (non observer)
	ISS.read(reinterpret_cast<char*>(&SaveHash), 4);			// magic number

	if (ISS.eof() || ISS.fail()) {
    if (m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
      Print( "[SAVEGAME] failed to parse savegame header" );
    }
		m_Valid = false;
		return false;
	}

	m_SaveHash = CreateByteArray(SaveHash, false);
  return m_Valid;
}
