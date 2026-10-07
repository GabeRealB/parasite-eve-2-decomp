#ifndef MAIN_SOUND_IDS_H
#define MAIN_SOUND_IDS_H

#include "types.h"

#include "main/areas.h"

/// Sound request ids, as passed to the `SndEvt_Enqueue*` functions.
///
/// Bits 28..31 select the bank type, bits 16..27 the bank, bits 8..15 an
/// instance (often an enemy's place index, added at run time) and bits 0..7
/// the entry within the bank. An id with only the type set names every bank
/// of that type, for stopping or fading them together.
enum {
    SOUND_BANK_TYPE_COMMON    = 0, // The always-loaded bank: menus, prompts, the shop
    SOUND_BANK_TYPE_WEAPON    = 2, // A weapon's bank
    SOUND_BANK_TYPE_CHARACTER = 4, // One bank per enemy or character
    SOUND_BANK_TYPE_AREA      = 5, // A room's bank: stage in bits 8..11 of the bank, area in 0..7
};

#define SOUND_ID(type, bank, entry) ((s32)(((u32)(type) << 28) | ((u32)(bank) << 16) | (u32)(entry)))
/// An entry of a room's bank, given its stage and area.
#define SOUND_AREA(stage, area, entry) SOUND_ID(SOUND_BANK_TYPE_AREA, ((stage) << 8) | (area), (entry))
/// An entry of a character's bank.
#define SOUND_CHARACTER(bank, entry) SOUND_ID(SOUND_BANK_TYPE_CHARACTER, (bank), (entry))
/// An entry of a weapon's bank.
#define SOUND_WEAPON(bank, entry) SOUND_ID(SOUND_BANK_TYPE_WEAPON, (bank), (entry))
/// An entry of the always-loaded common bank.
#define SOUND_COMMON(entry) SOUND_ID(SOUND_BANK_TYPE_COMMON, 0, (entry))

/// Character and weapon banks, by the code that plays them.
enum {
    SOUND_BANK_ACTOR_311500  = 0x0A, // actor_311500 and actor_317000
    SOUND_BANK_ACTOR_356100  = 0x0D, // actor_356100 and actor_401300
    SOUND_BANK_ACTOR_800100  = 0x65, // companion actor_800100
    SOUND_BANK_ACTOR_800200  = 0x72, // companion actor_800200 and actor_136100
    SOUND_BANK_BRAHMAN       = 0x23, // Brahman
    SOUND_BANK_BURNER        = 0x1F, // Burner
    SOUND_BANK_DESERT_CHASER = 0x01, // Desert Chaser; also played by actor_00100 and actor_323400
    SOUND_BANK_GLUTTON       = 0x20, // Glutton
    SOUND_BANK_HYPERVELOCITY = 0x16, // Hypervelocity
    SOUND_BANK_M4A1_PYKE     = 0x1C, // M4A1 Pyke
    SOUND_BANK_MAD_CHASER    = 0x2C, // Mad Chaser
    SOUND_BANK_PLAYER        = 0x68, // the player and the companions
    SOUND_BANK_STRANGER      = 0x1D, // Stranger
    SOUND_BANK_SUCKLERCEPH   = 0x46, // Sucklerceph
};

/// Common bank.
enum {
    /// Played when the in-game menu root opens (unless spawned with arg 0x44).
    SOUND_MENU_OPEN = SOUND_COMMON(1),
    /// Menu cursor move: up/down/left/right in item, map, prompt and title menus.
    SOUND_MENU_CURSOR = SOUND_COMMON(2),
    /// Confirm/accept in the gameplay menus (load, exchange, move, yes).
    SOUND_MENU_CONFIRM = SOUND_COMMON(3),
    /// Cancel/back out of a gameplay menu list.
    SOUND_MENU_CANCEL = SOUND_COMMON(4),
    /// Played when the in-game menu root tears down and closes (unless arg 0x44/0x42).
    SOUND_MENU_CLOSE = SOUND_COMMON(5),
    /// Played at the player when an enemy lands a hit or takes hold of them.
    SOUND_PLAYER_STRUCK = SOUND_COMMON(6),
    /// Played when a weapon is equipped or a part attached (Equip/Attach prompt, ammo
    /// row equip).
    SOUND_WEAPON_EQUIP = SOUND_COMMON(0x0A),
    /// Played when the area-leave task starts, before the ending/area music is queued.
    SOUND_AREA_EXIT = SOUND_COMMON(0x0B),
    /// Played when the Antibody aura reacts to the player losing HP.
    SOUND_ANTIBODY_AURA_HIT = SOUND_COMMON(0x0E),
    /// Selection change in system-style menus: memory-card yes/no, shop quantity +/-,
    /// caption choices, map room select.
    SOUND_SYSTEM_CURSOR = SOUND_COMMON(0x15),
    /// Confirm in memory-card, shop, telephone and jukebox menus (and some room switch
    /// events).
    SOUND_SYSTEM_CONFIRM = SOUND_COMMON(0x16),
    /// Sound stopped when the Antibody aura effect task ends.
    SOUND_ANTIBODY_AURA_LOOP = SOUND_COMMON(0x23),
    /// Cancel/back out in memory-card, telephone and shooting-gallery/saloon menus.
    SOUND_SYSTEM_CANCEL = SOUND_COMMON(0x3B),
    /// Played when the reload prompt shows ammo was loaded (state < 0x10, 'Loaded').
    SOUND_AMMO_LOAD = SOUND_COMMON(0x3C),
};

/// Weapon banks.
enum {
    /// Whole weapon bank type (2): stops every weapon sound on death/restart and stage
    /// init.
    SOUND_BANK_TYPE_WEAPON_ALL = SOUND_ID(SOUND_BANK_TYPE_WEAPON, 0, 0),
    /// Hypervelocity first charge stage: started when the charge begins, stopped at the
    /// 60-frame halfway mark or on cancel.
    SOUND_HYPERVELOCITY_CHARGE_START = SOUND_WEAPON(SOUND_BANK_HYPERVELOCITY, 3),
    /// Hypervelocity charge released early (power-down); cut every frame while the
    /// trigger stays held.
    SOUND_HYPERVELOCITY_CHARGE_CANCEL = SOUND_WEAPON(SOUND_BANK_HYPERVELOCITY, 4),
    /// Hypervelocity charge loop running for the whole charge, stopped when the shot
    /// fires or is cancelled.
    SOUND_HYPERVELOCITY_CHARGE_LOOP = SOUND_WEAPON(SOUND_BANK_HYPERVELOCITY, 5),
    /// Hypervelocity muzzle ring's one-shot report once it passes half brightness in
    /// the firing state.
    SOUND_HYPERVELOCITY_DISCHARGE = SOUND_WEAPON(SOUND_BANK_HYPERVELOCITY, 6),
    /// M4A1 Pyke tail played when a burst/dart ends; stopped on interrupt.
    SOUND_PYKE_FIRE_TAIL = SOUND_WEAPON(SOUND_BANK_M4A1_PYKE, 5),
};

