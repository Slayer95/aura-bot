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

#include "dota.h"
#include "../aura.h"
#include "../game_controller_data.h"
#include "../game_result.h"
#include "../auradb.h"
#include "../game.h"
#include "../game_user.h"
#include "../protocol/game_protocol.h"
#include "../util.h"

using namespace std;
using namespace Dota;

optional<uint8_t> Dota::EnsureHeroColor(uint32_t input)
{
  optional<uint8_t> result;
  if (GetIsHeroColor(input)) {
    result = static_cast<uint8_t>(input);
  }
  return result;
}

optional<uint8_t> Dota::EnsureActorColor(uint32_t input)
{
  optional<uint8_t> result;
  if (input <= MAX_SLOTS_LEGACY) {
    result = static_cast<uint8_t>(input);
  }
  return result;
}

optional<uint8_t> Dota::ParseHeroColor(const string& input)
{
  optional<uint8_t> result;
  optional<uint32_t> maybeColor = ToUint32(input);
  if (GetIsHeroColor(*maybeColor)) {
    result = static_cast<uint8_t>(*maybeColor);
  }
  return result;
}

optional<uint8_t> Dota::ParseActorColor(const string& input)
{
  optional<uint8_t> result;
  optional<uint32_t> maybeColor = ToUint32(input);
  if (*maybeColor <= MAX_SLOTS_LEGACY) {
    result = static_cast<uint8_t>(*maybeColor);
  }
  return result;
}

string Dota::GetLaneName(const uint8_t code)
{
  switch (code) {
    case 0: return "Top Lane";
    case 1: return "Mid Lane";
    case 2: return "Bot Lane";
  }
  return "Invalid Lane";
}

string Dota::GetTeamNameBaseZero(const uint8_t code)
{
  switch (code) {
    case 0: return "Sentinel";
    case 1: return "Scourge";
  }
  return string();
}

string Dota::GetTeamNameBaseOne(const uint8_t code)
{
  switch (code) {
    case 1: return "Sentinel";
    case 2: return "Scourge";
  }
  return string();
}

string Dota::GetRuneName(const uint8_t code)
{
  switch (code) {
    case 1: return "Haste";
    case 2: return "Regeneration";
    case 3: return "Double Damage";
    case 4: return "Illusion";
    case 5: return "Invisibility";
    case 6: return "Bounty";
    case 7: return "Arcane";
    case 8: return "Wisdom";
    case 9: return "Shield";
    case 49: return "Water";
    default:
      if (50 <= code) return GetRuneName(code % 48);
      return "#" + ToDecString(code);
  }
}

