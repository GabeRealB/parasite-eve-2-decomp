#include "items.h"

#include "types.h"

#include "attachments.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "gameplay/starter_inventory.h"

#include "main/mc.h"
#include "main/areas.h"
#include "main/session.h"
#include "main/wipsys.h"

/* Item names and descriptions shared by the inventory tables. */
extern const u8 D_80093E68[];

extern const u8 D_80093E70[];

extern const u8 D_80093EB4[];

extern const u8 D_80093EE0[];

extern const u8 D_80093F2C[];

extern const u8 D_80093F90[];

extern const u8 D_80093FD8[];

extern const u8 D_80094030[];

extern const u8 D_80094090[];

extern const u8 D_800940F8[];

extern const u8 D_80094148[];

extern const u8 D_80094198[];

extern const u8 D_800941FC[];

extern const u8 D_80094268[];

extern const u8 D_800942C4[];

extern const u8 D_80094338[];

extern const u8 D_80094368[];

extern const u8 D_800943A0[];

extern const u8 D_800943DC[];

extern const u8 D_80094400[];

extern const u8 D_80094438[];

extern const u8 D_8009446C[];

extern const u8 D_800944A0[];

extern const u8 D_800944D8[];

extern const u8 D_80094510[];

extern const u8 D_80094538[];

extern const u8 D_80094574[];

extern const u8 D_800945B0[];

extern const u8 D_800945E8[];

extern const u8 D_80094620[];

extern const u8 D_8009465C[];

extern const u8 D_80094694[];

extern const u8 D_800946D0[];

extern const u8 D_80094710[];

extern const u8 D_80094744[];

extern const u8 D_80094774[];

extern const u8 D_800947A8[];

extern const u8 D_800947C8[];

extern const u8 D_800947E4[];

extern const u8 D_80094804[];

extern const u8 D_80094834[];

extern const u8 D_80094864[];

extern const u8 D_8009489C[];

extern const u8 D_800948C4[];

extern const u8 D_800948EC[];

extern const u8 D_8009491C[];

extern const u8 D_8009495C[];

extern const u8 D_80094994[];

extern const u8 D_800949D4[];

extern const u8 D_80094A10[];

extern const u8 D_80094A4C[];

extern const u8 D_80094A88[];

extern const u8 D_80094AF8[];

extern const u8 D_80094B70[];

extern const u8 D_80094BEC[];

extern const u8 D_80094C5C[];

extern const u8 D_80094CC4[];

extern const u8 D_80094D34[];

extern const u8 D_80094DA8[];

extern const u8 D_80094E18[];

extern const u8 D_80094E84[];

extern const u8 D_80094EF0[];

extern const u8 D_80094F38[];

extern const u8 D_80094FAC[];

extern const u8 D_80095000[];

extern const u8 D_8009506C[];

extern const u8 D_800950D0[];

extern const u8 D_80095138[];

extern const u8 D_80095170[];

extern const u8 D_800951C4[];

extern const u8 D_8009521C[];

extern const u8 D_8009526C[];

extern const u8 D_800952B8[];

extern const u8 D_80095310[];

extern const u8 D_80095380[];

extern const u8 D_800953C8[];

extern const u8 D_80095418[];

extern const u8 D_80095470[];

extern const u8 D_800954C0[];

extern const u8 D_80095508[];

extern const u8 D_8009555C[];

extern const u8 D_80095590[];

extern const u8 D_800955DC[];

extern const u8 D_8009562C[];

extern const u8 D_8009565C[];

extern const u8 D_800956AC[];

extern const u8 D_800956F0[];

extern const u8 D_80095748[];

extern const u8 D_800957A8[];

extern const u8 D_80095804[];

extern const u8 D_80095834[];

extern const u8 D_8009585C[];

extern const u8 D_800958B0[];

extern const u8 D_800958E0[];

extern const u8 D_80095924[];

extern const u8 D_80095990[];

extern const u8 D_800959E4[];

extern const u8 D_80095A28[];

extern const u8 D_80095A6C[];

extern const u8 D_80095AAC[];

extern const u8 D_80095B24[];

extern const u8 D_80095B58[];

extern const u8 D_80095B84[];

extern const u8 D_80095BC0[];

extern const u8 D_80095BF0[];

extern const u8 D_80095C20[];

extern const u8 D_80095C74[];

extern const u8 D_80095CB8[];

extern const u8 D_80095CFC[];

extern const u8 D_80095D50[];

extern const u8 D_80095DB0[];

extern const u8 D_80095E10[];

extern const u8 D_80095E74[];

extern const u8 D_80095EDC[];

extern const u8 D_80095EEC[];

extern const u8 D_80095F60[];

extern const u8 D_80095FD0[];

extern const u8 D_80096048[];

extern const u8 D_800960B8[];

extern const u8 D_80096120[];

extern const u8 D_8009618C[];

extern const u8 D_800961FC[];

extern const u8 D_8009620C[];

extern const u8 D_8009621C[];

extern const u8 D_8009622C[];

extern const u8 D_8009623C[];

extern const u8 D_8009624C[];

extern const u8 D_80096258[];

extern const u8 D_80096268[];

extern const u8 D_800962C4[];

extern const u8 D_80096348[];

extern const u8 D_80096378[];

extern const u8 D_800963A4[];

extern const u8 D_8009641C[];

extern const u8 D_80096458[];

extern const u8 D_800964E4[];

extern const u8 D_80096518[];

extern const u8 D_80096578[];

extern const u8 D_800965D8[];

extern const u8 D_8009660C[];

extern const u8 D_80096678[];

extern const u8 D_800966C0[];

extern const u8 D_80096718[];

extern const u8 D_80096740[];

extern const u8 D_8009676C[];

extern const u8 D_800967D0[];

extern const u8 D_80096828[];

extern const u8 D_80096874[];

extern const u8 D_800968C0[];

extern const u8 D_800968DC[];

extern const u8 D_80096928[];

extern const u8 D_8009694C[];

extern const u8 D_8009698C[];

extern const u8 D_80096A00[];

extern const u8 D_80096A3C[];

extern const u8 D_80096A84[];

extern const u8 D_80096ADC[];

extern const u8 D_80096B24[];

extern const u8 D_80096B6C[];

extern const u8 D_80096BC4[];

extern const u8 D_80096C0C[];

extern const u8 D_80096C60[];

extern const u8 D_80096CE8[];

extern const u8 D_80096D44[];

extern const u8 D_80096D9C[];

extern const u8 D_80096DF0[];