/// Character banks.
enum {
    /// Whole character bank type (4): muted/unmuted while bosses hide or pause.
    SOUND_BANK_TYPE_CHARACTER_ALL = SOUND_ID(SOUND_BANK_TYPE_CHARACTER, 0, 0),
    /// Desert Chaser enters its lunge at the player in the chase.
    SOUND_DESERT_CHASER_LUNGE = SOUND_CHARACTER(SOUND_BANK_DESERT_CHASER, 6),
    /// actor_311500 takes a damaging hit from the player.
    SOUND_ACTOR_311500_HURT = SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 7),
    /// actor_311500 death sequence start (node unlinked, effect, fade to black,
    /// shrink).
    SOUND_ACTOR_311500_DEATH = SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 8),
    /// Stranger hit by the player without dying.
    SOUND_STRANGER_HURT = SOUND_CHARACTER(SOUND_BANK_STRANGER, 7),
    /// Stranger's attack body touching a 0x10000-kind target (attack connects).
    SOUND_STRANGER_ATTACK_HIT = SOUND_CHARACTER(SOUND_BANK_STRANGER, 0x0D),
    /// A falling Glutton chunk reaches the floor.
    SOUND_GLUTTON_CHUNK_LAND = SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0C),
    /// Brahman part death: played as a dead part re-parents to the view and flies
    /// off/rises.
    SOUND_BRAHMAN_PART_DEATH = SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 4),
    /// Brahman arm's attack window: collision pairs enabled at parent arm
    /// coordinate 6/12.
    SOUND_BRAHMAN_ARM_STRIKE = SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 9),
    /// Brahman death-sequence sound for the body and its parts, faded out (0x2D) as the
    /// sequence ends.
    SOUND_BRAHMAN_DEATH_LOOP = SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x10),
    /// Mad Chaser alert cry; cut when it recoils, is pulled, goes limp or vanishes.
    SOUND_MAD_CHASER_ALERT_CRY = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 2),
    /// Specimen (Sucklerceph bank) projectile launch.
    SOUND_SUCKLERCEPH_PROJECTILE_LAUNCH = SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 2),
    /// Specimen projectile hits a target or the ground.
    SOUND_SUCKLERCEPH_PROJECTILE_IMPACT = SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 7),
    /// Companion actor_800100 fires its weapon (effect 0x6002B at the weapon).
    SOUND_ACTOR_800100_ATTACK = SOUND_CHARACTER(SOUND_BANK_ACTOR_800100, 1),
    /// Companion counterpart of the Pyke fire tail, stopped when its Pyke burst is
    /// interrupted.
    SOUND_COMPANION_PYKE_FIRE_TAIL = SOUND_CHARACTER(SOUND_BANK_PLAYER, 2),
    /// Companion actor_800200 hit (hitRegion set after contact processing).
    SOUND_ACTOR_800200_HURT = SOUND_CHARACTER(SOUND_BANK_ACTOR_800200, 0x0A),
};

