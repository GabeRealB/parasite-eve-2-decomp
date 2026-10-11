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

/// High-nibble catalogue groups; low-nibble subtypes depend on the group.
enum {
    ITEM_DESCRIPTOR_CATEGORY_ARMOR      = 0x10,
    ITEM_DESCRIPTOR_CATEGORY_WEAPON     = 0x20,
    ITEM_DESCRIPTOR_CATEGORY_AMMUNITION = 0x30
};

/// Ammunition subtype of the built-in M4A1 Pyke fuel supply.
enum { ITEM_DESCRIPTOR_AMMUNITION_FUEL = 9 };

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
/// Six empty catalogue text fields shared by empty and unused descriptor rows.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemEmptyTextFields[] =
    "\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Recovery1.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemRecovery1TextFields[] =
    "Recovery1\n"
    "Multi-vitamin tablet.\n"
    "Heals some HP.\n"
    "Tablet\n"
    "A tablet.\n"
    "\n";

/// Identified and unidentified catalogue text for Recovery2.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemRecovery2TextFields[] =
    "Recovery2\n"
    "Analgesic capsule.\n"
    "Heals HP.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Recovery3.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemRecovery3TextFields[] =
    "Recovery3\n"
    "Nutritive tonic.\n"
    "Heals lots of HP.\n"
    "Bottle\n"
    "A medicine bottle.\n"
    "\n";

/// Identified and unidentified catalogue text for Stim.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemStimTextFields[] =
    "Stim\n"
    "Restores nerve function. Cures\n"
    "Silence, Berserker, Confusion.\n"
    "Ampoule\n"
    "Some sort of ampoule.\n"
    "\n";

/// Identified and unidentified catalogue text for Cola.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemColaTextFields[] =
    "Cola\n"
    "Canned cola.\n"
    "Restores HP and MP.\n"
    "Can\n"
    "A tasty beverage in a can.\n"
    "\n";

/// Identified and unidentified catalogue text for MP Boost1.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemMpBoost1TextFields[] =
    "MP Boost1\n"
    "Spring water.\n"
    "Restores some MP.\n"
    "Plastic Bottle\n"
    "A bottle filled with water.\n"
    "\n";

/// Identified and unidentified catalogue text for MP Boost2.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemMpBoost2TextFields[] =
    "MP Boost2\n"
    "Carbonated mineral water.\n"
    "Restores lots of MP.\n"
    "Bottle\n"
    "A bottle filled with water.\n"
    "\n";

/// Identified and unidentified catalogue text for Penicillin.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemPenicillinTextFields[] =
    "Penicillin\n"
    "Anti-bacterial medkit.\n"
    "Cures Darkness, Paralysis, Poison.\n"
    "Medkit\n"
    "A multi-purpose medkit.\n"
    "\n";

/// Identified and unidentified catalogue text for SMG Clip Holder.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemSmgClipHolderTextFields[] =
    "SMG Clip Holder\n"
    "MP5A5 magazine clip holder.\n"
    "Increases ammo capacity by 30.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Rifle Clip Holder.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemRifleClipHolderTextFields[] =
    "Rifle Clip Holder\n"
    "M4A1 magazine clip holder.\n"
    "Increases ammo capacity by 30.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Lipstick.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemLipstickTextFields[] =
    "Lipstick\n"
    "Moisturizing lip balm.\n"
    "Attach to prevent Silence.\n"
    "Cylinder\n"
    "A small metallic cylinder.\n"
    "\n";

/// Identified and unidentified catalogue text for Snail Magazine.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemSnailMagazineTextFields[] =
    "Snail Magazine\n"
    "P08 Snail magazine.\n"
    "Increases ammo capacity by 25.\n"
    "Magazine\n"
    "A snail-shaped ammo magazine.\n"
    "\n";

/// Identified and unidentified catalogue text for Belt Pouch.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemBeltPouchTextFields[] =
    "Belt Pouch\n"
    "A large belt pack.\n"
    "Increases # of armor attachments.\n"
    "Pouch\n"
    "A clip-on pouch.\n"
    "\n";

/// Identified and unidentified catalogue text for MD Player.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemMdPlayerTextFields[] =
    "MD Player\n"
    "MD player with \"whale songs\" disc.\n"
    "Prevents Confusion, Berserker.\n"
    "MD Player\n"
    "A portable music player.\n"
    "\n";

/// Identified and unidentified catalogue text for Pyrokinesis level 1.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemPyrokinesisLevel1TextFields[] =
    "Pyrokinesis\n"
    "Fires a burst\n"
    "of flame forward.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Pyrokinesis level 2.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemPyrokinesisLevel2TextFields[] =
    "Pyrokinesis\n"
    "Fires two bursts.\n"
    "Better power & range.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Pyrokinesis level 3.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemPyrokinesisLevel3TextFields[] =
    "Pyrokinesis\n"
    "Fires multiple bursts.\n"
    "Best power & range.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Combustion level 1.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemCombustionLevel1TextFields[] =
    "Combustion\n"
    "Immolates enemy.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Combustion level 2.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemCombustionLevel2TextFields[] =
    "Combustion\n"
    "Immolates enemy.\n"
    "Better power & range.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Combustion level 3.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemCombustionLevel3TextFields[] =
    "Combustion\n"
    "Immolates enemy.\n"
    "Best power & range.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Inferno level 1.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemInfernoLevel1TextFields[] =
    "Inferno\n"
    "Generates a small,\n"
    "localized explosion.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Inferno level 2.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemInfernoLevel2TextFields[] =
    "Inferno\n"
    "Generates a medium,\n"
    "localized explosion.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Inferno level 3.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemInfernoLevel3TextFields[] =
    "Inferno\n"
    "Generates a large,\n"
    "local area explosion.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Necrosis level 1.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemNecrosisLevel1TextFields[] =
    "Necrosis\n"
    "Discharges electricity.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Necrosis level 2.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemNecrosisLevel2TextFields[] =
    "Necrosis\n"
    "Discharges electricity.\n"
    "Better power & range.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Necrosis level 3.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemNecrosisLevel3TextFields[] =
    "Necrosis\n"
    "Discharges electricity.\n"
    "Best power & range.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Plasma level 1.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemPlasmaLevel1TextFields[] =
    "Plasma\n"
    "Knocks down targets\n"
    "w/ a small blast wave.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Plasma level 2.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemPlasmaLevel2TextFields[] =
    "Plasma\n"
    "Smashes down targets\n"
    "w/ a small blast wave.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Plasma level 3.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemPlasmaLevel3TextFields[] =
    "Plasma\n"
    "Flattens all targets\n"
    "in the surrounding area.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Apobiosis level 1.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemApobiosisLevel1TextFields[] =
    "Apobiosis\n"
    "Temporarily paralyzes\n"
    "adjacent enemies.\n"
    "\n"
    "\n"
    "\n";