extern const u8 D_80096E0C[28];

extern const u8 D_80096E28[];

extern const u8 D_80096E38[];

/* Item table a scan window lies in. */
static inline InventoryItemRow* _gpScanTable(const InventoryItemRange* scan);

/// Saves a complete moved row and marks its former slot free, retaining the slot metadata.
///
/// `rowTable` and `index` are evaluated three times and must have no side
/// effects; `result` is a disjoint writable InventoryItemRow lvalue. Captures
/// no locals. Use as a standalone block statement inside braces.
#define INVENTORY_TAKE_MOVED_ROW(rowTable, index, result) \
    {                                                     \
        (result)                   = (rowTable)[(index)]; \
        (rowTable)[(index)].itemId = INVENTORY_ITEM_NONE; \
        (rowTable)[(index)].qty    = 0;                   \
    }

/* Item names and descriptions shared by the inventory tables. */
const u8 D_80093E68[]   = "\n\n\n\n\n\n";
const u8 D_80093E70[]   = "Recovery1\nMulti-vitamin tablet.\nHeals some HP.\nTablet\nA tablet.\n\n";
const u8 D_80093EB4[]   = "Recovery2\nAnalgesic capsule.\nHeals HP.\n\n\n\n";
const u8 D_80093EE0[]   = "Recovery3\nNutritive tonic.\nHeals lots of HP.\nBottle\nA medicine bottle.\n\n";
const u8 D_80093F2C[]   = "Stim\nRestores nerve function. Cures\nSilence, Berserker, Confusion.\nAmpoule\nSome sort of ampoule.\n\n";
const u8 D_80093F90[]   = "Cola\nCanned cola.\nRestores HP and MP.\nCan\nA tasty beverage in a can.\n\n";
const u8 D_80093FD8[]   = "MP Boost1\nSpring water.\nRestores some MP.\nPlastic Bottle\nA bottle filled with water.\n\n";
const u8 D_80094030[]   = "MP Boost2\nCarbonated mineral water.\nRestores lots of MP.\nBottle\nA bottle filled with water.\n\n";
const u8 D_80094090[]   = "Penicillin\nAnti-bacterial medkit.\nCures Darkness, Paralysis, Poison.\nMedkit\nA multi-purpose medkit.\n\n";
const u8 D_800940F8[]   = "SMG Clip Holder\nMP5A5 magazine clip holder.\nIncreases ammo capacity by 30.\n\n\n\n";
const u8 D_80094148[]   = "Rifle Clip Holder\nM4A1 magazine clip holder.\nIncreases ammo capacity by 30.\n\n\n\n";
const u8 D_80094198[]   = "Lipstick\nMoisturizing lip balm.\nAttach to prevent Silence.\nCylinder\nA small metallic cylinder.\n\n";
const u8 D_800941FC[]   = "Snail Magazine\nP08 Snail magazine.\nIncreases ammo capacity by 25.\nMagazine\nA snail-shaped ammo magazine.\n\n";
const u8 D_80094268[]   = "Belt Pouch\nA large belt pack.\nIncreases # of armor attachments.\nPouch\nA clip-on pouch.\n\n";
const u8 D_800942C4[]   = "MD Player\nMD player with \"whale songs\" disc.\nPrevents Confusion, Berserker.\nMD Player\nA portable music player.\n\n";
const u8 D_80094338[]   = "Pyrokinesis\nFires a burst\nof flame forward.\n\n\n\n";
const u8 D_80094368[]   = "Pyrokinesis\nFires two bursts.\nBetter power & range.\n\n\n\n";
const u8 D_800943A0[]   = "Pyrokinesis\nFires multiple bursts.\nBest power & range.\n\n\n\n";
const u8 D_800943DC[]   = "Combustion\nImmolates enemy.\n\n\n\n\n";
const u8 D_80094400[]   = "Combustion\nImmolates enemy.\nBetter power & range.\n\n\n\n";
const u8 D_80094438[]   = "Combustion\nImmolates enemy.\nBest power & range.\n\n\n\n";
const u8 D_8009446C[]   = "Inferno\nGenerates a small,\nlocalized explosion.\n\n\n\n";
const u8 D_800944A0[]   = "Inferno\nGenerates a medium,\nlocalized explosion.\n\n\n\n";
const u8 D_800944D8[]   = "Inferno\nGenerates a large,\nlocal area explosion.\n\n\n\n";
const u8 D_80094510[]   = "Necrosis\nDischarges electricity.\n\n\n\n\n";
const u8 D_80094538[]   = "Necrosis\nDischarges electricity.\nBetter power & range.\n\n\n\n";
const u8 D_80094574[]   = "Necrosis\nDischarges electricity.\nBest power & range.\n\n\n\n";
const u8 D_800945B0[]   = "Plasma\nKnocks down targets\nw/ a small blast wave.\n\n\n\n";
const u8 D_800945E8[]   = "Plasma\nSmashes down targets\nw/ a small blast wave.\n\n\n\n";
const u8 D_80094620[]   = "Plasma\nFlattens all targets\nin the surrounding area.\n\n\n\n";
const u8 D_8009465C[]   = "Apobiosis\nTemporarily paralyzes\nadjacent enemies.\n\n\n\n";
const u8 D_80094694[]   = "Apobiosis\nCauses acute seizures\nin surrounding targets.\n\n\n\n";
const u8 D_800946D0[]   = "Apobiosis\nParalyzes all enemies\nin the surrounding area.\n\n\n\n";
const u8 D_80094710[]   = "Metabolism\nRestores normal\nstatus over time.\n\n\n\n";
const u8 D_80094744[]   = "Metabolism\nRapidly restores\nnormal status.\n\n\n\n";
const u8 D_80094774[]   = "Metabolism\nInstantly restores\nnormal status.\n\n\n\n";
const u8 D_800947A8[]   = "Healing\nHeals a little HP.\n\n\n\n\n";
const u8 D_800947C8[]   = "Healing\nHeals some HP.\n\n\n\n\n";
const u8 D_800947E4[]   = "Healing\nHeals a lot of HP.\n\n\n\n\n";
const u8 D_80094804[]   = "Lifedrain\nDrains HP from\nadjacent targets.\n\n\n\n";
const u8 D_80094834[]   = "Lifedrain\nDrains HP from\nnearby targets.\n\n\n\n";
const u8 D_80094864[]   = "Lifedrain\nDrains HP from all\nsurrounding targets.\n\n\n\n";
const u8 D_8009489C[]   = "Antibody\nReduces physical\ndamage.\n\n\n\n";
const u8 D_800948C4[]   = "Antibody\nWards off physical\ndamage.\n\n\n\n";
const u8 D_800948EC[]   = "Antibody\nGreatly reduces\nphysical damage.\n\n\n\n";
const u8 D_8009491C[]   = "Energyshot\nRaises bullets' kinetic\nenergy & penetration.\n\n\n\n";
const u8 D_8009495C[]   = "Energyshot\nRaises bullets' kinetic\nenergy & impact.\n\n\n\n";
const u8 D_80094994[]   = "Energyshot\nRaises bullets' kinetic\nenergy & killing power.\n\n\n\n";
const u8 D_800949D4[]   = "Energyball\nCreates a circling orb\nthat repels attacks.\n\n\n\n";
const u8 D_80094A10[]   = "Energyball\nCreates 2 circling orbs\nthat repel attacks.\n\n\n\n";
const u8 D_80094A4C[]   = "Energyball\nCreates a ring of orbs\nthat attack targets.\n\n\n\n";
const u8 D_80094A88[]   = "Skull Crystal\nSkull-shaped S. American crystal.\nAttach this and see what happens.\nCrystal\nA carved crystal.\n\n";
const u8 D_80094AF8[]   = "Medicine Wheel\nNative American wall ornament.\nAttach this and see what happens.\nWoven Ring\nA hand-made wall ornament.\n\n";
const u8 D_80094B70[]   = "Holy Water\nSmall bottle labeled \"Holy Water.\"\nAttach this and see what happens.\nBottle\nA small bottle filled with liquid.\n\n";
const u8 D_80094BEC[]   = "Ofuda\nCharm with Japanese lettering.\nAttach this and see what happens.\nCharm\nThere's some Japanese on this.\n\n";
const u8 D_80094C5C[]   = "Flare\nDisposable self-defense weapon.\nUse to blind surrounding enemies.\nSmall Box\nA small metal box.\n\n";
const u8 D_80094CC4[]   = "Pepper Spray\nDisposable self-defense weapon.\nUse to stun surrounding enemies.\nSpray Can\nA small spray can.\n\n";
const u8 D_80094D34[]   = "Protein Capsule\nProtein compound medicine capsule.\nFull HP recovery.  Max HP +5.\nCapsule\nSome sort of capsule.\n\n";
const u8 D_80094DA8[]   = "Ringer's Solution\nBlood substitute.\nRestores HP and MP to max.\nVinyl Pack\nA vinyl pack filled with liquid.\n\n";
const u8 D_80094E18[]   = "Eau de Toilette\nDisposable eau de toilette spray.\nScent releases latent powers.\nBottle\nA small bottle.\n\n";
const u8 D_80094E84[]   = "Hunter Goggles\nSpecial filtered goggles.\nAttach to prevent blindness.\nGoggles\nThese are colored goggles.\n\n";
const u8 D_80094EF0[]   = "GPS\nMilitary global positioning system.\nIncludes a motion detector.\n\n\n\n";
const u8 D_80094F38[]   = "Combat Light\nDisposable defense weapon.\nBlinds enemies in front of you.\nKey Chain\nA light and a key chain in one!\n\n";
const u8 D_80094FAC[]   = "Hammer\nHigh-voltage stun gun.\nCustom M4A1 attachment.\nStun Gun\nAn electric prod.\n\n";
const u8 D_80095000[]   = "Pyke\nSmall portable flamethrower.\nCustom M4A1 attachment.\nIgnition Device\nSome sort of ignition device.\n\n";
const u8 D_8009506C[]   = "Javelin\nSmall laser gun.\nCustom M4A1 attachment.\nLaser Attachment\nSome sort of weapon attachment.\n\n";
const u8 D_800950D0[]   = "M203\n40mm grenade launcher.\nCustom M4A1 attachment.\nGrenade Launcher\nA grenade launcher attachment.\n\n";
const u8 D_80095138[]   = "M9\nClose-combat bayonet.\nCustom M4A1 attachment.\n\n\n\n";
const u8 D_80095170[]   = "Leather Jacket\nAya's favorite leather jacket.\nDecent armor with special lining.\n\n\n\n";
const u8 D_800951C4[]   = "Tactical Armor\nModern all-terrain combat armor.\nIncludes heads-up motion detector.\n\n\n\n";
const u8 D_8009521C[]   = "Combat Armor\nHeavy police body armor.\nHelmet amplifier prevents Silence.\n\n\n\n";
const u8 D_8009526C[]   = "Assault Suit\nGood desert jacket. Has\nattachments, but poor protection.\n\n\n\n";
const u8 D_800952B8[]   = "PASGT Vest\nBody armor in current military use.\nIncludes an emergency first aid kit.\n\n\n\n";
const u8 D_80095310[]   = "Tactical Vest\nBulletproof SWAT vest.\nIncludes an emergency first aid kit.\nBody Armor\nA bulletproof SWAT vest.\n\n";
const u8 D_80095380[]   = "EOD Suit\nExplosives squad body armor.\nResists explosions and heat.\n\n\n\n";
const u8 D_800953C8[]   = "Turtle Vest\nNMC Hunter reinforced vest.\nCan be worn under regular clothing.\n\n\n\n";
const u8 D_80095418[]   = "Chicken Plate\nFlak jacket worn by bomber pilots.\nCeramic vest plates absorb impact.\n\n\n\n";
const u8 D_80095470[]   = "NBC Suit\nEnvironmental NBC protection suit.\nSuitable for long-term wear.\n\n\n\n";
const u8 D_800954C0[]   = "PsySuit\nNeo-mitochondria labsuit.\nIncludes remote ANMC analyzer.\n\n\n\n";
const u8 D_80095508[]   = "Aya Special\nAya's custom-made armored vest.\nOutfitted for anti-NMC operations.\n\n\n\n";
const u8 D_8009555C[]   = "Shoulder Holster\nHolster worn over a T-shirt.\n\n\n\n\n";
const u8 D_80095590[]   = "Monk Robe\nMitochondria worshipper's mantle.\nNMC blood stains boost PE.\n\n\n\n";
const u8 D_800955DC[]   = "P08(S. Magazine)\nP08 modified w/ a snail magazine.\nIncreased ammo capacity.\n\n\n\n";
const u8 D_8009562C[]   = "M93R\nSemi-auto 9mm, fires 3-round\nbursts.\n\n\n\n";
const u8 D_8009565C[]   = "M950\nFull-auto 9mm w/ high ammo\ncapacity.\nLarge Handgun\nA full-auto handgun.\n\n";
const u8 D_800956AC[]   = "P08\nSemi-auto 9mm w/ special\nloader.\nHandgun\nA semi-auto handgun.\n\n";
const u8 D_800956F0[]   = "P229\nSemi-auto 9mm w/ silencer.\nFlashlight attachment.\nHandgun\nA semi-auto handgun.\n\n";
const u8 D_80095748[]   = "Mongoose\nLarge 44-caliber revolver.\nSlow reload, but it's worth it.\nLarge Handgun\nA revolver.\n\n";
const u8 D_800957A8[]   = "Grenade Pistol\nPistol-sized 40mm\ngrenade launcher.\nGrenade Launcher\nA grenade launcher.\n\n";
const u8 D_80095804[]   = "MM1\n40mm launcher w/ revolving\nmagazine.\n\n\n\n";
const u8 D_80095834[]   = "PA3\nPump-action 12-gauge\nshotgun.\n\n\n\n";
const u8 D_8009585C[]   = "SP12\n12-gauge close-combat shotgun.\nPowerful, but cumbersome.\nShotgun\nA shotgun.\n\n";
const u8 D_800958B0[]   = "AS12\nRapid-fire 12-gauge\ncombat shotgun.\n\n\n\n";
const u8 D_800958E0[]   = "M4A1 Rifle\n5.56mm assault rifle.\nCustomizable with attachments.\n\n\n\n";
const u8 D_80095924[]   = "M249\nPortable light machine gun.\nHeavy firepower and a large clip.\nLight Machine Gun\nA light machine gun.\n\n";
const u8 D_80095990[]   = "Tonfa Baton\nBaton based on an Okinawan design.\nMasters can attack continuously.\n\n\n\n";
const u8 D_800959E4[]   = "M4A1(+1)\nM4A1 assault rifle.\nMagazine holds 30 extra rounds.\n\n\n\n";
const u8 D_80095A28[]   = "M4A1(+2)\nM4A1 assault rifle.\nMagazine holds 60 extra rounds.\n\n\n\n";
const u8 D_80095A6C[]   = "Hypervelocity\nMagnetic railgun.\nFires hypersonic rounds.\n\n\n\n";
const u8 D_80095AAC[]   = "Gunblade\nUltrahigh frequency particle blade.\nBlade with shotgun attachment.\nKatana???\nThis sword looks familiar...\n\n";
const u8 D_80095B24[]   = "M4A1 Hammer\nAssault rifle w/ heavy stun gun.\n\n\n\n\n";
const u8 D_80095B58[]   = "M4A1 Bayonet\nAssault rifle w/ bayonet.\n\n\n\n\n";
const u8 D_80095B84[]   = "M4A1 Grenade\nAssault rifle w/ 40mm\ngrenade launcher.\n\n\n\n";
const u8 D_80095BC0[]   = "M4A1 Pyke\nAssault rifle w/ flamethrower.\n\n\n\n\n";
const u8 D_80095BF0[]   = "M4A1 Javelin\nRifle w/ laser gun attached.\n\n\n\n\n";
const u8 D_80095C20[]   = "MP5A5\n9mm submachine gun.\nFlashlight attached.\nSubmachine Gun\nA submachine gun.\n\n";
const u8 D_80095C74[]   = "MP5A5(+1)\nSubmachine gun MP5A5.\nMagazine holds 30 extra rounds.\n\n\n\n";
const u8 D_80095CB8[]   = "MP5A5(+2)\nSubmachine gun MP5A5.\nMagazine holds 60 extra rounds.\n\n\n\n";
const u8 D_80095CFC[]   = "9mm P.B.\n9mm caliber full metal jacket round.\nFor handguns and submachine guns.\n\n\n\n";
const u8 D_80095D50[]   = "9mm Hydra\n9mm hollow-point round.\nSoft tip increases tissue damage.\n9mm\nHandgun ammunition.\n\n";
const u8 D_80095DB0[]   = "9mm Spartan\n9mm fragmentation round.\nFragments rupture vital organs.\n9mm\nHandgun ammunition.\n\n";
const u8 D_80095E10[]   = "44 Magnum\n44 caliber magnum round.\nImpressive destructive power.\nMagnum Round\nHandgun ammunition.\n\n";
const u8 D_80095E74[]   = "44 Maeda SP\nAnti-NMC 44 magnum round.\nContains toxic mitochondria.\nMagnum Round\nHandgun ammunition.\n\n";
const u8 D_80095EDC[]   = "44 Poison\n\n\n\n\n\n";
const u8 D_80095EEC[]   = "Grenade\n40mm fragmentation grenade.\nShoots fragments over a wide area.\nGrenade Round\nGrenade launcher ammunition.\n\n";
const u8 D_80095F60[]   = "Airburst\n40mm aerial burst grenade.\nArcs and explodes in the air.\nGrenade Round\nGrenade launcher ammunition.\n\n";
const u8 D_80095FD0[]   = "Riot\n40mm special acoustic round.\nEmits a glaring flash and loud noise.\nGrenade Round\nGrenade launcher ammunition.\n\n";
const u8 D_80096048[]   = "Buckshot\n12-gauge shotgun scatter shot.\nSpreads 9 lead shots in a burst.\nShotgun Shell\nShotgun ammunition.\n\n";
const u8 D_800960B8[]   = "Firefly\n12-gauge shotgun incendiary shot.\nIgnites flammable target.\nShotgun Shell\nShotgun ammunition.\n\n";
const u8 D_80096120[]   = "R. Slug\n12-gauge shotgun solid shot.\nPowerful shot with good accuracy.\nShotgun Shell\nShotgun ammunition.\n\n";
const u8 D_8009618C[]   = "5.56 Rifle\n5.56mm full metal jacket round.\nFor rifles and light machine guns.\nRifle Round\nRifle ammunition.\n\n";
const u8 D_800961FC[]   = "Paralizer\n\n\n\n\n\n";
const u8 D_8009620C[]   = "Flashbomb\n\n\n\n\n\n";
const u8 D_8009621C[]   = "Battery\n\n\n\n\n\n";
const u8 D_8009622C[]   = "Battery\n\n\n\n\n\n";
const u8 D_8009623C[]   = "Battery\n\n\n\n\n\n";
const u8 D_8009624C[]   = "Fuel\n\n\n\n\n\n";
const u8 D_80096258[]   = "Battery\n\n\n\n\n\n";
const u8 D_80096268[]   = "Parthenon Key\nIt opened the cafeteria...\n\nCafeteria Key\nCafeteria key from SWAT officer.\n\n";
const u8 D_800962C4[]   = "Micro Device\nPierce's modified NMC implant.\nDisplays my position on a monitor.\nMetallic Implant\nObject implanted in NMC's skull.\n\n";
const u8 D_80096348[]   = "Red Key\nKey on a skull-shaped key chain.\n\n\n\n\n";
const u8 D_80096378[]   = "Blue Key\nKey on a feather key chain.\n\n\n\n\n";
const u8 D_800963A4[]   = "Armory Cardkey\nKey to the shelter armory.\nThat man stored weapons here.\nBlack Card\nCard dropped by the SWAT imitator.\n\n";
const u8 D_8009641C[]   = "MIST Badge\nMIST ID card.\nLooks a lot like an FBI badge.\n\n\n\n";
const u8 D_80096458[]   = "Mendel (Sept. issue)\nThis month's feature: Mitochondria.\nA popular topic these days.\nScientific Journal\nMendel-- popular science journal.\n\n";
const u8 D_800964E4[]   = "Clipboard\nAgent Baldwin always carries this.\n\n\n\n\n";
const u8 D_80096518[]   = "MIST Search Warrant\nAuthorizes any search in Dryfield.\nIncludes private property searches.\n\n\n\n";
const u8 D_80096578[]   = "NMC Photo\nSo this was a picture of an NMC.\n\nUMA Photograph\nLooks a little big for a cougar...\n\n";
const u8 D_800965D8[]   = "Manual\nMIST civilian disinformation manual.\n\n\n\n\n";
const u8 D_8009660C[]   = "Dryfield Map\nMap of part of the Mojave Desert.\nThere's only one town on the road.\nMap\nMap of Dryfield.\n\n";
const u8 D_80096678[]   = "Motel Key No.6\nMotel room key from Mr. Douglas.\nThe tag says \"6.\"\n\n\n\n";
const u8 D_800966C0[]   = "Saloon Key\nKey to \"Gene & Roy's\" saloon.\n\nKey Taken from Corpse\nIt's engraved, \"G&R.\"\n\n";
const u8 D_80096718[]   = "Monkey Wrench\nAdjustable wrench.\n\n\n\n\n";
const u8 D_80096740[]   = "Lobby Key\nLobby key from Mr. Douglas.\n\n\n\n\n";
const u8 D_8009676C[]   = "Bronco Masterkey\nShould open all the motel rooms.\n\nKey Found in the Lobby\nThe tag says \"Bronco.\"\n\n";
const u8 D_800967D0[]   = "Wire Rope\nA sturdy rope.\nThis should support my weight.\nRope\nThis could be useful.\n\n";
const u8 D_80096828[]   = "Factory Key\nKey to Mr. Douglas's garage.\nThis key is a real antique!\n\n\n\n";
const u8 D_80096874[]   = "Truck Key\nTruck key from Mr. Douglas.\nThis is my ticket to the shelter.\n\n\n\n";
const u8 D_800968C0[]   = "Jerry Can\nIt's empty.\n\n\n\n\n";
const u8 D_800968DC[]   = "Gasoline\nGasoline (in the gas can).\nThis should get the truck running.\n\n\n\n";
const u8 D_80096928[]   = "Ice Bag\nBag filled with ice.\n\n\n\n\n";
const u8 D_8009694C[]   = "Bag of Water\nAll the ice melted.\nNo use for this anymore.\n\n\n\n";
const u8 D_8009698C[]   = "Bottlecap Magnet\nMagnet shaped like a cola cap.\nThe back is magnetized.\nMagnet\nI used to collect these in school.\n\n";
const u8 D_80096A00[]   = "SUV Key\nKey to Pierce's SUV.\nIt's on a cute key chain.\n\n\n\n";
const u8 D_80096A3C[]   = "Oak Board\nThere are footprints on it.\n\nBoard\nA sturdy-looking board.\n\n";
const u8 D_80096A84[]   = "Jumper Plug\nElectrical switchboard component.\n\nPlug\nLooks like a headphone jack...\n\n";
const u8 D_80096ADC[]   = "Bowman's Card\nThis belonged to a researcher.\n\nCard\nA plastic card.\n\n";
const u8 D_80096B24[]   = "Yoshida's Card\nThis belonged to a programmer.\n\nCard\nA plastic card.\n\n";
const u8 D_80096B6C[]   = "Electric Car Key\nKey to the shelter electric car.\n\nCar Key\nLooks like a car key...\n\n";
const u8 D_80096BC4[]   = "Teddy Bear\nThis is Eve's teddy bear.\n\nTeddy Bear\nHmm... A teddy bear.\n\n";
const u8 D_80096C0C[]   = "Pierce's Memo\nIt's in Pierce's handwriting.\n\nMessage\nSomething's scribbled on it.\n\n";
const u8 D_80096C60[]   = "Aeris (Sept. issue)\nA magazine about the Internet.\nSpecial issue: \"Next-Gen Viruses\"\nComputer Magazine\nAeris-- a computer magazine.\n\n";
const u8 D_80096CE8[]   = "Mr. Douglas's Letter\nIt's from Mr. Douglas.\n\nCanister\nSmall canister from Flint's neck.\n\n";
const u8 D_80096D44[]   = "Jumper Plug\nElectrical switchboard component.\n\nPlug\nLooks like a headphone jack...\n\n";
const u8 D_80096D9C[]   = "Micro Device\nPierce's modified NMC implant.\nDisplays my position on a monitor.\n\n\n\n";
const u8 D_80096DF0[]   = "Pierce Rescue Bonus\n\n\n\n\n\n";
const u8 D_80096E0C[28] = "Soldier Rescue Bonus\n\n\n\n\n\n\000\227";
const u8 D_80096E28[]   = "You do not have";
const u8 D_80096E38[]   = "applicable weapon";

