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

#ifndef AURA_SAVEGAME_H
#define AURA_SAVEGAME_H

#include "aura.h"
#include "gameslot.h"
#include "packed.h"

#include <string>
#include <vector>

//
// CSaveGame
//

class CSaveGame : public CPacked
{
public:
  CAura* m_Aura;

private:
	std::filesystem::path m_ServerPath;
	std::string m_ClientPath;
	std::string m_MapPath;
	std::string m_GameName;
	uint8_t m_NumSlots;
	std::vector<CGameSlot> m_Slots;
	uint32_t m_RandomSeed;
	std::vector<uint8_t> m_SaveHash;

public:
	CSaveGame(CAura* nAura, const std::filesystem::path& fromPath);
	virtual ~CSaveGame();

	std::filesystem::path GetServerPath()				{ return m_ServerPath; }
	std::string GetClientPath()			{ return m_ClientPath; }
	std::string GetGameName()				{ return m_GameName; }
	uint8_t GetNumSlots()		{ return m_NumSlots; }
	std::vector<CGameSlot> GetSlots()		{ return m_Slots; }
	uint32_t GetRandomSeed()			{ return m_RandomSeed; }
	std::vector<uint8_t> GetSaveHash()			{ return m_SaveHash; }
	bool Parse();
};

#endif // AURA_SAVEGAME_H