/// Area banks of `GAME_STAGE_ACROPOLIS`.
enum {
    /// Positional one-shot fired from a fixed high world point (0x19AA,-0xF96,0x8DE) on
    /// state 3 and repeated every 0x79 frames by the square's 'siren task' until the
    /// event ends; stopped with Type7 when the scene hands off.
    SOUND_ACROPOLIS_SQUARE_SIREN = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SQUARE, 9),
    /// Sound of a dormant (Odd/Horned) Stranger: queued once (OR'd with the enemy's
    /// place index) when the stranger enters its dormant state 23, stopped with Type7
    /// when the player comes in range or it takes hit state 0x17; also queued by the
    /// SWAT-member body actor 312200 from view 0x10.
    SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 8),
    /// Played by the cafeteria woman actor (anmc_woman_cafeteria_body) each time her
    /// animation 1 reaches the frame cue 0x15; what the sound is is not shown.
    SOUND_ACROPOLIS_CAFETERIA_WOMAN_CUE = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_CAFETERIA, 0x0D),
    /// Police officer actor's step sound on animation cue 2 (when not on display id
    /// 0x6C, which uses the 0x51050008..A table instead).
    SOUND_ACROPOLIS_PLAZA_POLICE_STEP_1 = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 6),
    /// Police officer actor's step sound on animation cue 1, the partner of 0x51050006.
    SOUND_ACROPOLIS_PLAZA_POLICE_STEP_2 = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 7),
    /// Played when key item 0x104 or 0x103 is used on the security room panel and
    /// consumed, setting the bit in flag nibble 9 that records which of the two
    /// shutters has been opened.
    SOUND_ACROPOLIS_SECURITY_ROOM_SHUTTER_UNLOCK = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 1),
    /// Click when the highlighted row of the security monitor's camera list changes the
    /// displayed camera view.
    SOUND_ACROPOLIS_SECURITY_ROOM_MONITOR_SELECT = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 3),
    /// Click when the security monitor's grey wash steps one detent brighter.
    SOUND_ACROPOLIS_SECURITY_ROOM_MONITOR_BRIGHTER = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 6),
    /// Click when the security monitor's grey wash steps one detent darker.
    SOUND_ACROPOLIS_SECURITY_ROOM_MONITOR_DARKER = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 7),
    /// Loop started with the security room's streamed cutscene and faded out (0x14) at
    /// movie frame 0x46 or when the player skips.
    SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 8),
    /// Played by Gp_ItemPickupTilt in the security room when the item container's part
    /// starts tilting open for a pickup.
    SOUND_ACROPOLIS_SECURITY_ROOM_ITEM_LID_OPEN = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 9),
    /// Played by Gp_ItemPickupTilt in the security room when the tilted part swings
    /// back after the pickup result.
    SOUND_ACROPOLIS_SECURITY_ROOM_ITEM_LID_CLOSE = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 0x0A),
    /// The fountain's waterfall loop, (re)started or re-panned per camera view while
    /// the streamed fountain video is in its playing window and faded out at movie
    /// frame 0xF0.
    SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FOUNTAIN, 1),
    /// Played by Gp_ItemPickupTilt in the sanctuary when the item container's part
    /// starts tilting open (case falls through and also queues 0x521B000B).
    SOUND_ACROPOLIS_SANCTUARY_ITEM_LID_OPEN = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 5),
    /// Played by Gp_ItemPickupTilt in the sanctuary when the tilted part swings back
    /// after the pickup.
    SOUND_ACROPOLIS_SANCTUARY_ITEM_LID_CLOSE = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 6),
    /// Looping ambience whose level follows the camera view (view 7 full, view 5 at
    /// 0x1E, silent elsewhere), started, ramped or faded only on a change.
    SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 5),
    /// Beep when a keypad hotspot (digit or clear) is pressed while entering the bridge
    /// code.
    SOUND_ACROPOLIS_BRIDGE_KEYPAD_BEEP = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_BRIDGE, 3),
    /// Played when a three-digit bridge code other than 0x561 is submitted.
    SOUND_ACROPOLIS_BRIDGE_CODE_REJECTED = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_BRIDGE, 4),
    /// Played when the correct bridge code (561) is entered, alongside the broadcast to
    /// the room's actors.
    SOUND_ACROPOLIS_BRIDGE_CODE_ACCEPTED = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_BRIDGE, 9),
    /// Looping ambience whose level follows the camera view (view 8 full, views 2-3
    /// 0x1E, views 4-5 0xF), stopped when the player leaves the room.
    SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FIRE_ESCAPE, 5),
    /// Positional one-shot each time the room's flickering glow effect jumps from dim
    /// (<=0x10) to bright (>=0x20).
    SOUND_ACROPOLIS_FIRE_ESCAPE_LIGHT_FLICKER = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FIRE_ESCAPE, 6),
    /// Positional one-shot when the helipad floodlight effect randomly flares, spawning
    /// one 0x6003B and six 0x600A4 effects and a point light.
    SOUND_HELICOPTER_LANDING_PAD_LIGHT_SPARK = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD, 1),
    /// Pierce Carradine's walk sound on animation cue 1 (outside view 0x10); the cue-2
    /// partner is 0x5113000F / 0x51130013.
    SOUND_MIST_PARKING_PIERCE_STEP_2 = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_PARKING, 0x10),
    /// Played by a shooting-gallery target once its scales have grown to full size as
    /// it appears.
    SOUND_MIST_SHOOTING_GALLERY_TARGET_APPEAR = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 7),
    /// Played for each of the first eight hits a shooting-gallery target takes.
    SOUND_MIST_SHOOTING_GALLERY_TARGET_HIT = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 8),
    /// Played by a shooting-gallery target's death sequence (node unlinked, light set
    /// black, then shrunk away), distinct from the kill sequence's 0x51140010-12.
    SOUND_MIST_SHOOTING_GALLERY_TARGET_DEATH = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 9),
    /// Played at the player's coordinate when an active target's 210-frame timer fires,
    /// spawning effect 0x601BD on the player and dealing 10 damage.
    SOUND_MIST_SHOOTING_GALLERY_TARGET_ATTACK = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0x0E),
    /// Played by every shooting-gallery course when its countdown finishes and table
    /// entry 1 is spawned to start the round.
    SOUND_MIST_SHOOTING_GALLERY_ROUND_START = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0x0F),
    /// Played when an active target's timer reaches 120 frames and its light switches
    /// to the weighted (warning) mode, 90 frames before it attacks.
    SOUND_MIST_SHOOTING_GALLERY_TARGET_CHARGE = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0x13),
};