ItemDesc Gp_ItemDescs[] = {
    { 0, 0, 0x00, D_80093E68 },
    { 100, 1, 0x00, D_80093E70 },
    { 180, 1, 0x8, D_80093EB4 },
    { 350, 1, 0x00, D_80093EE0 },
    { 80, 1, 0x00, D_80093F2C },
    { 120, 1, 0x00, D_80093F90 },
    { 320, 1, 0x00, D_80093FD8 },
    { 580, 1, 0x00, D_80094030 },
    { 80, 1, 0x00, D_80094090 },
    { 3980, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800940F8 },
    { 1800, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094148 },
    { 5000, 0, 0x00, D_80094198 },
    { 1000, 0, ITEM_FLAG_NO_ATTACHMENT, D_800941FC },
    { 10000, 0, ITEM_FLAG_NO_ATTACHMENT, D_80094268 },
    { 1000, 0, 0x00, D_800942C4 },
    { 500, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094338 },
    { 1250, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094368 },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800943A0 },
    { 750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800943DC },
    { 1750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094400 },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094438 },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009446C },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800944A0 },
    { 5000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800944D8 },
    { 500, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094510 },
    { 1250, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094538 },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094574 },
    { 750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800945B0 },
    { 1750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800945E8 },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094620 },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009465C },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094694 },
    { 5000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800946D0 },
    { 500, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094710 },
    { 1250, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094744 },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094774 },
    { 750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800947A8 },
    { 1750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800947C8 },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800947E4 },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094804 },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094834 },
    { 5000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094864 },
    { 500, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009489C },
    { 1250, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800948C4 },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800948EC },
    { 750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009491C },
    { 1750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009495C },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094994 },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800949D4 },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094A10 },
    { 5000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80094A4C },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 5000, 0, 0x00, D_80094A88 },
    { 27800, 0, 0x00, D_80094AF8 },
    { 5000, 0, 0x00, D_80094B70 },
    { 5000, 0, 0x00, D_80094BEC },
    { 150, 0, 0x00, D_80094C5C },
    { 100, 0, 0x00, D_80094CC4 },
    { 10000, 1, 0x00, D_80094D34 },
    { 200, 1, 0x00, D_80094DA8 },
    { 190, 1, 0x00, D_80094E18 },
    { 1000, 0, 0x00, D_80094E84 },
    { 1000, 0, ITEM_FLAG_NO_DISCARD | 0x8, D_80094EF0 },
    { 60, 0, 0x00, D_80094F38 },
    { 3720, 0, ITEM_FLAG_NO_ATTACHMENT, D_80094FAC },
    { 5180, 0, ITEM_FLAG_NO_ATTACHMENT, D_80095000 },
    { 7500, 0, ITEM_FLAG_NO_ATTACHMENT, D_8009506C },
    { 2130, 0, ITEM_FLAG_NO_ATTACHMENT, D_800950D0 },
    { 980, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80095138 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 1000, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80095170 },
    { 12800, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800951C4 },
    { 3250, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009521C },
    { 1000, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009526C },
    { 2980, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800952B8 },
    { 2120, 16, ITEM_FLAG_NO_ATTACHMENT, D_80095310 },
    { 4580, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80095380 },
    { 1680, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800953C8 },
    { 1000, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80095418 },
    { 3980, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80095470 },
    { 4580, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800954C0 },
    { 8000, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80095508 },
    { 2580, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009555C },
    { 3000, 16, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80095590 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 1000, 33, 0x8, D_800955DC },
    { 2000, 33, ITEM_FLAG_NO_DISCARD | 0x8, D_8009562C },
    { 5750, 34, 0x00, D_8009565C },
    { 680, 33, 0x00, D_800956AC },
    { 1880, 33, 0x00, D_800956F0 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 2850, 34, 0x00, D_80095748 },
    { 0, 0, 0x00, D_80093E68 },
    { 1680, 36, 0x00, D_800957A8 },
    { 23500, 36, 0x8, D_80095804 },
    { 1000, 37, 0x8, D_80095834 },
    { 3980, 37, 0x00, D_8009585C },
    { 12500, 37, 0x8, D_800958B0 },
    { 2450, 38, 0x8, D_800958E0 },
    { 15800, 35, 0x00, D_80095924 },
    { 0, 0, 0x00, D_80093E68 },
    { 1000, 39, ITEM_FLAG_NO_DISCARD | 0x8, D_80095990 },
    { 5000, 38, 0x8, D_800959E4 },
    { 7500, 38, 0x8, D_80095A28 },
    { 20000, 36, 0x8, D_80095A6C },
    { 10000, 32, 0x00, D_80095AAC },
    { 0, 0, 0x00, D_80093E68 },
    { 6000, 38, 0x8, D_80095B24 },
    { 4000, 38, 0x8, D_80095B58 },
    { 4500, 38, 0x8, D_80095B84 },
    { 8000, 38, 0x8, D_80095BC0 },
    { 9000, 38, 0x8, D_80095BF0 },
    { 6980, 35, 0x00, D_80095C20 },
    { 11000, 35, 0x8, D_80095C74 },
    { 15000, 35, 0x8, D_80095CB8 },
    { 30, 49, 0x8, D_80095CFC },
    { 50, 49, 0x00, D_80095D50 },
    { 80, 49, 0x00, D_80095DB0 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 100, 51, 0x00, D_80095E10 },
    { 800, 51, 0x00, D_80095E74 },
    { 0, 51, 0x8, D_80095EDC },
    { 280, 52, 0x00, D_80095EEC },
    { 450, 52, 0x00, D_80095F60 },
    { 80, 52, 0x00, D_80095FD0 },
    { 60, 53, 0x00, D_80096048 },
    { 90, 53, 0x00, D_800960B8 },
    { 120, 53, 0x00, D_80096120 },
    { 100, 54, 0x00, D_8009618C },
    { 0, 54, 0x8, D_800961FC },
    { 0, 54, 0x8, D_8009620C },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 56, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009621C },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 56, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009622C },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 56, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009623C },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 57, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_8009624C },
    { 0, 56, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80096258 },
    { 0, 0, 0x00, D_80093E68 },
};

