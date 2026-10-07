#ifndef MAIN_GAMEFLAG_IDS_H
#define MAIN_GAMEFLAG_IDS_H

/// Game flag indices for `gameFlagGetNibble` / `gameFlagSetNibble`.
///
/// Each flag is a four-bit value in the live nibble bank. A name says what the
/// flag records where the code establishes it; `GAME_FLAG_<PLACE>_<HHH>` and
/// `GAME_FLAG_<HHH>` are placeholders for flags whose meaning is not yet known,
/// named by the room that uses them where it is a single one.
enum {
    /// Akropolis Tower main story counter: 1 after the east elevator hall scene, 2
    /// after the cafeteria scene, 3 when the follow-up room cutscene runs
    /// (cafeteria/square), 5 once the fountain's warp sequence is taken. Patio,
    /// hallway, cafeteria, fountain and forked-road doors answer by its value.
    GAME_FLAG_ACROPOLIS_PROGRESS = 0x000,
    /// Akropolis progress along the security room -> forked road -> observatory ->
    /// promenade route: 2 after the security-room panel releases its first lock, 3 when
    /// the forked-road cutscene to the observatory plays, 4 when leaving the
    /// observatory for the promenade, 5 on reaching the promenade/sanctuary. The
    /// security-room enemy (actor_311900) is removed from 3 on.
    GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS = 0x001,
    /// Sanctuary/bridge progress: 0 until the sanctuary is entered from the promenade
    /// (warp 3), then 2; 3 once the bridge cutscene completes and the bridge room
    /// switches to room 2. The promenade's bridge door is refused while 0 and leads to
    /// room 2 at 3; the fire escape's bridge warp likewise picks room 2 at 3.
    GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS = 0x002,
    /// State of the follow-up CAP command selected by flag 0x155 (run as 0x155+0x10
    /// after a room cutscene). Cleared to 0 whenever 0x155 is changed; when 0x155 is
    /// 0xE and this is 0 the cutscene task sets it to 1 and loops the branching follow-
    /// up, setting 2 when that exits.
    GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE = 0x003,
    /// Set to 1 once the sanctuary scene has been passed (on first reaching the roof
    /// garden or the bridge from the fire escape); the sanctuary then pushes its
    /// blocker cage collision aside instead of across the doorway and skips arming its
    /// slot-4 actor.
    GAME_FLAG_SANCTUARY_BLOCKER_CLEARED = 0x006,
    /// One-shot latch for the sanctuary's scripted event: 1 when the sanctuary hotspot
    /// (0x13EF sub-id 1) spawns the room task, 2 when the player leaves to the
    /// promenade or reaches the roof garden first (also raising bit-2 flag 0x13), which
    /// suppresses the event.
    GAME_FLAG_SANCTUARY_EVENT_LATCH = 0x007,
    /// Patio door to the cafeteria: 0/1 locked (trying it plays CAP 3 and sets 1), 2
    /// once the east elevator hall scene has run (door can be opened: spawns the
    /// opening task and marks item 0x101 seen), 3 after the opening CAP reports key 2
    /// and the warp to the cafeteria starts.
    GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE = 0x008,
    /// Bitmask of the two locks released at the security-room panel: bit 0 when item
    /// 0x104 is used (also sets flag 1 to 2), bit 1 when item 0x103 is used. Door gates
    /// at the fountain/observatory (bit 0) and hallway/patio/forked road (bit 1) answer
    /// room 2 once their bit is set.
    GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED = 0x009,
    /// Bitmask of one-shot security-monitor events: bit 0 once camera view 0xB has been
    /// shown (starts CAP slot 0xD), bit 1 once camera view 0xA's enemy scene has played
    /// (actor_311900 sets it, and is torn down on later loads).
    GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN = 0x00A,
    /// Unidentified. Written every tick by enemy actor_510900 with a value from its
    /// table D_actor_510900_80167CEC indexed by its state; no reader found.
    GAME_FLAG_00D = 0x00D,
    /// Unidentified. Akropolis cafeteria-related state: 1 after the cafeteria scene, 2
    /// when the patio door message (3) is used while it equals 3, 4 when the follow-up
    /// room cutscene advances flag 0 from 2 to 3. Meaning of values unclear.
    GAME_FLAG_00E = 0x00E,
    /// Set to 1 the first time the bridge is entered from the fire escape, which plays
    /// the bridge's arrival scene (and also sets flag 6); passed to the bridge's view-
    /// sprite helper on entry.
    GAME_FLAG_BRIDGE_ARRIVAL_SCENE_SEEN = 0x010,
    /// Unidentified. Set to 1 when fountain point 4 is examined (CAP slot 4 starts and
    /// a collision trigger is swapped in); the swap is reapplied on later entries while
    /// set.
    GAME_FLAG_ACROPOLIS_FOUNTAIN_012 = 0x012,
    /// Set to 1 the first time the path between the fountain and the forked road is
    /// used (from the fountain side, or from the forked road after flag 0 reaches 3,
    /// which plays CAP slot 1 step 3 once).
    GAME_FLAG_FOUNTAIN_FORKED_ROAD_PATH_USED = 0x013,
    /// Unidentified. Set to 1 when a CAP scene ends: the patio's CAP 6 task when the
    /// scene reports event key 1, and the square's CAP 5 task on completion.
    GAME_FLAG_015 = 0x015,
    /// Unidentified. Set to 1 together with 0x18 and other story flags in a shelter B2
    /// laboratory story sequence; no reader found.
    GAME_FLAG_SHELTER_B2_LABORATORY_017 = 0x017,
    /// Unidentified. Set to 1 together with 0x17 and other story flags in a shelter B2
    /// laboratory story sequence; no reader found.
    GAME_FLAG_SHELTER_B2_LABORATORY_018 = 0x018,
    /// Unidentified. One-shot latch for an Akropolis square event: the first call sets
    /// it, switches the square to placement variant 2, raises eventState and spawns the
    /// room's task 0; later calls answer 1.
    GAME_FLAG_ACROPOLIS_SQUARE_01F = 0x01F,
    /// Opening Akropolis sequence: 1 when leaving the west elevator hall into the
    /// square (scene event 1), 3 when the patio's arrival scene plays, 4 once the
    /// patio's slot-4 actor is gone or a patio message is used at 3. Square, fountain
    /// and cafeteria doors answer 1 while it is below 2 and 2 after.
    GAME_FLAG_ACROPOLIS_OPENING_PROGRESS = 0x021,
    /// Set to 1 when the patio's cafeteria door is used while flag 0 is 2, which plays
    /// a scene once; afterwards the door runs CAP 8 instead.
    GAME_FLAG_PATIO_CAFETERIA_DOOR_SCENE_SEEN = 0x023,
    /// Set to 1 the first time the observatory is left for the forked road; later uses
    /// answer warp 5 instead. Also set when the sanctuary scene runs.
    GAME_FLAG_OBSERVATORY_EXIT_USED = 0x025,
    /// Unidentified. One-shot patio scene: set to 1 when trigger 2 is hit while flag 0
    /// is 2, playing a scene; while clear, entering the patio as variant 2 sends slot-4
    /// actors 2 and 3 a command.
    GAME_FLAG_ACROPOLIS_PATIO_026 = 0x026,
    /// Dryfield junk yard / trailer coach story counter: 1 when first entering the
    /// trailer coach from the junk yard (also companion flag 0x4B = 4), 2 after the
    /// trailer coach scene (sets 0x3A=1, 0x4B=1), 3 after a later trailer coach scene.
    /// Junk yard plays a one-shot scene once it reaches 2.
    GAME_FLAG_TRAILER_COACH_PROGRESS = 0x028,
    /// Index (0-4) into the security-room camera table
    /// D_acropolis_security_room_801826B4 of the feed on screen when the monitor was
    /// last left (0 if not in the table); the monitor reopens on that camera.
    GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA = 0x02A,
    /// Unidentified. Set to 1 by the factory's first action-1 hotspot use (starts CAP
    /// 0xB) and by the garage Gary cutscene actor; no other reader found.
    GAME_FLAG_02C = 0x02C,
    /// Set to 1 when the garage cutscene actor (actor_120300, Gary Douglas model) plays
    /// its scripted scene; on later loads the actor skips straight to its post-scene
    /// state.
    GAME_FLAG_GARAGE_GARY_SCENE_SEEN = 0x02D,
    /// Event-gate flag for the motel balcony door to motel room 6 (area 0x1E), which
    /// needs collected item bit 0xF; set to 1 when the door is unlocked. Also read by
    /// the garage Gary actor (sets its work field_4D6).
    GAME_FLAG_MOTEL_ROOM_6_DOOR_UNLOCKED = 0x02E,
    /// Set to 1 the first time the garage door to the junk yard is used after it opens
    /// (flag 0x33 set); at that moment companion flag 0x4B is set to 3.
    GAME_FLAG_GARAGE_JUNK_YARD_DOOR_PASSED = 0x02F,
    /// Unidentified. Set to 1 when the motel room 6 door event fires; while 1 the
    /// driveway's door to the factory is refused (CAP 2), and the factory's warp to the
    /// driveway runs a one-shot event (negative gate flag) that clears it. Night motel
    /// room 6 also clears it.
    GAME_FLAG_030 = 0x030,
    /// Unidentified. One-shot latch: the first action-0 request in motel room 6 sets it
    /// and spawns the room's script task.
    GAME_FLAG_DRYFIELD_MOTEL_ROOM_6_031 = 0x031,
    /// Water tower event: 0 initially, 1 when the room-action trigger spawns its event
    /// task, 2 when the scene finishes (triggers disabled). By day the water tower's
    /// way to the water tank answers 0 (blocked) until it is 2.
    GAME_FLAG_WATER_TOWER_PROGRESS = 0x032,
    /// Set to 1 when the water tank's action-2 scene plays (also sets hint 0x155=3);
    /// the garage door to the junk yard is refused until it is set.
    GAME_FLAG_WATER_TANK_SCENE_SEEN = 0x033,
    /// Set by _roomEventGate when the G&R kitchen / water tower door is first opened:
    /// from the kitchen freely (CAP 3), from the water tower only with collected bit
    /// 0x10 (else CAP 6 locked message; marks item 0x110 seen). Once set the door
    /// passes freely; shared by day and night stages.
    GAME_FLAG_KITCHEN_WATER_TOWER_DOOR_UNLOCKED = 0x034,
    /// Event-gate flag for the door between the parking lot and the saloon (G&R): from
    /// the parking lot it needs collected item bit 0x10, from the saloon it opens
    /// freely. Night garage story sequence also sets it.
    GAME_FLAG_SALOON_PARKING_LOT_DOOR_UNLOCKED = 0x035,
    /// Unidentified. One-shot latch: the water tank's first action-1 request sets it,
    /// spawns task 0 of the room's table and sets nibble 0x56 to 1.
    GAME_FLAG_DRYFIELD_WATER_TANK_036 = 0x036,
    /// Event-gate flag for the door between the breezeway and the factory: opened from
    /// the breezeway with collected item bit 0x15; the factory side refuses the
    /// breezeway door (CAP 0xD) while it is clear.
    GAME_FLAG_BREEZEWAY_FACTORY_DOOR_UNLOCKED = 0x037,
    /// Junk yard companion event: 1 when the action-1 hotspot starts the sequence task,
    /// 2 when the scene finishes (companion past x 0x5209 on action 2, or using the
    /// trailer-coach door while 1). While 0 the companion is sent its opening messages
    /// on entry.
    GAME_FLAG_JUNK_YARD_PROGRESS = 0x038,
    /// One-shot latch for the junk yard scene that plays on entry once flag 0x28 has
    /// reached 2.
    GAME_FLAG_JUNK_YARD_RETURN_SCENE_SEEN = 0x039,
    /// Driveway story event: 1 after the trailer coach scene, 2 when the driveway's
    /// room-action trigger fires and its cutscene plays. Below 2 the driveway refuses
    /// the water hole door; doors leading to the driveway pick room 2 by day once it is
    /// 2; junk yard CAP point 6 switches command once it is set.
    GAME_FLAG_DRIVEWAY_PROGRESS = 0x03A,
    /// Event-gate flag for the door between the gas station and the general store:
    /// unlocked from the general store side (no item); the gas station refuses it (CAP
    /// 7) while clear.
    GAME_FLAG_GENERAL_STORE_DOOR_UNLOCKED = 0x03B,
    /// Set to 1 when the warehouse's room-action trigger fires and its cutscene starts.
    /// Afterwards the warehouse's door to the dilapidated house works (refused with CAP
    /// 3 before) and the back street / dilapidated house doors to the warehouse answer
    /// room 2.
    GAME_FLAG_WAREHOUSE_EVENT_SEEN = 0x03C,
    /// Event-gate flag for the door between the back street and the dilapidated house:
    /// unlocked from inside the (night) dilapidated house; the back street refuses it
    /// (CAP 2 by day, 9 at night) while clear.
    GAME_FLAG_DILAPIDATED_HOUSE_DOOR_UNLOCKED = 0x03F,
    /// Dryfield parking-lot door to the motel lobby (area 0x11): clear means locked;
    /// opening it needs collected bit 0x12 (else CAP 1), runs CAP 6, sets the flag,
    /// applies gParkingLotAreaRecs and sets nibbles 0x46 and 0x97.
    GAME_FLAG_PARKING_LOT_LOBBY_DOOR_UNLOCKED = 0x040,
    /// Main-street door to motel room 3 (area 0xD): locked while clear; opened with
    /// collected bit 0x13 (else CAP 5), running CAP 0xA, and the key swap clears
    /// collected bits 0x10F/0x112 and marks item 0x113 seen.
    GAME_FLAG_MOTEL_ROOM_3_DOOR_UNLOCKED = 0x041,
    /// Main-street door to motel room 4 (area 0xE): locked while clear; opened with
    /// collected bit 0x13 (else CAP 6), running CAP 0xB, with the same 0x10F/0x112 ->
    /// 0x113 item-bit swap.
    GAME_FLAG_MOTEL_ROOM_4_DOOR_UNLOCKED = 0x042,
    /// Motel-balcony door to motel room 5 (area 0x1C): locked while clear; opened with
    /// collected bit 0x13 (else CAP 4), running CAP 7, with the 0x10F/0x112 -> 0x113
    /// item-bit swap.
    GAME_FLAG_MOTEL_ROOM_5_DOOR_UNLOCKED = 0x043,
    /// Motel-balcony door to the motel loft (area 0x1F): locked while clear; opened
    /// with collected bit 0x13 (else CAP 2), running CAP 5, with the 0x10F/0x112 ->
    /// 0x113 item-bit swap.
    GAME_FLAG_MOTEL_LOFT_DOOR_UNLOCKED = 0x044,
    /// While 1 the gas station's way to main street is refused with CAP 8. Set by the
    /// dilapidated house scene-clear task and cleared when the night gas station
    /// sequence is armed.
    GAME_FLAG_GAS_STATION_MAIN_STREET_BLOCKED = 0x045,
    /// Unidentified. Set to 1 when the parking lot's motel-lobby event (gate flag 0x40,
    /// item bit 0x12) fires; while 1 the night junk yard's door to the trailer coach
    /// runs CAP 4 and is refused. actor_136100 clears it.
    GAME_FLAG_046 = 0x046,
    /// Set to 1 by the factory's white-out cutscene; the factory's barrier collision
    /// then slides 2000 units aside, the factory CAP scene is skipped, panel views
    /// switch 0x12->0x13, and the breezeway/driveway/garage doors to the factory answer
    /// room 2.
    GAME_FLAG_FACTORY_BARRIER_CLEARED = 0x047,
    /// Set to 1 by the factory power cutscene (CAP event key 3, together with 0x4A=1);
    /// with it set the factory panel's lift controls work, its view switches 0xC->5 and
    /// a glow is drawn.
    GAME_FLAG_FACTORY_POWER_ON = 0x048,
    /// Bitmask of the factory lift's position: bit 1 raised/lowered by panel steps 0/1,
    /// bit 0 turned out/back (toggled by panel step 2 and the lift turn handlers). The
    /// lift model seeds its state from it.
    GAME_FLAG_FACTORY_LIFT_POSITION = 0x049,
    /// Factory lamp/power stage: 1 after the power cutscene, 2 after the lamp cutscene
    /// (CAP key 3) or command 21. The factory's door to the garage is refused (CAP slot
    /// 4) until 2, and the glow position differs between 1 and 2.
    GAME_FLAG_FACTORY_LAMP_PROGRESS = 0x04A,
    /// Story index into the room-presence table D_80114198 for companion type 2:
    /// selects which Dryfield rooms that ally appears in and is passed as its spawn
    /// variant. Values 1-7 set across the Dryfield story (trailer coach 1, driveway 2,
    /// garage 3, junk yard 4, night garage 5, dilapidated house 6, actor_146000 7), 0
    /// for none.
    GAME_FLAG_COMPANION_2_SCHEDULE = 0x04B,
    /// Story index into the room-presence table D_801141F0 for companion type 1 (the
    /// combat ally with four variants, apparently Kyle): selects the rooms/stage where
    /// that ally joins. Set 1-4 across Dryfield and the mine (e.g. 4 entering the mine
    /// mesa), 0 when he leaves.
    GAME_FLAG_COMPANION_1_SCHEDULE = 0x04C,
    /// Story index into the room-presence table D_80114248 for companion type 3: 1 set
    /// in the shelter B6 training room sequence, 0 cleared by actor_450900.
    GAME_FLAG_COMPANION_3_SCHEDULE = 0x04D,
    /// Transient factory hatch state: 1 while the hatch cutscene is open (set at its
    /// start, cleared after its CAP command); the hatch model watches it to animate
    /// open/closed and loads rotated open when 1.
    GAME_FLAG_FACTORY_HATCH_OPEN = 0x04E,
    /// Unidentified. Read only: in the day trailer-coach scene task, while collected
    /// bit 0x111 is not held and this nibble is non-zero, the conversation plays the
    /// 0x80186D2C/0x80187074 lines (first time latching 0xFD). Never set in C;
    /// presumably set by CAP script or area records.
    GAME_FLAG_DRYFIELD_TRAILER_COACH_04F = 0x04F,
    /// Unidentified. Read only: on the day driveway (stage 2, variant 1, nibble 0x3A !=
    /// 2), a transition request to the water hole spawns drivewayCutsceneTask instead
    /// while this nibble is clear. No C setter.
    GAME_FLAG_050 = 0x050,
    /// Toggle (0/1) flipped by the underpass's first switch (underpassSwitchMsg arg 1,
    /// CAP 1). Selects the water-hole and underpass room variants (1 when set, 2 when
    /// clear, +2 with 0x53; 5/6 without 0xC9) and, at 1, lights the night water hole's
    /// glow shafts.
    GAME_FLAG_UNDERPASS_SWITCH_1 = 0x051,
    /// Toggle (0/1) flipped by the underpass's second switch (underpassSwitchMsg arg 2,
    /// CAP 2). Selects the cellar room variant (1 when set, 2 when clear) and at 1
    /// draws the glow sprites in the day and night cellar.
    GAME_FLAG_UNDERPASS_SWITCH_2 = 0x052,
    /// Unidentified. Read only in C: adds 2 to the water-hole/underpass room variant
    /// choice (with 0x51, 0xC9) and suppresses the underpass glow sprites (day and
    /// night) and is read the same way in shelter_b4_water_supply. Setter not in C
    /// (script/area records).
    GAME_FLAG_053 = 0x053,
    /// Latched (1) the first time the player takes the day motel room 6 exit toward the
    /// water tower: that first request is refused and CAP command 7 plays; later
    /// requests pass.
    GAME_FLAG_MOTEL_ROOM_6_WATER_TOWER_EXIT_SEEN = 0x054,
    /// State of the water tower/tank mechanism: 0 initial; 2 when operated at the water
    /// tower (CAP 7 key 0xA, or cap-script step), dropped back to 1 on the next room
    /// transition request and set to 1 when the cap script restores the collision; 3
    /// once operated from the water tank (CAP slot 0xE key 0xA). 0-1 show the tower
    /// sprites and tank model, 2-3 hide them.
    GAME_FLAG_WATER_TOWER_MECHANISM_STATE = 0x055,
    /// Progress at the breezeway's factory door: 1 set at the water tank event (with
    /// 0x36); 2 after the CAP answer 0xB or once collected bit 0x11B is held; 3 when
    /// collected bit 0x115 is held; 5/6 while 0xFE is set without 0x11B; 4 once the
    /// factory door event gate (flag 0x37, collected bit 0x15) latches, freezing the
    /// counter. >=2 enables hotspot 3.
    GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS = 0x056,
    /// Main street: the first transition to motel room 1 (area 0xB) plays CAP 3 and
    /// stage sound 0x52020005 before the room change. Set to 1 when the latched one-
    /// shot event starts; while set the transition goes through with no event.
    GAME_FLAG_MAIN_STREET_TO_MOTEL_ROOM_1_SCENE = 0x057,
    /// Main street: the first transition to motel room 2 (area 0xC) plays CAP 4 and
    /// stage sound 0x52020005 before the room change. Set to 1 when the latched one-
    /// shot event starts; while set the transition goes through with no event.
    GAME_FLAG_MAIN_STREET_TO_MOTEL_ROOM_2_SCENE = 0x058,
    /// Latched (1) when the night saloon's action point 7 plays its one-time cutscene.
    /// Also forced to 1 by the dilapidated-house and night motel room 6 story
    /// transitions and reset to 0 in the night garage.
    GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN = 0x059,
    /// Night saloon conversation state: 0 -> event 8 plays the first script and sets 1;
    /// 1 -> event 8 plays the repeat script; event 10 has a line while < 2; 2 (set by
    /// the dilapidated-house / motel room 6 story transitions) silences both. Reset to
    /// 0 in the night garage.
    GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS = 0x05A,
    /// Latched (1) the first time the night trailer-coach conversation ends with CAP
    /// key 0xD: clears 0x4C, applies area records and plays the story cutscene; later
    /// picks play a short script instead.
    GAME_FLAG_NIGHT_TRAILER_COACH_STORY_SCENE_SEEN = 0x05B,
    /// Latched (1) when the day motel room 1's hotspot (sub-id 1) on variant 3 arms the
    /// room's script task; while clear on variant 3 the entry task announces the room
    /// to the scene task.
    GAME_FLAG_MOTEL_ROOM_1_EVENT_SEEN = 0x05C,
    /// Latched (1) when the breezeway's hotspot sub-id 1 first arms the room event
    /// task; while clear the room entry spawns the event setup and the room effect runs
    /// its view-2 behaviour, at 1 it stops it.
    GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN = 0x05D,
    /// Day general store cutscene: 0 not seen (entry broadcasts the actor setup); 1
    /// cutscene task spawned by message 1; 2 done (message 2 or the next room entry
    /// advances it).
    GAME_FLAG_GENERAL_STORE_CUTSCENE_STATE = 0x05E,
    /// Latched (1) when the day main street's action point 1 plays its one-time
    /// cutscene (also clears nibble 3, sets 0x155); while clear the room entry
    /// broadcasts the scene setup.
    GAME_FLAG_MAIN_STREET_CUTSCENE_SEEN = 0x05F,
    /// Latched (1) when the day toilet's hotspot on variant 1 plays its cutscene pair.
    /// Once set, a collision face is shifted 2000 units -x and actor_323300 no longer
    /// spawns.
    GAME_FLAG_TOILET_EVENT_SEEN = 0x060,
    /// Set (1) when the night motel balcony's story scene plays (variant 2, room 2). It
    /// then switches Dryfield night room variants (main street, parking lot, gas
    /// station, saloon: 0x61+1; balcony 3) and alternative CAP lines in motel room 6,
    /// and refuses driveway->main street with CAP 6.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN = 0x061,
    /// While set, the general store's passage to the underpass (area 0x26) runs CAP
    /// command 0xE instead of spawning storeCutsceneTask. Set by the dilapidated-house
    /// scene clear, cleared when the night gas station sequence arms.
    GAME_FLAG_GENERAL_STORE_UNDERPASS_BLOCKED = 0x062,
    /// Night gas station sequence: 0 not started (scatter effects drawn); 1 sequence
    /// armed (script played, area records applied); 2 after the follow-up cutscene,
    /// enabling the companion's scenes there. Non-zero also changes the gas station
    /// room variant chosen from main street / store.
    GAME_FLAG_NIGHT_GAS_STATION_PROGRESS = 0x063,
    /// Latched (1) the first time the player enters the night trailer coach from the
    /// junk yard; that first transition uses warp point 2 (which plays the arrival
    /// cutscene).
    GAME_FLAG_NIGHT_TRAILER_COACH_FIRST_ENTRY = 0x064,
    /// Night garage event: 0 not done; 1 when, with items 0x113 and 0x118 held, action
    /// point 6 on variant 2 plays the cutscene (0x118 consumed, triggers swapped); 2 on
    /// the next garage entry. Positive values place/remove actor_135400 and enable the
    /// motel room 6 rest event.
    GAME_FLAG_NIGHT_GARAGE_PROGRESS = 0x06C,
    /// Set to 2 when the night motel room 6 story event (CAP 0x10) is accepted: HP/MP
    /// refilled, story flags updated and the game moved to area 8. While < 2 (and 0x6C
    /// > 0) event 6 offers it.
    GAME_FLAG_NIGHT_MOTEL_ROOM_6_REST_TAKEN = 0x070,
    /// Latched (1) when the mine mesa's action point 1 fires its one-time event
    /// (companion line, 0xA2/0x1C); while clear, action point 2 plays the companion
    /// cutscenes (using 0x91).
    GAME_FLAG_MINE_MESA_TRIGGER_1_SEEN = 0x071,
    /// Unidentified. Counter 0..2 incremented by actor_136300's event-script callback
    /// each time it runs; selects CAP slot 0x10 + value.
    GAME_FLAG_072 = 0x072,
    /// Set (1) by the Burner actor (actor_403100) 0xBE frames into its post-fight top-
    /// level state. Read to choose later story branches: night main street cutscene
    /// phase, junk yard -> trailer warp 3, actor_146000 cutscene, and the 0x4B/0x4C
    /// hint values in the shelter tent and garbage incinerator.
    GAME_FLAG_BURNER_DEFEATED = 0x073,
    /// Latched (1) at the end of the night motel lobby's event task (spawned from
    /// action point 1 while clear), after it applies the room's area records.
    GAME_FLAG_NIGHT_MOTEL_LOBBY_EVENT_SEEN = 0x074,
    /// Set (1) when the forked tunnel's mechanism at action point 1 is confirmed (CAP 1
    /// key 1, cutscene). Once set the room collision is lowered 0xBB8 and the moving
    /// object uses its alternate placement; action 1 no longer offers it.
    GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED = 0x075,
    /// Set (1) when the sterilization room's action point 1 cutscene plays (requires
    /// 0x84). While set and 0x77 clear, the room's hazard runs and normal interactions
    /// are replaced by CAP 0xA.
    GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED = 0x076,
    /// Set (1) when the sterilization room's CAP 8 prompt returns key 1 (cutscene,
    /// restart mode normal). While clear the room's task damages the player
    /// periodically; set restores normal interactions and alters the CAP lines.
    GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED = 0x077,
    /// Latched (1) on the first variant-1 entry into the shelter B3 dumping hole
    /// (cutscene when arriving at warp 3, 0xA2/0x21).
    GAME_FLAG_DUMPING_HOLE_ARRIVAL_SEEN = 0x078,
    /// Latched (1) when the night parking lot's action point 1 on variant 3 plays its
    /// script; once set the room entry sets actor01600Wave to 2.
    GAME_FLAG_NIGHT_PARKING_LOT_EVENT_SEEN = 0x079,
    /// Global story chapter: 0 start (MIST arrival caption); 1 Akropolis west elevator
    /// hall scene; 2 Dryfield gas station arrival; 3 night dilapidated house; 4 end of
    /// Dryfield night (actor_136300 event); 5 shelter B2 main corridor; 6 shelter 1F
    /// bulwark. Drives boot captions, music, room variants and cutscene choices.
    GAME_FLAG_STORY_CHAPTER = 0x07A,
    /// Set to 2 at the end of the night water tank event; then 3, 4 and 5 as the item
    /// with collected bit 0x119 is handed to actor_146300's character (bit cleared each
    /// time). >=2 enables the main street talk.
    GAME_FLAG_ITEM_119_HANDOVER_PROGRESS = 0x07B,
    /// Set (1) when the night main street cutscene actor (actor_136100) starts its
    /// scene; once set the actor kills itself on spawn. Junk yard point 8 uses it with
    /// 0x73.
    GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN = 0x07C,
    /// Latched (1) when the shelter B1 pod access tunnel's room entry plays its one-
    /// time cutscene (0xA2/0x1E).
    GAME_FLAG_POD_ACCESS_TUNNEL_SCENE_SEEN = 0x07E,
    /// Unidentified. Set to 1 by actor_335800 when it hides two sprite batches of view
    /// record 39; then the night motel balcony spawns effect 0x60094, turns two glows
    /// off and moves it to 2, night main street drops two glow anchors, and night-
    /// Dryfield map flag 0x1D reports 0x802.
    GAME_FLAG_07F = 0x07F,
    /// Unidentified. Set (1) in shelter_r47 at point 1 when 0x83 is clear (companion
    /// cutscene, ally HP refilled, 0x4C=8, 0xD1=2). Disables a collision trigger and
    /// selects task 1 for request 8; the branch counterpart of 0x83 there.
    GAME_FLAG_SHELTER_R47_080 = 0x080,
    /// Latched (1) when shelter_r47 point 2 plays sound 0x542F0001 once while 0x83 is
    /// 1.
    GAME_FLAG_SHELTER_R47_POINT_2_SOUND_PLAYED = 0x081,
    /// shelter_r47 event sequence (with 0x83 = 1): 1 after point 3's cutscene; 2 and 4
    /// when the room holds the player and spawns its task; 3 and 5 after the two
    /// follow-up cutscenes. Point 4 plays a line at >=4; >=2 enables a B2 main corridor
    /// event.
    GAME_FLAG_SHELTER_R47_EVENT_PROGRESS = 0x082,
    /// Unidentified. Set (1) at the end of the night water tank event (with 0x7B=2) and
    /// read as a story branch through the shelter and Neo Ark: shelter_r47 events, B2
    /// laboratory CAP lines and area records, actor_443500 visibility,
    /// actor_450800/neo_ark_observatory cutscene choice, pod access tunnel ally event.
    GAME_FLAG_083 = 0x083,
    /// Latched (1) when the shelter B2 north maintenance walkway's hotspot on variant 1
    /// plays its cutscene and applies area records. Read by the B1 north walkway's
    /// sprite setup and required for the sterilization room's point-1 event.
    GAME_FLAG_B2_NORTH_WALKWAY_SCENE_SEEN = 0x084,
    /// Saved scenery state of night motel balcony section 0 (0 intact, 1/2 altered
    /// sprite sets), written by func_dryfield_night_motel_balcony_8017E250 when the
    /// Burner (actor_403100) fire hits that region, and reapplied on room entry.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_0_STATE = 0x085,
    /// Saved scenery state of night motel balcony section 1 (0 intact, 1/2 altered
    /// sprite sets), written by func_dryfield_night_motel_balcony_8017E250 when the
    /// Burner (actor_403100) fire hits that region, and reapplied on room entry.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_1_STATE = 0x086,
    /// Saved scenery state of night motel balcony section 2 (0 intact, 1/2 altered
    /// sprite sets), written by func_dryfield_night_motel_balcony_8017E250 when the
    /// Burner (actor_403100) fire hits that region, and reapplied on room entry.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_2_STATE = 0x087,
    /// Saved scenery state of night motel balcony section 3 (0 intact, 1/2 altered
    /// sprite sets), written by func_dryfield_night_motel_balcony_8017E250 when the
    /// Burner (actor_403100) fire hits that region, and reapplied on room entry.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_3_STATE = 0x088,
    /// Saved scenery state of night motel balcony section 4 (0 intact, 1/2 altered
    /// sprite sets), written by func_dryfield_night_motel_balcony_8017E250 when the
    /// Burner (actor_403100) fire hits that region, and reapplied on room entry.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_4_STATE = 0x089,
    /// Saved scenery state of night motel balcony section 5 (0 intact, 1/2 altered
    /// sprite sets), written by func_dryfield_night_motel_balcony_8017E250 when the
    /// Burner (actor_403100) fire hits that region, and reapplied on room entry.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_5_STATE = 0x08A,
    /// Saved scenery state of night motel balcony section 6 (0 intact, 1/2 altered
    /// sprite sets), written by func_dryfield_night_motel_balcony_8017E250 when the
    /// Burner (actor_403100) fire hits that region, and reapplied on room entry.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_6_STATE = 0x08B,
    /// Saved scenery state of night motel balcony section 7 (0 intact, 1/2 altered
    /// sprite sets), written by func_dryfield_night_motel_balcony_8017E250 when the
    /// Burner (actor_403100) fire hits that region, and reapplied on room entry.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_7_STATE = 0x08C,
    /// Saved scenery state of night motel balcony section 8 (0 intact, 1/2 altered
    /// sprite sets), written by func_dryfield_night_motel_balcony_8017E250 when the
    /// Burner (actor_403100) fire hits that region, and reapplied on room entry. Also
    /// read by the night gas station to show/hide two sprite records.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_8_STATE = 0x08D,
    /// Latched (1) the first time the night garage is entered on variant 3 with the
    /// companion present (ally HP refilled, cutscene); later entries play a short
    /// script.
    GAME_FLAG_NIGHT_GARAGE_COMPANION_SCENE_SEEN = 0x08E,
    /// Latched (0->1) the first time the night junk yard's door to the trailer coach is
    /// used while 0x73==1, 0x61==1 and story nibble 0x7A==3; that one transition
    /// arrives through warp 3 instead of the normal one.
    GAME_FLAG_TRAILER_COACH_NIGHT_EVENT_ENTRY = 0x08F,
    /// Set to 1 when the mine mesa's first-arrival scene (companion dialogue, caption
    /// 0xA2/0x1B) has played on place 1; while clear the room entry also seeds the
    /// companion's HP and spawns the arrival task.
    GAME_FLAG_MINE_MESA_ARRIVAL_SEEN = 0x090,
    /// Set to 1 the first time the mine mesa's action 2 hotspot is examined (before
    /// nibble 0x71); later examinations play a different companion script.
    GAME_FLAG_MINE_MESA_COMPANION_TALK_SEEN = 0x091,
    /// Set to 1 on the first entry to the night dilapidated house, which plays its
    /// cutscene and advances story nibble 0x7A to 3.
    GAME_FLAG_NIGHT_DILAPIDATED_HOUSE_EVENT_SEEN = 0x092,
    /// Unidentified. Read only by the shared main street handler: after the motel room
    /// 3 door (message 0xD, key gate on 0x41 / collected bit 0x13), while this nibble
    /// is clear the door's own warp-flag nibble is reset to 0. Nothing in C sets it.
    GAME_FLAG_093 = 0x093,
    /// Unidentified. Same as 0x93 for the motel room 4 door (message 0xE, gate on
    /// 0x42): while clear the door's warp-flag nibble is reset to 0. Nothing in C sets
    /// it.
    GAME_FLAG_094 = 0x094,
    /// Set to 1 the first time the night water hole is entered on place 1 through
    /// arrival 1 or 2, playing that arrival's script; once set, later place-1 entries
    /// send the slot-4 actor an apply message keyed by the arrival.
    GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT = 0x095,
    /// Set to 1 when the motel loft event fires after collected bit 0x117 is first seen
    /// (script, caption 0xA2/0x15, sceneEvent 3); once set the loft entry reconfigures
    /// the slot-4 actor and the motel balcony door resolves the loft as room 2 instead
    /// of 1.
    GAME_FLAG_NIGHT_MOTEL_LOFT_EVENT_SEEN = 0x096,
    /// Unidentified. Set to 1 together with 0x46 when the parking lot's key-gated event
    /// for message 0x11 (nibble 0x40, collected bit 0x12) fires; while set the night
    /// garage's action-1 hotspot starts CAP slot 0x14 instead of 0x36. Cleared by the
    /// Dryfield-to-mine departure.
    GAME_FLAG_097 = 0x097,
    /// Unidentified. Set to 1 by actor_136300's departure sequence that moves the game
    /// from Dryfield to the mine mesa (with 0x7A=4); no reader in C.
    GAME_FLAG_098 = 0x098,
    /// Unidentified. Set to 1 by the same Dryfield-to-mine departure sequence as 0x98;
    /// no reader in C.
    GAME_FLAG_09A = 0x09A,
    /// Set to 1 the first time the night junk yard is entered on place 1 through
    /// arrival 3, playing its script; when set it hides sprite command 5 of the room's
    /// view, re-announces the scene on place-1 entries, and the full Dryfield map
    /// resolves the junk yard (area 0x1A) as room nibble+1.
    GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN = 0x09F,
    /// Set to 1 on the first entry to the night gas station, which plays its arrival
    /// scene (caption 0xA2/0x12) and sets nibble 0x4C to 2.
    GAME_FLAG_NIGHT_GAS_STATION_FIRST_VISIT = 0x0A0,
    /// Set to 1 the first time the mine tunnel is entered on place 1 through arrival 1
    /// (weapon taken away, script plays); when it is 1 on later place-1 entries the
    /// room calls its setup with 2.
    GAME_FLAG_MINE_TUNNEL_EVENT_SEEN = 0x0A1,
    /// Set to 1 when the mine gorge's room-event collision trigger fires its cutscene,
    /// which switches the session to room 2; the shelter room resolver then gives the
    /// gorge (area 5) room nibble+1.
    GAME_FLAG_MINE_GORGE_TRIGGER_EVENT_DONE = 0x0A4,
    /// Door between B1 main corridor and B1 elevator hall: written by the main
    /// corridor's unkeyed event gate (it opens from that side); while 0 the elevator
    /// hall side reports it locked (CAP 2). actor_450900 resets it to 0.
    GAME_FLAG_B1_CORRIDOR_ELEVATOR_HALL_UNLOCKED = 0x0A5,
    /// Door between B1 armory and B1 storeroom: written by the armory's unkeyed event
    /// gate; while 0 the storeroom side reports it locked.
    GAME_FLAG_B1_ARMORY_STOREROOM_DOOR_UNLOCKED = 0x0A6,
    /// Door between B3 elevator hall and the incinerator control room: written by the
    /// elevator hall's unkeyed event gate; while 0 the control room side refuses the
    /// transition.
    GAME_FLAG_B3_INCINERATOR_CONTROL_DOOR_UNLOCKED = 0x0A7,
    /// Door between B2 north maintenance walkway and operating room: written by the
    /// walkway's event gate (requires collected bit 0x22) or the operating room's
    /// unkeyed gate; when set the walkway's door indicator glow turns from red to blue.
    GAME_FLAG_OPERATING_ROOM_NORTH_DOOR_UNLOCKED = 0x0A8,
    /// Door between B2 elevator hall and B2 south maintenance walkway, unlocked through
    /// the elevator hall's event gate with collected bit 0x21; when set the hall's door
    /// indicator glow turns from red to blue.
    GAME_FLAG_B2_HALL_SOUTH_WALKWAY_DOOR_UNLOCKED = 0x0A9,
    /// Door between B2 south maintenance walkway and operating room: written by the
    /// walkway's unkeyed event gate; while 0 the operating room side reports it locked
    /// (CAP 3).
    GAME_FLAG_OPERATING_ROOM_SOUTH_DOOR_UNLOCKED = 0x0AA,
    /// Door between B2 elevator hall and B2 main corridor: written by the elevator
    /// hall's unkeyed event gate; while 0 the main corridor side reports it locked.
    /// actor_450900 sets it to 1.
    GAME_FLAG_B2_CORRIDOR_ELEVATOR_HALL_UNLOCKED = 0x0AB,
    /// First of the five lock toggles edited on the shelter_r47 terminal screen (0/1);
    /// while 0 the B1 main corridor's door to the transfer tunnel reports locked (CAP
    /// 1).
    GAME_FLAG_B1_TRANSFER_TUNNEL_DOOR_UNLOCKED = 0x0AC,
    /// Door between B1 access tunnel and B1 control room: written by the access
    /// tunnel's unkeyed event gate; while 0 the control room side refuses the
    /// transition.
    GAME_FLAG_B1_CONTROL_ROOM_TUNNEL_DOOR_UNLOCKED = 0x0AD,
    /// Third shelter_r47 terminal lock toggle (0/1), also set to 1 by a B2 main
    /// corridor event once 0x82>=2; while 0 the corridor's directed action 7 is refused
    /// (CAP 3), once set it departs to Neo Ark area 7 (or shelter area 0x31 while 0xDA
    /// is clear).
    GAME_FLAG_B2_CORRIDOR_OBSERVATORY_ACCESS = 0x0AE,
    /// Set to 1 on the first place-1 entry to the night toilet, which spawns entry 0 of
    /// the gameplay table Actor04000_D0C6FC.
    GAME_FLAG_NIGHT_TOILET_EVENT_SEEN = 0x0AF,
    /// Set to 1 when the night saloon's place-2 scripted actor sequence finishes
    /// (action 2 broadcasts command 1 to the actors); while clear, place-2 entries
    /// announce the scene to the actors and action 1 unlinks a room object with a
    /// sound.
    GAME_FLAG_NIGHT_SALOON_ENCOUNTER_DONE = 0x0B0,
    /// Door between B2 laboratory and B2 main corridor: written by the laboratory's
    /// unkeyed event gate; while 0 the main corridor side reports it locked.
    GAME_FLAG_B2_LABORATORY_DOOR_UNLOCKED = 0x0B1,
    /// Set to 1 by the 1F guardroom control cutscene (shows a sprite in the guardroom);
    /// while 0 the vehicular airlock's door to the bulwark is locked (CAP 2), and at 1
    /// the airlock draws extra indicator glows. Further guardroom uses run CAP 3.
    GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED = 0x0B2,
    /// Set to 1 by the B2 laboratory event that advances nibble 0xD0 from 2 to 3; while
    /// 0 the B1 pod access tunnel's door to shelter_r47 reports locked (CAP 3).
    GAME_FLAG_B1_POD_TUNNEL_R47_DOOR_UNLOCKED = 0x0B3,
    /// Set to 1 at the end of the B1 pod access tunnel event that moves the save to the
    /// pod service gantry; while 0 the B2 pod access tunnel's door to shelter_r48
    /// reports locked.
    GAME_FLAG_B2_POD_TUNNEL_R48_DOOR_UNLOCKED = 0x0B4,
    /// Output 1 of the mine power panel (actor_548100): set to 1 while the panel is
    /// switched on (0xC3) and the matched circuit powers output 1, else 0; while 0 the
    /// gorge's door to the cavern refuses (CAP 3). mine_gorge clears it when panel
    /// stage 0xBE reaches 2.
    GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED = 0x0B5,
    /// Set to 1 by the B4 reservoir event (with 0xB7); the shelter room resolver gives
    /// the B3 incinerator control room (area 41) room nibble+1.
    GAME_FLAG_INCINERATOR_CONTROL_ROOM_STATE = 0x0B6,
    /// Set to 1 when the B4 reservoir event finishes (room switches to 2); afterwards
    /// the reservoir resolves as room 2, its water level and glows change, the
    /// reservoir's door to the upper sewer is refused, the lower/upper sewer spawn
    /// extra tasks/effects, and map room 6 is enabled.
    GAME_FLAG_B4_RESERVOIR_EVENT_DONE = 0x0B7,
    /// Set to 1 by the B4 upper sewer's mechanism event; while clear the night water
    /// hole runs its water effects, and once set the water hole's command 2 departs to
    /// the shelter's B4 water supply and the water-supply valve action proceeds.
    GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN = 0x0B8,
    /// Set to 1 (with 0xDF) in Neo Ark power plant 2 once the tracked actor is gone;
    /// while 0 the EVE access tunnel's elevator transition runs the 'closed' CAP, once
    /// set it starts the cutscene leading to the EVE encounter.
    GAME_FLAG_NEO_ARK_EVE_ELEVATOR_UNLOCKED = 0x0B9,
    /// Set to 1 the first time the B2 elevator is taken from the B3 elevator hall
    /// (plays CAP 2); while 0 the B1 and B2 elevator halls refuse the elevator. Cleared
    /// to 0 by the 1F tent's first-entry reset.
    GAME_FLAG_SHELTER_ELEVATOR_ENABLED = 0x0BA,
    /// Mine power panel output 2 / secret passage state: 0 unpowered, 2 powered (panel
    /// stage 1), 3 powered (panel stage 2), 1 opened by the cavern CAP event 0x15. Only
    /// at 1 does the cavern let the player into the secret passage; the cavern and
    /// refuge hotspots pick their CAP by it.
    GAME_FLAG_MINE_SECRET_PASSAGE_STATE = 0x0BB,
    /// Set (to 1) by a callback in the Neo Ark submarine tunnel's event script; while
    /// clear, arriving through warp 2 on place 1 plays that script. actor_00400 also
    /// changes behaviour once it is set.
    GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN = 0x0BC,
    /// Set to 1 the first time the Neo Ark forest zone is entered on place 1 through
    /// arrival 1, starting its scripted scene; while clear the room setup announces
    /// that scene to the actors.
    GAME_FLAG_NEO_ARK_FOREST_ZONE_EVENT_SEEN = 0x0BD,
    /// Stage of the mine power panel puzzle (actor_548100): 0 never opened, 1 set on
    /// first opening (first circuit table), 2 after the cavern CAP event 0xB (second
    /// circuit table; output 2 then yields 0xBB=3).
    GAME_FLAG_MINE_POWER_PANEL_STAGE = 0x0BE,
    /// Highest of the panel's four socket nibbles 0xBF-0xC2 (socket + 0xBE, sockets
    /// counted from 1) that record which sockets are filled; preset to 1 when the
    /// panel is first opened.
    GAME_FLAG_MINE_POWER_PANEL_SOCKET_4 = 0x0C2,
    /// Set to 1 when the player throws the mine power panel's switch with a complete
    /// route, cleared on CAP event key 0x15 and by the cavern event; while set the
    /// panel energises outputs 0xB5/0xBB and the refuge draws indicator rings.
    GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON = 0x0C3,
    /// Unidentified. Set to 1 by the mine cavern's CAP event key 0xB (with 0xBE=2,
    /// 0xC3=0) and cleared by the refuge's action-1 event; while 1 the cavern spawns
    /// random 0x600E0 effects and its action-6 hotspot runs CAP 6.
    GAME_FLAG_0C4 = 0x0C4,
    /// Set to 1 when the gorge's action-1 trigger on place 1 starts its cutscene
    /// script; once set, place-1 entries set the combat wave to 0x15.
    GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN = 0x0C5,
    /// Nursery progress: 0 not entered, 1 set on first entry with its cutscene (caption
    /// 0x30), 2 after the nursery's command 0xA event (caption 0x31). Once non-zero the
    /// mine cavern, Neo Ark garden, B2 operating room and B1 underground parking switch
    /// to their later CAP texts.
    GAME_FLAG_B6_NURSERY_PROGRESS = 0x0C7,
    /// Counts (saturating at 3) the view-4 nursery scenes played by actor_450800 once
    /// 0xC7 is past 1; the first plays a caption-0x32 script, later ones a repeat
    /// script, and the count picks CAP slot 8 or 9. Non-zero enables the nursery's
    /// action-3 scene.
    GAME_FLAG_B6_NURSERY_SCENE_COUNT = 0x0C8,
    /// Set to 1 the first time the Dryfield underpass is entered on place 1 through
    /// arrival 1 (script plays). Once set, the underpass resolves to rooms 1-4 from
    /// switch nibbles 0x53/0x51, otherwise to 5 or 6.
    GAME_FLAG_UNDERPASS_EVENT_SEEN = 0x0C9,
    /// Set to 1 when the observatory's sub-id 1 trigger spawns its event task on room
    /// 2; while clear room-2 entries set combat wave 1, and the forked road's path-back
    /// message answers room 2.
    GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN = 0x0CA,
    /// Roof garden scene progress on places 1/7: 0 -> 1 on arrival 1, 1 -> 2 on arrival
    /// 2 (plays its script); at 2 the maggot/caterpillar actor spawns in its alternate
    /// placement.
    GAME_FLAG_ROOF_GARDEN_PROGRESS = 0x0CB,
    /// Set to 1 when the forked road's action-1 script plays on place 4 or 8 (after
    /// nibble 9 bit 1); at 1 the maggot/caterpillar actor spawns in its alternate
    /// placement.
    GAME_FLAG_FORKED_ROAD_EVENT_SEEN = 0x0CC,
    /// Unidentified. One-shot mine mesa event on variant 1 with the companion present:
    /// the actor-event handler sets it to 1 (and raises the attachment event lock in `Gp_StateC08.flags`),
    /// and the room tick then plays a scripted scene once the fight state clears.
    GAME_FLAG_MINE_MESA_0CD = 0x0CD,
    /// Set to 1 when the actor_400500 enemy (Gray Stalker models) is removed in its
    /// death path; Dryfield rooms then apply an extra set of area records.
    GAME_FLAG_GRAY_STALKER_DEFEATED = 0x0CE,
    /// Unidentified. Shelter elevator access state: 1 when the first-visit scene in
    /// shelter_b2_elevator runs, 2 when set at the Dryfield-night water hole (variant
    /// 0xA, companion present). While nonzero the B3 elevator hall offers the ride
    /// directly; while 0 the B4 water supply sets nibble 0x4C to 6 when the companion
    /// is present.
    GAME_FLAG_0CF = 0x0CF,
    /// Laboratory console progress: 1 when the console task reports success, 2 after
    /// actor_143000's scene, 3 once the follow-up event runs; below 2 the console
    /// starts the scripted task, from 2 it replays cap 6.
    GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS = 0x0D0,
    /// Unidentified. Multi-step companion/route progress across the Shelter and Neo
    /// Ark: 1 set leaving the B1 pod access tunnel toward shelter_r47 (ally HP
    /// refilled, 0x4C=7), 2 set in shelter_r47's scene (0x80=1, 0x4C=8), 3 set at the
    /// Neo Ark observatory. At 2 several Shelter transitions are refused with a cap
    /// message.
    GAME_FLAG_0D1 = 0x0D1,
    /// Fifth switch of the shelter_r47 control console (toggle 4); while it is 0 the
    /// Watcher actor (actor_102100) runs its line-of-sight scan. Cleared again by
    /// actor_450900's story reset.
    GAME_FLAG_SHELTER_WATCHERS_DISABLED = 0x0D2,
    /// Unidentified. One-shot event in shelter_b2_main_corridor (actionId 1 once nibble
    /// 0x82 >= 2): sets this to 1, sets console switch 0xAE, clears 0x1C4 and runs a
    /// script.
    GAME_FLAG_SHELTER_B2_MAIN_CORRIDOR_0D3 = 0x0D3,
    /// Dryfield-night gas station, room 4, event 0x17 after item bit 0x11E is
    /// collected: first examine sets 1 (cap 0x20), later ones set 2 (cap 0x1F); 0 keeps
    /// the default cap.
    GAME_FLAG_NIGHT_GAS_STATION_EXAMINE_STATE = 0x0D4,
    /// Second switch (toggle 1) of the shelter_r47 control console; its low bit picks
    /// area view 0x12 (off) or 0x24 (on).
    GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_2 = 0x0D5,
    /// Fourth switch (toggle 3) of the shelter_r47 control console, mirrored into
    /// `ShelterR47ConsoleWork::backdropToggle`.
    GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_4 = 0x0D6,
    /// Unidentified. One-shot Neo Ark observatory event (actionId 1): set to 1 and
    /// plays one of two scenes depending on nibble 0x83 (the other sets 0xD1 to 3).
    /// While set and the companion is present the observatory mesh is placed at vy 0
    /// instead of 10000; actor_450200 plays a script when set.
    GAME_FLAG_0D7 = 0x0D7,
    /// Unidentified. Set to 1 when the companion's save-point script in the B6 growth
    /// room (actor_450900) ends on cap event key 0xB (the confirming choice);
    /// afterwards the growth room runs cap 1 / 0x11 instead of the task / cap 0x10.
    GAME_FLAG_0D8 = 0x0D8,
    /// Altar choice in the Neo Ark altar: cap event key 11 sets 0, key 21 sets 1. It
    /// drives the altar switch sprites and walls, picks the room variant (value+1) for
    /// the linked area and the map room offset. Reset to 0 by actor_450900.
    GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE = 0x0D9,
    /// Unidentified. Set to 1 (with 0x7A=5) the first time the shelter_b2_main_corridor
    /// departure goes to area 0x31; afterwards destination 7 leads to Neo Ark area 7
    /// instead of Shelter area 0x31.
    GAME_FLAG_SHELTER_B2_MAIN_CORRIDOR_0DA = 0x0DA,
    /// Set to 1 when the shrine's arrangement puzzle completes (helper returns 1);
    /// while clear the transition to Power Plant 1 is refused.
    GAME_FLAG_NEO_ARK_SHRINE_PUZZLE_SOLVED = 0x0DB,
    /// Set when the first tile sequence (D_neo_ark_altar_8017F050) is walked on the
    /// altar; opens the garden's transition to the Substation and selects room
    /// variants.
    GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED = 0x0DC,
    /// Set when the second tile sequence (D_neo_ark_altar_8017F068) is walked on the
    /// altar; selects room variants in the Neo Ark map resolver.
    GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED = 0x0DD,
    /// Set to 1 in Power Plant 1 once the area's first placed actor is no longer
    /// present (also sets 0xF6, area recs, sceneEvent 0x16); stops the room's random
    /// spark effect and switches its cap messages.
    GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED = 0x0DE,
    /// Set to 1 in Power Plant 2 once the area's first placed actor is no longer
    /// present (sets 0xB9, area recs, sceneEvent 0x17); many Neo Ark/Shelter rooms
    /// switch scripts and routes on it.
    GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED = 0x0DF,
    /// Set to 1 when the Dryfield-night trailer coach plays its one-time scene
    /// (requires 0x7A >= 4); also picks 0x20/0x21 for shopOpenSession.
    GAME_FLAG_NIGHT_TRAILER_COACH_CHAPTER4_SCENE_SEEN = 0x0E0,
    /// Unidentified. One-shot Neo Ark observatory event (actionId 2): sets 1, sets
    /// 0x155=6, sceneEvent 0x15 and plays a scene; selects a room variant (value+1) in
    /// the Neo Ark map resolver and an effect on observatory entry.
    GAME_FLAG_0E1 = 0x0E1,
    /// Bitmask (bits 0-3) of the mine cavern's four destructible points: a bit is set
    /// when that object's HP reaches 0 and its model is hidden. The bit count sets the
    /// darkness tint and glows; actor_403000 (Blizzard Chaser) reacts when a new bit
    /// appears near it.
    GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED = 0x0E2,
    /// Conversation counter (0-4) for the Shelter 1F heliport talker (actor_260400
    /// Rupert / actor_260500 Jodie); each talk plays the next script and advances it.
    /// While 0 on variant 1 the transition to the Bulwark is refused.
    GAME_FLAG_HELIPORT_TALK_PROGRESS = 0x0E3,
    /// Heliport soldier (actor_161500) exchange with the companion present: 0 none, 1
    /// after the first scene (request pending), 2 once item 0x124 is handed over (item
    /// seen bit 0x124 set).
    GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE = 0x0E4,
    /// Set to 1 at the end of actor_150400's control-room scene; while clear the B1
    /// access tunnel refuses the transition to Golem Freezer 1 (cap 1).
    GAME_FLAG_GOLEM_FREEZER_UNLOCKED = 0x0E5,
    /// Mine cavern event sequence: 0->1 raises the attachment event lock in `Gp_StateC08.flags`, 1->2 plays
    /// the quake shake and camera pan scene; the room tick and the release helper act
    /// on each stage.
    GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS = 0x0E6,
    /// Unidentified. Read only (no C setter): in the B1 underground parking, once 0xC7
    /// is set, a clear value runs cap 2 (or the task when 0xE8 is set) and selects cap
    /// 2 vs 3 for the prompt.
    GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_0E7 = 0x0E7,
    /// Unidentified. Read only (no C setter): while 0xE7 is clear, a set value lets
    /// action 1 in the B1 underground parking start the task instead of cap 2.
    GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_0E8 = 0x0E8,
    /// Unidentified. Set to 1 the first time the Neo Ark shrine's 0x5A-frame step
    /// completes; afterwards the shrine latches modes 4/5 instead of 1/2 and the Neo
    /// Ark map resolver uses room 4 for the linked area.
    GAME_FLAG_0E9 = 0x0E9,
    /// Sterilization room event on variant 5: 2 when it ran with the companion present
    /// (0x116=1, 0x4B=8), 1 when without (0x116=2). Value 1 enables trigger 22 and the
    /// mesh/slot-4 chain; value 2 awards collected bit 0x130 at actor_461800's exit and
    /// picks actor_161500's caps.
    GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE = 0x0EA,
    /// Unidentified. Septic tank event state written by the room's script callback
    /// func_8017D97C: at 0 the view-4 scene runs, at 1 warp 2 plays a scene;
    /// actor_00400 (Diver) is lockable only at 2 and jumps to sub-state 3 at 1.
    GAME_FLAG_0EB = 0x0EB,
    /// Number of times the pyramid's rotating quad has been turned (0-4); each turn
    /// sets the quad angle, and at 4 the closing sound and cap 2 play and the puzzle is
    /// complete.
    GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT = 0x0EC,
    /// Unidentified. Set to 1 by the MIST parking action 2 scene; afterwards the
    /// shooting gallery switches Pierce's (actor_215100/113100) scripts and slot-4
    /// chain, and parking event 15 spawns a task.
    GAME_FLAG_0ED = 0x0ED,
    /// Shelter B1 main corridor: the first transition to the armory plays CAP 3. Set to
    /// 1 when the latched one-shot event starts; while set the transition goes through
    /// with no event.
    GAME_FLAG_B1_CORRIDOR_TO_ARMORY_SCENE = 0x0EE,
    /// Shelter B1 main corridor: the first transition to the sleeping quarters plays
    /// CAP 4. Set to 1 when the latched one-shot event starts; while set the transition
    /// goes through with no event.
    GAME_FLAG_B1_CORRIDOR_TO_QUARTERS_SCENE = 0x0EF,
    /// Set to 1 the first time item 0x105 is used on the armory's trigger; while clear
    /// the transition into the armory is refused, and the lock glow uses tint 0x5C00
    /// instead of 0x50C0.
    GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED = 0x0F0,
    /// Unidentified. MIST parking variant-2 progress: set by script callback
    /// func_80183780 and stepped 1->2->3 by event 15, each step playing a script; while
    /// nonzero part of Pierce's model (actor_113100) is hidden.
    GAME_FLAG_0F1 = 0x0F1,
    /// Bitmask of one-shot cap messages already shown in the sterilization room: bit 2
    /// for action 5 (cap 5), bit 8 for action 8 (cap 3).
    GAME_FLAG_STERILIZATION_ROOM_NOTES_SHOWN = 0x0F2,
    /// Unidentified. Set to 1 when actor_206100 is removed in its death path; when
    /// Power Plant 2 is cleared and this is set an extra set of area records is
    /// applied.
    GAME_FLAG_0F3 = 0x0F3,
    /// Layout state of the Shelter B1 underground parking: 0/1/2/3 select room variant
    /// 1/6/7/8. 1 after the cap choice (item seen 0x123), 2 after the follow-up scene,
    /// 3 set by the sterilization room event.
    GAME_FLAG_UNDERGROUND_PARKING_STATE = 0x0F4,
    /// Conversation counter for actor_215100 (Pierce) at the MIST shooting gallery:
    /// 0->1->2 each talk plays the next script.
    GAME_FLAG_PIERCE_TALK_PROGRESS = 0x0F5,
    /// Set to 1 together with 0xDE when Power Plant 1 is cleared; while clear the north
    /// promenade refuses the transition to the Forest Zone.
    GAME_FLAG_NEO_ARK_FOREST_ZONE_UNLOCKED = 0x0F6,
    /// Set to 1 when, after Power Plant 2 is cleared, the observatory's exit sends the
    /// player back to the Shelter B1 control room (warp 3); afterwards the control room
    /// and armory use their later cap messages.
    GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN = 0x0F7,
    /// Unidentified. Set to 1 when the b2 main corridor departure to area 8 happens
    /// with 0xDF == 1; afterwards that departure and the Eve access tunnel's action 0xA
    /// are refused with a cap message and task 0x1AF.
    GAME_FLAG_0F8 = 0x0F8,
    /// Unidentified. One-shot in the altar cutscene (state 5): set to 1 and applies
    /// area records, only while 0xDF is clear.
    GAME_FLAG_NEO_ARK_ALTAR_0F9 = 0x0F9,
    /// Unidentified. One-shot on garden event 4: set to 1 and applies area records,
    /// only while 0xDC is clear.
    GAME_FLAG_NEO_ARK_GARDEN_0FA = 0x0FA,
    /// Unidentified. Set to 1 the first time Power Plant 1 reaches view 3, which resets
    /// the battle state and starts a script; while clear, room entry marks the battle
    /// finished and a reset pending.
    GAME_FLAG_NEO_ARK_POWER_PLANT_1_0FB = 0x0FB,
    /// Unidentified. Set to 1 in actor_450900's story reset (with 0x4D, 0xD9, 0xD2
    /// cleared) and cleared in shelter_1f_tent; selects alternate cap messages in the
    /// B1/B2 pod access tunnels.
    GAME_FLAG_0FC = 0x0FC,
    /// Unidentified. Set to 1 the first time the Dryfield trailer coach plays its scene
    /// while item 0x111 is not collected and 0x4F is set (later visits play another);
    /// the garage then answers event 0x10 with cap 0x16 instead of 0x10.
    GAME_FLAG_0FD = 0x0FD,
    /// Unidentified. Set to 1 when the breezeway spawns its task (bit2 flag 6 set, not
    /// in battle); feeds the 0x56 progress logic when item 0x11B is not collected.
    GAME_FLAG_DRYFIELD_BREEZEWAY_0FE = 0x0FE,
    /// Neo Ark submarine tunnel progress on variant 3: 0->1 on the first warp-3 entry
    /// (plays a scene), 1->2 at warp 1 (scene, 0x11F=1, sceneEvent 0x1A).
    GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS = 0x0FF,
    /// Unidentified. shelter_r48 state written by actor_503500 (Brahman) callback and
    /// by the room: at 1 using item 0x121/0x122 on the trigger runs a task and sets 2
    /// (0x12A=4); 0 hides the room's effect draw; 1/2 pick the glow tint; values select
    /// cap messages.
    GAME_FLAG_100 = 0x100,
    /// Conversation counter (0-3) for the companion in the Neo Ark observatory
    /// (actor_450200), advanced each time action 3 is used in view 2.
    GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT = 0x101,
    /// Set to 1 by actor_311900 once its scripted movement (seen on the Akropolis
    /// security-room monitor, view 0xA) has run for 0x5A frames; while clear, selecting
    /// monitor view 0xA in the security room starts CAP slot 0xC.
    GAME_FLAG_SECURITY_MONITOR_CAM_A_SCENE_DONE = 0x102,
    /// Dialogue counter of the soldier actor actor_161500 (soldier_b model), first
    /// conversation set: selects the line (separate tables for variant 1) and
    /// increments 0..3, staying at 3.
    GAME_FLAG_SOLDIER_B_TALK_COUNT_A = 0x103,
    /// Dialogue counter of the soldier actor actor_161500, second conversation set:
    /// selects the line and increments 0..3, staying at 3. The Shelter 1F heliport's
    /// trigger 0x22 plays CAP 0x22 once it reaches 1 (2 in variant 1), CAP 0x25 before.
    GAME_FLAG_SOLDIER_B_TALK_COUNT_B = 0x104,
    /// Unidentified. Selects between two dialogue scripts of the soldier actor
    /// actor_161500 (0: D_801352A8, non-zero: D_801354B8). No writer found by direct
    /// gameFlagSetNibble.
    GAME_FLAG_105 = 0x105,
    /// Set to 1 the first time directed action 4 in the MIST shooting gallery is used,
    /// which runs its one-shot scene (and sets caption state 0xA2 to 0x3B); later uses
    /// do nothing.
    GAME_FLAG_SHOOTING_GALLERY_ACTION_4_SEEN = 0x106,
    /// Dryfield night garage: 0 until the garage CAP sequence's second step has run
    /// once, then 1; the step starts its CAP slot at entry flag+1, so the first run
    /// plays entry 1 and later runs entry 2.
    GAME_FLAG_NIGHT_GARAGE_SCENE_REPEAT = 0x107,
    /// When 1, Stage_RequestSpecialFlag stops the stage ambient sound (0x60010001) in
    /// areas whose music entry is 0x80 instead of starting it. Set to 1/0 by
    /// actor_335800 event-script callbacks.
    GAME_FLAG_STAGE_AMBIENT_MUTED = 0x108,
    /// Set to 1 on the first entry to the Shelter 1F tent, which also refills HP/MP,
    /// resets several other flags, applies the room's area records and picks the
    /// follow-up scene (0x155 = 0xA or 0xB by flag 0x112).
    GAME_FLAG_SHELTER_1F_TENT_ARRIVED = 0x109,
    /// Remaining enemy count of roaming-enemy pool B in the Neo Ark forest zone /
    /// woodland path: increased by the pool's per-variant count when a new location
    /// variant is armed, and rewritten with the still-alive spawn slots after each
    /// battle.
    GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE = 0x10A,
    /// Location variant for which roaming-enemy pool B last added its spawn count to
    /// flag 0x10A, so the count is added only once per variant.
    GAME_FLAG_NEO_ARK_ROAMER_POOL_B_VARIANT = 0x10B,
    /// Remaining enemy count of roaming-enemy pool A in the Neo Ark forest zone /
    /// woodland path: increased by the pool's per-variant count when a new variant is
    /// armed and rewritten with the still-alive spawn slots after each battle (the
    /// difference goes into flag 0x168).
    GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE = 0x10C,
    /// Location variant for which roaming-enemy pool A last added its spawn count to
    /// flag 0x10C.
    GAME_FLAG_NEO_ARK_ROAMER_POOL_A_VARIANT = 0x10D,
    /// Dryfield night motel balcony follow-up script: set to 1 when the room's arrival
    /// scene starts (place 2, room 2, with flag 0x61), moved to 2 once the event state
    /// is idle and the one-shot follow-up script has been started.
    GAME_FLAG_NIGHT_MOTEL_BALCONY_SCRIPT_STATE = 0x10E,
    /// Set to 1 when the mine cavern's variant-1 arrival scene runs; once set the
    /// shelter room-variant resolver sends area 2 to room 2.
    GAME_FLAG_MINE_CAVERN_INTRO_SEEN = 0x10F,
    /// Unidentified. Set to 1 by actor_548100 when a step's 2-bit slot reads 2
    /// (clearing that step's 0xBE+step nibble); when set, the actor's cap event key 0xB
    /// marks item bits 0x120 and 0x12C seen.
    GAME_FLAG_110 = 0x110,
    /// Set to 1 when item 0x125's specs panel is opened; the room cutscene task then
    /// sets flag 0x112 (with progress 0x7A == 5).
    GAME_FLAG_ITEM_125_EXAMINED = 0x111,
    /// Set to 1 by the room cutscene task when progress 0x7A is 5 and item 0x125 has
    /// been examined (also sets 0x155 = 9). Read by the Shelter 1F tent (alternate area
    /// records and scene 0xB), by actor_161500 dialogue choice, and by the ending exit,
    /// which ends the game when both this and 0x113 are 0.
    GAME_FLAG_ITEM_125_FOLLOWUP_SEEN = 0x112,
    /// Progress with the character actor_160700: 0 unmet, 1-3 after successive
    /// conversations (first sets 0x155 = 0xC), 4 once the B1 access tunnel's control-
    /// room / transfer-tunnel exits are used, 5 after soldier B's remark. Non-zero
    /// awards collected bit 0x12F at the ending and selects the extended ending in
    /// shelter_r36.
    GAME_FLAG_ACTOR_160700_MEETING_PROGRESS = 0x113,
    /// Dialogue counter of soldier actor actor_460200 (soldier_c model), first set:
    /// plays line N and advances 0..3, staying at 3.
    GAME_FLAG_SOLDIER_C_TALK_COUNT_A = 0x114,
    /// Dialogue counter of soldier actor actor_460200, second set: plays line N and
    /// advances 0..3, staying at 3.
    GAME_FLAG_SOLDIER_C_TALK_COUNT_B = 0x115,
    /// Pending remark of soldier actor actor_161500: 1 or 2 set in the B1 sterilization
    /// room outcome (companion present / absent, with 0xEA = 2 / 1), 4 when 0x113
    /// reaches 4; each plays its line and moves to 3 or 0, 3 plays one more line then
    /// 0.
    GAME_FLAG_SOLDIER_B_REMARK_STATE = 0x116,
    /// Unidentified. Shelter pod event state: set to 1 by the B1 pod service gantry
    /// sequence, moved to 2 on entering the B1 pod access tunnel (spawns a task,
    /// caption 0x37). At 2 the tunnel's R47 / sterilization-room exits and B2 pod
    /// tunnel's septic-tank exit are refused with a CAP message, and the pod ride lands
    /// in B2 pod access tunnel room 2.
    GAME_FLAG_118 = 0x118,
    /// Mine secret passage: 1 after first trying it while flag 0xBB is not 1 (CAP 0xD),
    /// 2 after going through once 0xBB is 1 (also sets 0x155 = 0). At 2 the mine
    /// tunnel/mesa use their later CAP messages and area 2 resolves to room 3.
    GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS = 0x11A,
    /// Unidentified. Read by the Dryfield cellar (day and night) message handler: below
    /// 2 it runs CAP 0xD on event 0xD, otherwise CAP 4 or 0xE depending on item 0x83.
    /// No direct writer found.
    GAME_FLAG_11B = 0x11B,
    /// Dryfield driveway: the first transition to the factory (area 0x17), once nibble
    /// 0x30 is not 1, plays CAP 9 and stage sound 0x52190003. Set to 1 when the latched
    /// one-shot event starts; while set the transition goes through with no event.
    GAME_FLAG_DRIVEWAY_TO_FACTORY_SCENE = 0x11C,
    /// Unidentified. Shelter B3 dumping hole event state: 1 when the room's scripted
    /// task finishes, 2 when actor_341900's script sequence finishes; non-zero switches
    /// trigger 0x12 from CAP 0x17 to 0x12.
    GAME_FLAG_11D = 0x11D,
    /// Unidentified. Gate for actor_342100's controller state 1: once non-zero, an
    /// action-room trigger hit spawns its task. No direct writer found.
    GAME_FLAG_11E = 0x11E,
    /// Global music override read on every room load: 1 (set at the Neo Ark submarine
    /// tunnel event, scene event 0x1A) uses scene music entry 1 at Dryfield night and
    /// otherwise skips area music; 2 (set at the B1 underground parking, scene event
    /// 0x1B) uses scene music entry 9.
    GAME_FLAG_SCENE_MUSIC_OVERRIDE = 0x11F,
    /// Set to 1 the first time trigger 0x16 in the Shelter B2 breeding room runs its
    /// scripted scene; afterwards the trigger replays CAP 0x16.
    GAME_FLAG_BREEDING_ROOM_FIRST_SCENE_SEEN = 0x120,
    /// Set to 1 when actor_215100 (shooting-gallery helper) first runs its introductory
    /// scene (caption state 0x3A); later runs skip straight to spawning the gallery
    /// task.
    GAME_FLAG_SHOOTING_GALLERY_INTRO_SEEN = 0x121,
    /// Set to 1 on first entry to the Shelter B1 elevator hall (caption state 0xA2 set
    /// to 0x1D).
    GAME_FLAG_SHELTER_B1_ELEVATOR_HALL_VISITED = 0x122,
    /// Unidentified. Set once in the B1 underground parking room setup when progress
    /// 0x7A is at least 6, triggering a one-time room update (func_8018390C).
    GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_123 = 0x123,
    /// Set to 1 the first time trigger 0xE in the Akropolis square fires, which starts
    /// CAP 0xE once.
    GAME_FLAG_ACROPOLIS_SQUARE_TRIGGER_E_SEEN = 0x124,
    /// Prize state of shooting-gallery difficulty 4 (flags 0x125+difficulty): 1 or 2
    /// when earned, 2 meaning it waits at MIST parking; collecting it there gives item
    /// 0x6C and sets 3.
    GAME_FLAG_SHOOTING_GALLERY_PRIZE_4_STATE = 0x129,
    /// Shelter room R48 (Brahman) scene state: 1 on room setup, 2 after the follow-up
    /// script or 3 after spawning the three tasks, 4 when the trigger ends the fight
    /// phase (with 0x100 = 2). actor_503500's effect task fades differently by state.
    GAME_FLAG_SHELTER_R48_SCENE_STATE = 0x12A,
    /// Set to 1 when the player confirms the mine refuge request-1 prompt (cap key 5);
    /// afterwards request 1 plays the cutscene without the prompt.
    GAME_FLAG_MINE_REFUGE_PROMPT_ACCEPTED = 0x12B,
    /// Shelter B1 main corridor: the first transition to the sterilization room plays
    /// CAP 6. Set to 1 when the latched one-shot event starts; while set the transition
    /// goes through with no event.
    GAME_FLAG_B1_CORRIDOR_TO_STERILIZATION_SCENE = 0x12C,
    /// Shelter B1 main corridor: the first transition to the control-room access tunnel
    /// plays CAP 7. Set to 1 when the latched one-shot event starts; while set the
    /// transition goes through with no event.
    GAME_FLAG_B1_CORRIDOR_TO_CONTROL_TUNNEL_SCENE = 0x12D,
    /// Shelter B1 main corridor: the first transition to the transfer tunnel, allowed
    /// only once nibble 0xAC is set (else CAP 1 refusal), plays CAP 8. Set to 1 when
    /// the latched one-shot event starts; while set the transition goes through with no
    /// event.
    GAME_FLAG_B1_CORRIDOR_TO_TRANSFER_SCENE = 0x12E,
    /// Set to 1 on the first request 1 in the Shelter B1 pod access tunnel (CAP 0xA);
    /// later requests spawn the action task.
    GAME_FLAG_B1_POD_ACCESS_TUNNEL_FIRST_USE = 0x12F,
    /// Shelter B1 pod access tunnel: the first transition to the sterilization room
    /// (when 0x118 != 2 and 0xD1 != 2) plays CAP 6. Set to 1 when the latched one-shot
    /// event starts; while set the transition goes through with no event.
    GAME_FLAG_B1_POD_TUNNEL_TO_STERILIZE_SCENE = 0x130,
    /// Shelter B2 septic tank: the first transition to the B2 main corridor plays CAP
    /// 3. Set to 1 when the latched one-shot event starts; while set the transition
    /// goes through with no event.
    GAME_FLAG_B2_SEPTIC_TANK_TO_CORRIDOR_SCENE = 0x131,
    /// Shelter B2 pod access tunnel: the first transition to the septic tank (when
    /// 0x118 != 2) plays CAP 5. Set to 1 when the latched one-shot event starts; while
    /// set the transition goes through with no event.
    GAME_FLAG_B2_POD_TUNNEL_TO_SEPTIC_SCENE = 0x132,
    /// Shelter B2 north maintenance walkway: the first transition to the breeding room
    /// plays CAP 4. Set to 1 when the latched one-shot event starts; while set the
    /// transition goes through with no event.
    GAME_FLAG_B2_NORTH_WALKWAY_TO_BREEDING_SCENE = 0x137,
    /// Set to 1 on the first request 1 in the Shelter B3 incinerator control room (CAP
    /// 6); later requests run the room cutscene and warp.
    GAME_FLAG_INCINERATOR_CONTROL_FIRST_USE = 0x138,
    /// Set to 1 the first time the Shelter B4 water-supply valve action is used while
    /// flag 0xB8 is set (CAP 3 plus a task); later uses call func_8017DB18.
    GAME_FLAG_WATER_SUPPLY_VALVE_FIRST_USE = 0x139,
    /// Shelter B2 operating room: the first transition to the south maintenance walkway
    /// plays CAP 0xE. Set to 1 when the latched one-shot event starts; while set the
    /// transition goes through with no event.
    GAME_FLAG_B2_OPERATING_TO_SOUTH_WALKWAY_SCENE = 0x13A,
    /// Shelter B2 operating room: the first transition to the laboratory plays CAP 0xD.
    /// Set to 1 when the latched one-shot event starts; while set the transition goes
    /// through with no event.
    GAME_FLAG_B2_OPERATING_TO_LAB_SCENE = 0x13B,
    /// Shelter B2 south maintenance walkway: the first transition to the B2 elevator
    /// hall plays CAP 2. Set to 1 when the latched one-shot event starts; while set the
    /// transition goes through with no event.
    GAME_FLAG_B2_SOUTH_WALKWAY_TO_ELEVATOR_SCENE = 0x13C,
    /// Set to 1 the first time the Shelter B2 laboratory console action is used (CAP
    /// 0x1E); later uses spawn the console task depending on flag 0xD0.
    GAME_FLAG_LABORATORY_CONSOLE_FIRST_USE = 0x13D,
    /// Set to 1 on the first request 1 in Shelter room R47 (spawns entry 2 of the room
    /// task table); later requests play the room cutscene (or CAP 0x2A while bit2 flag
    /// 0x22 is 1).
    GAME_FLAG_SHELTER_R47_FIRST_USE = 0x13E,
    /// Shelter B1 access tunnel: the first transition to the underground parking plays
    /// CAP 4. Set to 1 when the latched one-shot event starts; while set the transition
    /// goes through with no event.
    GAME_FLAG_B1_ACCESS_TUNNEL_TO_PARKING_SCENE = 0x13F,
    /// Neo Ark forest zone: the first transition to the woodland path plays CAP 2. Set
    /// to 1 when the latched one-shot event starts; while set the transition goes
    /// through with no event.
    GAME_FLAG_FOREST_ZONE_TO_WOODLAND_PATH_SCENE = 0x140,
    /// Unidentified. Selects alternate CAP messages in the Neo Ark garden (6 instead of
    /// 4) and pavilion (5 instead of 1). No direct writer found.
    GAME_FLAG_141 = 0x141,
    /// Set to 1 when part 0 of the Eve encounter (actor_205200) in the Neo Ark Eve
    /// access tunnel is destroyed, cleared when that encounter spawns; set selects CAP
    /// 8 instead of the battle message 6.
    GAME_FLAG_EVE_ACCESS_TUNNEL_PART_0_DOWN = 0x142,
    /// Set to 1 when part 1 of the Eve encounter in the Neo Ark Eve access tunnel is
    /// destroyed, cleared on encounter spawn; set selects CAP 9 instead of 7.
    GAME_FLAG_EVE_ACCESS_TUNNEL_PART_1_DOWN = 0x143,
    /// Set to 1 when part 0 of the Eve encounter in the Shelter B6 corridor is
    /// destroyed, cleared on encounter spawn; set selects CAP 5 instead of 2.
    GAME_FLAG_B6_CORRIDOR_EVE_PART_0_DOWN = 0x144,
    /// Set to 1 when part 1 of the Eve encounter in the Shelter B6 corridor is
    /// destroyed, cleared on encounter spawn; set selects CAP 6 instead of 3.
    GAME_FLAG_B6_CORRIDOR_EVE_PART_1_DOWN = 0x145,
    /// Apparently set by the Eve encounter for corridor part 2 (`slot` + 0x144 with
    /// slot 2), but not cleared on spawn; set selects CAP 7 for trigger 4 in the B6
    /// corridor and training room.
    GAME_FLAG_B6_CORRIDOR_EVE_PART_2_DOWN = 0x146,
    /// Set to 1 when the generator's life-support part in Neo Ark power plant 2 is
    /// destroyed (0 at spawn); selects CAP 6 and the spark effect in view 6.
    GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN = 0x147,
    /// Set to 1 when the generator's life-support part in Neo Ark power plant 1 is
    /// destroyed (0 at spawn); selects CAP 6 and the spark effect in views 6/7.
    GAME_FLAG_POWER_PLANT_1_GENERATOR_PART_DOWN = 0x148,
    /// Unidentified. Set to 1 when the sterilization room's CAP task runs with a non-
    /// zero spawn argument (CAP 7/8 by flag 0x77); afterwards action 9 spawns the room
    /// tasks instead of entry 8/1.
    GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_149 = 0x149,
    /// Unidentified. Set to 1 (with 0x151 = 1) when the sterilization room's CAP task
    /// runs with spawn argument 0 while flag 0x77 is set (CAP 9); selects entry 8/5
    /// instead of 8/4 for action 4.
    GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14A = 0x14A,
    /// Shelter B1 south maintenance walkway: the first transition to the B1 elevator
    /// hall plays CAP 1. Set to 1 when the latched one-shot event starts; while set the
    /// transition goes through with no event.
    GAME_FLAG_B1_SOUTH_WALKWAY_TO_ELEVATOR_SCENE = 0x14B,
    /// Shelter B1 south maintenance walkway: the first transition to the storeroom
    /// plays CAP 2. Set to 1 when the latched one-shot event starts; while set the
    /// transition goes through with no event.
    GAME_FLAG_B1_SOUTH_WALKWAY_TO_STOREROOM_SCENE = 0x14C,
    /// Unidentified. Sterilization room trigger 0xE while 0x76 == 1 and 0x77 == 0: 0
    /// runs a scene and sets 1, then the next use spawns entry 8/0xB and sets 2.
    GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14F = 0x14F,
    /// One-shot: set to 1 the first time action 9 in the B1 sterilization room plays
    /// its extra scene (task table 8 entry 2) after flag 0x149 is set.
    GAME_FLAG_STERILIZATION_ROOM_ACTION9_SCENE = 0x150,
    /// Progress of the action-4 scenes in the B1 sterilization room: 0 none, 1 first
    /// scene played (or set by the CAP 9 task when 0x77 is set), 2 the follow-up scene
    /// (entry 4 or 5 by flag 0x14A) played.
    GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE = 0x151,
    /// Unidentified. Read-only in C: in the mine forked tunnel, request 2 (with bit2
    /// flag 1 set) runs CAP 5 while clear and spawns task-table entry 1 once set; no
    /// setter found in code.
    GAME_FLAG_MINE_FORKED_TUNNEL_152 = 0x152,
    /// Unidentified. Training-room event state: when nonzero, request 5 in the B6
    /// training room runs CAP 7 instead of CAP 5; actor_205200 clears it to 0 together
    /// with 0x154 while hiding the room's sprite batches. No setter to nonzero found in
    /// C.
    GAME_FLAG_153 = 0x153,
    /// Unidentified. Pair of 0x153: when nonzero, request 6 in the B6 training room
    /// runs CAP 8 instead of CAP 6; actor_205200 clears it. No setter to nonzero found
    /// in C.
    GAME_FLAG_154 = 0x154,
    /// Story-progress index (0-0xF) that picks the follow-up CAP dialogue of slot-1
    /// room cutscenes (CAP command 0x155+0x10 in roomCutsceneTask); story beats across
    /// all stages set it to increasing values (always clearing flag 3), and stage
    /// starts reset it to 0. Value 0xE adds a looping dialogue choice.
    GAME_FLAG_STORY_DIALOGUE_INDEX = 0x155,
    /// One-shot: set when request 0x10 in Akropolis square first runs CAP 0x10 or 0x11,
    /// chosen by the save's button layout (likely a controls hint).
    GAME_FLAG_ACROPOLIS_SQUARE_CONTROLS_HINT = 0x156,
    /// One-shot: set on first entry to the B1 north maintenance walkway in variant 2,
    /// when CAP 4 is started.
    GAME_FLAG_NORTH_MAINTENANCE_WALKWAY_SCENE = 0x157,
    /// One-shot: request 13 in the B1 underground parking spawns its first-time task
    /// (table 6 entry 1) while clear and sets it; afterwards the room cutscene is run
    /// instead.
    GAME_FLAG_UNDERGROUND_PARKING_FIRST_SCENE = 0x158,
    /// Shelter 1F parking garage: the first transition to the 1F airlock plays CAP 3.
    /// Set to 1 when the latched one-shot event starts; while set the transition goes
    /// through with no event.
    GAME_FLAG_1F_GARAGE_TO_AIRLOCK_SCENE = 0x159,
    /// Shelter 1F vehicular airlock: the first transition to the 1F airlock plays CAP
    /// 6. Set to 1 when the latched one-shot event starts; while set the transition
    /// goes through with no event.
    GAME_FLAG_VEHICULAR_AIRLOCK_TO_AIRLOCK_SCENE = 0x15A,
    /// Shelter 1F vehicular airlock: the first transition to the bulwark, allowed only
    /// once nibble 0xB2 is set (else CAP 2 refusal), plays CAP 4. Set to 1 when the
    /// latched one-shot event starts; while set the transition goes through with no
    /// event.
    GAME_FLAG_VEHICULAR_AIRLOCK_TO_BULWARK_SCENE = 0x15B,
    /// Shelter 1F bulwark: the first transition to the vehicular airlock plays CAP 6.
    /// Set to 1 when the latched one-shot event starts; while set the transition goes
    /// through with no event.
    GAME_FLAG_BULWARK_TO_VEHICULAR_AIRLOCK_SCENE = 0x15C,
    /// Read-only in C: while clear, a transition from the Shelter 1F bulwark to the 1F
    /// heliport is refused and CAP 1 runs; once set the heliport event (0x7A -> 6) can
    /// proceed. No setter found in code (likely a CAP script).
    GAME_FLAG_BULWARK_HELIPORT_UNBLOCKED = 0x15D,
    /// One-shot: set when the B6 nursery handler first runs CAP 0x17.
    GAME_FLAG_NURSERY_SCENE_SEEN = 0x160,
    /// One-shot: request 0x11 in the B1 sterilization room runs CAP 0x18 the first time
    /// and sets it; afterwards it plays the room cutscene.
    GAME_FLAG_STERILIZATION_ROOM_FIRST_SCENE = 0x161,
    /// Unidentified. Counter in room shelter_r47: set to 1 on the 0x82 event (warp 3),
    /// then incremented (capped at 3) each time the room's CAP-8 task finishes. No
    /// reader besides itself.
    GAME_FLAG_SHELTER_R47_165 = 0x165,
    /// Two-step event: mine gorge sets 1 when flag 0xBE is 2 (clearing 0xB5, CAP 8);
    /// the mine refuge then runs CAP 0xF once and sets 2.
    GAME_FLAG_MINE_REFUGE_SCENE_STATE = 0x166,
    /// Count of roaming enemies killed in the Neo Ark forest zone / woodland path pool:
    /// after a battle the pending slots minus survivors are added. Added to battlesWon
    /// for the play-data completion percentage (of 326).
    GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B = 0x167,
    /// Count of roaming enemies killed in roamer pool A (roamerTickPoolA, Neo Ark
    /// rooms); added to battlesWon for the play-data completion percentage.
    GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_A = 0x168,
    /// One-shot: request 1 in the Shelter 1F tent runs CAP 0x18 the first time and sets
    /// it; afterwards the room cutscene path is used.
    GAME_FLAG_TENT_FIRST_SCENE = 0x169,
    /// One-shot: event 4 on the Akropolis fire escape runs CAP 0xB the first time and
    /// sets it; afterwards the room cutscene (view 9) runs.
    GAME_FLAG_FIRE_ESCAPE_FIRST_SCENE = 0x16A,
    /// One-shot: request 1 at the Dryfield gas station runs CAP 0xB the first time and
    /// sets it; afterwards the room cutscene runs.
    GAME_FLAG_GAS_STATION_FIRST_SCENE = 0x16B,
    /// One-shot: request 0xE in the Dryfield trailer coach runs CAP 0x1D the first time
    /// and sets it; afterwards the room cutscene runs.
    GAME_FLAG_TRAILER_COACH_FIRST_SCENE = 0x16C,
    /// One-shot: request 3 in the Dryfield-by-night motel lobby runs CAP 0xA the first
    /// time and sets it; afterwards the room cutscene runs.
    GAME_FLAG_NIGHT_MOTEL_LOBBY_FIRST_SCENE = 0x16D,
    /// One-shot: in the Neo Ark observatory, action 4 with flag 0xDE set starts the
    /// room's event script once and sets it.
    GAME_FLAG_OBSERVATORY_EVENT_SEEN = 0x16E,
    /// Unidentified. Read-only in C: request 3 in the B1 sleeping quarters runs CAP 3
    /// while clear and CAP 0xF once set; no setter found in code.
    GAME_FLAG_SLEEPING_QUARTERS_16F = 0x16F,
    /// Set when the Dryfield-by-night motel loft's CAP script ends on event key 0x1F;
    /// afterwards the loft runs CAP 0x12 instead of 3.
    GAME_FLAG_NIGHT_MOTEL_LOFT_SCENE_DONE = 0x170,
    /// Mine mesa: the first transition to the mine tunnel entrance plays CAP 0xE. Set
    /// to 1 when the latched one-shot event starts; while set the transition goes
    /// through with no event.
    GAME_FLAG_MESA_TO_TUNNEL_ENTRANCE_SCENE = 0x171,
    /// One-shot: set on the first pass of the mine secret passage room task, which
    /// starts CAP 3.
    GAME_FLAG_MINE_SECRET_PASSAGE_INTRO_SEEN = 0x172,
    /// Neo Ark pavilion: the first transition to the submarine tunnel plays CAP 4 (no
    /// stage sound). Set to 1 when the latched one-shot event starts; while set the
    /// transition goes through with no event.
    GAME_FLAG_PAVILION_TO_SUB_TUNNEL_SCENE = 0x17E,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter table entry 26 (0x800
    /// variant); set to 2 at first arrival in the Shelter 1F tent (flag 0x109).
    GAME_FLAG_MAP_MARK_SHELTER_1AE = 0x1AE,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter table entry 24 (0x800
    /// variant); set to 2 at first arrival in the Shelter 1F tent.
    GAME_FLAG_MAP_MARK_SHELTER_1B0 = 0x1B0,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Neo Ark entry 6; cleared when
    /// Neo Ark power plant 1 is activated (0xDE, 0xF6 set).
    GAME_FLAG_MAP_MARK_POWER_PLANT_1 = 0x1B2,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 22 (0x800
    /// variant); set to 2 at first arrival in the Shelter 1F tent.
    GAME_FLAG_MAP_MARK_SHELTER_1B3 = 0x1B3,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 21 / Neo Ark entry
    /// 5; set to 2 by B1 underground parking warp 0xA (room < 7), cleared by its later
    /// event (0xF4 -> 2).
    GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING = 0x1B4,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entries 20 and 28; the
    /// B1/B2 pod access tunnels set 2 when the pod CAP ends on key 1 and clear it when
    /// the pod ride is taken (key 0xA); the 1F tent clears it on arrival.
    GAME_FLAG_MAP_MARK_POD = 0x1B6,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Neo Ark entry 3; cleared when
    /// the Neo Ark altar tile sequence is solved (0xDC set).
    GAME_FLAG_MAP_MARK_ALTAR = 0x1B7,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Neo Ark entry 2; cleared when
    /// the Neo Ark shrine arrangement puzzle completes (0xDB set).
    GAME_FLAG_MAP_MARK_SHRINE = 0x1B8,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter-stage entry 19; cleared
    /// in the mine cavern when the CAP ends on key 0x15 (0xBB set).
    GAME_FLAG_MAP_MARK_MINE_CAVERN = 0x1B9,
    /// Unidentified. Set to 2 at first arrival in the Shelter 1F tent together with the
    /// map-marker flags, but no stage map table lists it (the shelter table lists 0x1BB
    /// twice).
    GAME_FLAG_SHELTER_1F_TENT_1BA = 0x1BA,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entries 17, 18 and 29;
    /// set to 2 at first arrival in the Shelter 1F tent.
    GAME_FLAG_MAP_MARK_SHELTER_1BB = 0x1BB,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Neo Ark entry 1 (0x800 variant);
    /// cleared when Neo Ark power plant 2 is activated (0xDF, 0xB9 set).
    GAME_FLAG_MAP_MARK_POWER_PLANT_2 = 0x1BC,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Reused across stages: Dryfield
    /// entry 28 set to 2 by examining the water hole (day/night, CAP 2); Shelter entry
    /// 16 set to 2 at the B4 water-supply valve; cleared on entering mine mesa and when
    /// the B4 upper sewer event sets 0xB8.
    GAME_FLAG_MAP_MARK_WATER = 0x1BD,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 15 (0x800
    /// variant); set to 2 by the B4 reservoir event that sets 0xB6/0xB7.
    GAME_FLAG_MAP_MARK_RESERVOIR_1BE = 0x1BE,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 14 (0x800
    /// variant); set to 2 by the same B4 reservoir event.
    GAME_FLAG_MAP_MARK_RESERVOIR_1BF = 0x1BF,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 12; cleared when
    /// the B1 pod ride to the pod service gantry is taken (0xB4 set).
    GAME_FLAG_MAP_MARK_POD_SERVICE_GANTRY = 0x1C1,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 11; cleared by the
    /// B2 laboratory event that sets 0xD0=3 and 0xB3.
    GAME_FLAG_MAP_MARK_B2_LABORATORY = 0x1C2,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 9 / Neo Ark entry
    /// 0; cleared by the B2 main corridor event (0xD3, 0xAE set); the shelter_r47
    /// terminal sets 2 at its step 4 and 0 at step 5.
    GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR = 0x1C4,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 7; the shelter_r47
    /// terminal sets 2 at its step 0 and 0 at step 1.
    GAME_FLAG_MAP_MARK_SHELTER_R47_1C6 = 0x1C6,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 6; cleared by
    /// actor_450900's event (0xAB set, 0x155 -> 8).
    GAME_FLAG_MAP_MARK_SHELTER_1C7 = 0x1C7,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Shelter entry 0 (forced to the
    /// 0x800 sprite while 0x7A is 6); set to 2 at first arrival in the Shelter 1F tent.
    GAME_FLAG_MAP_MARK_SHELTER_1CD = 0x1CD,
    /// Map marker: listed in the stage map-marker tables read by menuMapGetMarkerState;
    /// func_800D0C34 draws a marker icon while the value is 2 (alternate sprite when
    /// the table entry carries bit 0x800), 0 hides it. Akropolis entry 4; cleared in
    /// the Akropolis security room's event task.
    GAME_FLAG_MAP_MARK_SECURITY_ROOM = 0x1EE,
};

#endif // MAIN_GAMEFLAG_IDS_H