/// Area banks of `GAME_STAGE_DRYFIELD`.
enum {
    /// Long-running sound started by the gas-station cutscene's command 1 and faded out
    /// (60) when the cutscene restores the player.
    SOUND_GAS_STATION_CUTSCENE_LOOP = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x11),
    /// Door sound of the main street's motel-room doors: sole sound of the one-time
    /// events into rooms 1/2, and the second sound (after the unlock) of the key-gated
    /// doors into rooms 3/4.
    SOUND_MAIN_STREET_MOTEL_DOOR_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 5),
    /// First sound of the key-gated (collected bit 0x13) motel-room 3/4 door events,
    /// played before the door-open sound.
    SOUND_MAIN_STREET_MOTEL_DOOR_UNLOCK = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 0x0A),
    /// Second sound of the general store's one-time door event to the gas station (area
    /// 1), played before the room change.
    SOUND_GENERAL_STORE_DOOR_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, 3),
    /// First sound of the general store's one-time door event to the gas station (area
    /// 1), before the door-open sound.
    SOUND_GENERAL_STORE_DOOR_UNLOCK = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, 0x0C),
    /// Played as the store's underpass (area 0x26) cutscene starts CAP command 0xF
    /// asking whether to go through.
    SOUND_GENERAL_STORE_UNDERPASS_PROMPT = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, 0x0D),
    /// Played when the underpass cutscene's choice is declined and the store restores
    /// the view and weapons.
    SOUND_GENERAL_STORE_UNDERPASS_CANCEL = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, 0x0E),
    /// View-dependent ambient loop of the back street: started/retuned by view (loudest
    /// in view 5) and stopped on leaving.
    SOUND_BACK_STREET_AMBIENCE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BACK_STREET, 6),
    /// View-dependent ambient loop of the warehouse (volume by view 2/3/4), stopped on
    /// leaving.
    SOUND_WAREHOUSE_AMBIENCE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 5),
    /// Second sound of the parking lot's key-gated door events (to the motel lobby, bit
    /// 0x12, and the saloon, bit 0x10).
    SOUND_PARKING_LOT_DOOR_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 7),
    /// First sound of the parking lot's key-gated door events, before the door-open
    /// sound.
    SOUND_PARKING_LOT_DOOR_UNLOCK = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 0x0B),
    /// Second sound of the saloon's one-time door event to the parking lot (area 0xF).
    SOUND_SALOON_G_R_DOOR_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_SALOON_G_R, 3),
    /// First sound of the saloon's one-time door event to the parking lot, before the
    /// door-open sound.
    SOUND_SALOON_G_R_DOOR_UNLOCK = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_SALOON_G_R, 5),
    /// Second sound of the water tower's key-gated (bit 0x10) door event into the G&R
    /// kitchen (area 0x13).
    SOUND_WATER_TOWER_KITCHEN_DOOR_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 3),
    /// Started by cap-script command 10 and cut when the falling cap reaches its height
    /// (followed by the landing sound).
    SOUND_WATER_TOWER_CAP_DROP = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 0x0B),
    /// Running loop of the water tower's cap mechanism, volume-set per view while it
    /// turns and stopped when the cap has lowered.
    SOUND_WATER_TOWER_CAP_RUNNING = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 0x0C),
    /// First sound of the water tower's key-gated door event into the G&R kitchen,
    /// before the door-open sound.
    SOUND_WATER_TOWER_KITCHEN_DOOR_UNLOCK = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 0x0E),
    /// One-shot when the falling cap reaches its height, with the two arrival effects.
    SOUND_WATER_TOWER_CAP_LAND = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 0x10),
    /// Played with 0x52150007 when the water tank's streamed movie is ready; faded if
    /// it is skipped.
    SOUND_WATER_TANK_MOVIE_SFX_A = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 6),
    /// Second sound started with the water tank's streamed movie; faded if it is
    /// skipped.
    SOUND_WATER_TANK_MOVIE_SFX_B = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 7),
    /// Room sound started on entering the water tank, stopped before its movie and
    /// restarted after.
    SOUND_WATER_TANK_AMBIENCE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 9),
    /// Ambience played while the water tank is in view 4, stopped from other views.
    SOUND_WATER_TANK_VIEW4_AMBIENCE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 0x11),
    /// Ambience played while the water tank is in view 0xA, stopped from other views.
    SOUND_WATER_TANK_VIEW10_AMBIENCE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 0x12),
    /// Loop started in view 2 while the breezeway's particle effect (0x6003C) runs
    /// before nibble 0x5D is set; stopped once it is.
    SOUND_BREEZEWAY_EFFECT_LOOP = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 0x0A),
    /// Positional one-shot fired at random in view 3 while the breezeway's particle
    /// effect runs.
    SOUND_BREEZEWAY_EFFECT_BURST = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 0x0B),
    /// Played each frame the breezeway's key-item hotspot cursor moves toward the
    /// prompt.
    SOUND_BREEZEWAY_CURSOR_MOVE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 0x0D),
    /// Lift platform raise/lower movement sound, started as it moves and stopped when
    /// it reaches the end.
    SOUND_FACTORY_LIFT_MOVE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 8),
    /// Played (day only) as the factory's white-out scene flashes white and reloads the
    /// room as variant 2.
    SOUND_FACTORY_WHITEOUT = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x0B),
    /// The factory lift's hatch swinging open.
    SOUND_FACTORY_HATCH_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x0D),
    /// The factory lift's hatch swinging shut.
    SOUND_FACTORY_HATCH_CLOSE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x0E),
    /// Lift platform quarter-turn movement sound, stopped when the turn ends.
    SOUND_FACTORY_LIFT_TURN = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x0F),
    /// Played as the lift platform finishes raising or lowering.
    SOUND_FACTORY_LIFT_MOVE_STOP = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x10),
    /// Played as the lift platform finishes a turn.
    SOUND_FACTORY_LIFT_TURN_STOP = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x11),
    /// Bang when a turn is tried with the lift lowered and it jams, with a screen jolt.
    SOUND_FACTORY_LIFT_JAM = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x12),
    /// Positional sound when the trailer coach's item container is opened and its lid
    /// model tilts.
    SOUND_TRAILER_COACH_ITEM_LID_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 0x0B),
    /// Second sound of the motel balcony's key-gated door events (rooms 5 and 6, loft).
    SOUND_MOTEL_BALCONY_DOOR_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 1),
    /// First sound of the motel balcony's key-gated door events, before the door-open
    /// sound.
    SOUND_MOTEL_BALCONY_DOOR_UNLOCK = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 0x0A),
    /// Started with motel room 6's streamed movie (Kyle Madigan actor_120500) and faded
    /// afterwards.
    SOUND_MOTEL_ROOM_6_MOVIE_SFX = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_6, 7),
    /// Start sound of motel room 6's event-0x16 cutscene record.
    SOUND_MOTEL_ROOM_6_SCENE_START = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_6, 8),
    /// Sound played by the sound task running beside motel room 6's event-0x16
    /// cutscene.
    SOUND_MOTEL_ROOM_6_SCENE_TRACK = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_6, 9),
    /// Played after motel room 6's event-0x16 cutscene when it was not skipped.
    SOUND_MOTEL_ROOM_6_SCENE_COMPLETE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_6, 0x0A),
    /// End sound of motel room 6's event-0x16 cutscene record.
    SOUND_MOTEL_ROOM_6_SCENE_END = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_6, 0x0B),
    /// Played when the water hole's passage is tried before it is open (cap 2, nibble
    /// 0x1BD = 2), and on CAP cue 4.
    SOUND_WATER_HOLE_LOCKED = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_HOLE, 4),
};