ItemDesc Gp_KeyItemDescs[] = {
    { 0, 0, 0x00, D_80093E68 },
    { 1000, 64, ITEM_FLAG_NO_ATTACHMENT, D_80096268 },
    { 1000, 64, 0x00, D_800962C4 },
    { 1000, 64, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_80096348 },
    { 1000, 64, 0x8, D_80096378 },
    { 20000, 64, 0x00, D_800963A4 },
    { 1000, 64, 0x8, D_8009641C },
    { 20000, 64, ITEM_FLAG_NO_ATTACHMENT, D_80096458 },
    { 1000, 64, ITEM_FLAG_NO_ATTACHMENT | 0x8, D_800964E4 },
    { 1000, 64, 0x8, D_80096518 },
    { 1000, 64, 0x00, D_80096578 },
    { 1000, 64, 0x8, D_800965D8 },
    { 1000, 64, 0x00, D_8009660C },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 1000, 64, 0x8, D_80096678 },
    { 1000, 64, 0x00, D_800966C0 },
    { 1000, 64, 0x8, D_80096718 },
    { 1000, 64, 0x8, D_80096740 },
    { 1000, 64, 0x00, D_8009676C },
    { 1000, 64, 0x00, D_800967D0 },
    { 1000, 64, 0x8, D_80096828 },
    { 1000, 64, 0x8, D_80096874 },
    { 1000, 64, 0x8, D_800968C0 },
    { 1000, 64, 0x8, D_800968DC },
    { 1000, 64, 0x8, D_80096928 },
    { 1000, 64, 0x8, D_8009694C },
    { 1000, 64, ITEM_FLAG_NO_ATTACHMENT, D_8009698C },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 20000, 64, 0x8, D_80096A00 },
    { 1000, 64, 0x00, D_80096A3C },
    { 1000, 64, 0x00, D_80096A84 },
    { 60000, 64, 0x00, D_80096ADC },
    { 30000, 64, 0x00, D_80096B24 },
    { 1000, 64, 0x00, D_80096B6C },
    { 20000, 64, 0x00, D_80096BC4 },
    { 50000, 64, 0x00, D_80096C0C },
    { 0, 0, 0x00, D_80093E68 },
    { 10000, 64, 0x00, D_80096C60 },
    { 20000, 64, 0x00, D_80096CE8 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 1000, 64, 0x00, D_80096D44 },
    { 10000, 64, 0x8, D_80096D9C },
    { 0, 0, 0x00, D_80093E68 },
    { 60000, 64, 0x8, D_80096DF0 },
    { 60000, 64, 0x8, D_80096E0C },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
    { 0, 0, 0x00, D_80093E68 },
};

