#include "items.h"

#include "types.h"

#include "attachments.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "gameplay/starter_inventory.h"

/* Count item `id` in saved rows 0..254 through a cleared range. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS, inventoryGetItemQuantity(&(scan), (id)))

#include "main/mc.h"
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
static inline InventoryItemRow* _gpScanTable(InventoryItemRange* scan);

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
#define GP_GIVE_LOADED(scan, weapon, ammo)           \
    do {                                             \
        inventoryGiveItem(scan, weapon, 1);          \
        Gp_EquipRelatedItem(scan, weapon, ammo, -1); \
    } while (0)

/* Clears the carried inventory, equips the starting armour, restores HP/MP,
 * and gives the initial supplies and their attachment slots. */
#define _gpInitStartingItems(scan, cfg)                      \
    do {                                                     \
        inventoryClearItems(scan);                           \
        inventoryGiveItem(scan, 0x60, 1);                    \
        Gp_EquipMod(0x60);                                   \
        (cfg)->hp = (cfg)->hpMax;                            \
        (cfg)->mp = (cfg)->mpMax;                            \
        inventoryGiveItem(scan, 0x92, 1);                    \
        inventoryGiveItem(scan, 0x40, 1)->attachSlot    = 1; \
        inventoryGiveItem(scan, 0xA0, 0x64)->attachSlot = 2; \
    } while (0)

void func_800B8014(void)
{
    InventoryItemRow*    rec;
    McSaveData*          save;
    const ItemDesc*      desc;
    const u8*            str;
    EquipmentWeaponLoad* slots;
    InventoryItemRange*  scan;
    InventoryItemRange** scans;
    PlayerStatus*        cfg;
    s32                  stageAreaKey;
    s32                  i;
    s32                  j;
    s32                  count;
    s32                  row;
    s32                  col;

    for (j = 0, rec = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows; j < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows); j++) {
        rec->itemId = INVENTORY_ITEM_NONE;
        rec->qty    = 0;
        rec++;
    }
    for (i = 0x5F; i >= 0; i--) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemSeenBits[i] = 0;
    }

    i = 0;
    do {
        count = 3;
        if (i < 0x100) {
            desc = &Gp_ItemDescs[i];
        } else {
            desc = &Gp_KeyItemDescs[(i)-0x100];
        }
        str = desc->textFields;
        while (count > 0) {
            if (*str == '\0' || *str == '\n') {
                count--;
            }
            str++;
        }
        if (*str == '\n') {
            itemSetIdentified(i, 1);
        }
        i++;
    } while (i < 0x180);
    inventoryClearCollectedBits();
    slots = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems;
    for (j = 0; j < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems); j++) {
        slots->primaryItemId   = INVENTORY_ITEM_NONE;
        slots->primaryQty      = 0;
        slots->secondaryItemId = EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE;
        slots->secondaryQty    = 0;
        // The M4A1 grenade launcher has a reloadable secondary slot.
        if (j == 0x9A - EQUIPMENT_WEAPON_ITEM_FIRST) {
            slots->secondaryItemId = INVENTORY_ITEM_NONE;
            slots->secondaryQty    = 0;
        }
        slots->field_4 = 0;
        slots++;
    }
    Gp_ApplyItemMap();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.firstRow = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.rowCount = 0x14;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.tableId  = INVENTORY_ITEM_TABLE_SAVED;
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 3; col++) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[col + row * 3] = 0;
        }
    }
    save                        = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    scan                        = &save->state.carriedItems;
    save->state.attachLevels[0] = 1;
    cfg                         = &gPlayerStatus;
    if (save->state.clearCount == 0) {
        cfg->bp = 0xC8;
        _gpInitStartingItems(scan, cfg);
    } else {
        _gpInitStartingItems(scan, cfg);
    }
    GP_GIVE_LOADED(scan, 0x81, 0xA0);
    inventoryGiveItem(scan, 2, 1)->attachSlot = 3;
    scans                                     = Gp_ScanPtrs;
    scan                                      = scans[1];
    inventoryClearItems(scan);
    inventoryGiveItem(scan, 1, 1);
    inventoryGiveItem(scan, 1, 1);
    inventoryGiveItem(scan, 4, 1);
    scan = scans[2];
    inventoryClearItems(scan);
    inventoryGiveItem(scan, 1, 1);
    inventoryGiveItem(scan, 1, 1);
    inventoryClearItems(scans[3]);
    scan = scans[4];
    inventoryClearItems(scan);
    inventoryGiveItem(scan, 0xA0, INVENTORY_GIVE_ONE_PACK);
    inventoryGiveItem(scan, 4, 1);
    inventoryGiveItem(scan, 4, 1);
    inventoryClearItems(scans[6]);
    inventoryClearItems(scans[5]);
    scan = scans[8];
    inventoryClearItems(scan);
    inventoryGiveItem(scan, 0xAC, 0x14);
    inventoryGiveItem(scan, 0xA9, 8);
    inventorySetCollectedBit(INVENTORY_COLLECTION_ID_MIST_BADGE);
    stageAreaKey  = GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
    stageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
    if (stageAreaKey == GAME_LOCATION_KEY(1, 0x14, 0, 0)) {
        Gp_ResetInventory();
        inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, 0x81, 1);
        equipmentEquipCarriedWeapon(0x81);
    }
}

/* Item table a scan window lies in. */
static inline InventoryItemRow* _gpScanTable(InventoryItemRange* scan)
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

void Gp_MoveItemSlot(InventoryItemRange* scan, s32 from, s32 to)
{
    InventoryItemRow* table;
    InventoryItemRow  saved;
    s32               i;

    table = _gpScanTable(scan);
    if (from == to) {
        return;
    }

    from += scan->firstRow;
    to   += scan->firstRow;

    if (from < to) {
        saved              = table[from];
        table[from].itemId = INVENTORY_ITEM_NONE;
        table[from].qty    = 0;
        for (i = to; from < i; i--) {
            if (table[i].itemId == INVENTORY_ITEM_NONE) {
                break;
            }
        }
        for (; i < to; i++) {
            table[i] = table[i + 1];
        }
    } else {
        saved              = table[from];
        table[from].itemId = INVENTORY_ITEM_NONE;
        table[from].qty    = 0;
        for (i = to; i < from; i++) {
            if (table[i].itemId == INVENTORY_ITEM_NONE) {
                break;
            }
        }
        for (; to < i; i--) {
            table[i] = table[i - 1];
        }
    }
    table[to] = saved;
}