/// Area banks of `GAME_STAGE_DRYFIELD_NIGHT`.
enum {
    /// Key press on the night motel lobby's keypad (digits, clear and activation).
    SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_PRESS = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, 7),
    /// Played in the step after the correct keypad code applies the area records and
    /// sets nibble 0x74.
    SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_ACCEPT = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, 8),
    /// Played when the keypad's enter key is pressed with a wrong code.
    SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_ERROR = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, 9),
    /// Night: lift platform raise/lower movement sound.
    SOUND_NIGHT_FACTORY_LIFT_MOVE = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 8),
    /// Night: the factory lift's hatch swinging open.
    SOUND_NIGHT_FACTORY_HATCH_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 0x0D),
    /// Night: the factory lift's hatch swinging shut.
    SOUND_NIGHT_FACTORY_HATCH_CLOSE = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 0x0E),
    /// Night: lift platform quarter-turn movement sound.
    SOUND_NIGHT_FACTORY_LIFT_TURN = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 0x0F),
    /// Night: played as the lift finishes raising or lowering.
    SOUND_NIGHT_FACTORY_LIFT_MOVE_STOP = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 0x10),
    /// Night: played as the lift finishes a turn.
    SOUND_NIGHT_FACTORY_LIFT_TURN_STOP = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 0x11),
    /// Night: bang when a turn jams with the lift lowered.
    SOUND_NIGHT_FACTORY_LIFT_JAM = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 0x12),
    /// Night: positional sound when the trailer coach's item container is opened.
    SOUND_NIGHT_TRAILER_COACH_ITEM_LID_OPEN = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 0x0B),
    /// Night: positional sound after the trailer coach's item is taken and the
    /// container closes.
    SOUND_NIGHT_TRAILER_COACH_ITEM_LID_CLOSE = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 0x0C),
    /// Night: played when the water hole's passage is tried before nibble 0xB8 opens it
    /// (cap 2, nibble 0x1BD = 2).
    SOUND_NIGHT_WATER_HOLE_LOCKED = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 4),
};