/* Gives `scan` one `weapon` and loads it with `ammo`. */
#define GP_GIVE_LOADED(scan, weapon, ammo)                                                    \
    do {                                                                                      \
        inventoryGiveItem(scan, weapon, 1);                                                   \
        equipmentLoadWeaponConsumable(scan, weapon, ammo, EQUIPMENT_WEAPON_LOAD_TO_CAPACITY); \
    } while (0)

/* Clears the carried inventory, equips the starting armour, restores HP/MP,
 * and gives the initial supplies and their attachment slots. */
#define _gpInitStartingItems(scan, cfg)                      \
    do {                                                     \
        inventoryClearItems(scan);                           \
        inventoryGiveItem(scan, 0x60, 1);                    \
        equipmentEquipCarriedArmor(0x60);                    \
        (cfg)->hp = (cfg)->hpMax;                            \
        (cfg)->mp = (cfg)->mpMax;                            \
        inventoryGiveItem(scan, 0x92, 1);                    \
        inventoryGiveItem(scan, 0x40, 1)->attachSlot    = 1; \
        inventoryGiveItem(scan, 0xA0, 0x64)->attachSlot = 2; \
    } while (0)

/// Clears identification storage and identifies catalogue entries without an unknown name.
///
/// Scans raw ids 0..383 after clearing all 96 stored words. Text contains three
/// NUL/newline-terminated identified fields; a following newline means there is
/// no unknown name. The raw ordinary/key mapping must preserve its physical aliases.
static inline void _itemInitializeNewGameIdentification(void)
{
    enum { ITEM_IDENTIFICATION_KEY_ITEM_FIRST = 0x100,
           ITEM_IDENTIFICATION_ID_LIMIT       = 0x180,
           ITEM_IDENTIFIED_TEXT_FIELD_COUNT   = 3 };
    const ItemDesc* descriptor;
    const u8*       text;
    // Reused first for stored words, then for raw item ids; keeps the matching allocation.
    s32 identificationIndex;
    s32 fieldsRemaining;

    for (identificationIndex = ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemSeenBits) - 1; identificationIndex >= 0; identificationIndex--) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemSeenBits[identificationIndex] = 0;
    }

    identificationIndex = 0;
    do {
        fieldsRemaining = ITEM_IDENTIFIED_TEXT_FIELD_COUNT;
        if (identificationIndex < ITEM_IDENTIFICATION_KEY_ITEM_FIRST) {
            descriptor = &Gp_ItemDescs[identificationIndex];
        } else {
            descriptor = &Gp_KeyItemDescs[identificationIndex - ITEM_IDENTIFICATION_KEY_ITEM_FIRST];
        }
        text = descriptor->textFields;
        while (fieldsRemaining > 0) {
            if (*text == '\0' || *text == '\n') {
                fieldsRemaining--;
            }
            text++;
        }
        if (*text == '\n') {
            itemSetIdentified(identificationIndex, 1);
        }
        identificationIndex++;
    } while (identificationIndex < ITEM_IDENTIFICATION_ID_LIMIT);
}