/// Identified and unidentified catalogue text for Apobiosis level 2.
///
/// Name and two description lines per form, identified first; final NUL.
/// Read-only storage for the gameplay image's lifetime.
static const u8 _gItemApobiosisLevel2TextFields[] =
    "Apobiosis\n"
    "Causes acute seizures\n"
    "in surrounding targets.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Apobiosis level 3.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemApobiosisLevel3TextFields[] =
    "Apobiosis\n"
    "Paralyzes all enemies\n"
    "in the surrounding area.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Metabolism level 1.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMetabolismLevel1TextFields[] =
    "Metabolism\n"
    "Restores normal\n"
    "status over time.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Metabolism level 2.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMetabolismLevel2TextFields[] =
    "Metabolism\n"
    "Rapidly restores\n"
    "normal status.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Metabolism level 3.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMetabolismLevel3TextFields[] =
    "Metabolism\n"
    "Instantly restores\n"
    "normal status.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Healing level 1.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemHealingLevel1TextFields[] =
    "Healing\n"
    "Heals a little HP.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Healing level 2.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemHealingLevel2TextFields[] =
    "Healing\n"
    "Heals some HP.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Healing level 3.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemHealingLevel3TextFields[] =
    "Healing\n"
    "Heals a lot of HP.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Lifedrain level 1.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemLifedrainLevel1TextFields[] =
    "Lifedrain\n"
    "Drains HP from\n"
    "adjacent targets.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Lifedrain level 2.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemLifedrainLevel2TextFields[] =
    "Lifedrain\n"
    "Drains HP from\n"
    "nearby targets.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Lifedrain level 3.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemLifedrainLevel3TextFields[] =
    "Lifedrain\n"
    "Drains HP from all\n"
    "surrounding targets.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Antibody level 1.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemAntibodyLevel1TextFields[] =
    "Antibody\n"
    "Reduces physical\n"
    "damage.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Antibody level 2.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemAntibodyLevel2TextFields[] =
    "Antibody\n"
    "Wards off physical\n"
    "damage.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Antibody level 3.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemAntibodyLevel3TextFields[] =
    "Antibody\n"
    "Greatly reduces\n"
    "physical damage.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Energyshot level 1.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemEnergyshotLevel1TextFields[] =
    "Energyshot\n"
    "Raises bullets' kinetic\n"
    "energy & penetration.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Energyshot level 2.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemEnergyshotLevel2TextFields[] =
    "Energyshot\n"
    "Raises bullets' kinetic\n"
    "energy & impact.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Energyshot level 3.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemEnergyshotLevel3TextFields[] =
    "Energyshot\n"
    "Raises bullets' kinetic\n"
    "energy & killing power.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Energyball level 1.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemEnergyballLevel1TextFields[] =
    "Energyball\n"
    "Creates a circling orb\n"
    "that repels attacks.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Energyball level 2.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemEnergyballLevel2TextFields[] =
    "Energyball\n"
    "Creates 2 circling orbs\n"
    "that repel attacks.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Energyball level 3.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemEnergyballLevel3TextFields[] =
    "Energyball\n"
    "Creates a ring of orbs\n"
    "that attack targets.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Skull Crystal.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemSkullCrystalTextFields[] =
    "Skull Crystal\n"
    "Skull-shaped S. American crystal.\n"
    "Attach this and see what happens.\n"
    "Crystal\n"
    "A carved crystal.\n"
    "\n";

/// Catalogue name and descriptions for Medicine Wheel.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMedicineWheelTextFields[] =
    "Medicine Wheel\n"
    "Native American wall ornament.\n"
    "Attach this and see what happens.\n"
    "Woven Ring\n"
    "A hand-made wall ornament.\n"
    "\n";

/// Catalogue name and descriptions for Holy Water.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemHolyWaterTextFields[] =
    "Holy Water\n"
    "Small bottle labeled \"Holy Water.\"\n"
    "Attach this and see what happens.\n"
    "Bottle\n"
    "A small bottle filled with liquid.\n"
    "\n";

/// Catalogue name and descriptions for Ofuda.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemOfudaTextFields[] =
    "Ofuda\n"
    "Charm with Japanese lettering.\n"
    "Attach this and see what happens.\n"
    "Charm\n"
    "There's some Japanese on this.\n"
    "\n";

/// Catalogue name and descriptions for Flare.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemFlareTextFields[] =
    "Flare\n"
    "Disposable self-defense weapon.\n"
    "Use to blind surrounding enemies.\n"
    "Small Box\n"
    "A small metal box.\n"
    "\n";

/// Catalogue name and descriptions for Pepper Spray.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemPepperSprayTextFields[] =
    "Pepper Spray\n"
    "Disposable self-defense weapon.\n"
    "Use to stun surrounding enemies.\n"
    "Spray Can\n"
    "A small spray can.\n"
    "\n";

/// Catalogue name and descriptions for Protein Capsule.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemProteinCapsuleTextFields[] =
    "Protein Capsule\n"
    "Protein compound medicine capsule.\n"
    "Full HP recovery.  Max HP +5.\n"
    "Capsule\n"
    "Some sort of capsule.\n"
    "\n";