/// Area banks of `GAME_STAGE_MINE_SHELTER`.
enum {
    /// Positional loop at cavern emitter point 1 (lit glow point enabled by flag nibble
    /// 0xE2 bit 1), volume per view, stopped in out-of-range views.
    SOUND_MINE_CAVERN_GLOW_POINT_1 = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 0x0E),
    /// Positional loop at cavern emitter point 0, started (per-view volume) once that
    /// point's bit in flag nibble 0xE2 is set - the point then also gets a light, a
    /// glow fan and periodic effect 0x60080 - and stopped in views out of range.
    SOUND_MINE_CAVERN_GLOW_POINT_0 = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 0x0F),
    /// Positional loop at cavern emitter point 2 (lit glow point enabled by flag nibble
    /// 0xE2 bit 2), volume per view, stopped in out-of-range views.
    SOUND_MINE_CAVERN_GLOW_POINT_2 = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 0x10),
    /// Positional loop at cavern emitter point 3 (lit glow point enabled by flag nibble
    /// 0xE2 bit 3), volume per view, stopped in out-of-range views.
    SOUND_MINE_CAVERN_GLOW_POINT_3 = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 0x11),
    /// Played as the refuge task opens the circuit/battery panel (spawns actor_548100
    /// from D_actor_548100_801358D8).
    SOUND_MINE_REFUGE_CIRCUIT_PANEL_OPEN = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 7),
    /// Played when the circuit/battery panel task has been killed and the refuge task
    /// ends.
    SOUND_MINE_REFUGE_CIRCUIT_PANEL_CLOSE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 8),
    /// Played when the circuit panel's switch is thrown (CAP key 0xB sets flag 0xC3,
    /// key 0x15 clears it).
    SOUND_MINE_REFUGE_CIRCUIT_SWITCH = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 9),
    /// Loop started with the switch as the panel's route ramp runs along the circuit,
    /// stopped when the ramp reaches the end of its legs.
    SOUND_MINE_REFUGE_CIRCUIT_CURRENT_LOOP = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0A),
    /// Played when a battery is placed into (item consumed, step flag set) or taken out
    /// of a circuit panel socket.
    SOUND_MINE_REFUGE_BATTERY_SOCKET = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0B),
    /// Played when the panel's current ramp finishes on a circuit that powers the
    /// panel's first output, before CAP command 0xC runs.
    SOUND_MINE_REFUGE_CIRCUIT_COMPLETE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0E),
    /// Sustained sound of the tunnel-switch event script while the path-walking object
    /// moves; restarted by a callback and faded out over 60 frames at the end or on
    /// skip.
    SOUND_MINE_FORKED_TUNNEL_OBJECT_MOVE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL, 5),
    /// Played after confirming the passage to Shelter B1 elevator hall, during the
    /// fade-out; the warp waits for it to finish.
    SOUND_MINE_SECRET_PASSAGE_EXIT_TRANSIT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_SECRET_PASSAGE, 3),
    /// Played after confirming the passage from the B1 elevator hall to the mine secret
    /// passage, during the fade-out; the warp waits for it.
    SOUND_SHELTER_B1_ELEV_HALL_MINE_TRANSIT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 7),
    /// Elevator ride sound: cued by the CAP (room sound 8) and waited on by
    /// shelterElevatorTask (spawnArg1) before the floor change.
    SOUND_SHELTER_B1_ELEVATOR_RIDE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 8),
    /// Played as the in-room passage task fades out before moving the player to another
    /// cell of the sterilization room.
    SOUND_SHELTER_B1_STERILIZATION_DOOR_OPEN = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x0A),
    /// Played as the passage task places the player at the destination and fades back
    /// in.
    SOUND_SHELTER_B1_STERILIZATION_DOOR_CLOSE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x0B),
    /// Played at the player each time the room's hazard deals periodic damage (flag
    /// 0x77 clear) and plays the player's reaction animation; stopped when paused or
    /// cleared.
    SOUND_SHELTER_B1_STERILIZATION_PLAYER_HURT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x11),
    /// Played by Gp_ItemPickupTilt in this room as the item container's lid tilts open.
    SOUND_SHELTER_B1_STERILIZATION_ITEM_LID_OPEN = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x12),
    /// Played by Gp_ItemPickupTilt in this room as the lid tilts back after the pickup.
    SOUND_SHELTER_B1_STERILIZATION_ITEM_LID_CLOSE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x13),
    /// Played after confirming the B1 pod access tunnel exit (toward R47, story flag
    /// 0x7A >= 6); the warp to the pod service gantry waits for it.
    SOUND_SHELTER_B1_POD_TUNNEL_GANTRY_TRANSIT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 4),
    /// Played after confirming the pod ride; the warp to the B2 pod access tunnel waits
    /// for it.
    SOUND_SHELTER_B1_POD_TUNNEL_RIDE_TO_B2 = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 6),
    /// Played on every button press of the parking-lot selector panel (toggles and the
    /// enter button).
    SOUND_SHELTER_B1_PARKING_PANEL_BUTTON = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 4),
    /// Played when the panel's enter button commits a changed selection (rooms other
    /// than 1), before the room layout switches.
    SOUND_SHELTER_B1_PARKING_PANEL_APPLY = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 5),
    /// Played when the panel's enter button commits a changed selection while in room
    /// 1.
    SOUND_SHELTER_B1_PARKING_PANEL_APPLY_R1 = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 6),
    /// Started during actor_403600's attack wind-up (state 6, with pad rumble) and
    /// stopped when the attack hits the player (damage + MP drain) or is reset.
    SOUND_SHELTER_B2_POD_BTM_ENEMY_DRAIN_WINDUP = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 1),
    /// Played as actor_403600 starts spawning its projectile tasks in a volley; stopped
    /// when the attack state ends.
    SOUND_SHELTER_B2_POD_BTM_ENEMY_VOLLEY = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 8),
    /// Played at an actor_403600 projectile when its launch delay expires and it begins
    /// steering.
    SOUND_SHELTER_B2_POD_BTM_PROJECTILE_LAUNCH = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 9),
    /// Played at an actor_403600 projectile when it registers a contact and expires.
    SOUND_SHELTER_B2_POD_BTM_PROJECTILE_HIT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 0x0A),
    /// Started when actor_403600's state-5 phase triggers (spawning effect task 3),
    /// during which pad rumble scales with player distance; stopped 30 ticks later.
    SOUND_SHELTER_B2_POD_BTM_ENEMY_RUMBLE_LOOP = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 0x0F),
    /// Played as actor_403600 charges an attack while effects burst from random body
    /// parts; faded out over 20 frames when the phase ends.
    SOUND_SHELTER_B2_POD_BTM_ENEMY_CHARGE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 0x13),
    /// Played when the code keypad's enter hotspot is picked, before the code is
    /// checked.
    SOUND_SHELTER_B2_LAB_KEYPAD_ENTER = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x10),
    /// Played during the code check when the entered code matches.
    SOUND_SHELTER_B2_LAB_KEYPAD_CODE_ACCEPTED = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x11),
    /// Played during the code check when the entered code does not match.
    SOUND_SHELTER_B2_LAB_KEYPAD_CODE_REJECTED = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x12),
    /// Key beep of the code keypad: on a key hotspot press and for each digit of the
    /// automatic code entry.
    SOUND_SHELTER_B2_LAB_KEYPAD_KEY = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x13),
    /// Played by Gp_ItemPickupTilt in this room as the item container's lid tilts open.
    SOUND_SHELTER_B2_LAB_ITEM_LID_OPEN = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x15),
    /// Played by Gp_ItemPickupTilt in this room as the lid tilts back after the pickup.
    SOUND_SHELTER_B2_LAB_ITEM_LID_CLOSE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x16),
    /// Played after confirming the pod ride; the warp to the B1 pod access tunnel waits
    /// for it.
    SOUND_SHELTER_B2_POD_TUNNEL_RIDE_TO_B1 = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 4),
    /// Played as the blaze encounter script ignites (broadcasts the actor command and
    /// starts the body-fire child).
    SOUND_SHELTER_B3_DUMPING_HOLE_BLAZE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 5),
    /// Played when the blaze encounter controller starts, alongside its caption
    /// schedule (CapCaption_RunSchedule 0xD0).
    SOUND_SHELTER_B3_DUMPING_HOLE_ALERT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 7),
    /// Played by Gp_ItemPickupTilt in this room as the item container's lid tilts open.
    SOUND_SHELTER_B3_DUMPING_HOLE_ITEM_LID_OPEN = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 8),
    /// Played by Gp_ItemPickupTilt in this room as the lid tilts back after the pickup.
    SOUND_SHELTER_B3_DUMPING_HOLE_ITEM_LID_CLOSE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 9),
    /// Loop while the incinerator lift model moves to its first rest pose after the
    /// action trigger; stopped on arrival.
    SOUND_SHELTER_B3_INCINERATOR_LIFT_MOVE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 3),
    /// Played when the incinerator lift reaches its first rest pose.
    SOUND_SHELTER_B3_INCINERATOR_LIFT_STOP = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 4),
    /// One-shot cue when the incinerator encounter controller starts (with its caption
    /// schedule 0xD0), also latched once by the actor_342000/actor_444000 events.
    SOUND_SHELTER_B3_INCINERATOR_ALERT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 5),
    /// Played after confirming the passage to the incinerator control room; the warp
    /// waits for it.
    SOUND_SHELTER_B3_INCINERATOR_EXIT_TRANSIT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 6),
    /// Played as the incinerator blaze script ignites (pulse, enemy cull zone, body-
    /// fire child spawned).
    SOUND_SHELTER_B3_INCINERATOR_BLAZE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 8),
    /// Loop while actor_342000's two model halves slide toward each other; stopped when
    /// they meet or the encounter is culled.
    SOUND_SHELTER_B3_INCINERATOR_DOORS_CLOSING = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 0x0B),
    /// Played when actor_342000's two sliding halves finish closing.
    SOUND_SHELTER_B3_INCINERATOR_DOORS_SHUT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 0x0C),
    /// Played when the incinerator lift control is operated (lift trigger, the warp-2
    /// control with its caption, and the final activation).
    SOUND_SHELTER_B3_INCINERATOR_SWITCH_PRESS = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 0x0D),
    /// Loop while the lift model makes its second move (15 units a frame) toward the
    /// second rest pose; stopped on arrival.
    SOUND_SHELTER_B3_INCINERATOR_LIFT_MOVE_2 = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 0x0E),
    /// Played when the lift reaches its second rest pose, as the model shakes for 16
    /// frames.
    SOUND_SHELTER_B3_INCINERATOR_LIFT_JOLT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 0x0F),
    /// Elevator sound the B3 elevator hall task waits on before warping to the B2
    /// elevator.
    SOUND_SHELTER_B3_ELEVATOR_RIDE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, 1),
    /// Played after confirming the passage to the reservoir or water supply, once the
    /// fade-out ends; the warp waits for it.
    SOUND_SHELTER_B4_UPPER_SEWER_EXIT_TRANSIT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 3),
    /// Played after confirming the passage to the upper sewer, once the fade-out ends;
    /// the warp waits for it.
    SOUND_SHELTER_B4_RESERVOIR_EXIT_TRANSIT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, 1),
    /// Played after confirming the passage to the upper sewer, once the fade-out ends;
    /// the warp waits for it.
    SOUND_SHELTER_B4_WATER_SUPPLY_EXIT_TRANSIT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, 5),
    /// Played when the map terminal's screen finishes opening (and after the idle
    /// countdown).
    SOUND_SHELTER_R47_MAP_TERMINAL_SCREEN_ON = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 3),
    /// Played when the map terminal's previous/next hotspot switches the map page.
    SOUND_SHELTER_R47_MAP_TERMINAL_PAGE_SWITCH = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 4),
    /// Loop started when the terminal's map quad is drawn, stopped on page switch or
    /// when the terminal closes.
    SOUND_SHELTER_R47_MAP_TERMINAL_LOOP = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 5),
    /// Room ambience loop panned per view (2-4), stopped in view 5 and during events.
    SOUND_SHELTER_R47_AMBIENCE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 0x10),
    /// Played at the player on frame 0x11 of the knock-back task actor_503500 spawns
    /// when it hits the player.
    SOUND_SHELTER_R48_PLAYER_KNOCKBACK = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48, 2),
};