/// Resets saved weapon loads and installs each weapon's built-in rechargeable supply.
///
/// Removable primary loads start empty. Only M4A1 Grenade starts with an available
/// removable secondary; every other secondary is unavailable before supplies
/// are installed. Does not grant weapons or change inventory rows.
static inline void _equipmentInitializeNewGameWeaponLoads(void)
{
    enum { EQUIPMENT_ITEM_M4A1_GRENADE = 0x9A };
    EquipmentWeaponLoad* weaponLoad;
    s32                  weaponIndex;

    weaponLoad = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems;
    for (weaponIndex = 0; weaponIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems); weaponIndex++) {
        weaponLoad->primaryItemId   = INVENTORY_ITEM_NONE;
        weaponLoad->primaryQty      = 0;
        weaponLoad->secondaryItemId = EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE;
        weaponLoad->secondaryQty    = 0;
        // The M4A1 grenade launcher has a reloadable secondary slot.
        if (weaponIndex == EQUIPMENT_ITEM_M4A1_GRENADE - EQUIPMENT_WEAPON_ITEM_FIRST) {
            weaponLoad->secondaryItemId = INVENTORY_ITEM_NONE;
            weaponLoad->secondaryQty    = 0;
        }
        weaponLoad->field_4 = 0;
        weaponLoad++;
    }
    equipmentInitializeWeaponSupplies();
}