/// Catalogue name and descriptions for Ringer's Solution.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemRingersSolutionTextFields[] =
    "Ringer's Solution\n"
    "Blood substitute.\n"
    "Restores HP and MP to max.\n"
    "Vinyl Pack\n"
    "A vinyl pack filled with liquid.\n"
    "\n";

/// Catalogue name and descriptions for Eau de Toilette.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemEauDeToiletteTextFields[] =
    "Eau de Toilette\n"
    "Disposable eau de toilette spray.\n"
    "Scent releases latent powers.\n"
    "Bottle\n"
    "A small bottle.\n"
    "\n";

/// Catalogue name and descriptions for Hunter Goggles.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemHunterGogglesTextFields[] =
    "Hunter Goggles\n"
    "Special filtered goggles.\n"
    "Attach to prevent blindness.\n"
    "Goggles\n"
    "These are colored goggles.\n"
    "\n";

/// Catalogue name and descriptions for GPS.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemGpsTextFields[] =
    "GPS\n"
    "Military global positioning system.\n"
    "Includes a motion detector.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Combat Light.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemCombatLightTextFields[] =
    "Combat Light\n"
    "Disposable defense weapon.\n"
    "Blinds enemies in front of you.\n"
    "Key Chain\n"
    "A light and a key chain in one!\n"
    "\n";

/// Catalogue name and descriptions for Hammer.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemHammerTextFields[] =
    "Hammer\n"
    "High-voltage stun gun.\n"
    "Custom M4A1 attachment.\n"
    "Stun Gun\n"
    "An electric prod.\n"
    "\n";

/// Catalogue name and descriptions for the Pyke M4A1 attachment.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemPykeTextFields[] =
    "Pyke\n"
    "Small portable flamethrower.\n"
    "Custom M4A1 attachment.\n"
    "Ignition Device\n"
    "Some sort of ignition device.\n"
    "\n";

/// Catalogue name and descriptions for the Javelin M4A1 attachment.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemJavelinTextFields[] =
    "Javelin\n"
    "Small laser gun.\n"
    "Custom M4A1 attachment.\n"
    "Laser Attachment\n"
    "Some sort of weapon attachment.\n"
    "\n";

/// Catalogue name and descriptions for the M203 M4A1 attachment.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM203TextFields[] =
    "M203\n"
    "40mm grenade launcher.\n"
    "Custom M4A1 attachment.\n"
    "Grenade Launcher\n"
    "A grenade launcher attachment.\n"
    "\n";

/// Catalogue name and descriptions for the M9 bayonet.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM9TextFields[] =
    "M9\n"
    "Close-combat bayonet.\n"
    "Custom M4A1 attachment.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Leather Jacket.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemLeatherJacketTextFields[] =
    "Leather Jacket\n"
    "Aya's favorite leather jacket.\n"
    "Decent armor with special lining.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Tactical Armor.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemTacticalArmorTextFields[] =
    "Tactical Armor\n"
    "Modern all-terrain combat armor.\n"
    "Includes heads-up motion detector.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Combat Armor.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemCombatArmorTextFields[] =
    "Combat Armor\n"
    "Heavy police body armor.\n"
    "Helmet amplifier prevents Silence.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Assault Suit.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemAssaultSuitTextFields[] =
    "Assault Suit\n"
    "Good desert jacket. Has\n"
    "attachments, but poor protection.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for PASGT Vest.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemPasgtVestTextFields[] =
    "PASGT Vest\n"
    "Body armor in current military use.\n"
    "Includes an emergency first aid kit.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Tactical Vest.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemTacticalVestTextFields[] =
    "Tactical Vest\n"
    "Bulletproof SWAT vest.\n"
    "Includes an emergency first aid kit.\n"
    "Body Armor\n"
    "A bulletproof SWAT vest.\n"
    "\n";

/// Catalogue name and descriptions for EOD Suit.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemEodSuitTextFields[] =
    "EOD Suit\n"
    "Explosives squad body armor.\n"
    "Resists explosions and heat.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Turtle Vest.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemTurtleVestTextFields[] =
    "Turtle Vest\n"
    "NMC Hunter reinforced vest.\n"
    "Can be worn under regular clothing.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Chicken Plate.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemChickenPlateTextFields[] =
    "Chicken Plate\n"
    "Flak jacket worn by bomber pilots.\n"
    "Ceramic vest plates absorb impact.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for NBC Suit.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemNbcSuitTextFields[] =
    "NBC Suit\n"
    "Environmental NBC protection suit.\n"
    "Suitable for long-term wear.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for PsySuit.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemPsySuitTextFields[] =
    "PsySuit\n"
    "Neo-mitochondria labsuit.\n"
    "Includes remote ANMC analyzer.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Aya Special.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemAyaSpecialTextFields[] =
    "Aya Special\n"
    "Aya's custom-made armored vest.\n"
    "Outfitted for anti-NMC operations.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Shoulder Holster.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemShoulderHolsterTextFields[] =
    "Shoulder Holster\n"
    "Holster worn over a T-shirt.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Monk Robe.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMonkRobeTextFields[] =
    "Monk Robe\n"
    "Mitochondria worshipper's mantle.\n"
    "NMC blood stains boost PE.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for P08 with a Snail Magazine.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemP08SnailMagazineTextFields[] =
    "P08(S. Magazine)\n"
    "P08 modified w/ a snail magazine.\n"
    "Increased ammo capacity.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M93R.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM93rTextFields[] =
    "M93R\n"
    "Semi-auto 9mm, fires 3-round\n"
    "bursts.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M950.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM950TextFields[] =
    "M950\n"
    "Full-auto 9mm w/ high ammo\n"
    "capacity.\n"
    "Large Handgun\n"
    "A full-auto handgun.\n"
    "\n";

/// Catalogue name and descriptions for P08.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemP08TextFields[] =
    "P08\n"
    "Semi-auto 9mm w/ special\n"
    "loader.\n"
    "Handgun\n"
    "A semi-auto handgun.\n"
    "\n";