/// Area banks of `GAME_STAGE_SHELTER_NEO_ARK`.
enum {
    /// Plays with the screen fade when the player leaves the bulwark for the heliport
    /// (latched room event stageSnd and the first-visit event task), waited on before
    /// the warp.
    SOUND_SHELTER_1F_BULWARK_TO_HELIPORT = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_BULWARK, 3),
    /// First of two loops the heliport starts when its room task sets up and stops when
    /// the player leaves for the tent or the bulwark.
    SOUND_SHELTER_1F_HELIPORT_AMBIENCE_1 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_HELIPORT, 6),
    /// Second of the two heliport room loops, started at room setup and stopped on exit
    /// together with 0x55040006.
    SOUND_SHELTER_1F_HELIPORT_AMBIENCE_2 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_HELIPORT, 7),
    /// Played after the player confirms going on to the EVE elevator; the task waits
    /// for it to finish before committing the new location (also the second sound of
    /// the tunnel's elevator-side warp record).
    SOUND_NEO_ARK_EVE_TUNNEL_TO_ELEVATOR = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL, 3),
    /// Room loop the forest zone starts when its room task sets up and fades out over
    /// 60 frames when the player leaves.
    SOUND_NEO_ARK_FOREST_ZONE_AMBIENCE = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 6),
    /// Played at the horned stranger's part 1 when its death-throes sequence starts.
    SOUND_NEO_ARK_FOREST_STRANGER_DEATH_START = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 7),
    /// Played at frame 0x31 of the horned stranger's death sequence together with a
    /// strong pad rumble (6/0xFF/0x80).
    SOUND_NEO_ARK_FOREST_STRANGER_DEATH_IMPACT = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 8),
    /// Played at frame 0xCE, the last cue of the horned stranger's death sequence.
    SOUND_NEO_ARK_FOREST_STRANGER_DEATH_END = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 9),
    /// Started by the submarine tunnel's room task when it sets up.
    SOUND_NEO_ARK_SUBMARINE_TUNNEL_AMBIENCE = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, 3),
    /// First of two sounds the pavilion's room entry task starts on setup.
    SOUND_NEO_ARK_PAVILION_AMBIENCE_1 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 5),
    /// Second of two sounds the pavilion's room entry task starts on setup.
    SOUND_NEO_ARK_PAVILION_AMBIENCE_2 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 6),
    /// First of the two island cues the island's room entry task plays on setup.
    SOUND_NEO_ARK_ISLAND_AMBIENCE_1 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 5),
    /// Second of the two island cues the island's room entry task plays on setup.
    SOUND_NEO_ARK_ISLAND_AMBIENCE_2 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 6),
    /// First of the garden ambience task's pair of positional loops, started and re-
    /// panned per camera view.
    SOUND_NEO_ARK_GARDEN_AMBIENCE_1 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_GARDEN, 3),
    /// Second of the garden ambience task's pair of positional loops, started and re-
    /// panned per camera view.
    SOUND_NEO_ARK_GARDEN_AMBIENCE_2 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_GARDEN, 4),
    /// Altar floor-tile puzzle: stepping onto tile 1 as the correct next step of a
    /// sequence (tiles 2-4 play entries 2-4).
    SOUND_NEO_ARK_ALTAR_TILE_1_CORRECT = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 1),
    /// Played when the first altar tile sequence (12 steps) is completed, as loop
    /// 0x55140003 is stopped and the area records change.
    SOUND_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 7),
    /// Altar floor-tile puzzle: stepping onto tile 1 when it breaks the sequence.
    SOUND_NEO_ARK_ALTAR_TILE_1_WRONG = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 8),
    /// Altar floor-tile puzzle: stepping onto tile 2 when it breaks the sequence.
    SOUND_NEO_ARK_ALTAR_TILE_2_WRONG = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 9),
    /// Altar floor-tile puzzle: stepping onto tile 3 when it breaks the sequence.
    SOUND_NEO_ARK_ALTAR_TILE_3_WRONG = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 0x0A),
    /// Altar floor-tile puzzle: stepping onto tile 4 when it breaks the sequence.
    SOUND_NEO_ARK_ALTAR_TILE_4_WRONG = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 0x0B),
    /// Played when the second altar tile sequence (16 steps) is completed.
    SOUND_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 0x0C),
    /// Played after the player flips the altar's switch (nibble 0xD9) while the wall
    /// sprites step to the new switch state over 30 frames.
    SOUND_NEO_ARK_ALTAR_SWITCH_TOGGLE = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 0x0D),
    /// Click for each tile moved in the shrine's sliding-tile puzzle.
    SOUND_NEO_ARK_SHRINE_TILE_SLIDE = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 6),
    /// Sliding-tile puzzle: a row pattern (or the pad-mode latch) is reached, starting
    /// the step that switches the shrine's room objects to their alternate state.
    SOUND_NEO_ARK_SHRINE_MECHANISM_ACTIVATE = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 7),
    /// Sliding-tile puzzle solved with the diagonal pattern; sets nibble 0xDB and
    /// starts cap slot 3.
    SOUND_NEO_ARK_SHRINE_PUZZLE_SOLVED = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 8),
    /// Played with a pad rumble as the shrine's first falling prop drops.
    SOUND_NEO_ARK_SHRINE_PROP_1_FALL = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 9),
    /// Played with a pad rumble when the shrine's room objects are switched back to
    /// their base state (room 1/4).
    SOUND_NEO_ARK_SHRINE_MECHANISM_REVERT = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 0x0A),
    /// Played as the shrine's second falling prop drops (rumble follows at tick 0x12).
    SOUND_NEO_ARK_SHRINE_PROP_2_FALL = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 0x0B),
    /// Positional loop at a fixed point in the nursery, re-panned per camera view and
    /// stopped when the room's cutscene starts.
    SOUND_SHELTER_B6_NURSERY_AMBIENCE = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY, 1),
    /// One of two random takes of the player's timed cue (with a scripted animation)
    /// every 210 ticks in the growth room.
    SOUND_SHELTER_B6_GROWTH_PLAYER_VOICE_1 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_GROWTH_ROOM, 3),
    /// The other random take of the player's timed cue in the growth room.
    SOUND_SHELTER_B6_GROWTH_PLAYER_VOICE_2 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_GROWTH_ROOM, 4),
    /// One of two random takes of the ally's voice cue, played at the ally every 210
    /// ticks as it takes damage, or from a script callback.
    SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_1 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_GROWTH_ROOM, 5),
    /// The other random take of the ally's voice cue in the growth room.
    SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_2 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_GROWTH_ROOM, 6),
    /// The ally's non-random voice cue, played by the script callback when its argument
    /// is nonzero.
    SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_3 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_GROWTH_ROOM, 7),
    /// Played after the death-sound countdown when the player is alive but companion
    /// type 3 is (the game over by losing the ally), after all area sounds are stopped.
    SOUND_SHELTER_B6_GROWTH_ALLY_DEATH = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_GROWTH_ROOM, 8),
    /// First of two sounds the bridge's room entry task starts on setup.
    SOUND_NEO_ARK_BRIDGE_AMBIENCE_1 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_BRIDGE, 3),
    /// Second of two sounds the bridge's room entry task starts on setup.
    SOUND_NEO_ARK_BRIDGE_AMBIENCE_2 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_BRIDGE, 4),
    /// First of two sounds the tent's room task starts on setup (on every visit after
    /// the first).
    SOUND_SHELTER_1F_TENT_AMBIENCE_1 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, 9),
    /// Second of two sounds the tent's room task starts on setup (on every visit after
    /// the first).
    SOUND_SHELTER_1F_TENT_AMBIENCE_2 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, 0x0A),
    /// Played at the player, with effect 0x60054, on the hit frame of the horned
    /// stranger's grab animations 4/5 when the in-range test passes (0x400D0013
    /// otherwise).
    SOUND_NEO_ARK_WOODLAND_STRANGER_HIT = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 5),
    /// Played when the horned stranger reaches its home point (or times out), disables
    /// its body collision and moves off, reporting to the room when its clip ends.
    SOUND_NEO_ARK_WOODLAND_STRANGER_WITHDRAW = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 8),
    /// Played with the screen fade after the player confirms travelling from the
    /// submarine gallery to the island, waited on before the warp.
    SOUND_NEO_ARK_SUB_GALLERY_TO_ISLAND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 1),
    /// Loop the diver starts at frame 0x54 of its attack sub-state (the six cue frames
    /// follow) and stops at 0x77, on interruption, or when it retires.
    SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 2),
    /// Played at frame 3 of the diver's state 4, as its teleport task is retired and
    /// before the particle ring splats.
    SOUND_NEO_ARK_SUB_GALLERY_DIVER_REAPPEAR = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 3),
    /// Played at frame 3 of the diver's state-1 body, which walks it out of the scene
    /// before it is parked and the player is placed.
    SOUND_NEO_ARK_SUB_GALLERY_DIVER_DEPART = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 4),
    /// Played as the pyramid's rotating quad starts its one-step turn after the player
    /// confirms.
    SOUND_NEO_ARK_PYRAMID_ROTATE = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 3),
    /// Played when one turn of the pyramid's rotating quad ends with fewer than four
    /// turns made.
    SOUND_NEO_ARK_PYRAMID_ROTATE_STOP = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 4),
    /// Closing sound when the fourth turn of the pyramid's rotating quad completes,
    /// before capture command 2.
    SOUND_NEO_ARK_PYRAMID_ROTATE_DONE = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 5),
    /// The substation's looping ambience, started on setup and re-panned per camera
    /// view.
    SOUND_NEO_ARK_SUBSTATION_AMBIENCE = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBSTATION, 3),
};