string Dota::GetItemName(const uint32_t code)
{
  switch (code) {
    case 0: return "(no item)";
    case FourCC("I0OY"): return "Abyssal Blade";
    case FourCC("I0OX"): return "Abyssal Blade";
    case FourCC("I0OZ"): return "Abyssal Blade";
    case FourCC("I0VD"): return "Abyssal Blade Recipe";
    case FourCC("I0VE"): return "Abyssal Blade Recipe";
    case FourCC("I0VF"): return "Abyssal Blade Recipe";
    case FourCC("I0AW"): return "Aegis of the Immortal";
    case FourCC("I0U2"): return "Aegis of the Immortal";
    case FourCC("I0AX"): return "Aegis of the Immortal";
    case FourCC("I11O"): return "Aeon Disk";
    case FourCC("I11P"): return "Aeon Disk";
    case FourCC("I11Q"): return "Aeon Disk";
    case FourCC("I11L"): return "Aeon Disk Recipe";
    case FourCC("I11M"): return "Aeon Disk Recipe";
    case FourCC("I11N"): return "Aeon Disk Recipe";
    case FourCC("I0UE"): return "Aether Lens";
    case FourCC("I0UF"): return "Aether Lens";
    case FourCC("I0UG"): return "Aether Lens (Muted)";
    case FourCC("I0V3"): return "Aether Lens Recipe";
    case FourCC("I0V4"): return "Aether Lens Recipe";
    case FourCC("I0V5"): return "Aether Lens Recipe (Muted)";
    case FourCC("I0XX"): return "Aghanim's Blessing";
    case FourCC("I0XY"): return "Aghanim's Blessing";
    case FourCC("I0XZ"): return "Aghanim's Blessing";
    case FourCC("I10X"): return "Aghanim's Blessing";
    case FourCC("I10Y"): return "Aghanim's Blessing";
    case FourCC("I10Z"): return "Aghanim's Blessing";
    case FourCC("I0XU"): return "Aghanim's Blessing Recipe";
    case FourCC("I0XV"): return "Aghanim's Blessing Recipe";
    case FourCC("I0XW"): return "Aghanim's Blessing Recipe";
    case FourCC("I233"): return "Aghanim's Scepter";
    case FourCC("I0AZ"): return "Aghanim's Scepter";
    case FourCC("I00F"): return "Aghanim's Scepter";
    case FourCC("I018"): return "Aghanim's Scepter";
    case FourCC("I019"): return "Aghanim's Scepter";
    case FourCC("I01A"): return "Aghanim's Scepter";
    case FourCC("I01B"): return "Aghanim's Scepter";
    case FourCC("I01H"): return "Aghanim's Scepter";
    case FourCC("I01I"): return "Aghanim's Scepter";
    case FourCC("I02K"): return "Aghanim's Scepter";
    case FourCC("I02L"): return "Aghanim's Scepter";
    case FourCC("I0B2"): return "Aghanim's Scepter";
    case FourCC("I0B3"): return "Aghanim's Scepter";
    case FourCC("I0B4"): return "Aghanim's Scepter";
    case FourCC("I0B5"): return "Aghanim's Scepter";
    case FourCC("I0B6"): return "Aghanim's Scepter";
    case FourCC("I0B7"): return "Aghanim's Scepter";
    case FourCC("I0B8"): return "Aghanim's Scepter";
    case FourCC("I0B9"): return "Aghanim's Scepter";
    case FourCC("I00B"): return "Aghanim's Scepter";
    case FourCC("I0IV"): return "Aghanim's Scepter";
    case FourCC("I0IW"): return "Aghanim's Scepter";
    case FourCC("I0IX"): return "Aghanim's Scepter";
    case FourCC("I0IZ"): return "Aghanim's Scepter";
    case FourCC("I0J0"): return "Aghanim's Scepter";
    case FourCC("I0J1"): return "Aghanim's Scepter";
    case FourCC("I0J2"): return "Aghanim's Scepter";
    case FourCC("I0J3"): return "Aghanim's Scepter";
    case FourCC("I0J4"): return "Aghanim's Scepter";
    case FourCC("I0JZ"): return "Aghanim's Scepter";
    case FourCC("I0K0"): return "Aghanim's Scepter";
    case FourCC("I0K1"): return "Aghanim's Scepter";
    case FourCC("I0K2"): return "Aghanim's Scepter";
    case FourCC("I0K3"): return "Aghanim's Scepter";
    case FourCC("I0K4"): return "Aghanim's Scepter";
    case FourCC("I0K5"): return "Aghanim's Scepter";
    case FourCC("I0L2"): return "Aghanim's Scepter";
    case FourCC("I0L3"): return "Aghanim's Scepter";
    case FourCC("I0L4"): return "Aghanim's Scepter";
    case FourCC("I0LU"): return "Aghanim's Scepter";
    case FourCC("I0M1"): return "Aghanim's Scepter";
    case FourCC("I0M5"): return "Aghanim's Scepter";
    case FourCC("I0M6"): return "Aghanim's Scepter";
    case FourCC("I0MF"): return "Aghanim's Scepter";
    case FourCC("I0MM"): return "Aghanim's Scepter";
    case FourCC("I0MO"): return "Aghanim's Scepter";
    case FourCC("I0NQ"): return "Aghanim's Scepter";
    case FourCC("I0NT"): return "Aghanim's Scepter";
    case FourCC("I0NW"): return "Aghanim's Scepter";
    case FourCC("I0NZ"): return "Aghanim's Scepter";
    case FourCC("I0OR"): return "Aghanim's Scepter";
    case FourCC("I0PG"): return "Aghanim's Scepter";
    case FourCC("I0PJ"): return "Aghanim's Scepter";
    case FourCC("I0PP"): return "Aghanim's Scepter";
    case FourCC("I0PS"): return "Aghanim's Scepter";
    case FourCC("I0PV"): return "Aghanim's Scepter";
    case FourCC("I0Q0"): return "Aghanim's Scepter";
    case FourCC("I0Q1"): return "Aghanim's Scepter";
    case FourCC("I0QL"): return "Aghanim's Scepter";
    case FourCC("I0QO"): return "Aghanim's Scepter";
    case FourCC("I0R1"): return "Aghanim's Scepter";
    case FourCC("I0R4"): return "Aghanim's Scepter";
    case FourCC("I0R7"): return "Aghanim's Scepter";
    case FourCC("I0RA"): return "Aghanim's Scepter";
    case FourCC("I0RD"): return "Aghanim's Scepter";
    case FourCC("I0RG"): return "Aghanim's Scepter";
    case FourCC("I0RU"): return "Aghanim's Scepter";
    case FourCC("I0RX"): return "Aghanim's Scepter";
    case FourCC("I0S0"): return "Aghanim's Scepter";
    case FourCC("I0S3"): return "Aghanim's Scepter";
    case FourCC("I0TA"): return "Aghanim's Scepter";
    case FourCC("I0TD"): return "Aghanim's Scepter";
    case FourCC("I0TG"): return "Aghanim's Scepter";
    case FourCC("I0TJ"): return "Aghanim's Scepter";
    case FourCC("I0TM"): return "Aghanim's Scepter";
    case FourCC("I0TP"): return "Aghanim's Scepter";
    case FourCC("I0TV"): return "Aghanim's Scepter";
    case FourCC("I0UO"): return "Aghanim's Scepter";
    case FourCC("I0UR"): return "Aghanim's Scepter";
    case FourCC("I0UU"): return "Aghanim's Scepter";
    case FourCC("I0UX"): return "Aghanim's Scepter";
    case FourCC("I0V0"): return "Aghanim's Scepter";
    case FourCC("I0W3"): return "Aghanim's Scepter";
    case FourCC("I0W9"): return "Aghanim's Scepter";
    case FourCC("I0WC"): return "Aghanim's Scepter";
    case FourCC("I0WF"): return "Aghanim's Scepter";
    case FourCC("I186"): return "Aghanim's Scepter";
    case FourCC("I189"): return "Aghanim's Scepter";
    case FourCC("I192"): return "Aghanim's Scepter";
    case FourCC("I0WI"): return "Aghanim's Scepter";
    case FourCC("I0WL"): return "Aghanim's Scepter";
    case FourCC("I0WO"): return "Aghanim's Scepter";
    case FourCC("I0WR"): return "Aghanim's Scepter";
    case FourCC("I0WU"): return "Aghanim's Scepter";
    case FourCC("I0WX"): return "Aghanim's Scepter";
    case FourCC("I100"): return "Aghanim's Scepter";
    case FourCC("I103"): return "Aghanim's Scepter";
    case FourCC("I106"): return "Aghanim's Scepter";
    case FourCC("I109"): return "Aghanim's Scepter";
    case FourCC("I10C"): return "Aghanim's Scepter";
    case FourCC("I10F"): return "Aghanim's Scepter";
    case FourCC("I10I"): return "Aghanim's Scepter";
    case FourCC("I10L"): return "Aghanim's Scepter";
    case FourCC("I10O"): return "Aghanim's Scepter";
    case FourCC("I10R"): return "Aghanim's Scepter";
    case FourCC("I10U"): return "Aghanim's Scepter";
    case FourCC("I110"): return "Aghanim's Scepter";
    case FourCC("I113"): return "Aghanim's Scepter";
    case FourCC("I116"): return "Aghanim's Scepter";
    case FourCC("I119"): return "Aghanim's Scepter";
    case FourCC("I11C"): return "Aghanim's Scepter";
    case FourCC("I123"): return "Aghanim's Scepter";
    case FourCC("I126"): return "Aghanim's Scepter";
    case FourCC("I129"): return "Aghanim's Scepter";
    case FourCC("I12C"): return "Aghanim's Scepter";
    case FourCC("I12F"): return "Aghanim's Scepter";
    case FourCC("I0PK"): return "Aghanim's Scepter (AA)";
    case FourCC("I0PL"): return "Aghanim's Scepter (AA)";
    case FourCC("I0JP"): return "Aghanim's Scepter (Abaddon)";
    case FourCC("I0JY"): return "Aghanim's Scepter (Abaddon)";
    case FourCC("I0US"): return "Aghanim's Scepter (Admiral)";
    case FourCC("I0UT"): return "Aghanim's Scepter (Admiral)";
    case FourCC("I0TQ"): return "Aghanim's Scepter (Alchemist)";
    case FourCC("I0TR"): return "Aghanim's Scepter (Alchemist)";
    case FourCC("I10S"): return "Aghanim's Scepter (Anti-Mage)";
    case FourCC("I10T"): return "Aghanim's Scepter (Anti-Mage)";
    case FourCC("I0L0"): return "Aghanim's Scepter (Axe)";
    case FourCC("I0L9"): return "Aghanim's Scepter (Axe)";
    case FourCC("I0JQ"): return "Aghanim's Scepter (Bane)";
    case FourCC("I0JV"): return "Aghanim's Scepter (Bane)";
    case FourCC("I0AY"): return "Aghanim's Scepter (Basic)";
    case FourCC("I0TW"): return "Aghanim's Scepter (Batrider)";
    case FourCC("I0TX"): return "Aghanim's Scepter (Batrider)";
    case FourCC("I0O0"): return "Aghanim's Scepter (Beastmaster)";
    case FourCC("I0O1"): return "Aghanim's Scepter (Beastmaster)";
    case FourCC("I0WJ"): return "Aghanim's Scepter (Bloodseeker)";
    case FourCC("I0WK"): return "Aghanim's Scepter (Bloodseeker)";
    case FourCC("I0UP"): return "Aghanim's Scepter (Bristleback)";
    case FourCC("I0UQ"): return "Aghanim's Scepter (Bristleback)";
    case FourCC("I11D"): return "Aghanim's Scepter (Broodmother)";
    case FourCC("I11E"): return "Aghanim's Scepter (Broodmother)";
    case FourCC("I0RV"): return "Aghanim's Scepter (Centaur)";
    case FourCC("I0RW"): return "Aghanim's Scepter (Centaur)";
    case FourCC("I0WP"): return "Aghanim's Scepter (Chaos Knight)";
    case FourCC("I0WQ"): return "Aghanim's Scepter (Chaos Knight)";
    case FourCC("I0IG"): return "Aghanim's Scepter (Chen)";
    case FourCC("I0IK"): return "Aghanim's Scepter (Chen)";
    case FourCC("I0R5"): return "Aghanim's Scepter (Chieftain)";
    case FourCC("I0R6"): return "Aghanim's Scepter (Chieftain)";
    case FourCC("I193"): return "Aghanim's Scepter (Clinkz)";
    case FourCC("I194"): return "Aghanim's Scepter (Clinkz)";
    case FourCC("I0I9"): return "Aghanim's Scepter (Clockwerk)";
    case FourCC("I0IU"): return "Aghanim's Scepter (Clockwerk)";
    case FourCC("I0NR"): return "Aghanim's Scepter (Dark Seer)";
    case FourCC("I0NS"): return "Aghanim's Scepter (Dark Seer)";
    case FourCC("I0JO"): return "Aghanim's Scepter (Dazzle)";
    case FourCC("I0JX"): return "Aghanim's Scepter (Dazzle)";
    case FourCC("I0MN"): return "Aghanim's Scepter (Destroyer)";
    case FourCC("I0L5"): return "Aghanim's Scepter (Dirge)";
    case FourCC("I0L8"): return "Aghanim's Scepter (Dirge)";
    case FourCC("I127"): return "Aghanim's Scepter (Dragon Knight)";
    case FourCC("I128"): return "Aghanim's Scepter (Dragon Knight)";
    case FourCC("I0WM"): return "Aghanim's Scepter (Drow Ranger)";
    case FourCC("I0WN"): return "Aghanim's Scepter (Drow Ranger)";
    case FourCC("I0R2"): return "Aghanim's Scepter (Earth Spirit)";
    case FourCC("I0R3"): return "Aghanim's Scepter (Earth Spirit)";
    case FourCC("I0L7"): return "Aghanim's Scepter (Earthshaker)";
    case FourCC("I0LA"): return "Aghanim's Scepter (Earthshaker)";
    case FourCC("I11A"): return "Aghanim's Scepter (Ember Spirit)";
    case FourCC("I11B"): return "Aghanim's Scepter (Ember Spirit)";
    case FourCC("I0MP"): return "Aghanim's Scepter (Enchantress)";
    case FourCC("I0MQ"): return "Aghanim's Scepter (Enchantress)";
    case FourCC("I0PW"): return "Aghanim's Scepter (Enigma)";
    case FourCC("I0PX"): return "Aghanim's Scepter (Enigma)";
    case FourCC("I011"): return "Aghanim's Scepter (Furion)";
    case FourCC("I0EY"): return "Aghanim's Scepter (Furion)";
    case FourCC("I0RE"): return "Aghanim's Scepter (Goblin Shredder)";
    case FourCC("I0RF"): return "Aghanim's Scepter (Goblin Shredder)";
    case FourCC("I114"): return "Aghanim's Scepter (Gondar)";
    case FourCC("I115"): return "Aghanim's Scepter (Gondar)";
    case FourCC("I12D"): return "Aghanim's Scepter (Grimstroke)";
    case FourCC("I12E"): return "Aghanim's Scepter (Grimstroke)";
    case FourCC("I0NU"): return "Aghanim's Scepter (Gyrocopter)";
    case FourCC("I0NV"): return "Aghanim's Scepter (Gyrocopter)";
    case FourCC("I234"): return "Aghanim's Scepter (Hoodwink)";
    case FourCC("I235"): return "Aghanim's Scepter (Hoodwink)";
    case FourCC("I0IC"): return "Aghanim's Scepter (Huskar)";
    case FourCC("I0IO"): return "Aghanim's Scepter (Huskar)";
    case FourCC("I0IB"): return "Aghanim's Scepter (Invoker)";
    case FourCC("I0IN"): return "Aghanim's Scepter (Invoker)";
    case FourCC("I0IE"): return "Aghanim's Scepter (Jakiro)";
    case FourCC("I0IM"): return "Aghanim's Scepter (Jakiro)";
    case FourCC("I0IA"): return "Aghanim's Scepter (Juggernaut)";
    case FourCC("I0IR"): return "Aghanim's Scepter (Juggernaut)";
    case FourCC("I00D"): return "Aghanim's Scepter (KOTL)";
    case FourCC("I0EZ"): return "Aghanim's Scepter (KOTL)";
    case FourCC("I190"): return "Aghanim's Scepter (Krobelus)";
    case FourCC("I191"): return "Aghanim's Scepter (Krobelus)";
    case FourCC("I0RY"): return "Aghanim's Scepter (Legion Commander)";
    case FourCC("I0RZ"): return "Aghanim's Scepter (Legion Commander)";
    case FourCC("I009"): return "Aghanim's Scepter (Leshrac)";
    case FourCC("I0F0"): return "Aghanim's Scepter (Leshrac)";
    case FourCC("I005"): return "Aghanim's Scepter (Lich)";
    case FourCC("I0F6"): return "Aghanim's Scepter (Lich)";
    case FourCC("I0TN"): return "Aghanim's Scepter (Lifestealer)";
    case FourCC("I0TO"): return "Aghanim's Scepter (Lifestealer)";
    case FourCC("I00G"): return "Aghanim's Scepter (Lina)";
    case FourCC("I0F1"): return "Aghanim's Scepter (Lina)";
    case FourCC("I004"): return "Aghanim's Scepter (Lion)";
    case FourCC("I0F2"): return "Aghanim's Scepter (Lion)";
    case FourCC("I0S1"): return "Aghanim's Scepter (Lone Druid)";
    case FourCC("I0S2"): return "Aghanim's Scepter (Lone Druid)";
    case FourCC("I003"): return "Aghanim's Scepter (Lucifer)";
    case FourCC("I0F3"): return "Aghanim's Scepter (Lucifer)";
    case FourCC("I0F5"): return "Aghanim's Scepter (Luna";
    case FourCC("I002"): return "Aghanim's Scepter (Luna)";
    case FourCC("I101"): return "Aghanim's Scepter (Lycan)";
    case FourCC("I102"): return "Aghanim's Scepter (Lycan)";
    case FourCC("I0WG"): return "Aghanim's Scepter (Magnus)";
    case FourCC("I0WH"): return "Aghanim's Scepter (Magnus)";
    case FourCC("I117"): return "Aghanim's Scepter (Mars)";
    case FourCC("I118"): return "Aghanim's Scepter (Mars)";
    case FourCC("I104"): return "Aghanim's Scepter (Medusa)";
    case FourCC("I105"): return "Aghanim's Scepter (Medusa)";
    case FourCC("I0NX"): return "Aghanim's Scepter (Meepo)";
    case FourCC("I0NY"): return "Aghanim's Scepter (Meepo)";
    case FourCC("I12A"): return "Aghanim's Scepter (Monkey King)";
    case FourCC("I12B"): return "Aghanim's Scepter (Monkey King)";
    case FourCC("I187"): return "Aghanim's Scepter (Morphling)";
    case FourCC("I188"): return "Aghanim's Scepter (Morphling)";
    case FourCC("I0S4"): return "Aghanim's Scepter (Naga)";
    case FourCC("I0S5"): return "Aghanim's Scepter (Naga)";
    case FourCC("I001"): return "Aghanim's Scepter (Necro)";
    case FourCC("I0F4"): return "Aghanim's Scepter (Necro)";
    case FourCC("I0TB"): return "Aghanim's Scepter (Nerubian Assassin)";
    case FourCC("I0TC"): return "Aghanim's Scepter (Nerubian Assassin)";
    case FourCC("I0IF"): return "Aghanim's Scepter (Nightstalker)";
    case FourCC("I0IL"): return "Aghanim's Scepter (Nightstalker)";
    case FourCC("I0ML"): return "Aghanim's Scepter (Obsidian Destroyer)";
    case FourCC("I00I"): return "Aghanim's Scepter (Ogre)";
    case FourCC("I0FE"): return "Aghanim's Scepter (Ogre)";
    case FourCC("I0PQ"): return "Aghanim's Scepter (Omni)";
    case FourCC("I0PR"): return "Aghanim's Scepter (Omniknight)";
    case FourCC("I0WA"): return "Aghanim's Scepter (Oracle)";
    case FourCC("I0WB"): return "Aghanim's Scepter (Oracle)";
    case FourCC("I111"): return "Aghanim's Scepter (PA)";
    case FourCC("I112"): return "Aghanim's Scepter (PA)";
    case FourCC("I0I6"): return "Aghanim's Scepter (Panda)";
    case FourCC("I0I7"): return "Aghanim's Scepter (Panda)";
    case FourCC("I0I8"): return "Aghanim's Scepter (Panda)";
    case FourCC("I0WS"): return "Aghanim's Scepter (Phantom Lancer)";
    case FourCC("I0WT"): return "Aghanim's Scepter (Phantom Lancer)";
    case FourCC("I0R8"): return "Aghanim's Scepter (Phoenix)";
    case FourCC("I0R9"): return "Aghanim's Scepter (Phoenix)";
    case FourCC("I10M"): return "Aghanim's Scepter (Pit Lord)";
    case FourCC("I10N"): return "Aghanim's Scepter (Pit Lord)";
    case FourCC("I0W4"): return "Aghanim's Scepter (PotM)";
    case FourCC("I0W5"): return "Aghanim's Scepter (PotM)";
    case FourCC("I0LV"): return "Aghanim's Scepter (Puck)";
    case FourCC("I0LW"): return "Aghanim's Scepter (Puck)";
    case FourCC("I0JR"): return "Aghanim's Scepter (Pudge)";
    case FourCC("I0JS"): return "Aghanim's Scepter (Pudge)";
    case FourCC("I00H"): return "Aghanim's Scepter (Pugna)";
    case FourCC("I0F9"): return "Aghanim's Scepter (Pugna)";
    case FourCC("I00P"): return "Aghanim's Scepter (QoP)";
    case FourCC("I0FA"): return "Aghanim's Scepter (QoP)";
    case FourCC("I0M3"): return "Aghanim's Scepter (Razor)";
    case FourCC("I0M4"): return "Aghanim's Scepter (Razor)";
    case FourCC("I00U"): return "Aghanim's Scepter (Rhasta)";
    case FourCC("I0F7"): return "Aghanim's Scepter (Rhasta)";
    case FourCC("I107"): return "Aghanim's Scepter (Riki)";
    case FourCC("I108"): return "Aghanim's Scepter (Riki)";
    case FourCC("I0Q3"): return "Aghanim's Scepter (Rubick)";
    case FourCC("I0Q4"): return "Aghanim's Scepter (Rubick)";
    case FourCC("I0Q5"): return "Aghanim's Scepter (Rubick)";
    case FourCC("I000"): return "Aghanim's Scepter (Rylai)";
    case FourCC("I0FD"): return "Aghanim's Scepter (Rylai)";
    case FourCC("I0ID"): return "Aghanim's Scepter (Sand King)";
    case FourCC("I0IP"): return "Aghanim's Scepter (Sand King)";
    case FourCC("I0UY"): return "Aghanim's Scepter (SF)";
    case FourCC("I0UZ"): return "Aghanim's Scepter (SF)";
    case FourCC("I0QP"): return "Aghanim's Scepter (Shadow Demon)";
    case FourCC("I0QQ"): return "Aghanim's Scepter (Shadow Demon)";
    case FourCC("I0PH"): return "Aghanim's Scepter (Silencer)";
    case FourCC("I0PI"): return "Aghanim's Scepter (Silencer)";
    case FourCC("I0TE"): return "Aghanim's Scepter (Skeleton King)";
    case FourCC("I0TF"): return "Aghanim's Scepter (Skeleton King)";
    case FourCC("I0PT"): return "Aghanim's Scepter (Skywrath Mage)";
    case FourCC("I0PU"): return "Aghanim's Scepter (Skywrath Mage)";
    case FourCC("I10A"): return "Aghanim's Scepter (Slardar)";
    case FourCC("I10B"): return "Aghanim's Scepter (Slardar)";
    case FourCC("I0WV"): return "Aghanim's Scepter (Slark)";
    case FourCC("I0WW"): return "Aghanim's Scepter (Slark)";
    case FourCC("I0WY"): return "Aghanim's Scepter (Sniper)";
    case FourCC("I0WZ"): return "Aghanim's Scepter (Sniper)";
    case FourCC("I10D"): return "Aghanim's Scepter (Spectre)";
    case FourCC("I10E"): return "Aghanim's Scepter (Spectre)";
    case FourCC("I0JN"): return "Aghanim's Scepter (Spiritbreaker)";
    case FourCC("I0JW"): return "Aghanim's Scepter (Spiritbreaker)";
    case FourCC("I0WD"): return "Aghanim's Scepter (Storm)";
    case FourCC("I0WE"): return "Aghanim's Scepter (Storm)";
    case FourCC("I0RB"): return "Aghanim's Scepter (Sven)";
    case FourCC("I0RC"): return "Aghanim's Scepter (Sven)";
    case FourCC("I0KH"): return "Aghanim's Scepter (Techies)";
    case FourCC("I0KI"): return "Aghanim's Scepter (Techies)";
    case FourCC("I0KJ"): return "Aghanim's Scepter (Techies)";
    case FourCC("I10G"): return "Aghanim's Scepter (Templar Assassin)";
    case FourCC("I10H"): return "Aghanim's Scepter (Templar Assassin)";
    case FourCC("I10J"): return "Aghanim's Scepter (Terrorblade)";
    case FourCC("I10K"): return "Aghanim's Scepter (Terrorblade)";
    case FourCC("I0Q2"): return "Aghanim's Scepter (Thrall)";
    case FourCC("I0UV"): return "Aghanim's Scepter (Tide)";
    case FourCC("I0UW"): return "Aghanim's Scepter (Tide)";
    case FourCC("I0QM"): return "Aghanim's Scepter (Tinker)";
    case FourCC("I0QN"): return "Aghanim's Scepter (Tinker)";
    case FourCC("I0MG"): return "Aghanim's Scepter (Tiny)";
    case FourCC("I0MH"): return "Aghanim's Scepter (Tiny)";
    case FourCC("I0RH"): return "Aghanim's Scepter (Treant)";
    case FourCC("I0RI"): return "Aghanim's Scepter (Treant)";
    case FourCC("I12G"): return "Aghanim's Scepter (Troll)";
    case FourCC("I12H"): return "Aghanim's Scepter (Troll)";
    case FourCC("I0TH"): return "Aghanim's Scepter (Tuskarr)";
    case FourCC("I0TI"): return "Aghanim's Scepter (Tuskarr)";
    case FourCC("I0V1"): return "Aghanim's Scepter (Ursa)";
    case FourCC("I0V2"): return "Aghanim's Scepter (Ursa)";
    case FourCC("I0II"): return "Aghanim's Scepter (Vengeful Spirit)";
    case FourCC("I0IS"): return "Aghanim's Scepter (Vengeful Spirit)";
    case FourCC("I016"): return "Aghanim's Scepter (Veno)";
    case FourCC("I0FC"): return "Aghanim's Scepter (Veno)";
    case FourCC("I0M7"): return "Aghanim's Scepter (Viper)";
    case FourCC("I0M8"): return "Aghanim's Scepter (Viper)";
    case FourCC("I0OS"): return "Aghanim's Scepter (Visage)";
    case FourCC("I0OT"): return "Aghanim's Scepter (Visage)";
    case FourCC("I0JM"): return "Aghanim's Scepter (Void)";
    case FourCC("I0JU"): return "Aghanim's Scepter (Void)";
    case FourCC("I0M0"): return "Aghanim's Scepter (Warlock)";
    case FourCC("I0M2"): return "Aghanim's Scepter (Warlock)";
    case FourCC("I0TK"): return "Aghanim's Scepter (Weaver)";
    case FourCC("I0TL"): return "Aghanim's Scepter (Weaver)";
    case FourCC("I0JL"): return "Aghanim's Scepter (Windrunner)";
    case FourCC("I0JT"): return "Aghanim's Scepter (Windrunner)";
    case FourCC("I10P"): return "Aghanim's Scepter (Winter Wyvern)";
    case FourCC("I10Q"): return "Aghanim's Scepter (Winter Wyvern)";
    case FourCC("I124"): return "Aghanim's Scepter (Wisp)";
    case FourCC("I125"): return "Aghanim's Scepter (Wisp)";
    case FourCC("I017"): return "Aghanim's Scepter (Witch Doctor)";
    case FourCC("I0FB"): return "Aghanim's Scepter (Witch Doctor)";
    case FourCC("I10V"): return "Aghanim's Scepter (Zet)";
    case FourCC("I10W"): return "Aghanim's Scepter (Zet)";
    case FourCC("I00O"): return "Aghanim's Scepter (Zeus)";
    case FourCC("I0F8"): return "Aghanim's Scepter (Zeus)";
    case FourCC("IR0K"): return "Amplify Damage Rune";
    case FourCC("IRRO"): return "Amplify Damage Rune";
    case FourCC("I05B"): return "Animal Courier";
    case FourCC("I05J"): return "Animal Courier";
    case FourCC("I12L"): return "Arcane Blink";
    case FourCC("I12M"): return "Arcane Blink";
    case FourCC("I12N"): return "Arcane Blink";
    case FourCC("I12I"): return "Arcane Blink Recipe";
    case FourCC("I12J"): return "Arcane Blink Recipe";
    case FourCC("I12K"): return "Arcane Blink Recipe";
    case FourCC("I0MI"): return "Arcane Boots";
    case FourCC("I0MJ"): return "Arcane Boots";
    case FourCC("I0MK"): return "Arcane Boots";
    case FourCC("I195"): return "Arcane Boots Recipe";
    case FourCC("I196"): return "Arcane Boots Recipe";
    case FourCC("I197"): return "Arcane Boots Recipe";
    case FourCC("IRV8"): return "Arcane Rune";
    case FourCC("IRV9"): return "Arcane Rune";
    case FourCC("I00M"): return "Armlet of Mordiggian";
    case FourCC("I00Q"): return "Armlet of Mordiggian";
    case FourCC("I00S"): return "Armlet of Mordiggian";
    case FourCC("I00T"): return "Armlet of Mordiggian";
    case FourCC("I00N"): return "Armlet of Mordiggian (Courier Edition)";
    case FourCC("I00R"): return "Armlet of Mordiggian (Courier Edition)";
    case FourCC("I00V"): return "Armlet of Mordiggian (Courier Edition)";
    case FourCC("I00W"): return "Armlet of Mordiggian (Courier Edition)";
    case FourCC("I0DE"): return "Armlet of Mordiggian (Off,Courier)";
    case FourCC("I01E"): return "Armlet of Mordiggian (Off)";
    case FourCC("I01F"): return "Armlet of Mordiggian (On,Courier)";
    case FourCC("I01C"): return "Armlet of Mordiggian (On)";
    case FourCC("I07S"): return "Armlet of Mordiggian Recipe";
    case FourCC("I08D"): return "Armlet of Mordiggian Recipe";
    case FourCC("I0ET"): return "Armlet of Mordiggian Recipe";
    case FourCC("I0BI"): return "Assault Cuirass";
    case FourCC("I0BJ"): return "Assault Cuirass";
    case FourCC("I0C2"): return "Assault Cuirass";
    case FourCC("I07R"): return "Assault Cuirass Recipe";
    case FourCC("I08C"): return "Assault Cuirass Recipe";
    case FourCC("I0ES"): return "Assault Cuirass Recipe";
    case FourCC("I099"): return "Battle Fury";
    case FourCC("I09A"): return "Battle Fury";
    case FourCC("I02B"): return "Battle Fury";
    case FourCC("I02Z"): return "Belt of Strength";
    case FourCC("I043"): return "Belt of Strength";
    case FourCC("I0CV"): return "Belt of Strength";
    case FourCC("I0G2"): return "Black King Bar (10)";
    case FourCC("I0G8"): return "Black King Bar (10)";
    case FourCC("I0G4"): return "Black King Bar (5)";
    case FourCC("I0GA"): return "Black King Bar (5)";
    case FourCC("I0G3"): return "Black King Bar (6)";
    case FourCC("I0GB"): return "Black King Bar (6)";
    case FourCC("I02D"): return "Black King Bar (7)";
    case FourCC("I0G5"): return "Black King Bar (7)";
    case FourCC("I09E"): return "Black King Bar (8)";
    case FourCC("I0G7"): return "Black King Bar (8)";
    case FourCC("I0G6"): return "Black King Bar (9)";
    case FourCC("I0G9"): return "Black King Bar (9)";
    case FourCC("I0FZ"): return "Black King Bar [10]";
    case FourCC("I0G1"): return "Black King Bar [5]";
    case FourCC("I09D"): return "Black King Bar [6]";
    case FourCC("I0FS"): return "Black King Bar [7]";
    case FourCC("I0FY"): return "Black King Bar [8]";
    case FourCC("I0G0"): return "Black King Bar [9]";
    case FourCC("I078"): return "Black King Bar Recipe";
    case FourCC("I07X"): return "Black King Bar Recipe";
    case FourCC("I0EC"): return "Black King Bar Recipe";
    case FourCC("I08N"): return "Blade Mail";
    case FourCC("I08O"): return "Blade Mail";
    case FourCC("I021"): return "Blade Mail";
    case FourCC("I120"): return "Blade Mail Recipe";
    case FourCC("I121"): return "Blade Mail Recipe";
    case FourCC("I122"): return "Blade Mail Recipe";
    case FourCC("I030"): return "Blade of Alacrity";
    case FourCC("I044"): return "Blade of Alacrity";
    case FourCC("I0D2"): return "Blade of Alacrity";
    case FourCC("I031"): return "Blades of Attack";
    case FourCC("I045"): return "Blades of Attack";
    case FourCC("I0D5"): return "Blades of Attack";
    case FourCC("I0VP"): return "Blight Stone";
    case FourCC("I0VQ"): return "Blight Stone";
    case FourCC("I0VR"): return "Blight Stone";
    case FourCC("I03E"): return "Blink Dagger";
    case FourCC("I04H"): return "Blink Dagger";
    case FourCC("I0C7"): return "Blink Dagger";
    case FourCC("I130"): return "Blitz Knuckles";
    case FourCC("I131"): return "Blitz Knuckles";
    case FourCC("I132"): return "Blitz Knuckles";
    case FourCC("I212"): return "Blood Grenade";
    case FourCC("I213"): return "Blood Grenade";
    case FourCC("I214"): return "Blood Grenade";
    case FourCC("I0BL"): return "Bloodstone";
    case FourCC("I0C3"): return "Bloodstone";
    case FourCC("I0BK"): return "Bloodstone";
    case FourCC("I0RJ"): return "Bloodstone Recipe";
    case FourCC("I0RK"): return "Bloodstone Recipe";
    case FourCC("I0RL"): return "Bloodstone Recipe";
    case FourCC("I0VJ"): return "Bloodthorn";
    case FourCC("I0VK"): return "Bloodthorn";
    case FourCC("I0VL"): return "Bloodthorn";
    case FourCC("I0VG"): return "Bloodthorn Recipe";
    case FourCC("I0VH"): return "Bloodthorn Recipe";
    case FourCC("I0VI"): return "Bloodthorn Recipe";
    case FourCC("I209"): return "Boots of Bearing";
    case FourCC("I210"): return "Boots of Bearing";
    case FourCC("I211"): return "Boots of Bearing";
    case FourCC("I206"): return "Boots of Bearing Recipe";
    case FourCC("I208"): return "Boots of Bearing Recipe";
    case FourCC("I02R"): return "Boots of Elvenskin";
    case FourCC("I02N"): return "Boots of Elvenskin";
    case FourCC("I0CS"): return "Boots of Elvenskin";
    case FourCC("I02O"): return "Boots of Speed";
    case FourCC("I02Q"): return "Boots of Speed";
    case FourCC("I00A"): return "Boots of Speed";
    case FourCC("I06B"): return "Boots of Travel";
    case FourCC("I06C"): return "Boots of Travel";
    case FourCC("I01P"): return "Boots of Travel";
    case FourCC("I11F"): return "Boots of Travel Consumable";
    case FourCC("I11G"): return "Boots of Travel Consumable";
    case FourCC("I11H"): return "Boots of Travel Consumable";
    case FourCC("I11I"): return "Boots of Travel Consumable Recipe";
    case FourCC("I11J"): return "Boots of Travel Consumable Recipe";
    case FourCC("I11K"): return "Boots of Travel Consumable Recipe";
    case FourCC("I05O"): return "Boots of Travel Recipe";
    case FourCC("I05T"): return "Boots of Travel Recipe";
    case FourCC("I0DU"): return "Boots of Travel Recipe";
    case FourCC("I0SZ"): return "Boots of Travel Upgraded";
    case FourCC("I0T0"): return "Boots of Travel Upgraded";
    case FourCC("I0T1"): return "Boots of Travel Upgraded";
    case FourCC("I0Q6"): return "Borrowed Tango";
    case FourCC("IRQR"): return "Bounty";
    case FourCC("IRRR"): return "Bounty";
    case FourCC("I06H"): return "Bracer";
    case FourCC("I06I"): return "Bracer";
    case FourCC("I01V"): return "Bracer";
    case FourCC("I05K"): return "Bracer Recipe";
    case FourCC("I05V"): return "Bracer Recipe";
    case FourCC("I0DX"): return "Bracer Recipe";
    case FourCC("I032"): return "Broadsword";
    case FourCC("I046"): return "Broadsword";
    case FourCC("I0CZ"): return "Broadsword";
    case FourCC("I0A3"): return "Buriza-do Kyanon";
    case FourCC("I0A4"): return "Buriza-do Kyanon";
    case FourCC("I0BQ"): return "Buriza-do Kyanon";
    case FourCC("I07E"): return "Buriza-do Kyanon Recipe";
    case FourCC("I083"): return "Buriza-do Kyanon Recipe";
    case FourCC("I0EI"): return "Buriza-do Kyanon Recipe";
    case FourCC("I0D7"): return "Chain Mail";
    case FourCC("I033"): return "Chainmail";
    case FourCC("I047"): return "Chainmail";
    case FourCC("I0B0"): return "Cheese";
    case FourCC("I0B1"): return "Cheese";
    case FourCC("I0U3"): return "Cheese";
    case FourCC("I0U4"): return "Cheese";
    case FourCC("I02X"): return "Circlet of Nobility";
    case FourCC("I02Y"): return "Circlet of Nobility";
    case FourCC("I0D4"): return "Circlet of Nobility";
    case FourCC("I042"): return "Clarity Potion";
    case FourCC("I05D"): return "Clarity Potion";
    case FourCC("I0PY"): return "Clarity Potion (muted)";
    case FourCC("I034"): return "Claymore";
    case FourCC("I048"): return "Claymore";
    case FourCC("I0CX"): return "Claymore";
    case FourCC("I03L"): return "Cloak";
    case FourCC("I04P"): return "Cloak";
    case FourCC("I0CE"): return "Cloak";
    case FourCC("I08J"): return "Cranium Basher";
    case FourCC("I08K"): return "Cranium Basher";
    case FourCC("I020"): return "Cranium Basher";
    case FourCC("I06R"): return "Cranium Basher Recipe";
    case FourCC("I06S"): return "Cranium Basher Recipe";
    case FourCC("I0E2"): return "Cranium Basher Recipe";
    case FourCC("I0QV"): return "Crimson Guard";
    case FourCC("I0QW"): return "Crimson Guard";
    case FourCC("I0QX"): return "Crimson Guard";
    case FourCC("I0QY"): return "Crimson Guard recipe";
    case FourCC("I0QZ"): return "Crimson Guard recipe";
    case FourCC("I0R0"): return "Crimson Guard recipe";
    case FourCC("I11R"): return "Crown";
    case FourCC("I11S"): return "Crown";
    case FourCC("I11T"): return "Crown";
    case FourCC("I09B"): return "Crystalys";
    case FourCC("I09C"): return "Crystalys";
    case FourCC("I02C"): return "Crystalys";
    case FourCC("I077"): return "Crystalys Recipe";
    case FourCC("I07W"): return "Crystalys Recipe";
    case FourCC("I0EB"): return "Crystalys Recipe";
    case FourCC("I09K"): return "Dagon Level 1";
    case FourCC("I09M"): return "Dagon Level 1";
    case FourCC("I01G"): return "Dagon Level 1";
    case FourCC("I09P"): return "Dagon Level 2";
    case FourCC("I09J"): return "Dagon Level 2";
    case FourCC("I02G"): return "Dagon Level 2";
    case FourCC("I09N"): return "Dagon Level 3";
    case FourCC("I09Q"): return "Dagon Level 3";
    case FourCC("I0DC"): return "Dagon Level 3";
    case FourCC("I09O"): return "Dagon Level 4";
    case FourCC("I09S"): return "Dagon Level 4";
    case FourCC("I0DD"): return "Dagon Level 4";
    case FourCC("I09L"): return "Dagon Level 5";
    case FourCC("I09R"): return "Dagon Level 5";
    case FourCC("I0D3"): return "Dagon Level 5";
    case FourCC("I07B"): return "Dagon Recipe";
    case FourCC("I080"): return "Dagon Recipe";
    case FourCC("I0EF"): return "Dagon Recipe";
    case FourCC("I035"): return "Demon Edge";
    case FourCC("I049"): return "Demon Edge";
    case FourCC("I0CI"): return "Demon Edge";
    case FourCC("I097"): return "Desolator";
    case FourCC("I098"): return "Desolator";
    case FourCC("I02A"): return "Desolator";
    case FourCC("I076"): return "Desolator Recipe";
    case FourCC("I07V"): return "Desolator Recipe";
    case FourCC("I0EA"): return "Desolator Recipe";
    case FourCC("I08R"): return "Diffusal Blade";
    case FourCC("I0EX"): return "Diffusal Blade (Empty)";
    case FourCC("I08S"): return "Diffusal Blade Level 1";
    case FourCC("I023"): return "Diffusal Blade Level 1";
    case FourCC("I0EV"): return "Diffusal Blade Level 1 (Empty)";
    case FourCC("I0EW"): return "Diffusal Blade Level 1 (Empty)";
    case FourCC("I0J9"): return "Diffusal Blade Level 2";
    case FourCC("I0JB"): return "Diffusal Blade Level 2";
    case FourCC("I0JD"): return "Diffusal Blade Level 2";
    case FourCC("I0JA"): return "Diffusal Blade Level 2 (Empty)";
    case FourCC("I0JC"): return "Diffusal Blade Level 2 (Empty)";
    case FourCC("I0JE"): return "Diffusal Blade Level 2(Empty)";
    case FourCC("I06X"): return "Diffusal Blade Recipe";
    case FourCC("I06Y"): return "Diffusal Blade Recipe";
    case FourCC("I0E5"): return "Diffusal Blade Recipe";
    case FourCC("I04I"): return "Disabled Blink Dagger";
    case FourCC("I0DH"): return "Disabled Blink Dagger";
    case FourCC("I0KL"): return "Disabled Heart of Tarrasque";
    case FourCC("I0KM"): return "Disabled Heart of Tarrasque";
    case FourCC("I177"): return "Disperser";
    case FourCC("I178"): return "Disperser";
    case FourCC("I179"): return "Disperser";
    case FourCC("I174"): return "Disperser Recipe";
    case FourCC("I175"): return "Disperser Recipe";
    case FourCC("I176"): return "Disperser Recipe";
    case FourCC("I0A1"): return "Divine Rapier";
    case FourCC("I0A2"): return "Divine Rapier";
    case FourCC("I0BP"): return "Divine Rapier";
    case FourCC("I0LK"): return "Divine Rapier";
    case FourCC("I0LI"): return "Divine Rapier Free";
    case FourCC("I0LJ"): return "Divine Rapier Free";
    case FourCC("I0UB"): return "Dragon Lance";
    case FourCC("I0UC"): return "Dragon Lance";
    case FourCC("I0UD"): return "Dragon Lance (Muted)";
    case FourCC("I0N9"): return "Drum of Endurance";
    case FourCC("I0NC"): return "Drum of Endurance";
    case FourCC("I0ND"): return "Drum of Endurance";
    case FourCC("I0NE"): return "Drum of Endurance";
    case FourCC("I0NK"): return "Drum of Endurance (empty unused)";
    case FourCC("I0NL"): return "Drum of Endurance (empty)";
    case FourCC("I0NM"): return "Drum of Endurance (empty)";
    case FourCC("I0NA"): return "Drum of Endurance Recipe";
    case FourCC("I0NB"): return "Drum of Endurance Recipe";
    case FourCC("I02M"): return "Dummy Item Scourge";
    case FourCC("I0PC"): return "Dummy Item Sentinel";
    case FourCC("I0GH"): return "Dust of Appearance";
    case FourCC("I0GI"): return "Dust of Appearance";
    case FourCC("I036"): return "Eaglehorn";
    case FourCC("I04A"): return "Eaglehorn";
    case FourCC("I0CH"): return "Eaglehorn";
    case FourCC("I0VM"): return "Echo Sabre";
    case FourCC("I0VN"): return "Echo Sabre";
    case FourCC("I0VO"): return "Echo Sabre";
    case FourCC("I05E"): return "Empty Bottle";
    case FourCC("I0AM"): return "Empty Bottle";
    case FourCC("I00L"): return "Empty Bottle";
    case FourCC("I0DN"): return "Empty Bottle";
    case FourCC("I0AV"): return "Empty Bottle Store";
    case FourCC("I0S6"): return "Enchanted Mango";
    case FourCC("I0S7"): return "Enchanted Mango";
    case FourCC("I0S8"): return "Enchanted Mango";
    case FourCC("I037"): return "Energy Booster";
    case FourCC("I04B"): return "Energy Booster";
    case FourCC("I0CR"): return "Energy Booster";
    case FourCC("I14I"): return "Eternal Shroud";
    case FourCC("I14J"): return "Eternal Shroud";
    case FourCC("I14F"): return "Eternal Shroud Recipe";
    case FourCC("I14G"): return "Eternal Shroud Recipe";
    case FourCC("I14H"): return "Eternal Shroud Recipe";
    case FourCC("I14K"): return "Eternal Shrouds";
    case FourCC("I0LR"): return "Ethereal Blade";
    case FourCC("I0LS"): return "Ethereal Blade";
    case FourCC("I0LT"): return "Ethereal Blade";
    case FourCC("I171"): return "Ethereal Blade Recipe";
    case FourCC("I172"): return "Ethereal Blade Recipe";
    case FourCC("I173"): return "Ethereal Blade Recipe";
    case FourCC("I08Z"): return "Eul's Scepter";
    case FourCC("I090"): return "Eul's Scepter";
    case FourCC("I026"): return "Eul's Scepter";
    case FourCC("I071"): return "Eul's Scepter Recipe";
    case FourCC("I072"): return "Eul's Scepter Recipe";
    case FourCC("I0E7"): return "Eul's Scepter Recipe";
    case FourCC("I0AD"): return "Eye of Skadi";
    case FourCC("I0AE"): return "Eye of Skadi";
    case FourCC("I0BV"): return "Eye of Skadi";
    case FourCC("I0U8"): return "Faerie Fire";
    case FourCC("I0U9"): return "Faerie Fire";
    case FourCC("I0UA"): return "Faerie Fire (Muted)";
    case FourCC("I143"): return "Falcon Blade";
    case FourCC("I144"): return "Falcon Blade";
    case FourCC("I140"): return "Falcon Blade Recipe";
    case FourCC("I141"): return "Falcon Blade Recipe";
    case FourCC("I142"): return "Falcon Blade Recipe";
    case FourCC("I145"): return "Falcon Blades";
    case FourCC("I221"): return "Fallen Sky";
    case FourCC("I222"): return "Fallen Sky";
    case FourCC("I223"): return "Fallen Sky";
    case FourCC("I224"): return "Fallen Sky Recipe";
    case FourCC("I225"): return "Fallen Sky Recipe";
    case FourCC("I226"): return "Fallen Sky Recipe";
    case FourCC("IRRS"): return "First Bounty";
    case FourCC("I13L"): return "Fluffy Cape";
    case FourCC("I13M"): return "Fluffy Cape";
    case FourCC("I13N"): return "Fluffy Cape";
    case FourCC("I014"): return "Flying Courier";
    case FourCC("I01D"): return "Flying Courier";
    case FourCC("I07P"): return "Flying Courier Recipe";
    case FourCC("I08A"): return "Flying Courier Recipe";
    case FourCC("I0EQ"): return "Flying Courier Recipe";
    case FourCC("I0HG"): return "Force Staff";
    case FourCC("I0HH"): return "Force Staff";
    case FourCC("I0HI"): return "Force Staff";
    case FourCC("I0HD"): return "Force Staff Recipe";
    case FourCC("I0HE"): return "Force Staff Recipe";
    case FourCC("I0HF"): return "Force Staff Recipe";
    case FourCC("I038"): return "Gauntlets of Strength";
    case FourCC("I04C"): return "Gauntlets of Strength";
    case FourCC("I0CJ"): return "Gauntlets of Strength";
    case FourCC("I039"): return "Gem of True Sight";
    case FourCC("I04D"): return "Gem of True Sight";
    case FourCC("I0DG"): return "Gem of True Sight";
    case FourCC("I0MS"): return "Gem of True Sight (Courier Edition)";
    case FourCC("I0MT"): return "Gem of True Sight (Courier Edition)";
    case FourCC("I0MR"): return "Gem of True Sight (Courier Edition)";
    case FourCC("I0JI"): return "Ghost Scepter";
    case FourCC("I0JJ"): return "Ghost Scepter";
    case FourCC("I0JK"): return "Ghost Scepter";
    case FourCC("I13F"): return "Gleipnir";
    case FourCC("I13G"): return "Gleipnir";
    case FourCC("I13H"): return "Gleipnir";
    case FourCC("I13C"): return "Gleipnir Recipe";
    case FourCC("I13D"): return "Gleipnir Recipe";
    case FourCC("I13E"): return "Gleipnir Recipe";
    case FourCC("I0S9"): return "Glimmer Cape";
    case FourCC("I0SA"): return "Glimmer Cape";
    case FourCC("I0SB"): return "Glimmer Cape";
    case FourCC("I02P"): return "Gloves of Haste";
    case FourCC("I02S"): return "Gloves of Haste";
    case FourCC("I0CA"): return "Gloves of Haste";
    case FourCC("I0SC"): return "Guardian Greaves";
    case FourCC("I0SD"): return "Guardian Greaves";
    case FourCC("I0SE"): return "Guardian Greaves";
    case FourCC("I0SF"): return "Guardian Greaves Recipe";
    case FourCC("I0SG"): return "Guardian Greaves Recipe";
    case FourCC("I0SH"): return "Guardian Greaves Recipe";
    case FourCC("I07K"): return "Guinsoo's Scythe";
    case FourCC("I0AL"): return "Guinsoo's Scythe";
    case FourCC("I0BY"): return "Guinsoo's Scythe";
    case FourCC("I06D"): return "Hand of Midas";
    case FourCC("I06E"): return "Hand of Midas";
    case FourCC("I01T"): return "Hand of Midas";
    case FourCC("I0DW"): return "Hand of Midas Recipe";
    case FourCC("I05P"): return "Hand of Midas Recipe";
    case FourCC("I05U"): return "Hand of Midas Recipe";
    case FourCC("I228"): return "Harpoon";
    case FourCC("I227"): return "Harpoon";
    case FourCC("I229"): return "Harpoon";
    case FourCC("I230"): return "Harpoon Recipe";
    case FourCC("I231"): return "Harpoon Recipe";
    case FourCC("I232"): return "Harpoon Recipe";
    case FourCC("IRRM"): return "Haste";
    case FourCC("I063"): return "Headdress";
    case FourCC("I064"): return "Headdress";
    case FourCC("I01K"): return "Headdress";
    case FourCC("I05L"): return "Headdress Recipe";
    case FourCC("I05R"): return "Headdress Recipe";
    case FourCC("I0DS"): return "Headdress Recipe";
    case FourCC("I056"): return "Healing Salve";
    case FourCC("I05F"): return "Healing Salve";
    case FourCC("I0PZ"): return "Healing Salve (muted)";
    case FourCC("I0A9"): return "Heart of Tarrasque";
    case FourCC("I0AA"): return "Heart of Tarrasque";
    case FourCC("I0BT"): return "Heart of Tarrasque";
    case FourCC("I07H"): return "Heart of Tarrasque Recipe";
    case FourCC("I084"): return "Heart of Tarrasque Recipe";
    case FourCC("I0EK"): return "Heart of Tarrasque Recipe";
    case FourCC("I0OU"): return "Heaven's Halberd";
    case FourCC("I0OV"): return "Heaven's Halberd";
    case FourCC("I0OW"): return "Heaven's Halberd";
    case FourCC("I14R"): return "Heaven's Halberd Recipe";
    case FourCC("I14S"): return "Heaven's Halberd Recipe";
    case FourCC("I14T"): return "Heaven's Halberd Recipe";
    case FourCC("I03B"): return "Helm of Iron Will";
    case FourCC("I04E"): return "Helm of Iron Will";
    case FourCC("I0DA"): return "Helm of Iron Will";
    case FourCC("I08U"): return "Helm of the Dominator";
    case FourCC("I024"): return "Helm of the Dominator";
    case FourCC("I08T"): return "Helm of the Dominator";
    case FourCC("I08W"): return "Helm of the Dominator (Courier Edition)";
    case FourCC("I0CO"): return "Helm of the Dominator (Courier)";
    case FourCC("I0X0"): return "Helm of the Dominator Recipe";
    case FourCC("I0X1"): return "Helm of the Dominator Recipe";
    case FourCC("I0X2"): return "Helm of the Dominator Recipe";
    case FourCC("I181"): return "Helm of the Overlord";
    case FourCC("I182"): return "Helm of the Overlord";
    case FourCC("I180"): return "Helm of the Overlord";
    case FourCC("I183"): return "Helm of the Overlord Recipe";
    case FourCC("I184"): return "Helm of the Overlord Recipe";
    case FourCC("I185"): return "Helm of the Overlord Recipe";
    case FourCC("I149"): return "Holy Locket";
    case FourCC("I14A"): return "Holy Locket";
    case FourCC("I146"): return "Holy Locket Recipe";
    case FourCC("I147"): return "Holy Locket Recipe";
    case FourCC("I148"): return "Holy Locket Recipe";
    case FourCC("I14B"): return "Holy Lockets";
    case FourCC("I0BM"): return "Hood of Defiance";
    case FourCC("I0BN"): return "Hood of Defiance";
    case FourCC("I0C4"): return "Hood of Defiance";
    case FourCC("I0W0"): return "Hurricane Pike";
    case FourCC("I0W1"): return "Hurricane Pike";
    case FourCC("I0W2"): return "Hurricane Pike";
    case FourCC("I0VX"): return "Hurricane Pike Recipe";
    case FourCC("I0VY"): return "Hurricane Pike Recipe";
    case FourCC("I0VZ"): return "Hurricane Pike Recipe";
    case FourCC("I03C"): return "Hyperstone";
    case FourCC("I04F"): return "Hyperstone";
    case FourCC("I0CQ"): return "Hyperstone";
    case FourCC("IR07"): return "Illusion";
    case FourCC("IRRN"): return "Illusion";
    case FourCC("I0W6"): return "Infused Raindrop";
    case FourCC("I0W7"): return "Infused Raindrop";
    case FourCC("I0W8"): return "Infused Raindrop";
    case FourCC("IR0J"): return "Invisibility";
    case FourCC("IRRQ"): return "Invisibility";
    case FourCC("I0UH"): return "Iron Talon";
    case FourCC("I0UI"): return "Iron Talon";
    case FourCC("I0UJ"): return "Iron Talon (Muted)";
    case FourCC("I0UK"): return "Iron Talon Recipe";
    case FourCC("I0UL"): return "Iron Talon Recipe";
    case FourCC("I0UM"): return "Iron Talon Recipe (Muted)";
    case FourCC("I03D"): return "Ironwood Branch";
    case FourCC("I04G"): return "Ironwood Branch";
    case FourCC("I0CU"): return "Ironwood Branch";
    case FourCC("I041"): return "Javelin";
    case FourCC("I055"): return "Javelin";
    case FourCC("I0D6"): return "Javelin";
    case FourCC("I0X3"): return "Kaya";
    case FourCC("I0X4"): return "Kaya";
    case FourCC("I0X5"): return "Kaya";
    case FourCC("I0X6"): return "Kaya Recipe";
    case FourCC("I0X7"): return "Kaya Recipe";
    case FourCC("I0X8"): return "Kaya Recipe";
    case FourCC("I203"): return "Khanda";
    case FourCC("I204"): return "Khanda";
    case FourCC("I205"): return "Khanda";
    case FourCC("I200"): return "Khanda Recipe";
    case FourCC("I201"): return "Khanda Recipe";
    case FourCC("I202"): return "Khanda Recipe";
    case FourCC("I09Z"): return "Linken's Sphere";
    case FourCC("I0A0"): return "Linken's Sphere";
    case FourCC("I0HK"): return "Linken's Sphere";
    case FourCC("I0HM"): return "Linken's Sphere";
    case FourCC("I0HJ"): return "Linken's Sphere Dummy";
    case FourCC("I07D"): return "Linken's Sphere Recipe";
    case FourCC("I082"): return "Linken's Sphere Recipe";
    case FourCC("I0EH"): return "Linken's Sphere Recipe";
    case FourCC("I0BO"): return "Linkin's Sphere";
    case FourCC("I0HL"): return "Linkin's Sphere";
    case FourCC("I09H"): return "Lothar's Edge";
    case FourCC("I09I"): return "Lothar's Edge";
    case FourCC("I02F"): return "Lothar's Edge";
    case FourCC("I0T5"): return "Lotus Orb";
    case FourCC("I0T6"): return "Lotus Orb";
    case FourCC("I0T7"): return "Lotus Orb";
    case FourCC("I08P"): return "Maelstrom";
    case FourCC("I08Q"): return "Maelstrom";
    case FourCC("I022"): return "Maelstrom";
    case FourCC("I06V"): return "Maelstrom Recipe";
    case FourCC("I06W"): return "Maelstrom Recipe";
    case FourCC("I0E4"): return "Maelstrom Recipe";
    case FourCC("I14O"): return "Mage Slayer";
    case FourCC("I14P"): return "Mage Slayer";
    case FourCC("I14L"): return "Mage Slayer Recipe";
    case FourCC("I14M"): return "Mage Slayer Recipe";
    case FourCC("I14N"): return "Mage Slayer Recipe";
    case FourCC("I14Q"): return "Mage Slayers";
    case FourCC("I0GC"): return "Magic Stick";
    case FourCC("I0GD"): return "Magic Stick";
    case FourCC("I0GE"): return "Magic Stick";
    case FourCC("I0HA"): return "Magic Wand";
    case FourCC("I0HB"): return "Magic Wand";
    case FourCC("I0HC"): return "Magic Wand";
    case FourCC("I13R"): return "Magic Wand Recipe";
    case FourCC("I13S"): return "Magic Wand Recipe";
    case FourCC("I13T"): return "Magic Wand Recipe";
    case FourCC("I0AN"): return "Magical Bottle - 1/3";
    case FourCC("I0AU"): return "Magical Bottle - 1/3";
    case FourCC("I0DO"): return "Magical Bottle - 1/3";
    case FourCC("I0AO"): return "Magical Bottle - 2/3";
    case FourCC("I0AS"): return "Magical Bottle - 2/3";
    case FourCC("I0DP"): return "Magical Bottle - 2/3";
    case FourCC("I0AP"): return "Magical Bottle - 3/3";
    case FourCC("I0AT"): return "Magical Bottle - 3/3";
    case FourCC("I0DQ"): return "Magical Bottle - 3/3";
    case FourCC("I0H0"): return "Magical Bottle - Amplify Damage Rune";
    case FourCC("I0AR"): return "Magical Bottle - Amplify Damage Rune";
    case FourCC("I0H6"): return "Magical Bottle - Amplify Damage Rune";
    case FourCC("I0VA"): return "Magical Bottle - Arcane rune";
    case FourCC("I0VC"): return "Magical Bottle - Arcane rune";
    case FourCC("I0VB"): return "Magical Bottle - Arcane rune";
    case FourCC("I0QS"): return "Magical Bottle - Bounty";
    case FourCC("I0QT"): return "Magical Bottle - Bounty";
    case FourCC("I0QU"): return "Magical Bottle - Bounty";
    case FourCC("I0H1"): return "Magical Bottle - Haste";
    case FourCC("I0GV"): return "Magical Bottle - Haste";
    case FourCC("I0H4"): return "Magical Bottle - Haste";
    case FourCC("I0GY"): return "Magical Bottle - Illusion";
    case FourCC("I0GW"): return "Magical Bottle - Illusion";
    case FourCC("I0H3"): return "Magical Bottle - Illusion";
    case FourCC("I0GZ"): return "Magical Bottle - Invisibility";
    case FourCC("I0AQ"): return "Magical Bottle - Invisibility";
    case FourCC("I0H5"): return "Magical Bottle - Invisibility";
    case FourCC("I0DR"): return "Magical Bottle - Regeneration";
    case FourCC("I0GX"): return "Magical Bottle - Regeneration";
    case FourCC("I0H2"): return "Magical Bottle - Regeneration";
    case FourCC("I163"): return "Magical Bottle - Shield rune";
    case FourCC("I164"): return "Magical Bottle - Shield rune";
    case FourCC("I165"): return "Magical Bottle - Shield rune";
    case FourCC("I167"): return "Magical Bottle - Water";
    case FourCC("I168"): return "Magical Bottle - Water";
    case FourCC("I166"): return "Magical Bottle - Water";
    case FourCC("I158"): return "Magical Bottle - Wisdom";
    case FourCC("I159"): return "Magical Bottle - Wisdom";
    case FourCC("I160"): return "Magical Bottle - Wisdom";
    case FourCC("I09G"): return "Manta Style";
    case FourCC("I02E"): return "Manta Style";
    case FourCC("I09F"): return "Manta Style";
    case FourCC("I0MV"): return "Manta Style (ranged)";
    case FourCC("I0MW"): return "Manta Style (ranged)";
    case FourCC("I0MU"): return "Manta Style (ranged)";
    case FourCC("I079"): return "Manta Style Recipe";
    case FourCC("I07Y"): return "Manta Style Recipe";
    case FourCC("I0ED"): return "Manta Style Recipe";
    case FourCC("I03F"): return "Mantle of Intelligence";
    case FourCC("I04J"): return "Mantle of Intelligence";
    case FourCC("I0CM"): return "Mantle of Intelligence";
    case FourCC("I03G"): return "Mask of Death";
    case FourCC("I04K"): return "Mask of Death";
    case FourCC("I0CD"): return "Mask of Death";
    case FourCC("I08X"): return "Mask of Madness";
    case FourCC("I08Y"): return "Mask of Madness";
    case FourCC("I025"): return "Mask of Madness";
    case FourCC("I06Z"): return "Mask of Madness Recipe";
    case FourCC("I070"): return "Mask of Madness Recipe";
    case FourCC("I0E6"): return "Mask of Madness Recipe";
    case FourCC("I0N0"): return "Medallion of Courage";
    case FourCC("I0N1"): return "Medallion of Courage";
    case FourCC("I0N2"): return "Medallion of Courage";
    case FourCC("I0NH"): return "Medallion of Courage Recipe";
    case FourCC("I0NI"): return "Medallion of Courage Recipe";
    case FourCC("I0NJ"): return "Medallion of Courage Recipe";
    case FourCC("I0U0"): return "Meepo Item placeholder";
    case FourCC("I093"): return "Mekansm";
    case FourCC("I094"): return "Mekansm";
    case FourCC("I028"): return "Mekansm";
    case FourCC("I073"): return "Mekansm Recipe";
    case FourCC("I074"): return "Mekansm Recipe";
    case FourCC("I0E8"): return "Mekansm Recipe";
    case FourCC("I03H"): return "Messerschmidt's Reaver";
    case FourCC("I04L"): return "Messerschmidt's Reaver";
    case FourCC("I0CG"): return "Messerschmidt's Reaver";
    case FourCC("I0XI"): return "Meteor Hammer";
    case FourCC("I0XJ"): return "Meteor Hammer";
    case FourCC("I0XK"): return "Meteor Hammer";
    case FourCC("I11X"): return "Meteor Hammer Recipe";
    case FourCC("I11Y"): return "Meteor Hammer Recipe";
    case FourCC("I11Z"): return "Meteor Hammer Recipe";
    case FourCC("I03I"): return "Mithril Hammer";
    case FourCC("I04M"): return "Mithril Hammer";
    case FourCC("I0D0"): return "Mithril Hammer";
    case FourCC("I0BE"): return "Mjollnir";
    case FourCC("I0BF"): return "Mjollnir";
    case FourCC("I0C0"): return "Mjollnir";
    case FourCC("I0KO"): return "Mjollnir";
    case FourCC("I07L"): return "Mjollnir Recipe";
    case FourCC("I0KN"): return "Mjollnir Recipe";
    case FourCC("I0A5"): return "Monkey King Bar";
    case FourCC("I0A6"): return "Monkey King Bar";
    case FourCC("I0K8"): return "Monkey King Bar";
    case FourCC("I0K9"): return "Monkey King Bar";
    case FourCC("I0BR"): return "Monkey King Bar";
    case FourCC("I0K7"): return "Monkey King Bar old";
    case FourCC("I133"): return "Monkey King Bar Recipe";
    case FourCC("I134"): return "Monkey King Bar Recipe";
    case FourCC("I135"): return "Monkey King Bar Recipe";
    case FourCC("I0SI"): return "Moon Shard";
    case FourCC("I0SJ"): return "Moon Shard";
    case FourCC("I0SK"): return "Moon Shard";
    case FourCC("I00E"): return "Morphling Bug Fix";
    case FourCC("I03J"): return "Mystic Staff";
    case FourCC("I04N"): return "Mystic Staff";
    case FourCC("I0CN"): return "Mystic Staff";
    case FourCC("I065"): return "Nathrezim Buckler";
    case FourCC("I066"): return "Nathrezim Buckler";
    case FourCC("I01L"): return "Nathrezim Buckler";
    case FourCC("I05M"): return "Nathrezim Buckler Recipe";
    case FourCC("I05S"): return "Nathrezim Buckler Recipe";
    case FourCC("I0DT"): return "Nathrezim Buckler Recipe";
    case FourCC("I09U"): return "Necronomicon Level 1";
    case FourCC("I09T"): return "Necronomicon Level 1";
    case FourCC("I02H"): return "Necronomicon Level 1";
    case FourCC("I09V"): return "Necronomicon Level 2";
    case FourCC("I09X"): return "Necronomicon Level 2";
    case FourCC("I02I"): return "Necronomicon Level 2";
    case FourCC("I09W"): return "Necronomicon Level 3";
    case FourCC("I09Y"): return "Necronomicon Level 3";
    case FourCC("I02J"): return "Necronomicon Level 3";
    case FourCC("I07C"): return "Necronomicon Recipe";
    case FourCC("I081"): return "Necronomicon Recipe";
    case FourCC("I0EG"): return "Necronomicon Recipe";
    case FourCC("I06L"): return "Null Talisman";
    case FourCC("I06M"): return "Null Talisman";
    case FourCC("I01X"): return "Null Talisman";
    case FourCC("I05Q"): return "Null Talisman Recipe";
    case FourCC("I05X"): return "Null Talisman Recipe";
    case FourCC("I0DZ"): return "Null Talisman Recipe";
    case FourCC("I0XF"): return "Nullifier";
    case FourCC("I0XG"): return "Nullifier";
    case FourCC("I0XH"): return "Nullifier";
    case FourCC("I06F"): return "Oblivion Staff";
    case FourCC("I06G"): return "Oblivion Staff";
    case FourCC("I01U"): return "Oblivion Staff";
    case FourCC("I058"): return "Observer Ward";
    case FourCC("I05G"): return "Observer Ward";
    case FourCC("I0SX"): return "Observer Wards (active)";
    case FourCC("I0SU"): return "Octarine Core";
    case FourCC("I0SV"): return "Octarine Core";
    case FourCC("I0SW"): return "Octarine Core";
    case FourCC("I03K"): return "Ogre Axe";
    case FourCC("I04O"): return "Ogre Axe";
    case FourCC("I0D8"): return "Ogre Axe";
    case FourCC("I13X"): return "Orb of Corrosion";
    case FourCC("I13Y"): return "Orb of Corrosion";
    case FourCC("I13Z"): return "Orb of Corrosion";
    case FourCC("I13U"): return "Orb of Corrosion Recipe";
    case FourCC("I13V"): return "Orb of Corrosion Recipe";
    case FourCC("I13W"): return "Orb of Corrosion Recipe";
    case FourCC("I0M9"): return "Orb of Venom";
    case FourCC("I0MA"): return "Orb of Venom";
    case FourCC("I0MB"): return "Orb of Venom";
    case FourCC("I012"): return "Orchid Malevolence";
    case FourCC("I013"): return "Orchid Malevolence";
    case FourCC("I0CB"): return "Orchid Malevolence";
    case FourCC("I0O9"): return "Orchid Malevolence Recipe";
    case FourCC("I0OA"): return "Orchid Malevolence Recipe";
    case FourCC("I0OB"): return "Orchid Malevolence Recipe";
    case FourCC("I12R"): return "Overwhelming Blink";
    case FourCC("I12S"): return "Overwhelming Blink";
    case FourCC("I12T"): return "Overwhelming Blink";
    case FourCC("I12O"): return "Overwhelming Blink Recipe";
    case FourCC("I12P"): return "Overwhelming Blink Recipe";
    case FourCC("I12Q"): return "Overwhelming Blink Recipe";
    case FourCC("I218"): return "Parasma";
    case FourCC("I219"): return "Parasma";
    case FourCC("I220"): return "Parasma";
    case FourCC("I215"): return "Parasma Recipe";
    case FourCC("I216"): return "Parasma Recipe";
    case FourCC("I217"): return "Parasma Recipe";
    case FourCC("I061"): return "Perseverance";
    case FourCC("I062"): return "Perseverance";
    case FourCC("I01J"): return "Perseverance";
    case FourCC("I0GJ"): return "Phase Boots";
    case FourCC("I0GK"): return "Phase Boots";
    case FourCC("I0GL"): return "Phase Boots";
    case FourCC("I157"): return "Phylactery";
    case FourCC("I161"): return "Phylactery";
    case FourCC("I162"): return "Phylactery";
    case FourCC("I198"): return "Phylactery Recipe";
    case FourCC("I199"): return "Phylactery Recipe";
    case FourCC("I156"): return "Phylactery Recipe";
    case FourCC("I0HX"): return "Pipe of Insight";
    case FourCC("I0HY"): return "Pipe of Insight";
    case FourCC("I0I0"): return "Pipe of Insight";
    case FourCC("I0I2"): return "Pipe of Insight";
    case FourCC("I0K6"): return "Pipe of Insight";
    case FourCC("I0HZ"): return "Pipe of Insight Recipe";
    case FourCC("I03M"): return "Plate Mail";
    case FourCC("I04Q"): return "Plate Mail";
    case FourCC("I0DB"): return "Plate Mail";
    case FourCC("I03N"): return "Point Booster";
    case FourCC("I04R"): return "Point Booster";
    case FourCC("I0CL"): return "Point Booster";
    case FourCC("I0KD"): return "Poor Man's Shield";
    case FourCC("I0JF"): return "Poor Man's Shield (melee)";
    case FourCC("I0JG"): return "Poor Man's Shield (melee)";
    case FourCC("I0JH"): return "Poor Man's Shield (melee)";
    case FourCC("I0KF"): return "Poor Man's Shield (ranged)";
    case FourCC("I0KE"): return "Poor Man's Shield (ranged)";
    case FourCC("I03O"): return "Power Treads (Agility)";
    case FourCC("I02T"): return "Power Treads (Agility)";
    case FourCC("I01R"): return "Power Treads (Agility)";
    case FourCC("I05Z"): return "Power Treads (Intelligence)";
    case FourCC("I060"): return "Power Treads (Intelligence)";
    case FourCC("I01S"): return "Power Treads (Intelligence)";
    case FourCC("I02U"): return "Power Treads (Strength)";
    case FourCC("I05Y"): return "Power Treads (Strength)";
    case FourCC("I01Q"): return "Power Treads (Strength)";
    case FourCC("I03P"): return "Quarterstaff";
    case FourCC("I04S"): return "Quarterstaff";
    case FourCC("I0CY"): return "Quarterstaff";
    case FourCC("I0HR"): return "Quelling Blade";
    case FourCC("I0HT"): return "Quelling Blade";
    case FourCC("I0HV"): return "Quelling Blade";
    case FourCC("I0HW"): return "Quelling Blade Ranged";
    case FourCC("I0A7"): return "Radiance";
    case FourCC("I0KQ"): return "Radiance";
    case FourCC("I0KR"): return "Radiance";
    case FourCC("I0BS"): return "Radiance";
    case FourCC("I0A8"): return "Radiance (On)";
    case FourCC("I0KP"): return "Radiance (Off)";
    case FourCC("I07G"): return "Radiance Recipe";
    case FourCC("I07F"): return "Radiance Recipe";
    case FourCC("I0EJ"): return "Radiance Recipe";
    case FourCC("I0AJ"): return "Refresher Orb";
    case FourCC("I0AK"): return "Refresher Orb";
    case FourCC("I0BX"): return "Refresher Orb";
    case FourCC("I07J"): return "Refresher Orb Recipe";
    case FourCC("I088"): return "Refresher Orb Recipe";
    case FourCC("I0EO"): return "Refresher Orb Recipe";
    case FourCC("I0XR"): return "Refresher Shard";
    case FourCC("I0XS"): return "Refresher Shard";
    case FourCC("I0XT"): return "Refresher Shard Shared";
    case FourCC("IR08"): return "Regeneration";
    case FourCC("IRRP"): return "Regeneration";
    case FourCC("I0P0"): return "Ring of Aquila";
    case FourCC("I0P2"): return "Ring of Aquila";
    case FourCC("I0P1"): return "Ring of Aquila (Normal)";
    case FourCC("I0P4"): return "Ring of Aquila (Heroes)";
    case FourCC("I0P3"): return "Ring of Aquila (Heroes)";
    case FourCC("I0P5"): return "Ring of Aquila (Heroes)";
    case FourCC("I067"): return "Ring of Basilius";
    case FourCC("I068"): return "Ring of Basilius";
    case FourCC("I01M"): return "Ring of Basilius";
    case FourCC("I06A"): return "Ring of Basilius (Heroes)";
    case FourCC("I069"): return "Ring of Basilius (Heroes)";
    case FourCC("I0C6"): return "Ring of Basilius (Heroes)";
    case FourCC("I13I"): return "Ring of Basilius Recipe";
    case FourCC("I13J"): return "Ring of Basilius Recipe";
    case FourCC("I13K"): return "Ring of Basilius Recipe";
    case FourCC("I03Q"): return "Ring of Health";
    case FourCC("I04T"): return "Ring of Health";
    case FourCC("I0DI"): return "Ring of Health";
    case FourCC("I03R"): return "Ring of Protection";
    case FourCC("I04U"): return "Ring of Protection";
    case FourCC("I0CW"): return "Ring of Protection";
    case FourCC("I03S"): return "Ring of Regeneration";
    case FourCC("I04V"): return "Ring of Regeneration";
    case FourCC("I0DJ"): return "Ring of Regeneration";
    case FourCC("I03T"): return "Robe of the Magi";
    case FourCC("I04W"): return "Robe of the Magi";
    case FourCC("I0CP"): return "Robe of the Magi";
    case FourCC("I0OL"): return "Rod of Atos";
    case FourCC("I0OM"): return "Rod of Atos";
    case FourCC("I0ON"): return "Rod of Atos";
    case FourCC("I11U"): return "Rod of Atos Recipe";
    case FourCC("I11V"): return "Rod of Atos Recipe";
    case FourCC("I11W"): return "Rod of Atos Recipe";
    case FourCC("I03U"): return "Sacred Relic";
    case FourCC("I04X"): return "Sacred Relic";
    case FourCC("I0CC"): return "Sacred Relic";
    case FourCC("I01Z"): return "Sange";
    case FourCC("I08H"): return "Sange";
    case FourCC("I08I"): return "Sange";
    case FourCC("I0X9"): return "Sange and Kaya";
    case FourCC("I0XA"): return "Sange and Kaya";
    case FourCC("I0XB"): return "Sange and Kaya";
    case FourCC("I096"): return "Sange and Yasha";
    case FourCC("I029"): return "Sange and Yasha";
    case FourCC("I095"): return "Sange and Yasha";
    case FourCC("I06P"): return "Sange Recipe";
    case FourCC("I06Q"): return "Sange Recipe";
    case FourCC("I0E1"): return "Sange Recipe";
    case FourCC("I0AB"): return "Satanic";
    case FourCC("I0AC"): return "Satanic";
    case FourCC("I0BU"): return "Satanic";
    case FourCC("I07O"): return "Satanic Recipe";
    case FourCC("I085"): return "Satanic Recipe";
    case FourCC("I0EL"): return "Satanic Recipe";
    case FourCC("I05A"): return "Scroll of Town Portal";
    case FourCC("I05I"): return "Scroll of Town Portal";
    case FourCC("I059"): return "Sentry Ward";
    case FourCC("I05H"): return "Sentry Ward";
    case FourCC("I0SY"): return "Sentry Wards (active)";
    case FourCC("I0P9"): return "Shadow Amulet";
    case FourCC("I0PA"): return "Shadow Amulet";
    case FourCC("I0PB"): return "Shadow Amulet";
    case FourCC("IR11"): return "Shield Rune";
    case FourCC("IR12"): return "Shield Rune";
    case FourCC("I00X"): return "Shiva's Guard";
    case FourCC("I00Z"): return "Shiva's Guard";
    case FourCC("I0CF"): return "Shiva's Guard";
    case FourCC("I00Y"): return "Shiva's Guard (Courier Edition)";
    case FourCC("I010"): return "Shiva's Guard (Courier Edition)";
    case FourCC("I0DF"): return "Shiva's Guard (Courier)";
    case FourCC("I07T"): return "Shiva's Guard Recipe";
    case FourCC("I08E"): return "Shiva's Guard Recipe";
    case FourCC("I0EU"): return "Shiva's Guard Recipe";
    case FourCC("I0SL"): return "Silver Edge";
    case FourCC("I0SM"): return "Silver Edge";
    case FourCC("I0SN"): return "Silver Edge";
    case FourCC("I0SO"): return "Silver Edge Recipe";
    case FourCC("I0SP"): return "Silver Edge Recipe";
    case FourCC("I0SQ"): return "Silver Edge Recipe";
    case FourCC("I03V"): return "Slippers of Agility";
    case FourCC("I04Y"): return "Slippers of Agility";
    case FourCC("I0C5"): return "Slippers of Agility";
    case FourCC("I0NF"): return "Smoke of Deceit";
    case FourCC("I0NG"): return "Smoke of Deceit";
    case FourCC("I03W"): return "Sobi Mask";
    case FourCC("I04Z"): return "Sobi Mask";
    case FourCC("I0DK"): return "Sobi Mask";
    case FourCC("I0SR"): return "Solar Crest";
    case FourCC("I0SS"): return "Solar Crest";
    case FourCC("I0ST"): return "Solar Crest";
    case FourCC("I14C"): return "Solar Crest Recipe";
    case FourCC("I14D"): return "Solar Crest Recipe";
    case FourCC("I14E"): return "Solar Crest Recipe";
    case FourCC("I091"): return "Soul Booster";
    case FourCC("I092"): return "Soul Booster";
    case FourCC("I027"): return "Soul Booster";
    case FourCC("I0LL"): return "Soul Ring";
    case FourCC("I0LP"): return "Soul Ring";
    case FourCC("I0LQ"): return "Soul Ring";
    case FourCC("I0LM"): return "Soul Ring Recipe";
    case FourCC("I0LN"): return "Soul Ring Recipe";
    case FourCC("I0LO"): return "Soul Ring Recipe";
    case FourCC("I0XL"): return "Spirit Vessel";
    case FourCC("I0XM"): return "Spirit Vessel";
    case FourCC("I0XN"): return "Spirit Vessel";
    case FourCC("I0XO"): return "Spirit Vessel Recipe";
    case FourCC("I0XP"): return "Spirit Vessel Recipe";
    case FourCC("I0XQ"): return "Spirit Vessel Recipe";
    case FourCC("I03X"): return "Staff of Wizardry";
    case FourCC("I050"): return "Staff of Wizardry";
    case FourCC("I0D1"): return "Staff of Wizardry";
    case FourCC("I0KB"): return "Stout Shield";
    case FourCC("I0D9"): return "Stout Shield (melee)";
    case FourCC("I03A"): return "Stout Shield (melee)";
    case FourCC("I051"): return "Stout Shield (melee)";
    case FourCC("I0KA"): return "Stout Shield (ranged)";
    case FourCC("I0KC"): return "Stout Shield (ranged)";
    case FourCC("I12X"): return "Swift Blink";
    case FourCC("I12Y"): return "Swift Blink";
    case FourCC("I12Z"): return "Swift Blink";
    case FourCC("I12U"): return "Swift Blink Recipe";
    case FourCC("I12V"): return "Swift Blink Recipe";
    case FourCC("I12W"): return "Swift Blink Recipe";
    case FourCC("I0J6"): return "Talisman of Evasion";
    case FourCC("I0J7"): return "Talisman of Evasion";
    case FourCC("I0J8"): return "Talisman of Evasion";
    case FourCC("I057"): return "Tango";
    case FourCC("I05C"): return "Tango";
    case FourCC("I0Q8"): return "Tango (muted)";
    case FourCC("I0Q9"): return "Tango (shared)";
    case FourCC("I0Q7"): return "Tango Single";
    case FourCC("I0FF"): return "Temporary Item";
    case FourCC("I0FG"): return "Temporary Item";
    case FourCC("I0AH"): return "The Butterfly";
    case FourCC("I0AI"): return "The Butterfly";
    case FourCC("I0BW"): return "The Butterfly";
    case FourCC("I0VV"): return "Tome of Knowledge";
    case FourCC("I0VW"): return "Tome of Knowledge";
    case FourCC("I0OF"): return "Tranquil Boots";
    case FourCC("I0OG"): return "Tranquil Boots";
    case FourCC("I0OH"): return "Tranquil Boots";
    case FourCC("I0OI"): return "Tranquil Boots (broken)";
    case FourCC("I0OJ"): return "Tranquil Boots Disabled";
    case FourCC("I0OK"): return "Tranquil Boots Disabled";
    case FourCC("I150"): return "Trident";
    case FourCC("I151"): return "Trident";
    case FourCC("I152"): return "Trident";
    case FourCC("I153"): return "Trident Recipe";
    case FourCC("I154"): return "Trident Recipe";
    case FourCC("I155"): return "Trident Recipe";
    case FourCC("I03Y"): return "Ultimate Orb";
    case FourCC("I052"): return "Ultimate Orb";
    case FourCC("I0C9"): return "Ultimate Orb";
    case FourCC("I0KX"): return "Urn of Shadows";
    case FourCC("I0KY"): return "Urn of Shadows";
    case FourCC("I0KZ"): return "Urn of Shadows";
    case FourCC("I0KU"): return "Urn of Shadows Recipe";
    case FourCC("I0KV"): return "Urn of Shadows Recipe";
    case FourCC("I0KW"): return "Urn of Shadows Recipe";
    case FourCC("I0BA"): return "Vanguard";
    case FourCC("I0BB"): return "Vanguard";
    case FourCC("I0BZ"): return "Vanguard";
    case FourCC("I0LC"): return "Vanguard (ranged) unused";
    case FourCC("I0LD"): return "Vanguard unused";
    case FourCC("I0LE"): return "Vanguard unused";
    case FourCC("I0O2"): return "Veil of Discord";
    case FourCC("I0O3"): return "Veil of Discord";
    case FourCC("I0O4"): return "Veil of Discord";
    case FourCC("I0O5"): return "Veil of Discord Recipe";
    case FourCC("I0O6"): return "Veil of Discord Recipe";
    case FourCC("I0O7"): return "Veil of Discord Recipe";
    case FourCC("I03Z"): return "Vitality Booster";
    case FourCC("I053"): return "Vitality Booster";
    case FourCC("I0CK"): return "Vitality Booster";
    case FourCC("I0BG"): return "Vladmir's Offering";
    case FourCC("I0BH"): return "Vladmir's Offering";
    case FourCC("I0C1"): return "Vladmir's Offering";
    case FourCC("I07Q"): return "Vladmir's Offering Recipe";
    case FourCC("I08B"): return "Vladmir's Offering Recipe";
    case FourCC("I0ER"): return "Vladmir's Offering Recipe";
    case FourCC("I040"): return "Void Stone";
    case FourCC("I054"): return "Void Stone";
    case FourCC("I0DL"): return "Void Stone";
    case FourCC("I13O"): return "Voodoo Mask";
    case FourCC("I13P"): return "Voodoo Mask";
    case FourCC("I13Q"): return "Voodoo Mask";
    case FourCC("IR13"): return "Water";
    case FourCC("IR14"): return "Water";
    case FourCC("I0VS"): return "Wind Lace";
    case FourCC("I0VT"): return "Wind Lace";
    case FourCC("I0VU"): return "Wind Lace";
    case FourCC("IR09"): return "Wisdom";
    case FourCC("IR10"): return "Wisdom";
    case FourCC("I139"): return "Witch Blade";
    case FourCC("I13A"): return "Witch Blade";
    case FourCC("I13B"): return "Witch Blade";
    case FourCC("I136"): return "Witch Blade Recipe";
    case FourCC("I137"): return "Witch Blade Recipe";
    case FourCC("I138"): return "Witch Blade Recipe";
    case FourCC("I06J"): return "Wraith Band";
    case FourCC("I06K"): return "Wraith Band";
    case FourCC("I01W"): return "Wraith Band";
    case FourCC("I05N"): return "Wraith Band Recipe";
    case FourCC("I05W"): return "Wraith Band Recipe";
    case FourCC("I0DY"): return "Wraith Band Recipe";
    case FourCC("I01Y"): return "Yasha";
    case FourCC("I08F"): return "Yasha";
    case FourCC("I08G"): return "Yasha";
    case FourCC("I0XC"): return "Yasha and Kaya";
    case FourCC("I0XD"): return "Yasha and Kaya";
    case FourCC("I0XE"): return "Yasha and Kaya";
    case FourCC("I06N"): return "Yasha Recipe";
    case FourCC("I06O"): return "Yasha Recipe";
    case FourCC("I0E0"): return "Yasha Recipe";
    default: return "item " + FourCCToString(code);
  }
}