/// Catalogue name and descriptions for P229.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemP229TextFields[] =
    "P229\n"
    "Semi-auto 9mm w/ silencer.\n"
    "Flashlight attachment.\n"
    "Handgun\n"
    "A semi-auto handgun.\n"
    "\n";

/// Catalogue name and descriptions for Mongoose.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMongooseTextFields[] =
    "Mongoose\n"
    "Large 44-caliber revolver.\n"
    "Slow reload, but it's worth it.\n"
    "Large Handgun\n"
    "A revolver.\n"
    "\n";

/// Catalogue name and descriptions for Grenade Pistol.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemGrenadePistolTextFields[] =
    "Grenade Pistol\n"
    "Pistol-sized 40mm\n"
    "grenade launcher.\n"
    "Grenade Launcher\n"
    "A grenade launcher.\n"
    "\n";

/// Catalogue name and descriptions for MM1.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMm1TextFields[] =
    "MM1\n"
    "40mm launcher w/ revolving\n"
    "magazine.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for PA3.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemPa3TextFields[] =
    "PA3\n"
    "Pump-action 12-gauge\n"
    "shotgun.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for SP12.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemSp12TextFields[] =
    "SP12\n"
    "12-gauge close-combat shotgun.\n"
    "Powerful, but cumbersome.\n"
    "Shotgun\n"
    "A shotgun.\n"
    "\n";

/// Catalogue name and descriptions for AS12.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemAs12TextFields[] =
    "AS12\n"
    "Rapid-fire 12-gauge\n"
    "combat shotgun.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M4A1 Rifle.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1RifleTextFields[] =
    "M4A1 Rifle\n"
    "5.56mm assault rifle.\n"
    "Customizable with attachments.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M249.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM249TextFields[] =
    "M249\n"
    "Portable light machine gun.\n"
    "Heavy firepower and a large clip.\n"
    "Light Machine Gun\n"
    "A light machine gun.\n"
    "\n";

/// Catalogue name and descriptions for Tonfa Baton.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemTonfaBatonTextFields[] =
    "Tonfa Baton\n"
    "Baton based on an Okinawan design.\n"
    "Masters can attack continuously.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M4A1(+1).
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1Plus1TextFields[] =
    "M4A1(+1)\n"
    "M4A1 assault rifle.\n"
    "Magazine holds 30 extra rounds.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M4A1(+2).
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1Plus2TextFields[] =
    "M4A1(+2)\n"
    "M4A1 assault rifle.\n"
    "Magazine holds 60 extra rounds.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Hypervelocity.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemHypervelocityTextFields[] =
    "Hypervelocity\n"
    "Magnetic railgun.\n"
    "Fires hypersonic rounds.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Gunblade.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemGunbladeTextFields[] =
    "Gunblade\n"
    "Ultrahigh frequency particle blade.\n"
    "Blade with shotgun attachment.\n"
    "Katana???\n"
    "This sword looks familiar...\n"
    "\n";

/// Catalogue name and descriptions for M4A1 Hammer.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1HammerTextFields[] =
    "M4A1 Hammer\n"
    "Assault rifle w/ heavy stun gun.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M4A1 Bayonet.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1BayonetTextFields[] =
    "M4A1 Bayonet\n"
    "Assault rifle w/ bayonet.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M4A1 Grenade.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1GrenadeTextFields[] =
    "M4A1 Grenade\n"
    "Assault rifle w/ 40mm\n"
    "grenade launcher.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M4A1 Pyke.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1PykeTextFields[] =
    "M4A1 Pyke\n"
    "Assault rifle w/ flamethrower.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for M4A1 Javelin.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1JavelinTextFields[] =
    "M4A1 Javelin\n"
    "Rifle w/ laser gun attached.\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for MP5A5.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMp5a5TextFields[] =
    "MP5A5\n"
    "9mm submachine gun.\n"
    "Flashlight attached.\n"
    "Submachine Gun\n"
    "A submachine gun.\n"
    "\n";

/// Catalogue name and descriptions for MP5A5(+1).
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMp5a5Plus1TextFields[] =
    "MP5A5(+1)\n"
    "Submachine gun MP5A5.\n"
    "Magazine holds 30 extra rounds.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for MP5A5(+2).
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemMp5a5Plus2TextFields[] =
    "MP5A5(+2)\n"
    "Submachine gun MP5A5.\n"
    "Magazine holds 60 extra rounds.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for 9mm P.B..
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first; the unidentified form is empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItem9mmPbTextFields[] =
    "9mm P.B.\n"
    "9mm caliber full metal jacket round.\n"
    "For handguns and submachine guns.\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for 9mm Hydra.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItem9mmHydraTextFields[] =
    "9mm Hydra\n"
    "9mm hollow-point round.\n"
    "Soft tip increases tissue damage.\n"
    "9mm\n"
    "Handgun ammunition.\n"
    "\n";

/// Catalogue name and descriptions for 9mm Spartan.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItem9mmSpartanTextFields[] =
    "9mm Spartan\n"
    "9mm fragmentation round.\n"
    "Fragments rupture vital organs.\n"
    "9mm\n"
    "Handgun ammunition.\n"
    "\n";

/// Catalogue name and descriptions for 44 Magnum.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItem44MagnumTextFields[] =
    "44 Magnum\n"
    "44 caliber magnum round.\n"
    "Impressive destructive power.\n"
    "Magnum Round\n"
    "Handgun ammunition.\n"
    "\n";

/// Catalogue name and descriptions for 44 Maeda SP.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItem44MaedaSpTextFields[] =
    "44 Maeda SP\n"
    "Anti-NMC 44 magnum round.\n"
    "Contains toxic mitochondria.\n"
    "Magnum Round\n"
    "Handgun ammunition.\n"
    "\n";