/// stage-relative.
enum {
    /// Not a single sound: a type-5 wildcard that SndScript_StopMatching /
    /// sndScriptSetMuteMatching match against every playing area-bank sound (high nibble
    /// compare), used to stop all area sounds on stage init and before the companion-
    /// death sting, and to mute/unmute them while the game menu is open.
    SOUND_AREA_BANK_ALL = SOUND_ID(SOUND_BANK_TYPE_AREA, 0, 0),
};

/// Other bank types.
enum {
    /// Stage ambient loop toggled by gStageAmbientOn; faded on restart/disk swap.
    SOUND_STAGE_AMBIENT = 0x60010001,
    /// Played at the player from the CD-loaded death bank (file 9/0x1E) after a hold
    /// kill.
    SOUND_PLAYER_DEATH = 0x70010001,
    /// Stops every script sound except type 6 (ambient); used for cutscene skips and
    /// restarts.
    SOUND_BANK_TYPE_ALL_NON_AMBIENT = (s32)0x80000000,
    /// Whole PE effect bank type (14): stopped before a new PE bank is loaded and at
    /// stage init.
    SOUND_BANK_TYPE_PE_ALL = (s32)0xE0000000,
    /// Ofuda effect sound, played as it starts and stopped if cancelled.
    SOUND_OFUDA_USE = (s32)0xE03D0001,
    /// Flare effect sound, played on the first frame and stopped if cancelled.
    SOUND_FLARE_USE = (s32)0xE03E0001,
    /// Pepper spray effect sound, played on start and stopped if cancelled.
    SOUND_PEPPER_SPRAY_USE = (s32)0xE03F0001,
};

#endif // MAIN_SOUND_IDS_H