string Dota::GetHeroName(const uint32_t code)
{
  switch (code) {
    case 0: return "(no hero)";
    case FourCC("H06S"): return "Admiral (Daelin Proudmoore)";
    case FourCC("N01I"): return "Alchemist (Razzil Darkbrew)";
    case FourCC("N01H"): return "Alchemist (Razzil Darkbrew)";
    case FourCC("N01J"): return "Alchemist (Razzil Darkbrew)";
    case FourCC("N01T"): return "Alchemist (Razzil Darkbrew)";
    case FourCC("N0HT"): return "Alchemist (Razzil Darkbrew)";
    case FourCC("N0HP"): return "Ancient Apparition (Kaldr)";
    case FourCC("Edem"): return "Anti-Mage (Magina)";
    case FourCC("N0MK"): return "Arc Warden (Zet)";
    case FourCC("N0MM"): return "Arc Warden (Zet)";
    case FourCC("H0DL"): return "Assimilate helper (Assimilate helper)";
    case FourCC("Opgh"): return "Axe (Mogul Kahn)";
    case FourCC("Oshd"): return "Bane Elemental (Atropos)";
    case FourCC("O016"): return "Batrider (Jin'zakk)";
    case FourCC("O017"): return "Batrider (Jin'zakk)";
    case FourCC("H00D"): return "Beastmaster (Rexxar)";
    case FourCC("Hvsh"): return "Bloodseeker (Strygwyr)";
    case FourCC("E004"): return "Bone Fletcher (Clinkz)";
    case FourCC("Naka"): return "Bounty Hunter (Gondar)";
    case FourCC("H008"): return "Bristleback (Rigwarl)";
    case FourCC("U006"): return "Broodmother (Black Arachnia)";
    case FourCC("U007"): return "Broodmother (Black Arachnia)";
    case FourCC("U106"): return "Broodmother Spider Control (Spider Control Center)";
    case FourCC("U00F"): return "Butcher (Pudge)";
    case FourCC("H000"): return "Centaur Warchief (Bradwarden)";
    case FourCC("U00A"): return "Chaos Knight (Nessaj)";
    case FourCC("H00T"): return "Clockwerk Goblin (Rattletrap)";
    case FourCC("Hjai"): return "Crystal Maiden (Rylai Crestfall)";
    case FourCC("H00N"): return "Dark Seer (Ish'kafel)";
    case FourCC("UC76"): return "Death Prophet (Krobelus)";
    case FourCC("UC18"): return "Demon Witch (Lion)";
    case FourCC("E02J"): return "Disruptor (Thrall)";
    case FourCC("UC42"): return "Doom Bringer (Lucifer)";
    case FourCC("H00F"): return "Dragon Knight (Arc Honist)";
    case FourCC("H00E"): return "Dragon Knight (Arc Honist)";
    case FourCC("H00G"): return "Dragon Knight (Arc Honist)";
    case FourCC("H00L"): return "Dragon Knight (Arc Honist)";
    case FourCC("Hlgr"): return "Dragon Knight (Knight Davion)";
    case FourCC("Nbrn"): return "Drow Ranger (Traxex)";
    case FourCC("Usyl"): return "Dwarven Sniper (Kardel Sharpeye)";
    case FourCC("N0MU"): return "Earth Spirit (Kaolin)";
    case FourCC("N0MW"): return "Earth Spirit (Kaolin)";
    case FourCC("Otch"): return "Earthshaker (Raigor Stonehoof)";
    case FourCC("N0M0"): return "Ember Spirit (Xin)";
    case FourCC("N0MH"): return "Ember Spirit (Xin)";
    case FourCC("Emoo"): return "Enchantress (Aiushtha)";
    case FourCC("Uktl"): return "Enigma (Darchrow)";
    case FourCC("EC45"): return "Faceless Void (Gorzerk)";
    case FourCC("N00B"): return "Faerie Dragon (Puck)";
    case FourCC("H07I"): return "Flesh Golem (Dirge)";
    case FourCC("H00I"): return "Geomancer (Meepo)";
    case FourCC("H00J"): return "Geomancer (Meepo)";
    case FourCC("E032"): return "Goblin Shredder (Rizzrak)";
    case FourCC("H00K"): return "Goblin Techies (Squee Spleen and Spoon)";
    case FourCC("H00V"): return "Gorgon (Medusa)";
    case FourCC("H084"): return "Gorgon (Medusa)";
    case FourCC("H08B"): return "Gorgon (Medusa)";
    case FourCC("H08C"): return "Gorgon (Medusa)";
    case FourCC("H08D"): return "Gorgon (Medusa)";
    case FourCC("E02X"): return "Grand Magus (Rubick)";
    case FourCC("QTW7"): return "Grand Magus (Rubick)";
    case FourCC("QTW8"): return "Grand Magus (Rubick)";
    case FourCC("QTW9"): return "Grand Magus (Rubick)";
    case FourCC("QTWA"): return "Grand Magus (Rubick)";
    case FourCC("QTWB"): return "Grand Magus (Rubick)";
    case FourCC("QTWG"): return "Grand Magus (Rubick)";
    case FourCC("QTWC"): return "Grand Magus (Rubick)";
    case FourCC("QTWD"): return "Grand Magus (Rubick)";
    case FourCC("QTWE"): return "Grand Magus (Rubick)";
    case FourCC("HGRM"): return "Grimstroke (Grimstroke)";
    case FourCC("O01F"): return "Guardian Wisp (Io)";
    case FourCC("E02N"): return "Gyrocopter (Aurel Vlaicu)";
    case FourCC("E02O"): return "Gyrocopter (Aurel Vlaicu)";
    case FourCC("H00A"): return "Holy Knight (Jackie Chen)";
    case FourCC("H1R0"): return "Hoodwink (Raccoon)";
    case FourCC("H0B8"): return "Ice Blast (Ice Blast)";
    case FourCC("H00U"): return "Invoker (Kael)";
    case FourCC("Nbbc"): return "Juggernaut (Yurnero)";
    case FourCC("Hblm"): return "Keeper of the Light (Ezalor)";
    case FourCC("H06X"): return "Keeper of the Light (Ezalor)";
    case FourCC("H0BC"): return "Keeper of the Light (Ezalor)";
    case FourCC("H06W"): return "Keeper of the Light (Ezalor)";
    case FourCC("H06Y"): return "Keeper of the Light (Ezalor)";
    case FourCC("E02K"): return "Legion Commander (Tresdin)";
    case FourCC("Ulic"): return "Lich (Kel'Thuzad)";
    case FourCC("U00C"): return "Lifestealer (Naix)";
    case FourCC("E002"): return "Lightning Revenant (Razor)";
    case FourCC("N01O"): return "Lone Druid (Syllabear)";
    case FourCC("N014"): return "Lone Druid (Syllabear)";
    case FourCC("N015"): return "Lone Druid (Syllabear)";
    case FourCC("N013"): return "Lone Druid Bear Form (Syllabear)";
    case FourCC("Udea"): return "Lord of Avernus (Abaddon)";
    case FourCC("Hmbr"): return "Lord of Olympus (Merlini|r)";
    case FourCC("U008"): return "Lycanthrope (Banehallow)";
    case FourCC("E015"): return "Lycanthrope (Banehallow)";
    case FourCC("UC11"): return "Magnataur (Magnus)";
    case FourCC("HMRS"): return "Mars (God of War)";
    case FourCC("H0MK"): return "Monkey King (Sun Wukong)";
    case FourCC("E005"): return "Moon Rider (Luna Moonfang)";
    case FourCC("O00P"): return "Morphling (Morphling)";
    case FourCC("H071"): return "Murloc Nightcrawler (Slark)";
    case FourCC("H0ER"): return "Murloc Nightcrawler (Slark)";
    case FourCC("HC49"): return "Naga Siren (Slithice)";
    case FourCC("UC60"): return "Necro'lic (Visage)";
    case FourCC("U00E"): return "Necrolyte (Rotund'jere)";
    case FourCC("U0A9"): return "Nerubian Assassin (Anub'arak)";
    case FourCC("U000"): return "Nerubian Assassin (Anub'arak)";
    case FourCC("Ubal"): return "Nerubian Weaver (Anub'seran)";
    case FourCC("EC77"): return "Netherdrake (Viper)";
    case FourCC("Udre"): return "Night Stalker (Balanar)";
    case FourCC("H00H"): return "Oblivion (Pugna)";
    case FourCC("U00P"): return "Obsidian Destroyer (Harbinger)";
    case FourCC("Hmkg"): return "Ogre Magi (Aggron Stonebreaker)";
    case FourCC("Harf"): return "Omniknight (Purist Thunderwrath)";
    case FourCC("N0MD"): return "Oracle (Nerif)";
    case FourCC("Npbm"): return "Pandaren Brewmaster (Mangix)";
    case FourCC("Ewar"): return "Phantom Assassin (Mortred)";
    case FourCC("Ogrh"): return "Phantom Lancer (Azwraith)";
    case FourCC("E02F"): return "Phoenix (Icarus)";
    case FourCC("N00R"): return "Pit Lord (Azgalor)";
    case FourCC("N01V"): return "Priestess of the Moon (Mirana Nightshade)";
    case FourCC("Emns"): return "Prophet (Furion)";
    case FourCC("HDUM"): return "Puppet (Puppet)";
    case FourCC("UC01"): return "Queen of Pain (Akasha)";
    case FourCC("H001"): return "Rogue Knight (Sven)";
    case FourCC("H0CQ"): return "Rogue Knight Morphed (Sven)";
    case FourCC("QTWF"): return "Rubick Bear Form (Rubick)";
    case FourCC("H00Q"): return "Sacred Warrior (Huskar)";
    case FourCC("U00K"): return "Sand King (Crixalis)";
    case FourCC("E02H"): return "Shadow Demon (Eredar)";
    case FourCC("Nfir"): return "Shadow Fiend (Nevermore)";
    case FourCC("N01W"): return "Shadow Priest (Dazzle)";
    case FourCC("Orkn"): return "Shadow Shaman (Rhasta)";
    case FourCC("N01A"): return "Silencer (Nortrom)";
    case FourCC("NC00"): return "Skeleton King (King Leoric)";
    case FourCC("H0DO"): return "Skywrath Mage (Dragonus)";
    case FourCC("H004"): return "Slayer (Lina Inverse)";
    case FourCC("UC91"): return "Slithereen Guard (Slardar)";
    case FourCC("Eevi"): return "Soul Keeper (Terrorblade)";
    case FourCC("Eevm"): return "Soul Keeper (Terrorblade)";
    case FourCC("E02U"): return "Soul Keeper (Terrorblade)";
    case FourCC("E02V"): return "Soul Keeper (Terrorblade)";
    case FourCC("E02W"): return "Soul Keeper (Terrorblade)";
    case FourCC("E01B"): return "Spectre (Mercurial)";
    case FourCC("QTW4"): return "Spectre (Mercurial)";
    case FourCC("H0DM"): return "Spirit Bear (Spirit Bear)";
    case FourCC("O00J"): return "Spiritbreaker (Barathrum)";
    case FourCC("HC92"): return "Stealth Assassin (Rikimaru)";
    case FourCC("Ucrl"): return "Stone Giant (Tiny)";
    case FourCC("U01X"): return "Stone Giant (Tiny)";
    case FourCC("H00S"): return "Storm Spirit (Raijin Thunderkeg)";
    case FourCC("H07G"): return "Storm Spirit (Raijin Thunderkeg)";
    case FourCC("H0ES"): return "Sun Wukong (Sun Wukong)";
    case FourCC("O015"): return "Tauren Chieftain (Taur Thunderhorn)";
    case FourCC("E01Y"): return "Templar Assassin (Lanaya)";
    case FourCC("Ofar"): return "Tidehunter (Leviathan)";
    case FourCC("Ntin"): return "Tinker (Boush)";
    case FourCC("Ekee"): return "Tormented Soul (Leshrac the Malicious)";
    case FourCC("Hamg"): return "Treant Protector (Rooftrellen)";
    case FourCC("N017"): return "Troll Warlord (Jah'rakal)";
    case FourCC("QTW2"): return "Troll Warlord (Jah'rakal)";
    case FourCC("QTW3"): return "Troll Warlord (Jah'rakal)";
    case FourCC("N02B"): return "Troll Warlord (Jah'rakal)";
    case FourCC("N016"): return "Troll Warlord (Jah'rakal)";
    case FourCC("E02I"): return "Tuskarr (Ymir)";
    case FourCC("E00P"): return "Twin Head Dragon (Jakiro)";
    case FourCC("H00R"): return "Undying (Dirge)";
    case FourCC("Huth"): return "Ursa Warrior (Ulfsaar)";
    case FourCC("QTW5"): return "Ursa Warrior (Ulfsaar)";
    case FourCC("Hvwd"): return "Vengeful Spirit (Shendelzare Silkwood)";
    case FourCC("EC57"): return "Venomancer (Lesale Deathbringer)";
    case FourCC("E01C"): return "Warlock (Demnok Lannik)";
    case FourCC("N0EG"): return "Windrunner (Alleria)";
    case FourCC("QTW6"): return "Windrunner (Alleria)";
    case FourCC("N0M7"): return "Winter Wyvern (Auroth)";
    case FourCC("N0MA"): return "Winter Wyvern (Auroth)";
    case FourCC("N0MB"): return "Winter Wyvern (Auroth)";
    case FourCC("N0MC"): return "Winter Wyvern (Auroth)";
    case FourCC("N0MO"): return "Winter Wyvern (Auroth)";
    case FourCC("E01A"): return "Witch Doctor (Vol'Jin)";
    default: return "Hero " + FourCCToString(code);
  }
}