void inventoryInitializeNewGame(void)
{
    enum {
        INVENTORY_NEW_GAME_ITEM_RECOVERY_1        = 0x01,
        INVENTORY_NEW_GAME_ITEM_RECOVERY_2        = 0x02,
        INVENTORY_NEW_GAME_ITEM_STIM              = 0x04,
        INVENTORY_NEW_GAME_ITEM_M93R              = 0x81,
        INVENTORY_NEW_GAME_ITEM_9MM_PB            = 0xA0,
        INVENTORY_NEW_GAME_ITEM_GRENADE           = 0xA9,
        INVENTORY_NEW_GAME_ITEM_BUCKSHOT          = 0xAC,
        INVENTORY_NEW_GAME_CARRIED_ROW_COUNT      = 20,
        INVENTORY_NEW_GAME_ENERGY_PAGE_COUNT      = 4,
        INVENTORY_NEW_GAME_ENERGY_COLUMN_COUNT    = 3,
        INVENTORY_NEW_GAME_STARTING_BP            = 200,
        INVENTORY_NEW_GAME_RECOVERY_ATTACHMENT    = 3,
        INVENTORY_NEW_GAME_STORED_BUCKSHOT_ROUNDS = 20,
        INVENTORY_NEW_GAME_STORED_GRENADES        = 8
    };
    InventoryItemRow*          itemRow;
    McSaveData*                save;
    InventoryItemRange*        itemRange;
    InventoryItemRange* const* containerRanges;
    PlayerStatus*              playerStatus;
    s32                        stageAreaKey;
    s32                        rowIndex;
    s32                        pageIndex;
    s32                        columnIndex;

    // Reset saved rows and identification; attachment bytes in empty rows survive.
    for (rowIndex = 0, itemRow = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows; rowIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows); rowIndex++) {
        itemRow->itemId = INVENTORY_ITEM_NONE;
        itemRow->qty    = 0;
        itemRow++;
    }
    _itemInitializeNewGameIdentification();
    // Reset collected items and every weapon load before installing built-in supplies.
    inventoryClearCollectedBits();
    _equipmentInitializeNewGameWeaponLoads();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.firstRow = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.rowCount = INVENTORY_NEW_GAME_CARRIED_ROW_COUNT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.tableId  = INVENTORY_ITEM_TABLE_SAVED;
    // Only the four three-entry energy pages are reset; the remaining six bytes survive.
    for (pageIndex = 0; pageIndex < INVENTORY_NEW_GAME_ENERGY_PAGE_COUNT; pageIndex++) {
        for (columnIndex = 0; columnIndex < INVENTORY_NEW_GAME_ENERGY_COLUMN_COUNT; columnIndex++) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[columnIndex + pageIndex * INVENTORY_NEW_GAME_ENERGY_COLUMN_COUNT] = 0;
        }
    }
    save                        = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    itemRange                   = &save->state.carriedItems;
    save->state.attachLevels[0] = 1;
    playerStatus                = &gPlayerStatus;
    if (save->state.clearCount == 0) {
        playerStatus->bp = INVENTORY_NEW_GAME_STARTING_BP;
        _gpInitStartingItems(itemRange, playerStatus);
    } else {
        _gpInitStartingItems(itemRange, playerStatus);
    }
    GP_GIVE_LOADED(itemRange, INVENTORY_NEW_GAME_ITEM_M93R, INVENTORY_NEW_GAME_ITEM_9MM_PB);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_2, 1)->attachSlot = INVENTORY_NEW_GAME_RECOVERY_ATTACHMENT;
    // Seed separate container ranges; repeated grants preserve their original row behavior.
    containerRanges = Gp_ScanPtrs;
    itemRange       = containerRanges[1];
    inventoryClearItems(itemRange);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_1, 1);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_1, 1);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_STIM, 1);
    itemRange = containerRanges[2];
    inventoryClearItems(itemRange);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_1, 1);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_1, 1);
    inventoryClearItems(containerRanges[3]);
    itemRange = containerRanges[4];
    inventoryClearItems(itemRange);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_9MM_PB, INVENTORY_GIVE_ONE_PACK);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_STIM, 1);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_STIM, 1);
    inventoryClearItems(containerRanges[6]);
    inventoryClearItems(containerRanges[5]);
    itemRange = containerRanges[8];
    inventoryClearItems(itemRange);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_BUCKSHOT, INVENTORY_NEW_GAME_STORED_BUCKSHOT_ROUNDS);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_GRENADE, INVENTORY_NEW_GAME_STORED_GRENADES);
    inventorySetCollectedBit(INVENTORY_COLLECTION_ID_MIST_BADGE);
    stageAreaKey  = GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
    stageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
    if (stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0, 0)) {
        inventoryInitializeShootingGalleryLoadout();
        inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, INVENTORY_NEW_GAME_ITEM_M93R, 1);
        equipmentEquipCarriedWeapon(INVENTORY_NEW_GAME_ITEM_M93R);
    }
}