/// Catalogue name for 44 Poison.
///
/// Six newline-delimited fields; only the identified name is populated.
/// Both descriptions and the unidentified form are empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItem44PoisonTextFields[] =
    "44 Poison\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and descriptions for Grenade.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemGrenadeRoundTextFields[] =
    "Grenade\n"
    "40mm fragmentation grenade.\n"
    "Shoots fragments over a wide area.\n"
    "Grenade Round\n"
    "Grenade launcher ammunition.\n"
    "\n";

/// Catalogue name and descriptions for Airburst.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemAirburstTextFields[] =
    "Airburst\n"
    "40mm aerial burst grenade.\n"
    "Arcs and explodes in the air.\n"
    "Grenade Round\n"
    "Grenade launcher ammunition.\n"
    "\n";

/// Catalogue name and descriptions for Riot.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemRiotTextFields[] =
    "Riot\n"
    "40mm special acoustic round.\n"
    "Emits a glaring flash and loud noise.\n"
    "Grenade Round\n"
    "Grenade launcher ammunition.\n"
    "\n";

/// Catalogue name and descriptions for Buckshot.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemBuckshotTextFields[] =
    "Buckshot\n"
    "12-gauge shotgun scatter shot.\n"
    "Spreads 9 lead shots in a burst.\n"
    "Shotgun Shell\n"
    "Shotgun ammunition.\n"
    "\n";

/// Catalogue name and descriptions for Firefly.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemFireflyTextFields[] =
    "Firefly\n"
    "12-gauge shotgun incendiary shot.\n"
    "Ignites flammable target.\n"
    "Shotgun Shell\n"
    "Shotgun ammunition.\n"
    "\n";

/// Catalogue name and descriptions for R. Slug.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemRSlugTextFields[] =
    "R. Slug\n"
    "12-gauge shotgun solid shot.\n"
    "Powerful shot with good accuracy.\n"
    "Shotgun Shell\n"
    "Shotgun ammunition.\n"
    "\n";

/// Catalogue name and descriptions for 5.56 Rifle.
///
/// Six newline-delimited fields: name and two description lines per form.
/// Identified text comes first, followed by the unidentified form.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItem556RifleRoundTextFields[] =
    "5.56 Rifle\n"
    "5.56mm full metal jacket round.\n"
    "For rifles and light machine guns.\n"
    "Rifle Round\n"
    "Rifle ammunition.\n"
    "\n";

/// Catalogue name for Paralizer.
///
/// Six newline-delimited fields; only the identified name is populated.
/// Both descriptions and the unidentified form are empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemParalizerTextFields[] =
    "Paralizer\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name for Flashbomb.
///
/// Six newline-delimited fields; only the identified name is populated.
/// Both descriptions and the unidentified form are empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemFlashbombTextFields[] =
    "Flashbomb\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name for the P229 and MP5A5 variants' built-in battery.
///
/// Six newline-delimited fields; only the identified name is populated.
/// Both descriptions and the unidentified form are empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemP229Mp5a5BatteryTextFields[] =
    "Battery\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name for the Hypervelocity built-in battery.
///
/// Six newline-delimited fields; only the identified name is populated.
/// Both descriptions and the unidentified form are empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemHypervelocityBatteryTextFields[] =
    "Battery\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name for the M4A1 Hammer built-in battery.
///
/// Six newline-delimited fields; only the identified name is populated.
/// Both descriptions and the unidentified form are empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1HammerBatteryTextFields[] =
    "Battery\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name for the M4A1 Pyke built-in fuel supply.
///
/// Six newline-delimited fields; only the identified name is populated.
/// Both descriptions and the unidentified form are empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1PykeFuelTextFields[] =
    "Fuel\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name for the M4A1 Javelin built-in battery.
///
/// Six newline-delimited fields; only the identified name is populated.
/// Both descriptions and the unidentified form are empty.
/// The final NUL ends the stream. Storage lives with the gameplay image.
static const u8 _gItemM4a1JavelinBatteryTextFields[] =
    "Battery\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n";

/// Catalogue name and description fields for Parthenon Key.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemParthenonKeyTextFields[] =
    "Parthenon Key\n"
    "It opened the cafeteria...\n"
    "\n"
    "Cafeteria Key\n"
    "Cafeteria key from SWAT officer.\n"
    "\n";
/// Catalogue name and description fields for Micro Device.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemMicroDeviceTextFields[] =
    "Micro Device\n"
    "Pierce's modified NMC implant.\n"
    "Displays my position on a monitor.\n"
    "Metallic Implant\n"
    "Object implanted in NMC's skull.\n"
    "\n";
/// Catalogue name and description fields for Red Key.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemRedKeyTextFields[] =
    "Red Key\n"
    "Key on a skull-shaped key chain.\n"
    "\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Blue Key.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemBlueKeyTextFields[] =
    "Blue Key\n"
    "Key on a feather key chain.\n"
    "\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Armory Cardkey.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemArmoryCardkeyTextFields[] =
    "Armory Cardkey\n"
    "Key to the shelter armory.\n"
    "That man stored weapons here.\n"
    "Black Card\n"
    "Card dropped by the SWAT imitator.\n"
    "\n";
/// Catalogue name and description fields for MIST Badge.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemMistBadgeTextFields[] =
    "MIST Badge\n"
    "MIST ID card.\n"
    "Looks a lot like an FBI badge.\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Mendel (Sept. issue).
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemMendelJournalTextFields[] =
    "Mendel (Sept. issue)\n"
    "This month's feature: Mitochondria.\n"
    "A popular topic these days.\n"
    "Scientific Journal\n"
    "Mendel-- popular science journal.\n"
    "\n";
/// Catalogue name and description fields for Clipboard.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemClipboardTextFields[] =
    "Clipboard\n"
    "Agent Baldwin always carries this.\n"
    "\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for MIST Search Warrant.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemMistSearchWarrantTextFields[] =
    "MIST Search Warrant\n"
    "Authorizes any search in Dryfield.\n"
    "Includes private property searches.\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for NMC Photo.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemNmcPhotoTextFields[] =
    "NMC Photo\n"
    "So this was a picture of an NMC.\n"
    "\n"
    "UMA Photograph\n"
    "Looks a little big for a cougar...\n"
    "\n";