//
// CDotaStats
//

CDotaStats::CDotaStats(shared_ptr<CGame> nGame)
  : m_Game(ref(*nGame)),
    m_Winner(Dota::WINNER_UNDECIDED),
    m_SwitchEnabled(false),
    m_Time(make_pair<uint32_t, uint32_t>(0u, 0u))
    
{
  Print("[STATS] using dota stats");
  m_Players.fill(nullptr);

  for (unsigned int i = 0; i < m_Players.size(); ++i) {
    if (GetIsHeroColor(i)) {
      m_Players[i] = new CDBDotAPlayer();
    }
  }
}

CDotaStats::~CDotaStats()
{
  for (auto& dotaPlayer : m_Players) {
    if (dotaPlayer) {
      delete dotaPlayer;
    }
  }
}

bool CDotaStats::EventGameCacheInteger(const uint8_t /*fromUID*/, const std::string& fileName, const std::string& missionKey, const std::string& key, const uint32_t cacheValue)
{
  if (fileName != "dr.x") {
    return true;
  }

  if (missionKey == "Data") {
    // these are received during the game
    // you could use these to calculate killing sprees and double or triple kills (you'd have to make up your own time restrictions though)
    // you could also build a table of "who killed who" data

    string eventName = key;
    string eventStringData;

    if (key.size() >= 4 && key.compare(0, 4, "Mode") == 0) {
      eventStringData = key.substr(4);
      eventName = "Mode";
    } else {
      string::size_type keyNumIndex = key.find_first_of("1234567890_-+");
      if (keyNumIndex != string::npos) {
        eventStringData = key.substr(keyNumIndex);
        eventName = key.substr(0, keyNumIndex);
      }
    }

    uint64_t eventHash = HashCode(eventName);

    switch (eventHash) {
      case HashCode("Hero"): {
        // a hero died
        optional<uint8_t> killerColor = EnsureActorColor(cacheValue);
        optional<uint8_t> victimColor = ParseHeroColor(eventStringData);
        if (!killerColor.has_value() || !victimColor.has_value()) {
          break;
        }

        GameUser::CGameUser* killerUser = GetUserFromColor(*killerColor);
        GameUser::CGameUser* victimUser = GetUserFromColor(*victimColor);
        bool isFriendlyFire = GetAreSameTeamColors(*killerColor, *victimColor);

        // only count kills from non-leavers
        if (killerUser) m_Players[*killerColor]->IncKills();
        m_Players[*victimColor]->IncDeaths();

        string action = "killed";
        if (isFriendlyFire) {
          action = "denied";
        }
        if (victimUser) {
          action.append(" player");
        } else {
          action.append(" leaver");
        }

        Print(GetLogPrefix() + GetActorNameFromColor(*killerColor) + " " + action + " [" + victimUser->GetName() + "]");
        break;
      }

      case HashCode("Assist"): {
        optional<uint8_t> assisterColor = ParseHeroColor(eventStringData);
        optional<uint8_t> victimColor = EnsureHeroColor(cacheValue);
        if (assisterColor.has_value() && victimColor.has_value()) {
          GameUser::CGameUser* assisterUser = GetUserFromColor(*assisterColor);
          if (assisterUser) { // only count assists from non-leavers
            m_Players[*assisterColor]->IncAssists();
            Print(GetLogPrefix() + GetActorNameFromColor(*assisterColor) + " assisted on killing [" + GetUserNameFromColor(*victimColor) + "]");
          }
        }
        break;
      }

      case HashCode("Tower"): {
        optional<uint8_t> siegeColor = EnsureActorColor(cacheValue);
        vector<uint8_t> towerId = SplitNumeral(eventStringData);
        if (siegeColor.has_value() && towerId.size() == 3) {
          GameUser::CGameUser* siegeUser = GetUserFromColor(*siegeColor);
          if (siegeUser) { // only count tower destroyed by non-leavers
            m_Players[cacheValue]->IncTowerKills();
          }

          bool isFriendlyFire = (towerId[0] == 0) == GetIsSentinelHeroColor(*siegeColor);
          string action = "destroyed";
          if (isFriendlyFire) {
            action = "denied";
          }

          Print(GetLogPrefix() + GetActorNameFromColor(cacheValue) + " " + action + " " + GetTeamNameBaseZero(towerId[0]) + "'s " + ToOrdinalName((size_t)(towerId[1])) + " tower at " + GetLaneName(towerId[2]));
        }

        break;
      }

      case HashCode("Rax"): {
        optional<uint8_t> siegeColor = EnsureActorColor(cacheValue);
        vector<uint8_t> raxId = SplitNumeral(eventStringData);
        if (siegeColor.has_value() && raxId.size() == 3) {
          GameUser::CGameUser* siegeUser = GetUserFromColor(*siegeColor);
          if (siegeUser) { // only count tower destroyed by non-leavers
            m_Players[cacheValue]->IncRaxKills();
          }

          bool isFriendlyFire = (raxId[0] == 0) == GetIsSentinelHeroColor(*siegeColor);
          string action = "destroyed";
          if (isFriendlyFire) {
            action = "denied";
          }

          string typeName = "melee";
          if (raxId[2] == 1) {
            typeName = "ranged";
          }

          Print(GetLogPrefix() + GetActorNameFromColor(cacheValue) + " " + action + " " + GetTeamNameBaseZero(raxId[0]) + "'s " + typeName + " Barracks at " + GetLaneName(raxId[1]));
        }
        break;
      }

      case HashCode("Courier"): {
        optional<uint8_t> killerColor = EnsureActorColor(cacheValue);
        optional<uint8_t> victimColor = ParseHeroColor(eventStringData);
        if (killerColor.has_value() && victimColor.has_value()) {
          GameUser::CGameUser* killerUser = GetUserFromColor(*killerColor);
          if (killerUser) { // only count couriers killed by non-leavers
            m_Players[cacheValue]->IncCourierKills();
          }

          bool isFriendlyFire = false;
          string action = "killed";
          if (isFriendlyFire) {
            action = "denied";
          }

          Print(GetLogPrefix() + GetActorNameFromColor(cacheValue) + " " + action + " [" + GetUserNameFromColor(*victimColor) + "]'s courier");
        }
        break;
      }

      case HashCode("Throne"): {
        if (!eventStringData.empty()) break;
        // the frozen throne got hurt
        Print(GetLogPrefix() + "the Frozen Throne is now at " + to_string(cacheValue) + "% HP");
        break;
      }

      case HashCode("Tree"): {
        if (!eventStringData.empty()) break;
        // the world tree got hurt
        Print(GetLogPrefix() + "the World Tree Tree is now at " + to_string(cacheValue) + "% HP");
        break;
      }

      case HashCode("CSS"): {
        // incremental creeping performance stats sent every 5 minutes
        // supersedes CSK, NK, CSD
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          uint32_t creepData = cacheValue;
          uint32_t creepDenies = creepData & 0xFF; // 8 bits
          creepData >>= 8;
          uint32_t neutralKills = creepData & 0xFFF; // 12 bits
          creepData >>= 12;
          uint32_t creepKills = creepData; // 12 bits

          m_Players[*heroColor]->AddCreepKills(creepKills);
          m_Players[*heroColor]->AddCreepDenies(creepDenies);
          m_Players[*heroColor]->AddNeutralKills(neutralKills);

          //string playerName = GetUserNameFromColor(*heroColor);
          //Print(GetLogPrefix() + "[" + playerName + "] " + to_string(creepKills) + " creeps killed, " + to_string(creepDenies) + " denied, " + to_string(neutralKills) + "neutrals");
        }
        break;
      }

      case HashCode("CK"): {
        // a player disconnected - the map sends a final creep performance stats update
        if (eventStringData.size() < 5) break; 
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        string::size_type denyIndex = eventStringData.find('D');
        if (denyIndex == string::npos) return true;
        string::size_type neutralIndex = eventStringData.find('N', denyIndex + 1);
        if (neutralIndex == string::npos) return true;

        string creepKillsString = eventStringData.substr(0, denyIndex);
        string creepDeniesString = eventStringData.substr(denyIndex + 1, neutralIndex - (denyIndex + 1));
        string neutralKillsString = eventStringData.substr(neutralIndex + 1);

        optional<uint32_t> creepKills = ToUint32(creepKillsString);
        optional<uint32_t> creepDenies = ToUint32(creepDeniesString);
        optional<uint32_t> neutralKills = ToUint32(neutralKillsString);
        if (!creepKills.has_value() || !creepDenies.has_value() || !neutralKills.has_value()) {
          return true;
        }

        m_Players[*heroColor]->SetCreepKills(*creepKills);
        m_Players[*heroColor]->SetCreepDenies(*creepDenies);
        m_Players[*heroColor]->SetNeutralKills(*neutralKills);

        string playerName = GetUserNameFromColor(*heroColor);
        Print(GetLogPrefix() + "[" + playerName + "] disconnected");
        break;
      }

      case HashCode("XGPM"): {
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          uint32_t xgData = cacheValue;
          uint32_t netWorth = xgData & 0xFFFF; // 16 bits
          xgData >>= 16;
          uint32_t xp = xgData; // 16 bits
          //string playerName = GetUserNameFromColor(*heroColor);
          //Print(GetLogPrefix() + "[" + playerName + "] " + to_string(netWorth) + " gold earned, " + to_string(xp) + " XP");
        }
        break;
      }

      case HashCode("CSK"): {
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          //string playerName = GetUserNameFromColor(*heroColor);
          //Print(GetLogPrefix() + "[" + playerName + "] " + to_string(cacheValue) + " creeps killed");
        }
        break;
      }

      case HashCode("NK"): {
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          //string playerName = GetUserNameFromColor(*heroColor);
          //Print(GetLogPrefix() + "[" + playerName + "] " + to_string(cacheValue) + " neutrals killed");
        }
        break;
      }

      case HashCode("CSD"): {
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          //string playerName = GetUserNameFromColor(*heroColor);
          //Print(GetLogPrefix() + "[" + playerName + "] " + to_string(cacheValue) + " creeps denied");
        }
        break;
      }

      case HashCode("Level"): {
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] advanced to Lv. " + eventStringData);
        }
        break;
      }

      case HashCode("Talent"): {
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          uint32_t tier = cacheValue / 10;
          uint32_t variant = cacheValue % 10;
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] chose option #" + to_string(variant) + " as their " + ToOrdinalName(tier) + " talent");
        }
        break;
      }

      case HashCode("FF"): {
        if (eventStringData.size() != 1) break;
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          if (eventStringData[0] == '+') {
            Print(GetLogPrefix() + "[" + playerName + "] voted for forfeiting the game");
          } else if (eventStringData[0] == '-') {
            Print(GetLogPrefix() + "[" + playerName + "] voted against forfeiting the game");
          }
        }
        break;
      }

      case HashCode("Buyback"): {
        if (eventStringData.empty()) break;
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] revived themselves (" + to_string(cacheValue) + " gold)");
        }
        break;
      }

      case HashCode("RuneUse"): {
        if (eventStringData.empty()) break;
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] used a " + GetRuneName((uint8_t)eventStringData[0]) + " rune");
        }        
        break;
      }

      case HashCode("RuneStore"): {
        if (eventStringData.empty()) break;
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] stored a " + GetRuneName((uint8_t)eventStringData[0]) + " rune");
        }        
        break;
      }

      case HashCode("AfkDetect"): {
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] detected as AFK");
        }
        break;
      }

      case HashCode("MHSuspected"): {
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(cacheValue);
          Print(GetLogPrefix() + "[" + playerName + "] suspected map hacker");
        }
        break;
      }

      case HashCode("GameStart"): {
        if (cacheValue == 1) {
          //m_Game.get().SetCreepSpawnTime(GetTime());
          Print(GetLogPrefix() + "creeps spawned");
        }
        break;
      }

      case HashCode("SWAP"): {
        // swap players
        string::size_type firstUnderscore = eventStringData.find('_');
        if (firstUnderscore == string::npos) return true;
        string::size_type secondUnderscore = eventStringData.find('_', firstUnderscore + 1);
        if (secondUnderscore == string::npos) return true;
        string fromString = eventStringData.substr(firstUnderscore + 1, secondUnderscore - (firstUnderscore + 1));
        string toString = eventStringData.substr(secondUnderscore + 1);
        optional<uint32_t> fromColor = ToUint32(fromString);
        optional<uint32_t> toColor = ToUint32(toString);
        if (!fromColor.has_value() || !toColor.has_value()) return true;
        if (!GetIsHeroColor(*fromColor) || !GetIsHeroColor(*toColor)) return true;
        GameUser::CGameUser* fromPlayer = m_Game.get().GetUserFromColor(*fromColor);
        GameUser::CGameUser* toPlayer = m_Game.get().GetUserFromColor(*toColor);
        
        if (!m_SwitchEnabled && GetIsSentinelHeroColor(*fromColor) != GetIsSentinelHeroColor(*toColor)) {
          m_Players[*toColor]->SetNewColor(*fromColor);
          m_Players[*fromColor]->SetNewColor(*toColor);
          
          CDBDotAPlayer* tmp = m_Players[*toColor];
          m_Players[*toColor] = m_Players[*fromColor];
          m_Players[*fromColor] = tmp;
          
          if (fromPlayer) fromString = fromPlayer->GetName();
          if (toPlayer) toString = toPlayer->GetName();

          Print(GetLogPrefix() + "swap players from [" + fromString + "] to [" + toString + "].");
        } else {
          Print(GetLogPrefix() + "swap players from [" + fromString + "] to [" + toString + "] (switch enabled).");
        }
        break;
      }

      case HashCode("APBan"): {
        Print(GetLogPrefix() + "banned <" + GetHeroName(cacheValue) + ">");
        break;
      }

      case HashCode("Mode"): {
        Print(GetLogPrefix() + "mode -" + eventStringData);
        // Game mode
        string::size_type KeyStringSize = eventStringData.size();
        if (KeyStringSize % 2 != 0) KeyStringSize--;
        for (string::size_type i = 0; i < KeyStringSize; i += 2) {
          if (eventStringData[i] == 's' && eventStringData[i + 1] == 'o') {
            m_SwitchEnabled = true;
            Print(GetLogPrefix() + "Switch On");
          }
        }
        break;
      }

      case HashCode("PUI"):
      case HashCode("DRI"): {
        if (eventStringData.size() < 2) break;
        optional<uint32_t> heroColor = ParseHeroColor(eventStringData.substr(1));
        if (heroColor.has_value()) {
          string action = "picked up";
          if (eventHash == HashCode("DRI")) {
            action = "dropped";
          }
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] " + action + " " + GetItemName(cacheValue) + ".");
        }
        break;
      }

      default: {
        if (m_Game.get().m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
          Print(GetLogPrefix() + "unhandled dota event: [" + key + "] for " + to_string(cacheValue));
        }
      }
    }
  }
  else if (missionKey == "Global")
  {
    // these are only received at the end of the game

    if (key == "Winner") {
      // Value 1 -> sentinel
      // Value 2 -> scourge

      uint8_t winner = static_cast<uint8_t>(cacheValue);
      if (winner == Dota::WINNER_SENTINEL || winner == Dota::WINNER_SCOURGE) {
        m_Winner = winner;
      }
      m_GameOverTime = m_Game.get().GetEffectiveTicks() / 1000;

      if (m_Winner == Dota::WINNER_SENTINEL)
        Print(GetLogPrefix() + "detected winner: Sentinel");
      else if (m_Winner == Dota::WINNER_SCOURGE)
        Print(GetLogPrefix() + "detected winner: Scourge");
      else
        Print(GetLogPrefix() + "detected winner: " + to_string(cacheValue));
    } else if (key == "m") {
      m_Time.first = cacheValue;
    } else if (key == "s") {
      m_Time.second = cacheValue;
    }
  }
  /*else if (missionKey == "DLL_State" || missionKey == "bonus" || missionKey == "bonush" || missionKey == "DonRepeatFail" || missionKey == "SyncCounter<XY>.000")
  {
  }*/
  else if (missionKey.size() <= 2 && missionKey.find_first_not_of("1234567890") == string::npos)
  {
    // most of these are only received at the end of the game

    optional<uint8_t> heroColor = ParseHeroColor(missionKey);
    if (heroColor.has_value())
    {
      // Key "3"		-> Creep Kills
      // Key "4"		-> Creep Denies
      // Key "7"		-> Neutral Kills
      // Key "id"     -> *heroColor (1-5 for sentinel, 6-10 for scourge, accurate after using -sp and/or -switch)

      string playerName = GetUserNameFromColor(*heroColor);

      switch (key[0]) {
        case '1':
          m_Players[*heroColor]->SetKills(cacheValue);
          break;
        case '2':
          m_Players[*heroColor]->SetDeaths(cacheValue);
          break;
        case '3':
          m_Players[*heroColor]->SetCreepKills(cacheValue);
          break;
        case '4':
          m_Players[*heroColor]->SetCreepDenies(cacheValue);
          break;
        case '5':
          m_Players[*heroColor]->SetAssists(cacheValue);
          break;
        case '6':
          m_Players[*heroColor]->SetGold(cacheValue);
          break;
        case '7':
          m_Players[*heroColor]->SetNeutralKills(cacheValue);
          break;
        case '8': {
          // 8_0 to 8_5
          if (key.size() >= 3 && key[1] == '_' && (0x30 <= key[2] && key[2] <= 0x35)) {
            const uint8_t slotIndex = key[2] - 0x30;
            m_Players[*heroColor]->SetItem(slotIndex, FourCCToString(cacheValue));
            Print(GetLogPrefix() + "[" + playerName + "] holds <" + GetItemName(cacheValue) + "> at slot " + ToDecString(slotIndex + 1) + ".");
          }
          break;
        }
        case '9': {
          // game start
          m_Players[*heroColor]->SetHero(FourCCToString(cacheValue));
          if (cacheValue == 0) {
            Print(GetLogPrefix() + "[" + playerName + "] failed to pick a hero.");
          } else {
            Print(GetLogPrefix() + "[" + playerName + "] picked <" + GetHeroName(cacheValue) + ">.");
          }
          break;
        }
        case 'i':
          if (key.size() >= 2 && key[1] == 'd' && 1 <= cacheValue && cacheValue <= 10) {
            // DotA sends id values from 1-10 with 1-5 being sentinel players and 6-10 being scourge players
            // unfortunately the actual player colours are from 1-5 and from 7-11 so we need to deal with this case here
            if (cacheValue >= 6)
              m_Players[*heroColor]->SetNewColor(static_cast<uint8_t>(cacheValue + 1));
            else
              m_Players[*heroColor]->SetNewColor(static_cast<uint8_t>(cacheValue));
          }
          break;

        default:
          break;
      }
    }
  }


  return true;
}