/* Item table a scan window lies in. */
static inline InventoryItemRow* _gpScanTable(const InventoryItemRange* scan)
{
    InventoryItemRow* table;

    switch (scan->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            table = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            table = Gp_ItemTable1;
            break;
        default:
            table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    return table;
}

void inventoryMoveItemRow(const InventoryItemRange* range, s32 sourceIndex, s32 destinationIndex)
{
    InventoryItemRow* rows;
    InventoryItemRow  movedRow;
    s32               holeIndex;

    rows = _gpScanTable(range);
    if (sourceIndex == destinationIndex) {
        return;
    }

    // Convert range-relative slots to absolute table indices.
    sourceIndex      += range->firstRow;
    destinationIndex += range->firstRow;

    if (sourceIndex < destinationIndex) {
        INVENTORY_TAKE_MOVED_ROW(rows, sourceIndex, movedRow);
        // The nearest free row bounds the occupied run that needs shifting.
        for (holeIndex = destinationIndex; sourceIndex < holeIndex; holeIndex--) {
            if (rows[holeIndex].itemId == INVENTORY_ITEM_NONE) {
                break;
            }
        }
        for (; holeIndex < destinationIndex; holeIndex++) {
            rows[holeIndex] = rows[holeIndex + 1];
        }
    } else {
        INVENTORY_TAKE_MOVED_ROW(rows, sourceIndex, movedRow);
        for (holeIndex = destinationIndex; holeIndex < sourceIndex; holeIndex++) {
            if (rows[holeIndex].itemId == INVENTORY_ITEM_NONE) {
                break;
            }
        }
        for (; destinationIndex < holeIndex; holeIndex--) {
            rows[holeIndex] = rows[holeIndex - 1];
        }
    }
    rows[destinationIndex] = movedRow;
}

#undef INVENTORY_TAKE_MOVED_ROW