/// Catalogue name and description fields for Manual.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemMistManualTextFields[] =
    "Manual\n"
    "MIST civilian disinformation manual.\n"
    "\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Dryfield Map.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemDryfieldMapTextFields[] =
    "Dryfield Map\n"
    "Map of part of the Mojave Desert.\n"
    "There's only one town on the road.\n"
    "Map\n"
    "Map of Dryfield.\n"
    "\n";
/// Catalogue name and description fields for Motel Key No.6.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemMotelRoom6KeyTextFields[] =
    "Motel Key No.6\n"
    "Motel room key from Mr. Douglas.\n"
    "The tag says \"6.\"\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Saloon Key.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemSaloonKeyTextFields[] =
    "Saloon Key\n"
    "Key to \"Gene & Roy's\" saloon.\n"
    "\n"
    "Key Taken from Corpse\n"
    "It's engraved, \"G&R.\"\n"
    "\n";
/// Catalogue name and description fields for Monkey Wrench.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemMonkeyWrenchTextFields[] =
    "Monkey Wrench\n"
    "Adjustable wrench.\n"
    "\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Lobby Key.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemLobbyKeyTextFields[] =
    "Lobby Key\n"
    "Lobby key from Mr. Douglas.\n"
    "\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Bronco Masterkey.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemBroncoMasterkeyTextFields[] =
    "Bronco Masterkey\n"
    "Should open all the motel rooms.\n"
    "\n"
    "Key Found in the Lobby\n"
    "The tag says \"Bronco.\"\n"
    "\n";
/// Catalogue name and description fields for Wire Rope.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemWireRopeTextFields[] =
    "Wire Rope\n"
    "A sturdy rope.\n"
    "This should support my weight.\n"
    "Rope\n"
    "This could be useful.\n"
    "\n";
/// Catalogue name and description fields for Factory Key.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemFactoryKeyTextFields[] =
    "Factory Key\n"
    "Key to Mr. Douglas's garage.\n"
    "This key is a real antique!\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Truck Key.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemTruckKeyTextFields[] =
    "Truck Key\n"
    "Truck key from Mr. Douglas.\n"
    "This is my ticket to the shelter.\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Jerry Can.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemJerryCanTextFields[] =
    "Jerry Can\n"
    "It's empty.\n"
    "\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Gasoline.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemGasolineTextFields[] =
    "Gasoline\n"
    "Gasoline (in the gas can).\n"
    "This should get the truck running.\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Ice Bag.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemIceBagTextFields[] =
    "Ice Bag\n"
    "Bag filled with ice.\n"
    "\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Bag of Water.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemBagOfWaterTextFields[] =
    "Bag of Water\n"
    "All the ice melted.\n"
    "No use for this anymore.\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Bottlecap Magnet.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemBottlecapMagnetTextFields[] =
    "Bottlecap Magnet\n"
    "Magnet shaped like a cola cap.\n"
    "The back is magnetized.\n"
    "Magnet\n"
    "I used to collect these in school.\n"
    "\n";
/// Catalogue name and description fields for SUV Key.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemSuvKeyTextFields[] =
    "SUV Key\n"
    "Key to Pierce's SUV.\n"
    "It's on a cute key chain.\n"
    "\n"
    "\n"
    "\n";
/// Catalogue name and description fields for Oak Board.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemOakBoardTextFields[] =
    "Oak Board\n"
    "There are footprints on it.\n"
    "\n"
    "Board\n"
    "A sturdy-looking board.\n"
    "\n";
/// Catalogue name and description fields for Jumper Plug.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemJumperPlugTextFields[] =
    "Jumper Plug\n"
    "Electrical switchboard component.\n"
    "\n"
    "Plug\n"
    "Looks like a headphone jack...\n"
    "\n";
/// Catalogue name and description fields for Bowman's Card.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemBowmanCardTextFields[] =
    "Bowman's Card\n"
    "This belonged to a researcher.\n"
    "\n"
    "Card\n"
    "A plastic card.\n"
    "\n";
/// Catalogue name and description fields for Yoshida's Card.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemYoshidaCardTextFields[] =
    "Yoshida's Card\n"
    "This belonged to a programmer.\n"
    "\n"
    "Card\n"
    "A plastic card.\n"
    "\n";
/// Catalogue name and description fields for Electric Car Key.
///
/// Identified name and two description lines, then the unidentified form.
/// Six newline-delimited fields and a final NUL; borrowed while gameplay is loaded.
static const u8 _gItemElectricCarKeyTextFields[] =
    "Electric Car Key\n"
    "Key to the shelter electric car.\n"
    "\n"
    "Car Key\n"
    "Looks like a car key...\n"
    "\n";
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