bool CDotaStats::UpdateQueue()
{
  return true;
}

void CDotaStats::FlushQueue()
{
}

[[nodiscard]] GameUser::CGameUser* CDotaStats::GetUserFromColor(const uint8_t color) const
{
  if (!GetIsHeroColor(color)) return nullptr;
  return m_Game.get().GetUserFromColor(color);
}

[[nodiscard]] std::string CDotaStats::GetUserNameFromColor(const uint8_t color) const
{
  switch (color) {
    case SENTINEL_CREEPS_COLOR:
      return "Sentinel"; // should never happen - use GetActorNameFromColor() instead
    case SCOURGE_CREEPS_COLOR:
      return "Scourge"; // should never happen - use GetActorNameFromColor() instead
    case NEUTRAL_CREEPS_COLOR:
      return "Neutral Creeps";
    default: {
      GameUser::CGameUser* user = GetUserFromColor(color);
      if (user) return user->GetName();
      return "Player " + ToDecString(color);
    }
  }
}

[[nodiscard]] std::string CDotaStats::GetActorNameFromColor(const uint8_t color) const
{
  switch (color) {
    case SENTINEL_CREEPS_COLOR:
      return "the Sentinel";
    case SCOURGE_CREEPS_COLOR:
      return "the Scourge";
    case NEUTRAL_CREEPS_COLOR:
      return "Neutral Creeps";
    default: {
      GameUser::CGameUser* user = GetUserFromColor(color);
      if (user) return "Player [" + user->GetName() + "]";
      return "Player " + ToDecString(color);
    }
  }
}