ItemDesc gItemDescriptors[ITEM_DESCRIPTOR_COUNT] = {
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 100, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemRecovery1TextFields },
    { 180, ITEM_SUBTYPE_MEDICINE, 0x8, _gItemRecovery2TextFields },
    { 350, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemRecovery3TextFields },
    { 80, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemStimTextFields },
    { 120, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemColaTextFields },
    { 320, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemMpBoost1TextFields },
    { 580, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemMpBoost2TextFields },
    { 80, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemPenicillinTextFields },
    { 3980, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemSmgClipHolderTextFields },
    { 1800, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemRifleClipHolderTextFields },
    { 5000, 0, 0x00, _gItemLipstickTextFields },
    { 1000, 0, ITEM_FLAG_NO_ATTACHMENT, _gItemSnailMagazineTextFields },
    { 10000, 0, ITEM_FLAG_NO_ATTACHMENT, _gItemBeltPouchTextFields },
    { 1000, 0, 0x00, _gItemMdPlayerTextFields },
    { 500, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemPyrokinesisLevel1TextFields },
    { 1250, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemPyrokinesisLevel2TextFields },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemPyrokinesisLevel3TextFields },
    { 750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemCombustionLevel1TextFields },
    { 1750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemCombustionLevel2TextFields },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemCombustionLevel3TextFields },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemInfernoLevel1TextFields },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemInfernoLevel2TextFields },
    { 5000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemInfernoLevel3TextFields },
    { 500, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemNecrosisLevel1TextFields },
    { 1250, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemNecrosisLevel2TextFields },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemNecrosisLevel3TextFields },
    { 750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemPlasmaLevel1TextFields },
    { 1750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemPlasmaLevel2TextFields },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemPlasmaLevel3TextFields },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemApobiosisLevel1TextFields },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemApobiosisLevel2TextFields },
    { 5000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemApobiosisLevel3TextFields },
    { 500, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemMetabolismLevel1TextFields },
    { 1250, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemMetabolismLevel2TextFields },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemMetabolismLevel3TextFields },
    { 750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemHealingLevel1TextFields },
    { 1750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemHealingLevel2TextFields },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemHealingLevel3TextFields },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemLifedrainLevel1TextFields },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemLifedrainLevel2TextFields },
    { 5000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemLifedrainLevel3TextFields },
    { 500, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemAntibodyLevel1TextFields },
    { 1250, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemAntibodyLevel2TextFields },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemAntibodyLevel3TextFields },
    { 750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemEnergyshotLevel1TextFields },
    { 1750, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemEnergyshotLevel2TextFields },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemEnergyshotLevel3TextFields },
    { 3000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemEnergyballLevel1TextFields },
    { 4000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemEnergyballLevel2TextFields },
    { 5000, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemEnergyballLevel3TextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 5000, 0, 0x00, _gItemSkullCrystalTextFields },
    { 27800, 0, 0x00, _gItemMedicineWheelTextFields },
    { 5000, 0, 0x00, _gItemHolyWaterTextFields },
    { 5000, 0, 0x00, _gItemOfudaTextFields },
    { 150, 0, 0x00, _gItemFlareTextFields },
    { 100, 0, 0x00, _gItemPepperSprayTextFields },
    { 10000, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemProteinCapsuleTextFields },
    { 200, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemRingersSolutionTextFields },
    { 190, ITEM_SUBTYPE_MEDICINE, 0x00, _gItemEauDeToiletteTextFields },
    { 1000, 0, 0x00, _gItemHunterGogglesTextFields },
    { 1000, 0, ITEM_FLAG_NO_DISCARD | 0x8, _gItemGpsTextFields },
    { 60, 0, 0x00, _gItemCombatLightTextFields },
    { 3720, 0, ITEM_FLAG_NO_ATTACHMENT, _gItemHammerTextFields },
    { 5180, 0, ITEM_FLAG_NO_ATTACHMENT, _gItemPykeTextFields },
    { 7500, 0, ITEM_FLAG_NO_ATTACHMENT, _gItemJavelinTextFields },
    { 2130, 0, ITEM_FLAG_NO_ATTACHMENT, _gItemM203TextFields },
    { 980, 0, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemM9TextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 1000, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemLeatherJacketTextFields },
    { 12800, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemTacticalArmorTextFields },
    { 3250, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemCombatArmorTextFields },
    { 1000, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemAssaultSuitTextFields },
    { 2980, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemPasgtVestTextFields },
    { 2120, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT, _gItemTacticalVestTextFields },
    { 4580, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemEodSuitTextFields },
    { 1680, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemTurtleVestTextFields },
    { 1000, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemChickenPlateTextFields },
    { 3980, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemNbcSuitTextFields },
    { 4580, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemPsySuitTextFields },
    { 8000, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemAyaSpecialTextFields },
    { 2580, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemShoulderHolsterTextFields },
    { 3000, ITEM_DESCRIPTOR_CATEGORY_ARMOR, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemMonkRobeTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 1000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 1, 0x8, _gItemP08SnailMagazineTextFields },
    { 2000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 1, ITEM_FLAG_NO_DISCARD | 0x8, _gItemM93rTextFields },
    { 5750, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 2, 0x00, _gItemM950TextFields },
    { 680, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 1, 0x00, _gItemP08TextFields },
    { 1880, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 1, 0x00, _gItemP229TextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 2850, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 2, 0x00, _gItemMongooseTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 1680, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 4, 0x00, _gItemGrenadePistolTextFields },
    { 23500, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 4, 0x8, _gItemMm1TextFields },
    { 1000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 5, 0x8, _gItemPa3TextFields },
    { 3980, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 5, 0x00, _gItemSp12TextFields },
    { 12500, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 5, 0x8, _gItemAs12TextFields },
    { 2450, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 6, 0x8, _gItemM4a1RifleTextFields },
    { 15800, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 3, 0x00, _gItemM249TextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 1000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 7, ITEM_FLAG_NO_DISCARD | 0x8, _gItemTonfaBatonTextFields },
    { 5000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 6, 0x8, _gItemM4a1Plus1TextFields },
    { 7500, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 6, 0x8, _gItemM4a1Plus2TextFields },
    { 20000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 4, 0x8, _gItemHypervelocityTextFields },
    { 10000, ITEM_DESCRIPTOR_CATEGORY_WEAPON, 0x00, _gItemGunbladeTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 6000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 6, 0x8, _gItemM4a1HammerTextFields },
    { 4000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 6, 0x8, _gItemM4a1BayonetTextFields },
    { 4500, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 6, 0x8, _gItemM4a1GrenadeTextFields },
    { 8000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 6, 0x8, _gItemM4a1PykeTextFields },
    { 9000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 6, 0x8, _gItemM4a1JavelinTextFields },
    { 6980, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 3, 0x00, _gItemMp5a5TextFields },
    { 11000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 3, 0x8, _gItemMp5a5Plus1TextFields },
    { 15000, ITEM_DESCRIPTOR_CATEGORY_WEAPON | 3, 0x8, _gItemMp5a5Plus2TextFields },
    { 30, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_9MM, 0x8, _gItem9mmPbTextFields },
    { 50, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_9MM, 0x00, _gItem9mmHydraTextFields },
    { 80, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_9MM, 0x00, _gItem9mmSpartanTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 100, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_44_MAGNUM, 0x00, _gItem44MagnumTextFields },
    { 800, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_44_MAGNUM, 0x00, _gItem44MaedaSpTextFields },
    { 0, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_44_MAGNUM, 0x8, _gItem44PoisonTextFields },
    { 280, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_40MM, 0x00, _gItemGrenadeRoundTextFields },
    { 450, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_40MM, 0x00, _gItemAirburstTextFields },
    { 80, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_40MM, 0x00, _gItemRiotTextFields },
    { 60, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_12_GAUGE, 0x00, _gItemBuckshotTextFields },
    { 90, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_12_GAUGE, 0x00, _gItemFireflyTextFields },
    { 120, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_12_GAUGE, 0x00, _gItemRSlugTextFields },
    { 100, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_556MM, 0x00, _gItem556RifleRoundTextFields },
    { 0, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_556MM, 0x8, _gItemParalizerTextFields },
    { 0, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_556MM, 0x8, _gItemFlashbombTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_BATTERY, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemP229Mp5a5BatteryTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_BATTERY, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemHypervelocityBatteryTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_BATTERY, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemM4a1HammerBatteryTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_DESCRIPTOR_AMMUNITION_FUEL, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemM4a1PykeFuelTextFields },
    { 0, ITEM_DESCRIPTOR_CATEGORY_AMMUNITION | ITEM_AMMO_BATTERY, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemM4a1JavelinBatteryTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
};

ItemDesc Gp_KeyItemDescs[] = {
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 1000, 64, ITEM_FLAG_NO_ATTACHMENT, _gItemParthenonKeyTextFields },
    { 1000, 64, 0x00, _gItemMicroDeviceTextFields },
    { 1000, 64, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemRedKeyTextFields },
    { 1000, 64, 0x8, _gItemBlueKeyTextFields },
    { 20000, 64, 0x00, _gItemArmoryCardkeyTextFields },
    { 1000, 64, 0x8, _gItemMistBadgeTextFields },
    { 20000, 64, ITEM_FLAG_NO_ATTACHMENT, _gItemMendelJournalTextFields },
    { 1000, 64, ITEM_FLAG_NO_ATTACHMENT | 0x8, _gItemClipboardTextFields },
    { 1000, 64, 0x8, _gItemMistSearchWarrantTextFields },
    { 1000, 64, 0x00, _gItemNmcPhotoTextFields },
    { 1000, 64, 0x8, _gItemMistManualTextFields },
    { 1000, 64, 0x00, _gItemDryfieldMapTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 1000, 64, 0x8, _gItemMotelRoom6KeyTextFields },
    { 1000, 64, 0x00, _gItemSaloonKeyTextFields },
    { 1000, 64, 0x8, _gItemMonkeyWrenchTextFields },
    { 1000, 64, 0x8, _gItemLobbyKeyTextFields },
    { 1000, 64, 0x00, _gItemBroncoMasterkeyTextFields },
    { 1000, 64, 0x00, _gItemWireRopeTextFields },
    { 1000, 64, 0x8, _gItemFactoryKeyTextFields },
    { 1000, 64, 0x8, _gItemTruckKeyTextFields },
    { 1000, 64, 0x8, _gItemJerryCanTextFields },
    { 1000, 64, 0x8, _gItemGasolineTextFields },
    { 1000, 64, 0x8, _gItemIceBagTextFields },
    { 1000, 64, 0x8, _gItemBagOfWaterTextFields },
    { 1000, 64, ITEM_FLAG_NO_ATTACHMENT, _gItemBottlecapMagnetTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 20000, 64, 0x8, _gItemSuvKeyTextFields },
    { 1000, 64, 0x00, _gItemOakBoardTextFields },
    { 1000, 64, 0x00, _gItemJumperPlugTextFields },
    { 60000, 64, 0x00, _gItemBowmanCardTextFields },
    { 30000, 64, 0x00, _gItemYoshidaCardTextFields },
    { 1000, 64, 0x00, _gItemElectricCarKeyTextFields },
    { 20000, 64, 0x00, D_80096BC4 },
    { 50000, 64, 0x00, D_80096C0C },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 10000, 64, 0x00, D_80096C60 },
    { 20000, 64, 0x00, D_80096CE8 },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 1000, 64, 0x00, D_80096D44 },
    { 10000, 64, 0x8, D_80096D9C },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 60000, 64, 0x8, D_80096DF0 },
    { 60000, 64, 0x8, D_80096E0C },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
    { 0, 0, 0x00, _gItemEmptyTextFields },
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
/// no unknown name. Requires live saved storage and terminated catalogue text.
/// Raw ids 192..255 use the first 64 key-item descriptors through the contiguous
/// ordinary-table address; ids 256..383 use key rows 0..127 again. Those physical
/// aliases are retained, but their containing C declaration remains unproven.
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

    // Reset the whole saved storage, including words beyond the queried id range.
    for (identificationIndex = ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemSeenBits) - 1; identificationIndex >= 0; identificationIndex--) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemSeenBits[identificationIndex] = 0;
    }

    // Identify entries whose three known fields are followed by an empty unknown name.
    identificationIndex = 0;
    do {
        fieldsRemaining = ITEM_IDENTIFIED_TEXT_FIELD_COUNT;
        if (identificationIndex < ITEM_IDENTIFICATION_KEY_ITEM_FIRST) {
            descriptor = &gItemDescriptors[identificationIndex];
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
/// are installed at capacity. Resets all 32 live-save records for weapon item
/// IDs 0x80..0x9F, including each record's unexplained final word. Saved storage
/// and the supply/capacity tables must be live. Inventory rows remain intact.
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
            table = gInventoryBattleRewardRows;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            table = gInventoryIndirectRows;
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