vector<CGameController*> CDotaStats::GetSentinelControllers() const
{
  vector<CGameController*> controllers;
  for (uint8_t color = 1, end = 5; color <= end; color++) {
    CGameController* controllerData = m_Game.get().GetGameControllerFromColor(color);
    if (!controllerData) continue;
    controllers.push_back(controllerData);
  }
  return controllers;
}

vector<CGameController*> CDotaStats::GetScourgeControllers() const
{
  vector<CGameController*> controllers;
  for (uint8_t color = 7, end = 11; color <= end; color++) {
    CGameController* controllerData = m_Game.get().GetGameControllerFromColor(color);
    if (!controllerData) continue;
    controllers.push_back(controllerData);
  }
  return controllers;
}

optional<GameResults> CDotaStats::GetGameResults(const GameResultConstraints& /*constraints*/) const
{
  optional<GameResults> gameResults;
  if (m_Winner == Dota::WINNER_UNDECIDED) {
    return gameResults;
  }

  vector<CGameController*> sentinel = GetSentinelControllers();
  vector<CGameController*> scourge = GetScourgeControllers();

  gameResults.emplace();

  if (m_Winner == Dota::WINNER_SENTINEL) {
    gameResults->winners.swap(sentinel);
    gameResults->losers.swap(scourge);
  } else {
    gameResults->winners.swap(scourge);
    gameResults->losers.swap(sentinel);
  }

  auto it = gameResults->winners.begin();
  while (it != gameResults->winners.end()) {
    // 5 seconds of grace period for players to quit just before the game ends
    if ((*it)->GetHasLeftGame() && (*it)->GetLeftGameTime() + 5u < GetGameOverTime()) {
      gameResults->losers.push_back(*it);
      it = gameResults->winners.erase(it);
    } else {
      ++it;
    }
  }

  return gameResults;
}

string CDotaStats::GetLogPrefix() const
{
  return "[DOTA: " + m_Game.get().GetGameName() + "] ";
}

void CDotaStats::LogMetaData(int64_t gameTicks, const string& text) const
{
  m_Game.get().Log(text, gameTicks);
}
