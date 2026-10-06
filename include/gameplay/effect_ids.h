#ifndef GAMEPLAY_EFFECT_IDS_H
#define GAMEPLAY_EFFECT_IDS_H

/// Effect ids for `Gp_SpawnEff` and the room effect slots.
///
/// An id packs the `Task_Spawn` bank in bits 16..30 and the task type in the
/// low 16 bits. Bank 6 is the gameplay effect table `D_8010FC2C`, whose slot is
/// the effect's own identifier; many slots hold one room's private handler.
/// `EFFECT_<HHH>` is a placeholder for an effect whose look is not yet known.
#define EFFECT_ID(bank, type) (((bank) << 16) | (type))

/// Bank of the gameplay effect table `D_8010FC2C`.
#define EFFECT_TASK_BANK 6

/// Set on an id to spawn it even when the ordinary effect limit is reached.
#define EFFECT_SPAWN_UNLIMITED ((s32)0x80000000)

enum {
    /// Bank-1 model task (Gp_EffAttachTask37) that shows a caller-supplied TmdSource,
    /// flings it in a random direction while tumbling, then spawns the 0x600A5 corpse-
    /// burn effect on it; actor_01100 uses it for the burst arm/head pieces of its
    /// model.
    EFFECT_FLYING_BODY_PART = EFFECT_ID(1, 0x032),
    /// Bank-2 effect that flings one model chunk (the model put in D_800678F0[0], with
    /// the enemy texture page/CLUT) from an enemy joint when the body bursts; spawned
    /// by madChaserSpawnGibs and the diver/actor_400500/400600/405800 burst code.
    EFFECT_BODY_CHUNK = EFFECT_ID(2, 0x010),
    /// Bank-4 task that flings one detached enemy model part (the model is passed
    /// through D_80067704[0]); spawned when an enemy bursts apart (Scorpion
    /// head/pincers, Brain Stinger, Mind Suckler parts, bat wings, Maggot Caterpillar
    /// husk).
    EFFECT_BURST_BODY_PART_BANK4 = EFFECT_ID(4, 0x007),
    /// The dryfield_motel_balcony instance of the room-effect library's
    /// _roomVisualEffectsFlashTask: a flash: two fans and an inward-shrinking ring ramping up over
    /// spawnArg1 ticks, peaking with a coloured fade quad, then fading through a star;
    /// the room stores it in slot gRoomEffectFlashId, read by the Pawn/Rook GOLEM silence-
    /// scream state (golem_pawn_rook_silence_scream.inc.c, part 4).
    EFFECT_DRYFIELD_MOTEL_BALCONY_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x005),
    /// dryfield_night_gas_station's copy of the RoomFx flash: ramps up two fans and a
    /// shrinking ring with a fade quad at its peak, then fades out through a star; the
    /// room stores it in slot gRoomEffectFlashId, which the Pawn/Rook golem library spawns at
    /// body part 4 for its scream (golemPawnRookSilenceScream).
    EFFECT_DRYFIELD_NIGHT_GAS_STATION_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x006),
    /// Twin trail (room_visual_effects_trail_task): two points offset from the anchor
    /// recorded in rings of eight and drawn as a fading beam; stored in gRoomEffectTwinTrailId,
    /// which the beam-sword golems (actor_02000/02300) spawn at model part 7.
    EFFECT_DRYFIELD_NIGHT_GAS_STATION_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x008),
    /// dryfield_night_gas_station's copy of the shared spark burst
    /// (RoomFx_SparkBurstTask): spawns impact flash 0x60076, then sprays sparks/smoke
    /// or draws expanding rings for seven ticks; the room stores it in gRoomEffectSparkBurstId
    /// (spark-burst slot, spawned by golemPawnRookBulletFly when a golem bullet ends
    /// its flight).
    EFFECT_DRYFIELD_NIGHT_GAS_STATION_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x009),
    /// dryfield_night_back_street's copy of _roomVisualEffectsFlashTask (fans and a shrinking ring
    /// ramping to a coloured screen fade), stored in gRoomEffectFlashId, which the Pawn/Rook
    /// GOLEM's silence-scream state spawns on its part 4.
    EFFECT_DRYFIELD_NIGHT_BACK_STREET_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x00A),
    /// Hypervelocity discharge cone: two flaring textured walls
    /// (`hypervelocityDischargeConeTask`) that climb, open and dim over a few frames;
    /// spawned and adopted by the round in flight for its first 0x15 frames.
    EFFECT_HYPERVELOCITY_DISCHARGE_CONE = EFFECT_ID(EFFECT_TASK_BANK, 0x00B),
    /// The hypervelocity round in flight: ring and two trail halves along its path,
    /// ground splash, sparks, room light, shrinking ring on impact; spawned by the
    /// hypervelocity weapon when it fires.
    EFFECT_HYPERVELOCITY_ROUND = EFFECT_ID(EFFECT_TASK_BANK, 0x00C),
    /// Expanding blue-white band (effectDrawRaisedGlowBand) that widens 0x40 and fades 0x10 per
    /// frame; spawned by the Hypervelocity weapon when its beam fires and reparented
    /// under the beam task.
    EFFECT_HYPERVELOCITY_SHOCK_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x00D),
    /// Attaches to player joint 8 while Berserker is active and, on each burstRequest
    /// (a shot), draws a pink triangle fan and two rings; spawned by func_800ECA54 when
    /// the Berserker status is applied.
    EFFECT_BERSERKER_SHOT_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x00E),
    /// Five-tick colored screen pulse (`_effectStatusScreenTintTaskF`) for one
    /// player-status visual bit; replacing `gRoomEffectState->peFadeMask` ends
    /// it early. Spawned when a status is inflicted.
    EFFECT_STATUS_AILMENT_SCREEN_TINT = EFFECT_ID(EFFECT_TASK_BANK, 0x00F),
    /// The Pyrokinesis cast body: a flame cone and rings that travel from the player
    /// with a collision pair, burst into 0x600F6 flames on a hit and fade on a wall;
    /// spawned by the pyrokinesis entry task.
    EFFECT_PYROKINESIS_CAST = EFFECT_ID(EFFECT_TASK_BANK, 0x010),
    /// Expanding, fading flame cone (glowDrawFlameCone) drawn at the pyrokinesis launch
    /// point; spawned once when the Pyrokinesis projectile is launched.
    EFFECT_PYROKINESIS_LAUNCH_CONE = EFFECT_ID(EFFECT_TASK_BANK, 0x011),
    /// Spinning additive billboard quad (effectDrawSpinningBillboard, or the fading effectDrawModulatedBillboard
    /// quad) that lifts and animates for a few frames; spawned repeatedly around the
    /// player by the Metabolism PE.
    EFFECT_METABOLISM_SPARKLE = EFFECT_ID(EFFECT_TASK_BANK, 0x013),
    /// Rising spark sprite (`_risingSparkTask`) emitted every eighth tick by the Healing
    /// PE effect around the player.
    EFFECT_HEALING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x016),
    /// Healing PE particle: an animated sprite that drifts for 30 ticks at the PE
    /// level's size and every eighth tick parents a 0x60016 child; sprayed by the
    /// Healing cast.
    EFFECT_HEALING_SPARKLE = EFFECT_ID(EFFECT_TASK_BANK, 0x017),
    /// Necrosis trail puff: a shrinking animated spriteQuadDraw sprite that sheds
    /// 0x6001A every third frame; spawned each frame along the Necrosis cast path.
    EFFECT_NECROSIS_TRAIL_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x019),
    /// One drifting necrosis mist puff or spore-cloud sprite, flung outward on a random
    /// bearing; spawned every third tick by the Necrosis cast.
    EFFECT_NECROSIS_MIST_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x01A),
    /// Combustion PE controller: parents to the player model, plays the ignition sound,
    /// fades the screen and spawns a flame every frame while drifting the flame
    /// overlay; Combustion spawns two (arg +1/-1).
    EFFECT_COMBUSTION_FLAME_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x01B),
    /// One flame of the Combustion PE burn, parented to the player, drawn every frame
    /// and trailing embers; spawned each frame by func_combustion_8012EF34.
    EFFECT_COMBUSTION_FLAME = EFFECT_ID(EFFECT_TASK_BANK, 0x01C),
    /// Pulsing red gradient-quad beacon glow (red_beacon_task) placed at fixed points
    /// in view 2 of the Akropolis west elevator hall by its room task.
    EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON = EFFECT_ID(EFFECT_TASK_BANK, 0x01F),
    /// Pulsing red warning light: one frame of a pair of red gradient quads whose level
    /// pulses with animFrame (`acropolisEastElevatorHallRedBeaconTask`); respawned at fixed points by the
    /// Acropolis east elevator hall in view 2.
    EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON = EFFECT_ID(EFFECT_TASK_BANK, 0x022),
    /// One-frame soft light billboard (semi-transparent POLY_FT4 shrinking with
    /// distance) in the Akropolis west elevator hall, respawned while camera view 5 is
    /// active.
    EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_LIGHT = EFFECT_ID(EFFECT_TASK_BANK, 0x025),
    /// Flickering lens-flare POLY_FT4 (tpage 0x2B, CLUT alternating per frame) at the
    /// projected model position; respawned per camera view by the observatory's
    /// ambient-effect task.
    EFFECT_ACROPOLIS_OBSERVATORY_LENS_FLARE = EFFECT_ID(EFFECT_TASK_BANK, 0x028),
    /// Muzzle flash controller for P229/M93R/M950/MP5A5 and NPC guns (spawnArg 0x21):
    /// parks on the weapon muzzle offset, lights a white transient point light, spawns
    /// flare sprite 0x60034 and shell-casing model 0x60036/0x60066.
    EFFECT_HANDGUN_MUZZLE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x02B),
    /// The M4A1 javelin's guide beam: a muzzle flare with red transient light and ring
    /// of tracers, then the beam to the impact point; spawned when the javelin is
    /// fired.
    EFFECT_JAVELIN_GUIDE_BEAM = EFFECT_ID(EFFECT_TASK_BANK, 0x02F),
    /// Unidentified. Shared gravity particle (Gp_EffSprTask30): an animated textured
    /// sprite (Gp_DrawEffSpark) that flies with gravity, bounces off geometry spawning
    /// 0x60055/0x60070, then lies spreading an additive ground quad and shedding
    /// 0x600A7; spawned from enemy bodies on bursts and deaths (Mad Chaser gibs,
    /// Sucklerceph burst, many actors).
    EFFECT_030 = EFFECT_ID(EFFECT_TASK_BANK, 0x030),
    /// A particle orbiting and bobbing around the player, spreading out and fading as
    /// the charge ends; up to 32 are spawned by the PE charge task func_800FAA14 while
    /// a Parasite Energy is charging.
    EFFECT_PE_CHARGE_PARTICLE = EFFECT_ID(EFFECT_TASK_BANK, 0x032),
    /// Ramps the lift bay CLUT from its unlit to its lit palette (gpuBlendRgb555ClutRow)
    /// and keeps it lit only in camera view 5; spawned by a west-elevator-hall
    /// message handler.
    EFFECT_ACROPOLIS_WEST_ELEVATOR_BAY_LIGHTS = EFFECT_ID(EFFECT_TASK_BANK, 0x033),
    /// Two-frame rotated textured flare sprite (tpage 0x2A) at a muzzle; spawned by the
    /// muzzle flash controllers 0x2B/0x6A/0x6B/0xA1 and by Actor02100.
    EFFECT_MUZZLE_FLARE = EFFECT_ID(EFFECT_TASK_BANK, 0x034),
    /// Small animated spark sprite (tpage 0x28, 24px cells) with optional random drift;
    /// sprayed by the gun muzzle-flash controllers (pistols/SMG 0x2B, grenade launcher
    /// 0x6C, 0x6A, 0x6E, 0xA1).
    EFFECT_MUZZLE_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x035),
    /// Ejected cartridge case: a small TMD model (D_80111FC8) thrown with spin and
    /// gravity by effectThrownModelTask; spawned by the handgun/SMG firing controller 0x6002B
    /// and six at once by the reload effect 0x6006D.
    EFFECT_BULLET_CASING = EFFECT_ID(EFFECT_TASK_BANK, 0x036),
    /// Blade trail (_bladeTrailDraw over eight base/tip frame pairs) following the tonfa
    /// baton during a swing; spawned by the tonfa baton weapon on its attack.
    EFFECT_TONFA_BATON_SWING_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x03A),
    /// Bullet/impact spark: a rotated flash sprite (_effectDrawImpactSparkFlash) shown for 4
    /// frames plus six 0x600A4 spark tiles; spawned at weapon hit points (player hit
    /// dispatcher, M4A1 Javelin, several enemies, helipad floodlight).
    EFFECT_IMPACT_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x03B),
    /// Bouncing camera-facing sprite particle that collides with geometry, spawns dust
    /// 0x60054 on impacts and fades; thrown by the Dryfield breezeway first-event
    /// emitter in views 2/3.
    EFFECT_DRYFIELD_BREEZEWAY_BOUNCING_PARTICLE = EFFECT_ID(EFFECT_TASK_BANK, 0x03C),
    /// Dryfield night motel balcony debris chunk: an animated piece thrown with drift
    /// that bounces off collision (worldCollisionProbeGridSegment) losing speed; sprayed by the
    /// balcony's break bursts and ambient task.
    EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS = EFFECT_ID(EFFECT_TASK_BANK, 0x03D),
    /// M4A1 bayonet blade trail: records tip and hilt frames in an 8-slot ring each
    /// frame and draws the ribbon with _bladeTrailDraw for 13 frames; spawned on the
    /// bayonet stab.
    EFFECT_M4A1_BAYONET_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x03E),
    /// A small drifting sprite particle with random velocity, spawned at random by the
    /// attached projectile sprite 0x60081 while it bursts.
    EFFECT_PROJECTILE_BURST_PARTICLE = EFFECT_ID(EFFECT_TASK_BANK, 0x03F),
    /// The P229 pistol's muzzle-flash task (_muzzleFlashTask); spawned when the P229
    /// fires.
    EFFECT_P229_MUZZLE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x040),
    /// MP5A5 muzzle flash (shared _muzzleFlashTask: white point light, spinning core,
    /// additive screen tint and four streaks) used by the weapon's second firing branch, which
    /// consumes ammo with the 0x101 flag.
    EFFECT_MP5A5_ALT_FIRE_MUZZLE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x041),
    /// Small rising puff sprite (tpage 9 row 0xB8, selectable blend) shed behind moving
    /// projectiles: the specimen projectile 0x60081 and attach task 0x37.
    EFFECT_TRAIL_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x042),
    /// Flame-jet emitter attached to a part of actor_510900: holds an orange transient
    /// point light and, by an intensity mode the actor sets, keeps throwing
    /// 0x60045/0x6004C/0x60052/0x60059 sprite particles backward along -x.
    EFFECT_ACTOR_510900_FLAME_JET = EFFECT_ID(EFFECT_TASK_BANK, 0x043),
    /// One-shot flash at No.9 golem (Akropolis) model part 8: an orange transient point
    /// light plus effect 0x6003B, six 0x60065 sprites and six pixel sparks; spawned
    /// periodically in its sub-state 3 attack.
    EFFECT_NO9_MUZZLE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x044),
    /// Animated six-frame additive sprite (tpage 0x2B) that rises while fading and
    /// casts a ground glow (effectDrawGroundGlow); spawned in pairs by actor_510900's
    /// fire controller (0x60043), which also drives an orange point light.
    EFFECT_NO9_GOLEM_FLAME = EFFECT_ID(EFFECT_TASK_BANK, 0x045),
    /// Flat textured quad lying in the coordinate's horizontal plane (tpage 0x29) that
    /// grows, holds or fades out by spawn flags; spawned under Actor02500, actor_160900
    /// and actor_560800 parts.
    EFFECT_GROUND_DECAL = EFFECT_ID(EFFECT_TASK_BANK, 0x046),
    /// Pulsing red-or-cyan gouraud glow quads (and optional line) at fixed points in
    /// some Akropolis square views, spawned by the room task.
    EFFECT_ACROPOLIS_SQUARE_BEACON_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x047),
    /// One security-camera monitor feed: a single-frame 128x128 screen-space textured
    /// quad picked by spawnArg (4 feeds); the security room respawns all four each
    /// frame after re-blending their CLUTs.
    EFFECT_ACROPOLIS_SECURITY_MONITOR_FEED = EFFECT_ID(EFFECT_TASK_BANK, 0x049),
    /// One-frame flickering star glow (animated core quad and rotating flare) at
    /// the Akropolis promenade lamps; respawned each frame by the room spawner for
    /// views that see it.
    EFFECT_ACROPOLIS_PROMENADE_GLOW_STAR = EFFECT_ID(EFFECT_TASK_BANK, 0x04B),
    /// Unidentified. Eight-frame additive sprite (tpage 0x2B, v 0x70) whose CLUT steps
    /// with age and which rises by a random amount; spawned alongside 0x60045 by
    /// actor_510900's fire controller (0x60043).
    EFFECT_04C = EFFECT_ID(EFFECT_TASK_BANK, 0x04C),
    /// Pulsing red or cyan flare (diamond with optional crossed streaks, or radial
    /// glow) at view-dependent offsets; spawned every frame by the Acropolis fire
    /// escape effect emitter for views 3/6/8/9.
    EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE = EFFECT_ID(EFFECT_TASK_BANK, 0x04F),
    /// Debris-burst controller for the night motel balcony: by kind sprays 0x6003D
    /// chunks and 0x60095 puffs once; spawned by actor_403100 where it strikes a
    /// balcony region (first strike marks the region broken).
    EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK = EFFECT_ID(EFFECT_TASK_BANK, 0x050),
    /// Falling leaf (`_leafFallTask`) of the Acropolis forked road; three are knocked
    /// loose by the Maggot Caterpillar entrance.
    EFFECT_ACROPOLIS_FORKED_ROAD_FALLING_LEAF = EFFECT_ID(EFFECT_TASK_BANK, 0x051),
    /// A 12-frame animated 32x48 billboard with cycling CLUT that optionally falls,
    /// spawned in bursts by No.9 golem (Akropolis) fire controller 0x60043 under an
    /// orange point light.
    EFFECT_NO9_FLAME_SPRITE = EFFECT_ID(EFFECT_TASK_BANK, 0x052),
    /// Persistent task parented to the player's root part that draws the ground-shadow
    /// quad (effectDrawGroundShadow, size 0x1C0, groundShadowShade) under the player;
    /// spawned once when the room-effect state is initialised.
    EFFECT_PLAYER_GROUND_SHADOW = EFFECT_ID(EFFECT_TASK_BANK, 0x053),
    /// Sand-tinted (0x68,0x70,0x38) semi-transparent 8-frame puff sprite drifting
    /// slightly upward, optionally spawning smaller puffs; spawned by Desert Chaser
    /// footsteps/landings, Dryfield rooms and many actors.
    EFFECT_DUST_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x054),
    /// Rising impact puff sprite (tpage 9, two palettes by bit 28, random drift); the
    /// general hit effect for bullet impacts (func_800FDB18 kinds 1/2/6/8), damage-
    /// over-time ticks and many enemy hits.
    EFFECT_HIT_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x055),
    /// Screen-space drip: a DR_MOVE that smears a one-pixel strip of the frame buffer
    /// down a row at a time while the camera stays on its view; the promenade spawns
    /// 0x28 on a view change and two per frame after.
    EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP = EFFECT_ID(EFFECT_TASK_BANK, 0x056),
    /// One-frame flickering glow quad lying flat on the ground under the promenade
    /// lamp; respawned each frame by the room spawner.
    EFFECT_ACROPOLIS_PROMENADE_GROUND_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x057),
    /// Expanding, fading splash quad (_waterRippleTask, _waterDrawSplash); stored in
    /// gRoomEffectWaterRippleId, which several actors (actor_800100, 401300, 400600, 01100) spawn as
    /// water ripples.
    EFFECT_NEO_ARK_WOODLAND_PATH_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x058),
    /// Additive 8-frame sprite (tpage 0x4B, clut 0x4382) that rises a random amount per
    /// tick; thrown backwards in bursts by No.9's gunfire controller (0x60043, orange
    /// transient light).
    EFFECT_NO9_GUNFIRE_PARTICLE = EFFECT_ID(EFFECT_TASK_BANK, 0x059),
    /// Akropolis helipad ember/spark sprite (bright or dim variant, flickering, may
    /// fire 0x600E0); spawned around the helipad and by actor_510900.
    EFFECT_ACROPOLIS_HELIPAD_EMBER = EFFECT_ID(EFFECT_TASK_BANK, 0x05A),
    /// Sparking helipad light: random spark lines and a flickering bluish light on slot
    /// 4, and every few frames a spark sound, a 0x6003B flash, six 0x600A4 tiles and a
    /// second light; spawned by actor_510900 when it is grabbed/struck.
    EFFECT_HELIPAD_LIGHT_SPARKS = EFFECT_ID(EFFECT_TASK_BANK, 0x05B),
    /// Growing, spinning animated billboard from the shared 12-frame effect atlas,
    /// which can spawn smaller copies of itself; spawned when generators, golem parts
    /// and other enemy parts blow up.
    EFFECT_EXPLOSION = EFFECT_ID(EFFECT_TASK_BANK, 0x05C),
    /// One drifting, rotating lens-flare POLY_FT4 (tpage 0x2B) that may flicker
    /// green/blue-white and fire 0x600E0/0x6005A; spawned in pairs by the helipad
    /// beacon task.
    EFFECT_ACROPOLIS_HELIPAD_LENS_FLARE = EFFECT_ID(EFFECT_TASK_BANK, 0x05E),
    /// Unidentified. Emitter in the Acropolis helipad overlay that lights transient
    /// point light 4 bluish, spawns pairs of 0x6005E lens flares and occasional
    /// 0x6005A; spawned when No.9's grab child lands a hit.
    EFFECT_05F = EFFECT_ID(EFFECT_TASK_BANK, 0x05F),
    /// Akropolis cafeteria event billboard: a ten-cell 48px animated sprite drifting
    /// along Z then Y while the room's effect gate holds in view mode 9; sprayed by
    /// 0x6009D.
    EFFECT_ACROPOLIS_CAFETERIA_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x061),
    /// One-frame lamp glow sprite (GLOW_LAMP_TASK: one of three flickering
    /// lamp cells); the promenade respawns it each frame at the lamps its view mask
    /// allows.
    EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x062),
    /// Unidentified. A small room model (gAcropolisCafeteriaModel077D8) that wanders
    /// with random turns and then runs off, playing a cafeteria sound in view 7;
    /// several are spawned by the Akropolis cafeteria event task.
    EFFECT_064 = EFFECT_ID(EFFECT_TASK_BANK, 0x064),
    /// Debris trail: the coordinate drifts by a random step and the segment between
    /// last and current position is drawn as a fading LINE_F2; six are spawned with an
    /// 0x6003B spark and 0x600A4 tiles by actor_510900's burst effect (0x60044).
    EFFECT_NO9_GOLEM_DEBRIS_STREAK = EFFECT_ID(EFFECT_TASK_BANK, 0x065),
    /// Ejected shell-casing model (TmdSource D_80112200) thrown from the muzzle by the
    /// muzzle-flash controller for weapon id 5 (P229) and NPC gunfire (0x21).
    EFFECT_P229_SHELL_CASING = EFFECT_ID(EFFECT_TASK_BANK, 0x066),
    /// Muzzle-flash model piece (effectThrownModelTask with TmdSource D_801120E4) placed at
    /// the weapon's muzzle offset; spawned by the rifle muzzle-flash controller 0x6006B
    /// (M4A1 family, M249).
    EFFECT_RIFLE_MUZZLE_FLASH_MODEL = EFFECT_ID(EFFECT_TASK_BANK, 0x067),
    /// Ejected shotgun shell: a small TMD model (D_8011231C) thrown by effectThrownModelTask;
    /// spawned by the shotgun firing controller 0x600A1 (AS12, PA3, gunblade).
    EFFECT_SHOTGUN_SHELL_CASING = EFFECT_ID(EFFECT_TASK_BANK, 0x068),
    /// A rising 8-frame flame sprite (`_pyroFlameDrawSprite`) with random spin, spawned
    /// along the Pyrokinesis projectile while it flies.
    EFFECT_PYROKINESIS_FLAME_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x069),
    /// Gun muzzle flash without a flash model: lights the transient point light white
    /// for ~5 frames at the D_801124DC muzzle offset, spawns a 0x60034 flash and three
    /// 0x60035 puffs and raises burstRequest; sibling of the weapons' 0x6006B, spawned
    /// by NPC actors (actor_310600, actor_511000) when they fire.
    EFFECT_ACTOR_MUZZLE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x06A),
    /// Muzzle flash controller for the M4A1 family, M249 and companion rifle fire:
    /// point light at the muzzle, flare 0x60034, sparks 0x60072 and casing model
    /// 0x60067.
    EFFECT_RIFLE_MUZZLE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x06B),
    /// Grenade-launcher muzzle flash controller: transient white point light, flash
    /// sprite, smoke 0x6006F and sparks 0x60035, then a 0x60091 model; spawned when the
    /// grenade pistol, M4A1 grenade launcher or companion fires a grenade.
    EFFECT_GRENADE_MUZZLE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x06C),
    /// Reload casing drop: identity-rotated controller that spawns six 0x60036 casings
    /// in the falling mode (arg 9) and dies; spawned on the player reload state unless
    /// reloadEffectSuppressed, and by actor_310600.
    EFFECT_RELOAD_CASINGS_DROP = EFFECT_ID(EFFECT_TASK_BANK, 0x06D),
    /// Controller placed at a per-weapon offset (D_801125EC) on its parent that spawns
    /// three 0x60035 sprites, then model effect 0x60091 every tick for 12 ticks;
    /// spawned in the player reload state (unless reloadEffectSuppressed) and by
    /// golem/actor_800100 equivalents.
    EFFECT_RELOAD_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x06E),
    /// Short-lived 8-frame animated sprite (tpage 0x28) thrown with a random velocity;
    /// four are spawned with the flash/smoke of the gun-blast controllers 0x600A1
    /// (func_800ED42C) and 0x6006C (grenade launcher).
    EFFECT_MUZZLE_SPARK_THROWN = EFFECT_ID(EFFECT_TASK_BANK, 0x06F),
    /// Generic 8-frame smoke puff sprite (same atlas cell as the dust puff, untinted)
    /// that drifts or rises and can spawn smaller puffs; used by explosions, generator
    /// deaths, projectiles and many actors.
    EFFECT_SMOKE_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x070),
    /// Grenade explosion: flash light, fade quad and arcs, spawning smoke (0x60070),
    /// 0x6005C, 0x60076/0x7C/0x92 pieces; spawned where a grenade shell lands.
    EFFECT_GRENADE_EXPLOSION = EFFECT_ID(EFFECT_TASK_BANK, 0x071),
    /// Two-frame additive rotated muzzle sprite (tpage 0x28, 32-px animated cells)
    /// sized by spawnArg; always spawned together with 0x60034 by the rifle (0x6006B)
    /// and shotgun (0x600A1) firing controllers and by actor_02100.
    EFFECT_MUZZLE_FLARE_ADDITIVE = EFFECT_ID(EFFECT_TASK_BANK, 0x072),
    /// dryfield_motel_balcony's copy of the RoomFx twin trail: two eight-slot rings of
    /// frames drawn as a fading beam behind an anchor; the room stores it in slot
    /// gRoomEffectTwinTrailId, which the beam-sword Pawn/Rook golems spawn at body part 7
    /// (golemPawnRookDelayedEffectTick).
    EFFECT_DRYFIELD_MOTEL_BALCONY_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x073),
    /// Spark burst (RoomFx_SparkBurstTask): a flash then a spray of jittered sparks, or
    /// two widening rings, over seven ticks; stored in gRoomEffectSparkBurstId, which the grenade
    /// golems (actor_05600/05700) spawn when a shot ends.
    EFFECT_DRYFIELD_MOTEL_BALCONY_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x074),
    /// Four-frame additive flash sprite from Gp_EffSprRecs at a point, released after
    /// four ticks; first effect of RoomFx spark bursts and gunblade/func_800F4308
    /// impacts.
    EFFECT_IMPACT_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x076),
    /// Akropolis sanctuary mosaic controller: spawns one 0x60079 falling tile per
    /// mosaic entry (plus a second pass over 16 tiles) and releases.
    EFFECT_ACROPOLIS_SANCTUARY_MOSAIC = EFFECT_ID(EFFECT_TASK_BANK, 0x078),
    /// Falling mosaic tile: a whole textured tile of the sanctuary mosaic drifting and
    /// tumbling, shedding 0x6007A shards and shattering on the wall line.
    EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_TILE = EFFECT_ID(EFFECT_TASK_BANK, 0x079),
    /// A falling, spinning textured triangle shard of the Akropolis sanctuary mosaic
    /// that bounces or shatters into further shards.
    EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_SHARD = EFFECT_ID(EFFECT_TASK_BANK, 0x07A),
    /// Glowing spark particle that falls under gravity, bounces off the traced ground
    /// (halving its speed), draws a ground glow under itself and fades over 31 frames;
    /// used by spark bursts (RoomFx_SparkBurstTask, func_800F4308, gunblade).
    EFFECT_BOUNCING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x07C),
    /// Drifting 10-frame sprite anchored at its base (extends 3/4 above), fading over
    /// its last ticks, emitted in bursts with smoke 0x60070 by the night Motel balcony
    /// event.
    EFFECT_NIGHT_MOTEL_BALCONY_FLAME = EFFECT_ID(EFFECT_TASK_BANK, 0x07E),
    /// Hit-blast controller: for a few ticks sprays 0x60080 / 0x6008D bursts then
    /// 0x60070 smoke from random offsets; spawned by func_800FDB18 hit kinds 3/11/16,
    /// generator body hits and actor_02100.
    EFFECT_HIT_BLAST = EFFECT_ID(EFFECT_TASK_BANK, 0x07F),
    /// Additive animated billboard (6-frame strip on tpage 0x29) that grows over 12
    /// frames, optionally rises, and fades at the end of its life; spawned on
    /// Sucklerceph collapse, enemy remains, gas-station and mine-cavern points and at
    /// the muzzle for weapon slot item 0xE.
    EFFECT_ADDITIVE_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x080),
    /// Attached glowing sprite that follows a projectile coordinate, trailing 0x60042
    /// sprites, and on its owner's signal plays a burst quad with 0x6003F particles;
    /// spawned by the actor_07000 and actor_01100 specimen projectiles.
    EFFECT_PROJECTILE_GLOW_SPRITE = EFFECT_ID(EFFECT_TASK_BANK, 0x081),
    /// Flickering camera-facing sprite (tpage 0x2B) at one of 14 anchors, visible per a
    /// view mask; the patio fountain task spawns 14 of them once (three main jets, four
    /// and seven smaller) plus mist 0x6008F.
    EFFECT_ACROPOLIS_PATIO_FOUNTAIN_JET = EFFECT_ID(EFFECT_TASK_BANK, 0x087),
    /// Flickering water-spray sprite of the Acropolis fountain, drawn only in the eight
    /// views that see it.
    EFFECT_ACROPOLIS_FOUNTAIN_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x088),
    /// Akropolis forked-road wall lamp: a flickering screen-aligned sprite per placed
    /// lamp, skipped on days whose bit is clear; spawned by the room task.
    EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP = EFFECT_ID(EFFECT_TASK_BANK, 0x089),
    /// Roof-garden glow sprite: a flickering, distance-scaled semi-transparent
    /// billboard in one of three cells, drawn only in the views its mask allows; ten
    /// are placed at fixed points by the roof-garden task.
    EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x08A),
    /// The Akropolis sanctuary's flickering flame sprite, visible only from its listed
    /// camera views; twelve are spawned by the sanctuary room task.
    EFFECT_ACROPOLIS_SANCTUARY_FLAME = EFFECT_ID(EFFECT_TASK_BANK, 0x08B),
    /// Flickering three-layer glow of POLY_G4 wedges, shown on views 2/3/7, that plays
    /// SOUND_ACROPOLIS_FIRE_ESCAPE_LIGHT_FLICKER when it brightens; spawned once by the
    /// fire escape's emitter task.
    EFFECT_ACROPOLIS_FIRE_ESCAPE_FLICKER_LIGHT = EFFECT_ID(EFFECT_TASK_BANK, 0x08C),
    /// Additive 8-frame animated sprite (clut 0x430D) that grows then fades while
    /// rising; spawned with 0x60080 and smoke on special-ammo hits (weaponSlotItem
    /// 0xE), by Gp_EffCtlTask7F and by the night gas-station explosion.
    EFFECT_FIRE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x08D),
    /// Hit spark burst: draws the E2 sprite and sprays 0x600E0 then 0x600E1 sparks from
    /// random offsets; spawned by func_800FDB18 hit kinds 7 and 15.
    EFFECT_HIT_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x08E),
    /// Fountain mist mote: a shimmering grey 1x1 tile that random-walks or gathers back
    /// toward its jet anchor; spawned per jet by the patio task.
    EFFECT_ACROPOLIS_PATIO_FOUNTAIN_MIST = EFFECT_ID(EFFECT_TASK_BANK, 0x08F),
    /// Pulsating Gouraud flare with optional rays on the Akropolis roof garden, spawned
    /// by the room task (func_acropolis_roof_garden_8017DCDC) for particular camera
    /// views.
    EFFECT_ACROPOLIS_ROOF_GARDEN_FLARE = EFFECT_ID(EFFECT_TASK_BANK, 0x090),
    /// Unidentified. effectThrownModelTask instance of the small procedural model D_801124B8
    /// placed at the D_8011280C/D_801125EC muzzle offsets; spawned once by the grenade-
    /// fire controller 0x6006C and every frame for ~12 frames by the post-reload
    /// controller 0x6006E.
    EFFECT_091 = EFFECT_ID(EFFECT_TASK_BANK, 0x091),
    /// Fading one-pixel line streaking away from its origin in a random direction,
    /// orange or blue by spawn argument; spawned in fans by gunblade impacts and
    /// func_800F4308.
    EFFECT_SPARK_STREAK = EFFECT_ID(EFFECT_TASK_BANK, 0x092),
    /// Night motel balcony falling piece: dropped with random drift under gravity and
    /// puffing 0x60095 when it reaches the floor; spawned by the balcony's ambient task
    /// in view 0x27.
    EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_FALLING = EFFECT_ID(EFFECT_TASK_BANK, 0x093),
    /// Burst controller of the night motel balcony: for up to 150 frames it randomly
    /// spawns 0x6003D, 0x60093 and 0x60095 bouncing/drifting particles; spawned once at
    /// a light point when GAME_FLAG_07F reaches 1, as the room switches two of its
    /// light glows off.
    EFFECT_NIGHT_MOTEL_BALCONY_LAMP_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x094),
    /// A 12-frame drifting animated sprite puff that rises/drifts on a rolled velocity;
    /// spawned by the Dryfield night motel balcony ambient task and every eighth tick
    /// by the dying actor_403100 (Burner).
    EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x095),
    /// Persistent additive glow disc of eight POLY_G4 wedges at a plaza anchor: warm
    /// random flicker for slots below 0x10, a slow red pulse for 0x10+; the plaza
    /// ambient spawner places seven (slots 0xC-0x12).
    EFFECT_ACROPOLIS_PLAZA_LIGHT_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x096),
    /// dryfield_night_back_street's copy of the shared twin trail
    /// (room_visual_effects_trail_task): two rings of eight frames drawn as a fading
    /// gouraud beam; the room stores it in gRoomEffectTwinTrailId (twin-trail slot, spawned by the
    /// Pawn/Rook golem library at joint 7 via golemPawnRookDelayedEffectTick).
    EFFECT_DRYFIELD_NIGHT_BACK_STREET_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x097),
    /// Akropolis plaza rotating flickering light (blue for slots 1-4, red for 5-6) with
    /// a transient point light and glow cone; six placed by the plaza ambient spawner.
    EFFECT_ACROPOLIS_PLAZA_SIREN_LIGHT = EFFECT_ID(EFFECT_TASK_BANK, 0x098),
    /// Plaza light flare: an eight-spoke POLY_G4 star around the projected point,
    /// pulsing with animFrame, blue for spawn index below 9 and red from 9 up; spawned
    /// at fixed plaza points with indices 7..10.
    EFFECT_ACROPOLIS_PLAZA_LIGHT_FLARE = EFFECT_ID(EFFECT_TASK_BANK, 0x099),
    /// Emitter that for a set time spawns 0x60055 particles at random offsets within a
    /// cube around a hit point; spawned by the weapon hit-effect dispatcher
    /// func_800FDB18 (kind 4).
    EFFECT_HIT_PARTICLE_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x09A),
    /// Emitter that, for a period set by the spawn argument, spawns 0x60055 falling
    /// splatter sprites along a (given or random) direction with shrinking speed; used
    /// by the enemy hit-effect dispatcher (kinds 5 and 9) and by stalkers when hit.
    EFFECT_HIT_SPLATTER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x09B),
    /// Shaded burst ring of shards drawn for eight ticks where a hit rolled a critical
    /// (damage multiplied after Gp_RollEnemyChance); spawned by most enemies' hit
    /// handlers.
    EFFECT_CRITICAL_HIT = EFFECT_ID(EFFECT_TASK_BANK, 0x09C),
    /// Akropolis cafeteria emitter: on entering view mode 9 spawns 40 0x60061
    /// billboards, then two per tick, until the room's effect gate clears; spawned by
    /// the cafeteria event message.
    EFFECT_ACROPOLIS_CAFETERIA_PUFF_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x09D),
    /// Long-lived additive red ground quad laid flat under the coordinate at a random
    /// yaw, slowly dimming over 1024 ticks; spawned at enemies' roots on death/hits
    /// (actor_04000, actor_01200, Sucklerceph, Skull Stalker).
    EFFECT_RED_GROUND_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x09E),
    /// A six-frame animated sprite puff carried on a velocity, emitted every tick along
    /// a direction by the toilet jet emitter 0x600A2 (posted by actor_323300 on its
    /// part 6).
    EFFECT_DRYFIELD_TOILET_JET_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x09F),
    /// Single-frame flash of two gouraud quads and two lines at a projected point, red
    /// and/or green by spawnArg1 bits; spawned for each lit monitor feed by the
    /// security room's monitor task.
    EFFECT_ACROPOLIS_SECURITY_MONITOR_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x0A0),
    /// Muzzle flash controller for shotguns (AS12, PA3, gunblade shot) and
    /// actor_335800: reddish point light, flare 0x60034, sparks, smoke and pellet
    /// effects 0x600A3/0x600A4.
    EFFECT_SHOTGUN_MUZZLE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x0A1),
    /// Dryfield toilet scripted emitter: for N ticks spawns 0x6009F sprites along a
    /// fixed direction with falling speed; posted on part 6 of the scripted
    /// actor_323300 by its message 0x7DB action 13.
    EFFECT_DRYFIELD_TOILET_SPRAY_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x0A2),
    /// Spark streak: a fading LINE_G2 shot forward from the muzzle and lengthening for
    /// four frames; twelve are spawned per shot by the shotgun firing controller.
    EFFECT_SHOTGUN_SPARK_LINE = EFFECT_ID(EFFECT_TASK_BANK, 0x0A3),
    /// A one-or-two-pixel orange TILE spark that flies on a random velocity and fades;
    /// spawned in batches by explosions and flashes (func_800ED42C, Gp_EffCtlTask3B,
    /// No.9 muzzle flash).
    EFFECT_PIXEL_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x0A4),
    /// Corpse-burn controller: starts a shared looping sound (SOUND_COMMON 0x0D,
    /// stopped when the last one ends) and spawns bursts of 0x600A6 orange flickering
    /// fire rings that rise and fade; spawned on dying enemies (generator death state,
    /// many actors) and on flung body parts.
    EFFECT_CORPSE_BURN = EFFECT_ID(EFFECT_TASK_BANK, 0x0A5),
    /// Additive flickering orange hexagonal flame cone that creeps along the ground
    /// then dies down; spawned in threes by the 0x600A5 controller (with fire sound)
    /// when a dying enemy turns semi-transparent.
    EFFECT_DEATH_FLAME = EFFECT_ID(EFFECT_TASK_BANK, 0x0A6),
    /// Additive 32px animated sprite that accelerates upward for eight cells; shed one
    /// time in three by Gp_EffSprTask30 (enemy death effect 0x60030) and by
    /// Gp_EffCtlTaskA6.
    EFFECT_RISING_WISP = EFFECT_ID(EFFECT_TASK_BANK, 0x0A7),
    /// Combustion ember: a rising flame/ember sprite shed by a Combustion flame, drawn
    /// with `_pyroFlameDrawSprite` or smaller variants.
    EFFECT_COMBUSTION_EMBER = EFFECT_ID(EFFECT_TASK_BANK, 0x0A9),
    /// Drifting animated sprite spawned at the reservoir's configured burst points with
    /// a random chance per frame while an event enables them.
    EFFECT_SHELTER_B4_RESERVOIR_BURST_SPRITE = EFFECT_ID(EFFECT_TASK_BANK, 0x0AA),
    /// Antibody PE aura: two rings around the player sized by the PE level, flashing
    /// and spawning 0x600C1 when the player takes damage and 0x600E0 sparkles
    /// otherwise, until the aura flag clears.
    EFFECT_ANTIBODY_AURA = EFFECT_ID(EFFECT_TASK_BANK, 0x0AC),
    /// Rising spark sprite (`_risingSparkTask`) emitted by the Life Drain PE effect.
    EFFECT_LIFEDRAIN_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x0AD),
    /// Life Drain mote: drifts from the hit enemy, sheds sparks, then homes on the
    /// player; spawned per hit by func_800FDB18 kind 13.
    EFFECT_LIFE_DRAIN_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x0AF),
    /// One-frame twinkling star glow with an animated core and rotating random-grey
    /// flare, respawned at the bridge lights.
    EFFECT_ACROPOLIS_BRIDGE_STAR_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x0B1),
    /// One-frame random-grey quad built like the promenade ground glow (unit quad
    /// scaled 0x300 by workm, corners collapsed onto the origin), spawned beside each
    /// bridge star glow 0x600B1.
    EFFECT_ACROPOLIS_BRIDGE_GROUND_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x0B2),
    /// Single-frame camera-facing glow sprite (shared acropolis_glows_lamp, three
    /// cell/brightness variants by spawnArg1); the bridge driver respawns it each frame
    /// at each placed emitter the current view can see.
    EFFECT_ACROPOLIS_BRIDGE_LAMP_GLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x0B3),
    /// One-pixel DR_MOVE smear streak falling down the screen buffer; spawned 30 at a
    /// time on view change and continually outside battle by the Acropolis bridge room
    /// task.
    EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK = EFFECT_ID(EFFECT_TASK_BANK, 0x0B4),
    /// Akropolis bridge falling dust streak (mid variant): a one-pixel DR_MOVE screen
    /// smear falling down the screen; one of the bridge's per-view looping ambience
    /// effects.
    EFFECT_ACROPOLIS_BRIDGE_MID_DUST_STREAK = EFFECT_ID(EFFECT_TASK_BANK, 0x0B5),
    /// Falling dust streak: a one-pixel DR_MOVE frame-buffer smear over the lower part
    /// of the drop while the view holds; the bridge spawns 0x1E on a view change and
    /// more each frame.
    EFFECT_ACROPOLIS_BRIDGE_LOW_DUST_STREAK = EFFECT_ID(EFFECT_TASK_BANK, 0x0B6),
    /// Tallest variant of the Akropolis bridge falling dust streak (DR_MOVE smear down
    /// the whole screen), one of the per-view ambience effects 0x600B4..0x600B8.
    EFFECT_ACROPOLIS_BRIDGE_TALL_DUST_STREAK = EFFECT_ID(EFFECT_TASK_BANK, 0x0B7),
    /// Falling streak drawn as a DR_MOVE that smears a one-pixel strip of the frame
    /// buffer down each frame at a random column/row; one of the bridge's five per-view
    /// looping ambience effects (0x600B4..0x600B8), burst 30 on entering the view.
    EFFECT_ACROPOLIS_BRIDGE_PARTICLE_STREAK = EFFECT_ID(EFFECT_TASK_BANK, 0x0B8),
    /// acropolis_bridge's expanding, fading water ripple uses the same texture as
    /// `_waterDrawSplash`; the room stores it in gRoomEffectWaterRippleId (water-
    /// ripple slot, spawned at the water surface by wading actors).
    EFFECT_ACROPOLIS_BRIDGE_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x0B9),
    /// Akropolis bridge tumbling debris piece (acropolisBridgeEffectSpriteDebrisTask, gravity, eight
    /// cells); stored in gRoomEffectWaterSprayId and trailed off the two moving joints by the bridge
    /// task.
    EFFECT_ACROPOLIS_BRIDGE_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x0BA),
    /// Falling dust mote: a 1x1 tile drifting and falling for 0x1F ticks; spawned in
    /// bursts of 0x20/8 at random points by the bridge.
    EFFECT_ACROPOLIS_BRIDGE_DUST_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x0BC),
    /// An expanding, fading coloured band (effectDrawInnerGlowBand) at a Z angle from the spawn
    /// argument, with colour from D_80112C6C; spawned three at a time 120 degrees apart
    /// by Gp_EffCtlTaskAC and the gunblade.
    EFFECT_EXPANDING_COLOR_BAND = EFFECT_ID(EFFECT_TASK_BANK, 0x0C1),
    /// Companion task of the Inferno PE cast: spins and fades a drawn shape through two
    /// draw kinds (`_infernoDrawRisingFanBand`/`_infernoDrawConstantLiftFanBand`),
    /// optionally walking outward; Inferno spawns several in its cast states.
    EFFECT_INFERNO_FLAME = EFFECT_ID(EFFECT_TASK_BANK, 0x0DA),
    /// Six-frame additive animated sprite (tpage 0x29, selectable palette) burst;
    /// spawned by generator deaths, diver impacts, golem deaths, mine cavern and
    /// Hypervelocity.
    EFFECT_FLASH_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x0E0),
    /// Eight-frame animated spark sprite on tpage 0x28 whose palette steps with its
    /// kind; the late-phase spark of hit spark bursts (0x6008E) and the hypervelocity
    /// charge particles.
    EFFECT_SPARK_FADE = EFFECT_ID(EFFECT_TASK_BANK, 0x0E1),
    /// Unidentified. Short-lived emitter that spawns one 0x60070 particle per frame for
    /// (spawnArg high * 4) frames at an offset from its parent; spawned by the hit-
    /// effect dispatcher func_800FDB18 (case 10).
    EFFECT_0E3 = EFFECT_ID(EFFECT_TASK_BANK, 0x0E3),
    /// dryfield_night_back_street's copy of the RoomFx spark burst: spawns a flash,
    /// then sprays jittered sparks or draws widening rings for seven ticks; the room
    /// stores it in slot gRoomEffectSparkBurstId, which the grenade-launcher Pawn/Rook golems spawn
    /// where a grenade lands (golemPawnRookBulletFly).
    EFFECT_DRYFIELD_NIGHT_BACK_STREET_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x0E4),
    /// Flash (_roomVisualEffectsFlashTask): fans and an inward ring ramping up, a fade quad at the
    /// peak, then a star fade-out; stored in gRoomEffectFlashId, which golems
    /// (actor_02300/05700) spawn at part 4 in their scream sequence.
    EFFECT_DRYFIELD_NIGHT_JUNK_YARD_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x0E5),
    /// dryfield_night_junk_yard's copy of the shared twin trail
    /// (room_visual_effects_trail_task): two rings of eight frames drawn as a fading
    /// gouraud beam; the room stores it in gRoomEffectTwinTrailId (twin-trail slot, spawned by the
    /// Pawn/Rook golem library at joint 7 via golemPawnRookDelayedEffectTick).
    EFFECT_DRYFIELD_NIGHT_JUNK_YARD_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x0E6),
    /// dryfield_night_junk_yard's copy of RoomFx_SparkBurstTask (a flash then sprayed
    /// sparks or a widening ring for seven ticks), stored in gRoomEffectSparkBurstId, which the
    /// grenade-launcher GOLEM's bullet spawns where it hits.
    EFFECT_DRYFIELD_NIGHT_JUNK_YARD_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x0E7),
    /// Darkness status screen dim: a flickering full-screen fade quad (effectDrawScreenTint)
    /// held while the player has PLAYER_STATUS_DARKNESS, faded out when it clears.
    EFFECT_DARKNESS_SCREEN_DIM = EFFECT_ID(EFFECT_TASK_BANK, 0x0E8),
    /// mine_mesa's copy of the RoomFx flash: ramps up two fans and a shrinking ring
    /// with a fade quad at its peak, then fades out through a star; the room stores it
    /// in slot gRoomEffectFlashId, which the Pawn/Rook golem library spawns at body part 4 for
    /// its scream (golemPawnRookSilenceScream).
    EFFECT_MINE_MESA_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x0E9),
    /// Blue expanding band (effectDrawInnerGlowBand) tilted by the spawn argument and sized by
    /// the PE level, fading 8 per frame; spawned in a loop and reparented by the
    /// Lifedrain PE.
    EFFECT_LIFEDRAIN_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x0EA),
    /// mine_mesa's copy of the shared twin trail (room_visual_effects_trail_task): two
    /// rings of eight frames drawn as a fading gouraud beam; the room stores it in
    /// gRoomEffectTwinTrailId (twin-trail slot, spawned by the Pawn/Rook golem library at joint 7
    /// via golemPawnRookDelayedEffectTick).
    EFFECT_MINE_MESA_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x0EB),
    /// mine_mesa's copy of RoomFx_SparkBurstTask (a flash then sprayed sparks or a
    /// widening ring for seven ticks), stored in gRoomEffectSparkBurstId, which the grenade-launcher
    /// GOLEM's bullet spawns where it hits.
    EFFECT_MINE_MESA_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x0EC),
    /// The shelter_b4_lower_sewer instance of the room-effect library's
    /// _roomVisualEffectsFlashTask: a flash: two fans and an inward-shrinking ring ramping up over
    /// spawnArg1 ticks, peaking with a coloured fade quad, then fading through a star;
    /// the room stores it in slot gRoomEffectFlashId, read by the Pawn/Rook GOLEM silence-
    /// scream state (golem_pawn_rook_silence_scream.inc.c, part 4).
    EFFECT_SHELTER_B4_LOWER_SEWER_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x0ED),
    /// shelter_b4_lower_sewer's copy of the RoomFx twin trail: two eight-slot rings of
    /// frames drawn as a fading beam behind an anchor; the room stores it in slot
    /// gRoomEffectTwinTrailId, which the beam-sword Pawn/Rook golems spawn at body part 7
    /// (golemPawnRookDelayedEffectTick).
    EFFECT_SHELTER_B4_LOWER_SEWER_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x0EE),
    /// Spark burst (RoomFx_SparkBurstTask): a flash then a spray of jittered sparks, or
    /// two widening rings, over seven ticks; stored in gRoomEffectSparkBurstId, which the grenade
    /// golems (actor_05600/05700) spawn when a shot ends.
    EFFECT_SHELTER_B4_LOWER_SEWER_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x0EF),
    /// shelter_b4_upper_sewer's copy of the shared flash task (_roomVisualEffectsFlashTask): ramps
    /// up two fans and a shrinking ring, queues a full-screen fade quad at its peak,
    /// then fades out; the room stores it in gRoomEffectFlashId (flash slot, spawned by
    /// golemPawnRookSilenceScreamState).
    EFFECT_SHELTER_B4_UPPER_SEWER_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x0F0),
    /// shelter_b4_upper_sewer's copy of the RoomFx twin-trail task (two eight-point
    /// rings drawn as a beam), stored in gRoomEffectTwinTrailId, which the beam-sword GOLEM's sword
    /// child spawns at part 7 ten frames after it appears.
    EFFECT_SHELTER_B4_UPPER_SEWER_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x0F1),
    /// The shelter_b4_upper_sewer instance of the room-effect library's
    /// RoomFx_SparkBurstTask: a spark burst: a flash plus jittered sparks (non-zero
    /// arg) or two dimming rings (zero arg) for seven ticks; the room stores it in slot
    /// gRoomEffectSparkBurstId, read on impact by the Pawn/Rook GOLEM grenade
    /// (golem_pawn_rook_bullet_fly.inc.c; grenade-launcher golems actor_05600/05700).
    EFFECT_SHELTER_B4_UPPER_SEWER_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x0F2),
    /// Energy Shot aura controller on the player: sets ROOM_EFFECT_PE_ENERGY_SHOT_AURA,
    /// draws a triangle burst and two rings on request, and spawns 0x600F4 at the
    /// player's hands; spawned by Energy Shot once charged.
    EFFECT_ENERGY_SHOT_AURA = EFFECT_ID(EFFECT_TASK_BANK, 0x0F3),
    /// Additive animated billboard (effectDrawSpinningBillboard) that rises for 8 animation frames
    /// with a random or fixed CLUT; spawned at random player hand joints by the Energy
    /// Shot aura (0x600F3), by Energy Shot itself, and at random joints of the model in
    /// the B2 pod bottom.
    EFFECT_RISING_ENERGY_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x0F4),
    /// Antibody PE mote that falls toward the player and draws sprite quads/arcs,
    /// released at tick 0x15.
    EFFECT_ANTIBODY_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x0F5),
    /// Pyrokinesis flame ring: an expanding flame ring (glowDrawFlameRing) rotated by
    /// its spawn angle, two or three per burst; spawned when the Pyrokinesis shot hits
    /// or expires.
    EFFECT_PYROKINESIS_FLAME_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x0F6),
    /// Apobiosis shard: pinned to the cast (non-zero arg) or drifting free, redrawn
    /// every other frame at an intensity row; spawned by the Apobiosis cast and by the
    /// hit-effect dispatcher (case 12).
    EFFECT_APOBIOSIS_SHARD = EFFECT_ID(EFFECT_TASK_BANK, 0x0F7),
    /// One ball of the Energy Ball cast: grows, flies homing at targets with its own
    /// light and collision, bursts into 0x600F9 effects on a hit.
    EFFECT_ENERGY_BALL = EFFECT_ID(EFFECT_TASK_BANK, 0x0F8),
    /// Green expanding band (effectDrawInnerGlowBand) rotated by the spawn argument and fading 8
    /// per frame; Energyball spawns three (0, 0x2AA, 0x555) when the ball makes
    /// contact.
    EFFECT_ENERGYBALL_IMPACT_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x0F9),
    /// Falling, tumbling leaf that stops and fades (`_leafFallTask`, forest
    /// zone copy); spawned at many joints by the Stranger enemies (actor_356100 death,
    /// actor_401300).
    EFFECT_NEO_ARK_FOREST_FALLING_LEAF = EFFECT_ID(EFFECT_TASK_BANK, 0x0FB),
    /// dryfield_water_hole's water ripple: a growing, fading quad drawn by
    /// _waterDrawSplash; stored in gRoomEffectWaterRippleId, which wading actors (companion, enemies)
    /// spawn at the water surface.
    EFFECT_DRYFIELD_WATER_HOLE_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x0FD),
    /// The dryfield_water_hole instance of the water library's `_waterDriftTaskU16`:
    /// a water-library drift particle: a fixed-angle rotated (or upright)
    /// animated sprite thrown by the kind in spawnArg bits 24-27, falling under
    /// gravity, released after frame 7; the room stores it in slot gRoomEffectWaterSprayId, read
    /// wherever something breaks the water surface (actor_800100, actor_401300,
    /// actor_400600 ring of 16 at waterY, actor_00400/206100 diver splashes, diver
    /// impact burst, actor_560800) and by the water rooms themselves with arg
    /// 0x1202180.
    EFFECT_DRYFIELD_WATER_HOLE_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x0FE),
    /// dryfield_night_water_hole's copy of the water ripple: a flat textured quad on
    /// the water that grows and dims; the room stores it in slot gRoomEffectWaterRippleId, which
    /// water-dwelling actors (actor_01100, actor_400600, actor_401300, actor_800100)
    /// spawn.
    EFFECT_DRYFIELD_NIGHT_WATER_HOLE_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x0FF),
    /// Water particle (`_waterDriftTaskU16`) drawn at a fixed random angle or upright, thrown
    /// with a velocity under gravity; stored in gRoomEffectWaterSprayId, which several actors spawn
    /// as splash drops.
    EFFECT_DRYFIELD_NIGHT_WATER_HOLE_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x11F),
    /// shelter_b2_main_corridor's copy of the shared expanding, fading water ripple
    /// (_waterRippleTask / _waterDrawSplash); the room stores it in gRoomEffectWaterRippleId (water-
    /// ripple slot, spawned at the water surface by wading actors).
    EFFECT_SHELTER_B2_MAIN_CORRIDOR_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x16A),
    /// Corridor water spray (`shelterB2MainCorridorWaterDriftTask`): eight cells
    /// drawn from the cached coordinate matrix while local motion and gravity
    /// advance. Published in `gRoomEffectWaterSprayId` for water-contact spawns.
    EFFECT_SHELTER_B2_MAIN_CORRIDOR_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x16B),
    /// The shelter_b2_septic_tank instance of the room-effect library's _waterRippleTask
    /// / waterRippleTaskFixedCoord: an expanding, fading flat splash quad
    /// (_waterDrawSplash) turned to a random yaw; the room stores it in slot gRoomEffectWaterRippleId,
    /// read alongside gRoomEffectWaterSprayId at the water surface (actor_800100, actor_401300,
    /// actor_400600, actor_01100, water-room surface hits with arg 0x40).
    EFFECT_SHELTER_B2_SEPTIC_TANK_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x16C),
    /// shelter_b2_septic_tank's copy of the water drift sprite: an eight-frame water
    /// sprite flung on a velocity under gravity; the room stores it in slot gRoomEffectWaterSprayId,
    /// which actors and the diver impact burst spawn as spray.
    EFFECT_SHELTER_B2_SEPTIC_TANK_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x16D),
    /// Expanding, fading splash quad (_waterRippleTask, _waterDrawSplash); stored in
    /// gRoomEffectWaterRippleId, which several actors (actor_800100, 401300, 400600, 01100) spawn as
    /// water ripples.
    EFFECT_SHELTER_B4_LOWER_SEWER_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x16E),
    /// shelter_b4_lower_sewer's copy of the shared water droplet sprite particle
    /// (`_waterDriftTaskU16`): fixed-angle rotated or upright sprite thrown by a velocity kind,
    /// pulled down by gravity; the room stores it in gRoomEffectWaterSprayId (water-spray slot).
    EFFECT_SHELTER_B4_LOWER_SEWER_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x16F),
    /// shelter_b4_upper_sewer's water ripple: a growing, fading quad drawn by
    /// _waterDrawSplash; stored in gRoomEffectWaterRippleId, which wading actors (companion, enemies)
    /// spawn at the water surface.
    EFFECT_SHELTER_B4_UPPER_SEWER_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x170),
    /// The shelter_b4_upper_sewer instance of the water library's `_waterDriftTask`:
    /// a water-library drift particle: a rotated (or upright)
    /// animated sprite thrown by the kind in spawnArg bits 24-27, falling under
    /// gravity, released after frame 7; the room stores it in slot gRoomEffectWaterSprayId, read
    /// wherever something breaks the water surface (actor_800100, actor_401300,
    /// actor_400600 ring of 16 at waterY, actor_00400/206100 diver splashes, diver
    /// impact burst, actor_560800) and by the water rooms themselves with arg
    /// 0x1202180.
    EFFECT_SHELTER_B4_UPPER_SEWER_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x171),
    /// shelter_b4_reservoir's copy of the water ripple: a flat textured quad on the
    /// water that grows and dims; the room stores it in slot gRoomEffectWaterRippleId, which water-
    /// dwelling actors (actor_01100, actor_400600, actor_401300, actor_800100) spawn.
    EFFECT_SHELTER_B4_RESERVOIR_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x172),
    /// Water particle (`_waterDriftTask`) drawn as a rotated or upright sprite, thrown
    /// with a velocity under gravity; stored in gRoomEffectWaterSprayId, which several actors spawn
    /// as splash drops.
    EFFECT_SHELTER_B4_RESERVOIR_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x173),
    /// shelter_b4_water_supply's copy of the shared expanding, fading water ripple
    /// (_waterRippleTask / _waterDrawSplash); the room stores it in gRoomEffectWaterRippleId (water-
    /// ripple slot, spawned at the water surface by wading actors).
    EFFECT_SHELTER_B4_WATER_SUPPLY_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x174),
    /// shelter_b4_water_supply's water splash droplet: an eight-frame sprite thrown
    /// with gravity (`_waterDriftTask`); stored in gRoomEffectWaterSprayId, which actors
    /// entering/leaving water spawn at the surface (ring of 16-32 on emergence).
    EFFECT_SHELTER_B4_WATER_SUPPLY_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x175),
    /// The neo_ark_pavilion instance of the room-effect library's _waterRippleTask /
    /// waterRippleTaskFixedCoord: an expanding, fading flat splash quad
    /// (_waterDrawSplash) turned to a random yaw; the room stores it in slot gRoomEffectWaterRippleId,
    /// read alongside gRoomEffectWaterSprayId at the water surface (actor_800100, actor_401300,
    /// actor_400600, actor_01100, water-room surface hits with arg 0x40).
    EFFECT_NEO_ARK_PAVILION_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x176),
    /// Pavilion's eight-cell water particle (`neoArkPavilionWaterSprayTask`),
    /// drawn from a cached transform while local velocity and gravity advance.
    /// Installed in `gRoomEffectWaterSprayId` for actor and diver-impact spray.
    EFFECT_NEO_ARK_PAVILION_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x177),
    /// Expanding, fading splash quad (waterRippleTaskFixedCoord, _waterDrawSplash);
    /// stored in gRoomEffectWaterRippleId, which several actors (actor_800100, 401300, 400600, 01100)
    /// spawn as water ripples.
    EFFECT_NEO_ARK_ISLAND_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x178),
    /// Island's eight-cell water particle (`neoArkIslandWaterSprayTask`),
    /// drawn from a cached transform while local velocity and gravity advance.
    /// Installed in `gRoomEffectWaterSprayId` for water-surface spray.
    EFFECT_NEO_ARK_ISLAND_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x179),
    /// neo_ark_bridge's water ripple: a growing, fading quad drawn by _waterDrawSplash;
    /// stored in gRoomEffectWaterRippleId, which wading actors (companion, enemies) spawn at the
    /// water surface.
    EFFECT_NEO_ARK_BRIDGE_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x17A),
    /// The neo_ark_bridge instance of the water library's `_waterDriftTask`:
    /// a water-library drift particle: a rotated (or upright)
    /// animated sprite thrown by the kind in spawnArg bits 24-27, falling under
    /// gravity, released after frame 7; the room stores it in slot gRoomEffectWaterSprayId, read
    /// wherever something breaks the water surface (actor_800100, actor_401300,
    /// actor_400600 ring of 16 at waterY, actor_00400/206100 diver splashes, diver
    /// impact burst, actor_560800) and by the water rooms themselves with arg
    /// 0x1202180.
    EFFECT_NEO_ARK_BRIDGE_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x17B),
    /// A falling leaf (`_leafDraw`) at a random height offset; five are spawned
    /// by the Maggot Caterpillar entrance.
    EFFECT_ACROPOLIS_ROOF_GARDEN_LEAF = EFFECT_ID(EFFECT_TASK_BANK, 0x17C),
    /// Ten-frame additive sprite (spriteQuadDraw, 48-texel cells) drifting along a
    /// direction taken from the spawn angle; spawned at random entries of the
    /// sterilization room's position table in that room's state-1 views.
    EFFECT_SHELTER_B1_STERILIZATION_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x17D),
    /// Flame jet of the M4A1 Pyke flamethrower attachment, flying forward, splashing on
    /// the ground and widening on impact.
    EFFECT_M4A1_PYKE_FLAME = EFFECT_ID(EFFECT_TASK_BANK, 0x17F),
    /// Companion weapon flare: a flare drawn at the weapon with a flickering red
    /// transient point light (mode 2 widens it and spawns 0x60181); spawned on the
    /// companion's weapon when companionVariant is 4.
    EFFECT_COMPANION_WEAPON_FLARE = EFFECT_ID(EFFECT_TASK_BANK, 0x180),
    /// Flying flame of the companion's Pyke attachment (`_pykeFlameTask`), spawned
    /// with increasing launch speed and size while the weapon emits its jet.
    EFFECT_ACTOR_800100_PYKE_FLAME = EFFECT_ID(EFFECT_TASK_BANK, 0x181),
    /// Spinning sprite flash with a short beam strip, drawn for 0x19 ticks at the hit
    /// point; spawned by the weapon hit dispatcher for the M4A1 Hammer (case 15, with
    /// 0x6008E).
    EFFECT_M4A1_HAMMER_IMPACT_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x182),
    /// Eight-frame spriteQuadDraw flash at a random angle; spawned each frame of the
    /// Javelin's states 5/6 at the nearest weapon contact point and reparented to the
    /// weapon.
    EFFECT_M4A1_JAVELIN_CONTACT_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x183),
    /// Expanding animated atlas sprite with child copies (fireball); spawned by No.9's
    /// explosion controller 0x60185 before it emits smoke, when No.9's projectile hits.
    EFFECT_NO9_EXPLOSION_FIREBALL = EFFECT_ID(EFFECT_TASK_BANK, 0x184),
    /// Impact controller of actor_510900's thrown spinning object: spawns 0x60184 then
    /// smoke (0x60070) every tick for about 60 ticks; spawned with 0x6005C and smoke
    /// when the object lands or hits.
    EFFECT_ACTOR_510900_IMPACT_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x185),
    /// Gunblade blade trail: tip and hilt frames recorded in an 8-slot ring and drawn
    /// as a ribbon; spawned on the gunblade slash.
    EFFECT_GUNBLADE_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x186),
    /// neo_ark_woodland_path's copy of the water drift sprite: an eight-frame water
    /// sprite flung on a velocity under gravity; the room stores it in slot gRoomEffectWaterSprayId,
    /// which actors and the diver impact burst spawn as spray.
    EFFECT_NEO_ARK_WOODLAND_PATH_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x187),
    /// Twin ribbon trail (two eight-slot coordinate rings drawn as fading POLY_G4
    /// quads) that lives for spawn-arg frames; spawned by actor_521100 on part 8 at its
    /// attack frame together with a pad rumble.
    EFFECT_NO9_GOLEM_SWING_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x188),
    /// Glowing tile sprite riding Brahman's small projectile, trailing 0x6018B
    /// particles and bursting into particles at the end; spawned by actor_503500 with
    /// Brahman sound 5.
    EFFECT_BRAHMAN_SMALL_ORB = EFFECT_ID(EFFECT_TASK_BANK, 0x189),
    /// Shelter r48 yellow charge: two growing rings and an arc for 90 ticks, then a
    /// screen fade flash and the 0x6018F ring wall; spawned by actor_503500's enemy
    /// spawn handler.
    EFFECT_SHELTER_R48_RING_FLASH_YELLOW = EFFECT_ID(EFFECT_TASK_BANK, 0x18A),
    /// Water-library drift particle (`_waterDriftTaskU16`: fixed-angle rotated/upright animated
    /// sprite thrown by kind bits, falling under gravity) used by the R48 burst
    /// controller 0x60189 in its staged bursts.
    EFFECT_SHELTER_R48_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x18B),
    /// Shelter R48's copy of the effect-sprite drift task: one animated sprite drifting
    /// on a rolled velocity; spawned in explosion bursts by the room and by
    /// actor_503500.
    EFFECT_SHELTER_R48_DRIFT_SPRITE = EFFECT_ID(EFFECT_TASK_BANK, 0x18C),
    /// Animated sprite (12-frame sheet) attached to a Brahman projectile task that
    /// sheds 0x6018B particles every other frame (and 0x6018C in its third mode);
    /// spawned with the Brahman's launch sound when the projectile is created.
    EFFECT_BRAHMAN_PROJECTILE = EFFECT_ID(EFFECT_TASK_BANK, 0x18D),
    /// Glow sprite of Brahman's large projectile (0x898 collision radius), trailing
    /// 0x6018C particles; spawned by actor_503500 with Brahman sound 8.
    EFFECT_BRAHMAN_LARGE_ORB = EFFECT_ID(EFFECT_TASK_BANK, 0x18E),
    /// Shelter r48 rising wall of three textured ring bands that widen and fade;
    /// spawned at the end of the 0x6018A charge flash.
    EFFECT_SHELTER_R48_RING_WALL = EFFECT_ID(EFFECT_TASK_BANK, 0x18F),
    /// Charging ring flash: two orange rings growing and an arc counting down spawnArg
    /// ticks, then a full-bright orange fade quad that decays while 0x60191 runs;
    /// spawned by actor_503500.
    EFFECT_SHELTER_R48_RING_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x190),
    /// Three textured ring bands expanding in the XZ plane under an orange fade-quad
    /// screen flash, then fading; spawned when the R48 effect task's countdown ends.
    EFFECT_SHELTER_R48_SHOCKWAVE_RINGS = EFFECT_ID(EFFECT_TASK_BANK, 0x191),
    /// Expanding, fading splash quad (_waterRippleTask, _waterDrawSplash); stored in
    /// gRoomEffectWaterRippleId, which several actors (actor_800100, 401300, 400600, 01100) spawn as
    /// water ripples.
    EFFECT_NEO_ARK_SUBMARINE_GALLERY_WATER_RIPPLE = EFFECT_ID(EFFECT_TASK_BANK, 0x193),
    /// neo_ark_submarine_gallery's copy of the shared water droplet sprite particle
    /// (`_waterDriftTaskU16`): fixed-angle rotated or upright sprite thrown by a velocity kind,
    /// pulled down by gravity; the room stores it in gRoomEffectWaterSprayId (water-spray slot).
    EFFECT_NEO_ARK_SUBMARINE_GALLERY_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x194),
    /// Shelter r48 pink/red charge: rings and two arcs for 30 ticks, then a screen fade
    /// flash; spawned by actor_503500's other enemy spawn handler.
    EFFECT_SHELTER_R48_RING_FLASH_PINK = EFFECT_ID(EFFECT_TASK_BANK, 0x195),
    /// Unidentified. Drifting animated effect sprite (shelterB3GarbageIncineratorEffectSpriteDriftTaskAimed of the
    /// garbage incinerator: velocity kind and speed from spawnArg, banked or rotated
    /// drawer); thrown by actor_341900 at four offsets and by actor_444000 (Glutton)
    /// while its parts sink.
    EFFECT_196 = EFFECT_ID(EFFECT_TASK_BANK, 0x196),
    /// Dumping-hole copy of the aimed effect-sprite drift task: one animated sprite
    /// drifting or falling on a rolled velocity; spawned in explosion bursts by the
    /// room and by actor_403200 (Glutton).
    EFFECT_SHELTER_B3_DUMPING_HOLE_DRIFT_SPRITE = EFFECT_ID(EFFECT_TASK_BANK, 0x199),
    /// Animated chip/billboard sprite thrown with a random or given velocity under
    /// gravity, releasing after frame 7; spawned in bursts by the Glutton rain
    /// projectile's effect (0x6019B).
    EFFECT_GLUTTON_RAIN_PARTICLE = EFFECT_ID(EFFECT_TASK_BANK, 0x19A),
    /// Billboard glow carried by a Glutton rain spawn as it drops, trailing 0x6019A
    /// particles and bursting on landing.
    EFFECT_GLUTTON_RAIN_BLOB = EFFECT_ID(EFFECT_TASK_BANK, 0x19B),
    /// Flare PE spark: flies away from the player at a random heading drawing an eight-
    /// frame sprite; sprayed by the Flare effect task.
    EFFECT_FLARE_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x19E),
    /// Growth-room mist puff: an animated sprite drifting horizontally while fading in
    /// and out over 10 frames; spawned in increasing numbers and heights at points
    /// around the room as its counter rises.
    EFFECT_GROWTH_ROOM_MIST = EFFECT_ID(EFFECT_TASK_BANK, 0x1A1),
    /// A 10-cell animated sprite drifting on a random bearing under light gravity;
    /// spawned continuously at random points by the growth-room task.
    EFFECT_SHELTER_B6_GROWTH_ROOM_DRIFT_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x1A2),
    /// Model chunk (gShelterB6NurseryModel07BAC) thrown and tumbled with collision
    /// bounces, trailing 0x601A4 particles every other frame; three are spawned in the
    /// nursery's view 12 event.
    EFFECT_SHELTER_B6_NURSERY_DEBRIS_CHUNK = EFFECT_ID(EFFECT_TASK_BANK, 0x1A3),
    /// Unidentified. Ten-frame sprite particle with an optional random velocity that
    /// drifts upward; emitted every other frame at fixed points in several views by the
    /// B6 nursery room task.
    EFFECT_1A4 = EFFECT_ID(EFFECT_TASK_BANK, 0x1A4),
    /// Shelter B6 nursery additive particle thrown outward and falling until it reaches
    /// the floor; 16 sprayed at once in view 13 by the nursery room task.
    EFFECT_SHELTER_B6_NURSERY_SPARK_SHOWER = EFFECT_ID(EFFECT_TASK_BANK, 0x1A5),
    /// Freezer floor mist puff: a grey semi-transparent animated sprite drifting
    /// horizontally at floor level; nine are spawned every fourth frame around the
    /// freezer points.
    EFFECT_GOLEM_FREEZER_FLOOR_MIST = EFFECT_ID(EFFECT_TASK_BANK, 0x1A6),
    /// Orange burst: expanding ring, glow and arc that fade out; spawned by
    /// actor_105100 together with a sound when it attacks.
    EFFECT_SHELTER_B6_TRAINING_ROOM_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x1A7),
    /// Violet growing discs and an outer-lit band (`effectDrawGouraudDisc`/`effectDrawOuterGlowBand`) that build up over the
    /// spawn-arg frames, hold, then fade; spawned by actor_105100 at the start of its
    /// split/summon step before it emits child enemies.
    EFFECT_SHELTER_B6_TRAINING_SUMMON_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x1A8),
    /// Orange rings and arc that grow as a countdown runs, then spawn three 0x601AA
    /// effects and fade with a screen flash; spawned by actor_105100 (fireball user)
    /// with a charge sound.
    EFFECT_SHELTER_B6_TRAINING_CHARGE_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x1A9),
    /// Shelter B6 training room rising ring band (three spawned, one per band) of
    /// animated textured quads that widen and fade; spawned at the end of the room's
    /// charge flash.
    EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL = EFFECT_ID(EFFECT_TASK_BANK, 0x1AA),
    /// Energy arc: a sprite at a random body joint plus an animated textured strip from
    /// that joint to the room's ring-glow coordinate, lasting 6..21 frames; spawned
    /// 1-in-8 frames on actor_105100's joints.
    EFFECT_TRAINING_ROOM_ENERGY_ARC = EFFECT_ID(EFFECT_TASK_BANK, 0x1AB),
    /// Short yellow arc-and-ring flash expanding over 9 ticks, spawned at an actor part
    /// or the player's model by actor_105100 and actor_205200 hits.
    EFFECT_SHELTER_B6_TRAINING_ROOM_HIT_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1AC),
    /// Unidentified. Additive animated billboard (effectDrawSpinningBillboard) that sinks 8 units per
    /// frame for 8 frames; shed at random by the training room's descending emitter
    /// 0x601AE.
    EFFECT_1AD = EFFECT_ID(EFFECT_TASK_BANK, 0x1AD),
    /// Unidentified. Four-frame sprite that moves down for 60 ticks, occasionally
    /// spawning 0x601AD; spawned in a rising spiral by a B6 training room task.
    EFFECT_1AE = EFFECT_ID(EFFECT_TASK_BANK, 0x1AE),
    /// Shelter B6 training room spiral emitter: for 21 ticks spawns 0x601AE at a rising
    /// spiral around a fixed point; spawned when actor_105100's paired heal restores
    /// 0x50 HP.
    EFFECT_SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL = EFFECT_ID(EFFECT_TASK_BANK, 0x1AF),
    /// Falling shard: a grey semi-transparent tumbling triangle falling from ceiling
    /// height, bouncing once on the floor and fading; spawned in bursts of 0x30 and
    /// 0x20 in views 8 and 3/10.
    EFFECT_NIGHT_MOTEL_LOFT_FALLING_SHARD = EFFECT_ID(EFFECT_TASK_BANK, 0x1B0),
    /// A drifting smoke puff from a 10-cell 48x48 sheet; the Dryfield main street room
    /// task spawns them at random points (0x30 at once in view 8).
    EFFECT_DRYFIELD_MAIN_STREET_SMOKE_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x1B1),
    /// Ten-cell animated billboard (`dryfieldNightMainStreetPuffTask`) with a fixed
    /// random rotation and nonpositive local-X drift. The night main street task
    /// spawns 0x30 at random spots on entering views 8/0x13 and one every other frame while there.
    EFFECT_DRYFIELD_NIGHT_MAIN_STREET_PUFF = EFFECT_ID(EFFECT_TASK_BANK, 0x1B2),
    /// Unidentified. B1 pod service gantry copy of the animated sprite-drift effect
    /// (effect_sprite_drift: sprite with rolled velocity and gravity); spawned by
    /// actor_160900 at listed points and with the water spray by actor_560800.
    EFFECT_1B4 = EFFECT_ID(EFFECT_TASK_BANK, 0x1B4),
    /// Dryfield r08 drifting animated sprite (effectSpriteDrift); actor_121300 spawns
    /// one at each point of a ring around the arena every fourth frame.
    EFFECT_DRYFIELD_R08_ARENA_RING_SPRITE = EFFECT_ID(EFFECT_TASK_BANK, 0x1B7),
    /// Expanding blue band ring: a 16-segment POLY_G4 band whose radius grows by 0x60
    /// while it dims; actor_403600 spawns it at parts 1, 15 and 19.
    EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x1B9),
    /// Three arcs stacked up the frame plus a fade quad, both ramping down in colour;
    /// spawned by actor_403600 on its part 1 with a sound.
    EFFECT_SHELTER_B2_POD_BOTTOM_ARC_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1BA),
    /// Two growing tinted rings plus a cycling arc that then fade; spawned by
    /// actor_403600 (Eve body package) on its hand-attached child at parts 14/18.
    EFFECT_EVE_ENERGY_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x1BB),
    /// Ramp-coloured ring and shrinking arc that charge up over spawnArg ticks, then a
    /// ring with eight blades fading out; spawned at joint 1 of actor_403600.
    EFFECT_SHELTER_B2_POD_BOTTOM_CHARGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x1BC),
    /// Mist shooting gallery muzzle-flash/tracer beam with glow toward a random
    /// endpoint, fading out; spawned when a gallery target attacks.
    EFFECT_MIST_GALLERY_TRACER = EFFECT_ID(EFFECT_TASK_BANK, 0x1BD),
    /// The shelter_b1_pod_service_gantry instance of the water library's `_waterDriftTaskU16`:
    /// a water-library drift particle: a fixed-angle rotated (or upright)
    /// animated sprite thrown by the kind in spawnArg bits 24-27, falling under
    /// gravity, released after frame 7; the room stores it in slot gRoomEffectWaterSprayId, read
    /// wherever something breaks the water surface (actor_800100, actor_401300,
    /// actor_400600 ring of 16 at waterY, actor_00400/206100 diver splashes, diver
    /// impact burst, actor_560800) and by the water rooms themselves with arg
    /// 0x1202180.
    EFFECT_SHELTER_B1_POD_SERVICE_GANTRY_WATER_SPRAY = EFFECT_ID(EFFECT_TASK_BANK, 0x1BE),
    /// The pod rooms' rising sprite: eight cells drawn in a random CLUT while moving
    /// along Y; spawned by actor_403600 at its body parts (with 0x60070) when parts
    /// burst.
    EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE = EFFECT_ID(EFFECT_TASK_BANK, 0x1BF),
    /// Vertical POLY_G4 beam (_shelterB2PodBottomDrawLightBeam) that slides 0x300
    /// per frame for 16 frames, fading in its last 8; actor_403600 spawns them at
    /// random points around the view, from above and below.
    EFFECT_EVE_LIGHT_BEAM = EFFECT_ID(EFFECT_TASK_BANK, 0x1C0),
    /// Falling leaf (`_leafFallTask`, woodland path copy) spawned at Stranger
    /// (actor_401300) joints in area 0x1D.
    EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF = EFFECT_ID(EFFECT_TASK_BANK, 0x1C1),
    /// shelter_1f_bulwark's copy of _roomVisualEffectsFlashTask (fans and a shrinking ring ramping
    /// to a coloured screen fade), stored in gRoomEffectFlashId, which the Pawn/Rook GOLEM's
    /// silence-scream state spawns on its part 4.
    EFFECT_SHELTER_1F_BULWARK_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1C2),
    /// The shelter_1f_bulwark instance of the room-effect library's
    /// room_visual_effects_trail_task.inc.c: a twin-trail beam: two rings of eight
    /// frames recording two points offset from the anchor, drawn as a beam between
    /// them; the room stores it in slot gRoomEffectTwinTrailId, read by the Pawn/Rook GOLEM
    /// delayed-effect child at part 7 (golem_pawn_rook_delayed_effect_tick.inc.c; the
    /// beam-sword golems actor_02000/02300 reference this slot).
    EFFECT_SHELTER_1F_BULWARK_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1C3),
    /// shelter_1f_bulwark's copy of the RoomFx spark burst: spawns a flash, then sprays
    /// jittered sparks or draws widening rings for seven ticks; the room stores it in
    /// slot gRoomEffectSparkBurstId, which the grenade-launcher Pawn/Rook golems spawn where a
    /// grenade lands (golemPawnRookBulletFly).
    EFFECT_SHELTER_1F_BULWARK_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x1C4),
    /// Rising eight-cell effectDrawSpinningBillboard sprite with a random CLUT
    /// (`shelterB1PodServiceGantryEffectSpriteRiseTask`); spawned every 128 ticks from part 2 of actor_560800's
    /// animated model part in the B1 pod service gantry.
    EFFECT_SHELTER_B1_GANTRY_RISING_SPRITE = EFFECT_ID(EFFECT_TASK_BANK, 0x1C6),
    /// dryfield_night_r08's copy of the shared flash task (_roomVisualEffectsFlashTask): ramps up
    /// two fans and a shrinking ring, queues a full-screen fade quad at its peak, then
    /// fades out; the room stores it in gRoomEffectFlashId (flash slot, spawned by
    /// golemPawnRookSilenceScreamState).
    EFFECT_DRYFIELD_NIGHT_R08_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1C7),
    /// shelter_b1_elevator_hall's copy of _roomVisualEffectsFlashTask (fans and a shrinking ring
    /// ramping to a coloured screen fade), stored in gRoomEffectFlashId, which the Pawn/Rook
    /// GOLEM's silence-scream state spawns on its part 4.
    EFFECT_SHELTER_B1_ELEVATOR_HALL_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1C8),
    /// The shelter_b1_s_walkway instance of the room-effect library's _roomVisualEffectsFlashTask:
    /// a flash: two fans and an inward-shrinking ring ramping up over spawnArg1 ticks,
    /// peaking with a coloured fade quad, then fading through a star; the room stores
    /// it in slot gRoomEffectFlashId, read by the Pawn/Rook GOLEM silence-scream state
    /// (golem_pawn_rook_silence_scream.inc.c, part 4).
    EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1C9),
    /// shelter_b1_storeroom's copy of the RoomFx flash: ramps up two fans and a
    /// shrinking ring with a fade quad at its peak, then fades out through a star; the
    /// room stores it in slot gRoomEffectFlashId, which the Pawn/Rook golem library spawns at
    /// body part 4 for its scream (golemPawnRookSilenceScream).
    EFFECT_SHELTER_B1_STOREROOM_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1CA),
    /// Flash (_roomVisualEffectsFlashTask): fans and an inward ring ramping up, a fade quad at the
    /// peak, then a star fade-out; stored in gRoomEffectFlashId, which golems
    /// (actor_02300/05700) spawn at part 4 in their scream sequence.
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1CB),
    /// shelter_b1_main_corridor's copy of the shared flash task (_roomVisualEffectsFlashTask):
    /// ramps up two fans and a shrinking ring, queues a full-screen fade quad at its
    /// peak, then fades out; the room stores it in gRoomEffectFlashId (flash slot, spawned by
    /// golemPawnRookSilenceScreamState).
    EFFECT_SHELTER_B1_MAIN_CORRIDOR_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1CC),
    /// shelter_b1_pod_access_tunnel's copy of _roomVisualEffectsFlashTask (fans and a shrinking
    /// ring ramping to a coloured screen fade), stored in gRoomEffectFlashId, which the
    /// Pawn/Rook GOLEM's silence-scream state spawns on its part 4.
    EFFECT_SHELTER_B1_POD_ACCESS_TUNNEL_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1CD),
    /// The shelter_b1_transfer_tunnel instance of the room-effect library's
    /// _roomVisualEffectsFlashTask: a flash: two fans and an inward-shrinking ring ramping up over
    /// spawnArg1 ticks, peaking with a coloured fade quad, then fading through a star;
    /// the room stores it in slot gRoomEffectFlashId, read by the Pawn/Rook GOLEM silence-
    /// scream state (golem_pawn_rook_silence_scream.inc.c, part 4).
    EFFECT_SHELTER_B1_TRANSFER_TUNNEL_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1CE),
    /// shelter_b1_control_room_access_tunnel's copy of the RoomFx flash: ramps up two
    /// fans and a shrinking ring with a fade quad at its peak, then fades out through a
    /// star; the room stores it in slot gRoomEffectFlashId, which the Pawn/Rook golem library
    /// spawns at body part 4 for its scream (golemPawnRookSilenceScream).
    EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1CF),
    /// Flash (_roomVisualEffectsFlashTask): fans and an inward ring ramping up, a fade quad at the
    /// peak, then a star fade-out; stored in gRoomEffectFlashId, which golems
    /// (actor_02300/05700) spawn at part 4 in their scream sequence.
    EFFECT_SHELTER_B2_ELEVATOR_HALL_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D0),
    /// shelter_b2_south_maintenance_walkway's copy of the shared flash task
    /// (_roomVisualEffectsFlashTask): ramps up two fans and a shrinking ring, queues a full-screen
    /// fade quad at its peak, then fades out; the room stores it in gRoomEffectFlashId (flash
    /// slot, spawned by golemPawnRookSilenceScreamState).
    EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D1),
    /// shelter_b2_north_maintenance_walkway's copy of _roomVisualEffectsFlashTask (fans and a
    /// shrinking ring ramping to a coloured screen fade), stored in gRoomEffectFlashId, which
    /// the Pawn/Rook GOLEM's silence-scream state spawns on its part 4.
    EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D2),
    /// The shelter_b2_main_corridor instance of the room-effect library's
    /// _roomVisualEffectsFlashTask: a flash: two fans and an inward-shrinking ring ramping up over
    /// spawnArg1 ticks, peaking with a coloured fade quad, then fading through a star;
    /// the room stores it in slot gRoomEffectFlashId, read by the Pawn/Rook GOLEM silence-
    /// scream state (golem_pawn_rook_silence_scream.inc.c, part 4).
    EFFECT_SHELTER_B2_MAIN_CORRIDOR_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D3),
    /// shelter_b2_septic_tank's copy of the RoomFx flash: ramps up two fans and a
    /// shrinking ring with a fade quad at its peak, then fades out through a star; the
    /// room stores it in slot gRoomEffectFlashId, which the Pawn/Rook golem library spawns at
    /// body part 4 for its scream (golemPawnRookSilenceScream).
    EFFECT_SHELTER_B2_SEPTIC_TANK_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D4),
    /// Flash (_roomVisualEffectsFlashTask): fans and an inward ring ramping up, a fade quad at the
    /// peak, then a star fade-out; stored in gRoomEffectFlashId, which golems
    /// (actor_02300/05700) spawn at part 4 in their scream sequence.
    EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D5),
    /// shelter_1f_parking_garage's copy of the shared flash task (_roomVisualEffectsFlashTask):
    /// ramps up two fans and a shrinking ring, queues a full-screen fade quad at its
    /// peak, then fades out; the room stores it in gRoomEffectFlashId (flash slot, spawned by
    /// golemPawnRookSilenceScreamState).
    EFFECT_SHELTER_1F_PARKING_GARAGE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D6),
    /// shelter_1f_vehicular_airlock's copy of _roomVisualEffectsFlashTask (fans and a shrinking
    /// ring ramping to a coloured screen fade), stored in gRoomEffectFlashId, which the
    /// Pawn/Rook GOLEM's silence-scream state spawns on its part 4.
    EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D7),
    /// The neo_ark_n_promenade instance of the room-effect library's _roomVisualEffectsFlashTask:
    /// a flash: two fans and an inward-shrinking ring ramping up over spawnArg1 ticks,
    /// peaking with a coloured fade quad, then fading through a star; the room stores
    /// it in slot gRoomEffectFlashId, read by the Pawn/Rook GOLEM silence-scream state
    /// (golem_pawn_rook_silence_scream.inc.c, part 4).
    EFFECT_NEO_ARK_NORTH_PROMENADE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D8),
    /// neo_ark_forest_zone's copy of the RoomFx flash: ramps up two fans and a
    /// shrinking ring with a fade quad at its peak, then fades out through a star; the
    /// room stores it in slot gRoomEffectFlashId, which the Pawn/Rook golem library spawns at
    /// body part 4 for its scream (golemPawnRookSilenceScream).
    EFFECT_NEO_ARK_FOREST_ZONE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1D9),
    /// Flash (_roomVisualEffectsFlashTask): fans and an inward ring ramping up, a fade quad at the
    /// peak, then a star fade-out; stored in gRoomEffectFlashId, which golems
    /// (actor_02300/05700) spawn at part 4 in their scream sequence.
    EFFECT_NEO_ARK_PAVILION_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1DA),
    /// neo_ark_island's copy of the shared flash task (_roomVisualEffectsFlashTask): ramps up two
    /// fans and a shrinking ring, queues a full-screen fade quad at its peak, then
    /// fades out; the room stores it in gRoomEffectFlashId (flash slot, spawned by
    /// golemPawnRookSilenceScreamState).
    EFFECT_NEO_ARK_ISLAND_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1DB),
    /// neo_ark_power_plant_2's copy of _roomVisualEffectsFlashTask (fans and a shrinking ring
    /// ramping to a coloured screen fade), stored in gRoomEffectFlashId, which the Pawn/Rook
    /// GOLEM's silence-scream state spawns on its part 4.
    EFFECT_NEO_ARK_POWER_PLANT_2_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1DC),
    /// The neo_ark_savanna instance of the room-effect library's _roomVisualEffectsFlashTask: a
    /// flash: two fans and an inward-shrinking ring ramping up over spawnArg1 ticks,
    /// peaking with a coloured fade quad, then fading through a star; the room stores
    /// it in slot gRoomEffectFlashId, read by the Pawn/Rook GOLEM silence-scream state
    /// (golem_pawn_rook_silence_scream.inc.c, part 4).
    EFFECT_NEO_ARK_SAVANNA_ZONE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1DD),
    /// neo_ark_south_promenade's copy of the RoomFx flash: ramps up two fans and a
    /// shrinking ring with a fade quad at its peak, then fades out through a star; the
    /// room stores it in slot gRoomEffectFlashId, which the Pawn/Rook golem library spawns at
    /// body part 4 for its scream (golemPawnRookSilenceScream).
    EFFECT_NEO_ARK_SOUTH_PROMENADE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1DE),
    /// Flash (_roomVisualEffectsFlashTask): fans and an inward ring ramping up, a fade quad at the
    /// peak, then a star fade-out; stored in gRoomEffectFlashId, which golems
    /// (actor_02300/05700) spawn at part 4 in their scream sequence.
    EFFECT_NEO_ARK_SHRINE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1DF),
    /// shelter_b6_nursery's copy of the shared flash task (_roomVisualEffectsFlashTask): ramps up
    /// two fans and a shrinking ring, queues a full-screen fade quad at its peak, then
    /// fades out; the room stores it in gRoomEffectFlashId (flash slot, spawned by
    /// golemPawnRookSilenceScreamState).
    EFFECT_SHELTER_B6_NURSERY_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1E0),
    /// neo_ark_bridge's copy of _roomVisualEffectsFlashTask (fans and a shrinking ring ramping to
    /// a coloured screen fade), stored in gRoomEffectFlashId, which the Pawn/Rook GOLEM's
    /// silence-scream state spawns on its part 4.
    EFFECT_NEO_ARK_BRIDGE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1E1),
    /// The neo_ark_pyramid instance of the room-effect library's _roomVisualEffectsFlashTask: a
    /// flash: two fans and an inward-shrinking ring ramping up over spawnArg1 ticks,
    /// peaking with a coloured fade quad, then fading through a star; the room stores
    /// it in slot gRoomEffectFlashId, read by the Pawn/Rook GOLEM silence-scream state
    /// (golem_pawn_rook_silence_scream.inc.c, part 4).
    EFFECT_NEO_ARK_PYRAMID_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x1E2),
    /// dryfield_night_r08's copy of the RoomFx twin trail: two eight-slot rings of
    /// frames drawn as a fading beam behind an anchor; the room stores it in slot
    /// gRoomEffectTwinTrailId, which the beam-sword Pawn/Rook golems spawn at body part 7
    /// (golemPawnRookDelayedEffectTick).
    EFFECT_DRYFIELD_NIGHT_R08_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1E3),
    /// Twin trail (room_visual_effects_trail_task): two points offset from the anchor
    /// recorded in rings of eight and drawn as a fading beam; stored in gRoomEffectTwinTrailId,
    /// which the beam-sword golems (actor_02000/02300) spawn at model part 7.
    EFFECT_SHELTER_B1_ELEVATOR_HALL_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1E4),
    /// shelter_b1_south_maintenance_walkway's copy of the shared twin trail
    /// (room_visual_effects_trail_task): two rings of eight frames drawn as a fading
    /// gouraud beam; the room stores it in gRoomEffectTwinTrailId (twin-trail slot, spawned by the
    /// Pawn/Rook golem library at joint 7 via golemPawnRookDelayedEffectTick).
    EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1E5),
    /// shelter_b1_storeroom's copy of the RoomFx twin-trail task (two eight-point rings
    /// drawn as a beam), stored in gRoomEffectTwinTrailId, which the beam-sword GOLEM's sword child
    /// spawns at part 7 ten frames after it appears.
    EFFECT_SHELTER_B1_STOREROOM_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1E6),
    /// The shelter_b1_n_walkway instance of the room-effect library's
    /// room_visual_effects_trail_task.inc.c: a twin-trail beam: two rings of eight
    /// frames recording two points offset from the anchor, drawn as a beam between
    /// them; the room stores it in slot gRoomEffectTwinTrailId, read by the Pawn/Rook GOLEM
    /// delayed-effect child at part 7 (golem_pawn_rook_delayed_effect_tick.inc.c; the
    /// beam-sword golems actor_02000/02300 reference this slot).
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1E7),
    /// shelter_b1_main_corridor's copy of the RoomFx twin trail: two eight-slot rings
    /// of frames drawn as a fading beam behind an anchor; the room stores it in slot
    /// gRoomEffectTwinTrailId, which the beam-sword Pawn/Rook golems spawn at body part 7
    /// (golemPawnRookDelayedEffectTick).
    EFFECT_SHELTER_B1_MAIN_CORRIDOR_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1E8),
    /// Twin trail (room_visual_effects_trail_task): two points offset from the anchor
    /// recorded in rings of eight and drawn as a fading beam; stored in gRoomEffectTwinTrailId,
    /// which the beam-sword golems (actor_02000/02300) spawn at model part 7.
    EFFECT_SHELTER_B1_POD_ACCESS_TUNNEL_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1E9),
    /// shelter_b1_transfer_tunnel's copy of the shared twin trail
    /// (room_visual_effects_trail_task): two rings of eight frames drawn as a fading
    /// gouraud beam; the room stores it in gRoomEffectTwinTrailId (twin-trail slot, spawned by the
    /// Pawn/Rook golem library at joint 7 via golemPawnRookDelayedEffectTick).
    EFFECT_SHELTER_B1_TRANSFER_TUNNEL_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1EA),
    /// shelter_b1_control_room_access_tunnel's copy of the RoomFx twin-trail task (two
    /// eight-point rings drawn as a beam), stored in gRoomEffectTwinTrailId, which the beam-sword
    /// GOLEM's sword child spawns at part 7 ten frames after it appears.
    EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1EB),
    /// The shelter_b2_elevator_hall instance of the room-effect library's
    /// room_visual_effects_trail_task.inc.c: a twin-trail beam: two rings of eight
    /// frames recording two points offset from the anchor, drawn as a beam between
    /// them; the room stores it in slot gRoomEffectTwinTrailId, read by the Pawn/Rook GOLEM
    /// delayed-effect child at part 7 (golem_pawn_rook_delayed_effect_tick.inc.c; the
    /// beam-sword golems actor_02000/02300 reference this slot).
    EFFECT_SHELTER_B2_ELEVATOR_HALL_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1EC),
    /// shelter_b2_south_maintenance_walkway's copy of the RoomFx twin trail: two eight-
    /// slot rings of frames drawn as a fading beam behind an anchor; the room stores it
    /// in slot gRoomEffectTwinTrailId, which the beam-sword Pawn/Rook golems spawn at body part 7
    /// (golemPawnRookDelayedEffectTick).
    EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1ED),
    /// Twin trail (room_visual_effects_trail_task): two points offset from the anchor
    /// recorded in rings of eight and drawn as a fading beam; stored in gRoomEffectTwinTrailId,
    /// which the beam-sword golems (actor_02000/02300) spawn at model part 7.
    EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1EE),
    /// shelter_b2_main_corridor's copy of the shared twin trail
    /// (room_visual_effects_trail_task): two rings of eight frames drawn as a fading
    /// gouraud beam; the room stores it in gRoomEffectTwinTrailId (twin-trail slot, spawned by the
    /// Pawn/Rook golem library at joint 7 via golemPawnRookDelayedEffectTick).
    EFFECT_SHELTER_B2_MAIN_CORRIDOR_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1EF),
    /// shelter_b2_septic_tank's copy of the RoomFx twin-trail task (two eight-point
    /// rings drawn as a beam), stored in gRoomEffectTwinTrailId, which the beam-sword GOLEM's sword
    /// child spawns at part 7 ten frames after it appears.
    EFFECT_SHELTER_B2_SEPTIC_TANK_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F0),
    /// The shelter_b2_pod_tunnel instance of the room-effect library's
    /// room_visual_effects_trail_task.inc.c: a twin-trail beam: two rings of eight
    /// frames recording two points offset from the anchor, drawn as a beam between
    /// them; the room stores it in slot gRoomEffectTwinTrailId, read by the Pawn/Rook GOLEM
    /// delayed-effect child at part 7 (golem_pawn_rook_delayed_effect_tick.inc.c; the
    /// beam-sword golems actor_02000/02300 reference this slot).
    EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F1),
    /// shelter_1f_parking_garage's copy of the RoomFx twin trail: two eight-slot rings
    /// of frames drawn as a fading beam behind an anchor; the room stores it in slot
    /// gRoomEffectTwinTrailId, which the beam-sword Pawn/Rook golems spawn at body part 7
    /// (golemPawnRookDelayedEffectTick).
    EFFECT_SHELTER_1F_PARKING_GARAGE_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F2),
    /// Twin trail (room_visual_effects_trail_task): two points offset from the anchor
    /// recorded in rings of eight and drawn as a fading beam; stored in gRoomEffectTwinTrailId,
    /// which the beam-sword golems (actor_02000/02300) spawn at model part 7.
    EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F3),
    /// neo_ark_north_promenade's copy of the shared twin trail
    /// (room_visual_effects_trail_task): two rings of eight frames drawn as a fading
    /// gouraud beam; the room stores it in gRoomEffectTwinTrailId (twin-trail slot, spawned by the
    /// Pawn/Rook golem library at joint 7 via golemPawnRookDelayedEffectTick).
    EFFECT_NEO_ARK_NORTH_PROMENADE_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F4),
    /// neo_ark_forest_zone's copy of the RoomFx twin-trail task (two eight-point rings
    /// drawn as a beam), stored in gRoomEffectTwinTrailId, which the beam-sword GOLEM's sword child
    /// spawns at part 7 ten frames after it appears.
    EFFECT_NEO_ARK_FOREST_ZONE_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F5),
    /// The neo_ark_pavilion instance of the room-effect library's
    /// room_visual_effects_trail_task.inc.c: a twin-trail beam: two rings of eight
    /// frames recording two points offset from the anchor, drawn as a beam between
    /// them; the room stores it in slot gRoomEffectTwinTrailId, read by the Pawn/Rook GOLEM
    /// delayed-effect child at part 7 (golem_pawn_rook_delayed_effect_tick.inc.c; the
    /// beam-sword golems actor_02000/02300 reference this slot).
    EFFECT_NEO_ARK_PAVILION_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F6),
    /// neo_ark_island's copy of the RoomFx twin trail: two eight-slot rings of frames
    /// drawn as a fading beam behind an anchor; the room stores it in slot gRoomEffectTwinTrailId,
    /// which the beam-sword Pawn/Rook golems spawn at body part 7
    /// (golemPawnRookDelayedEffectTick).
    EFFECT_NEO_ARK_ISLAND_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F7),
    /// Twin trail (room_visual_effects_trail_task): two points offset from the anchor
    /// recorded in rings of eight and drawn as a fading beam; stored in gRoomEffectTwinTrailId,
    /// which the beam-sword golems (actor_02000/02300) spawn at model part 7.
    EFFECT_NEO_ARK_POWER_PLANT_2_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F8),
    /// neo_ark_savanna_zone's copy of the shared twin trail
    /// (room_visual_effects_trail_task): two rings of eight frames drawn as a fading
    /// gouraud beam; the room stores it in gRoomEffectTwinTrailId (twin-trail slot, spawned by the
    /// Pawn/Rook golem library at joint 7 via golemPawnRookDelayedEffectTick).
    EFFECT_NEO_ARK_SAVANNA_ZONE_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1F9),
    /// neo_ark_south_promenade's copy of the RoomFx twin-trail task (two eight-point
    /// rings drawn as a beam), stored in gRoomEffectTwinTrailId, which the beam-sword GOLEM's sword
    /// child spawns at part 7 ten frames after it appears.
    EFFECT_NEO_ARK_SOUTH_PROMENADE_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1FA),
    /// The neo_ark_shrine instance of the room-effect library's
    /// room_visual_effects_trail_task.inc.c: a twin-trail beam: two rings of eight
    /// frames recording two points offset from the anchor, drawn as a beam between
    /// them; the room stores it in slot gRoomEffectTwinTrailId, read by the Pawn/Rook GOLEM
    /// delayed-effect child at part 7 (golem_pawn_rook_delayed_effect_tick.inc.c; the
    /// beam-sword golems actor_02000/02300 reference this slot).
    EFFECT_NEO_ARK_SHRINE_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1FB),
    /// shelter_b6_nursery's copy of the RoomFx twin trail: two eight-slot rings of
    /// frames drawn as a fading beam behind an anchor; the room stores it in slot
    /// gRoomEffectTwinTrailId, which the beam-sword Pawn/Rook golems spawn at body part 7
    /// (golemPawnRookDelayedEffectTick).
    EFFECT_SHELTER_B6_NURSERY_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1FC),
    /// Twin trail (room_visual_effects_trail_task): two points offset from the anchor
    /// recorded in rings of eight and drawn as a fading beam; stored in gRoomEffectTwinTrailId,
    /// which the beam-sword golems (actor_02000/02300) spawn at model part 7.
    EFFECT_NEO_ARK_BRIDGE_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1FD),
    /// neo_ark_pyramid's copy of the shared twin trail
    /// (room_visual_effects_trail_task): two rings of eight frames drawn as a fading
    /// gouraud beam; the room stores it in gRoomEffectTwinTrailId (twin-trail slot, spawned by the
    /// Pawn/Rook golem library at joint 7 via golemPawnRookDelayedEffectTick).
    EFFECT_NEO_ARK_PYRAMID_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x1FE),
    /// dryfield_night_r08's copy of RoomFx_SparkBurstTask (a flash then sprayed sparks
    /// or a widening ring for seven ticks), stored in gRoomEffectSparkBurstId, which the grenade-
    /// launcher GOLEM's bullet spawns where it hits.
    EFFECT_DRYFIELD_NIGHT_R08_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x1FF),
    /// The shelter_b1_elevator_hall instance of the room-effect library's
    /// RoomFx_SparkBurstTask: a spark burst: a flash plus jittered sparks (non-zero
    /// arg) or two dimming rings (zero arg) for seven ticks; the room stores it in slot
    /// gRoomEffectSparkBurstId, read on impact by the Pawn/Rook GOLEM grenade
    /// (golem_pawn_rook_bullet_fly.inc.c; grenade-launcher golems actor_05600/05700).
    EFFECT_SHELTER_B1_ELEVATOR_HALL_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x200),
    /// shelter_b1_south_maintenance_walkway's copy of the RoomFx spark burst: spawns a
    /// flash, then sprays jittered sparks or draws widening rings for seven ticks; the
    /// room stores it in slot gRoomEffectSparkBurstId, which the grenade-launcher Pawn/Rook golems
    /// spawn where a grenade lands (golemPawnRookBulletFly).
    EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x201),
    /// Spark burst (RoomFx_SparkBurstTask): a flash then a spray of jittered sparks, or
    /// two widening rings, over seven ticks; stored in gRoomEffectSparkBurstId, which the grenade
    /// golems (actor_05600/05700) spawn when a shot ends.
    EFFECT_SHELTER_B1_STOREROOM_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x202),
    /// shelter_b1_north_maintenance_walkway's copy of the shared spark burst
    /// (RoomFx_SparkBurstTask): spawns impact flash 0x60076, then sprays sparks/smoke
    /// or draws expanding rings for seven ticks; the room stores it in gRoomEffectSparkBurstId
    /// (spark-burst slot, spawned by golemPawnRookBulletFly when a golem bullet ends
    /// its flight).
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x203),
    /// shelter_b1_main_corridor's copy of RoomFx_SparkBurstTask (a flash then sprayed
    /// sparks or a widening ring for seven ticks), stored in gRoomEffectSparkBurstId, which the
    /// grenade-launcher GOLEM's bullet spawns where it hits.
    EFFECT_SHELTER_B1_MAIN_CORRIDOR_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x204),
    /// The shelter_b1_pod_tunnel instance of the room-effect library's
    /// RoomFx_SparkBurstTask: a spark burst: a flash plus jittered sparks (non-zero
    /// arg) or two dimming rings (zero arg) for seven ticks; the room stores it in slot
    /// gRoomEffectSparkBurstId, read on impact by the Pawn/Rook GOLEM grenade
    /// (golem_pawn_rook_bullet_fly.inc.c; grenade-launcher golems actor_05600/05700).
    EFFECT_SHELTER_B1_POD_ACCESS_TUNNEL_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x205),
    /// shelter_b1_transfer_tunnel's copy of the RoomFx spark burst: spawns a flash,
    /// then sprays jittered sparks or draws widening rings for seven ticks; the room
    /// stores it in slot gRoomEffectSparkBurstId, which the grenade-launcher Pawn/Rook golems spawn
    /// where a grenade lands (golemPawnRookBulletFly).
    EFFECT_SHELTER_B1_TRANSFER_TUNNEL_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x206),
    /// Spark burst (RoomFx_SparkBurstTask): a flash then a spray of jittered sparks, or
    /// two widening rings, over seven ticks; stored in gRoomEffectSparkBurstId, which the grenade
    /// golems (actor_05600/05700) spawn when a shot ends.
    EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x207),
    /// shelter_b2_elevator_hall's copy of the shared spark burst
    /// (RoomFx_SparkBurstTask): spawns impact flash 0x60076, then sprays sparks/smoke
    /// or draws expanding rings for seven ticks; the room stores it in gRoomEffectSparkBurstId
    /// (spark-burst slot, spawned by golemPawnRookBulletFly when a golem bullet ends
    /// its flight).
    EFFECT_SHELTER_B2_ELEVATOR_HALL_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x208),
    /// shelter_b2_south_maintenance_walkway's copy of RoomFx_SparkBurstTask (a flash
    /// then sprayed sparks or a widening ring for seven ticks), stored in gRoomEffectSparkBurstId,
    /// which the grenade-launcher GOLEM's bullet spawns where it hits.
    EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x209),
    /// The shelter_b2_n_walkway instance of the room-effect library's
    /// RoomFx_SparkBurstTask: a spark burst: a flash plus jittered sparks (non-zero
    /// arg) or two dimming rings (zero arg) for seven ticks; the room stores it in slot
    /// gRoomEffectSparkBurstId, read on impact by the Pawn/Rook GOLEM grenade
    /// (golem_pawn_rook_bullet_fly.inc.c; grenade-launcher golems actor_05600/05700).
    EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x20A),
    /// shelter_b2_main_corridor's copy of the RoomFx spark burst: spawns a flash, then
    /// sprays jittered sparks or draws widening rings for seven ticks; the room stores
    /// it in slot gRoomEffectSparkBurstId, which the grenade-launcher Pawn/Rook golems spawn where a
    /// grenade lands (golemPawnRookBulletFly).
    EFFECT_SHELTER_B2_MAIN_CORRIDOR_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x20B),
    /// Spark burst (RoomFx_SparkBurstTask): a flash then a spray of jittered sparks, or
    /// two widening rings, over seven ticks; stored in gRoomEffectSparkBurstId, which the grenade
    /// golems (actor_05600/05700) spawn when a shot ends.
    EFFECT_SHELTER_B2_SEPTIC_TANK_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x20C),
    /// shelter_b2_pod_access_tunnel's copy of the shared spark burst
    /// (RoomFx_SparkBurstTask): spawns impact flash 0x60076, then sprays sparks/smoke
    /// or draws expanding rings for seven ticks; the room stores it in gRoomEffectSparkBurstId
    /// (spark-burst slot, spawned by golemPawnRookBulletFly when a golem bullet ends
    /// its flight).
    EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x20D),
    /// shelter_1f_parking_garage's copy of RoomFx_SparkBurstTask (a flash then sprayed
    /// sparks or a widening ring for seven ticks), stored in gRoomEffectSparkBurstId, which the
    /// grenade-launcher GOLEM's bullet spawns where it hits.
    EFFECT_SHELTER_1F_PARKING_GARAGE_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x20E),
    /// The shelter_1f_airlock instance of the room-effect library's
    /// RoomFx_SparkBurstTask: a spark burst: a flash plus jittered sparks (non-zero
    /// arg) or two dimming rings (zero arg) for seven ticks; the room stores it in slot
    /// gRoomEffectSparkBurstId, read on impact by the Pawn/Rook GOLEM grenade
    /// (golem_pawn_rook_bullet_fly.inc.c; grenade-launcher golems actor_05600/05700).
    EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x20F),
    /// neo_ark_north_promenade's copy of the RoomFx spark burst: spawns a flash, then
    /// sprays jittered sparks or draws widening rings for seven ticks; the room stores
    /// it in slot gRoomEffectSparkBurstId, which the grenade-launcher Pawn/Rook golems spawn where a
    /// grenade lands (golemPawnRookBulletFly).
    EFFECT_NEO_ARK_NORTH_PROMENADE_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x210),
    /// Spark burst (RoomFx_SparkBurstTask): a flash then a spray of jittered sparks, or
    /// two widening rings, over seven ticks; stored in gRoomEffectSparkBurstId, which the grenade
    /// golems (actor_05600/05700) spawn when a shot ends.
    EFFECT_NEO_ARK_FOREST_ZONE_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x211),
    /// neo_ark_pavilion's copy of the shared spark burst (RoomFx_SparkBurstTask):
    /// spawns impact flash 0x60076, then sprays sparks/smoke or draws expanding rings
    /// for seven ticks; the room stores it in gRoomEffectSparkBurstId (spark-burst slot, spawned by
    /// golemPawnRookBulletFly when a golem bullet ends its flight).
    EFFECT_NEO_ARK_PAVILION_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x212),
    /// neo_ark_island's copy of RoomFx_SparkBurstTask (a flash then sprayed sparks or a
    /// widening ring for seven ticks), stored in gRoomEffectSparkBurstId, which the grenade-launcher
    /// GOLEM's bullet spawns where it hits.
    EFFECT_NEO_ARK_ISLAND_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x213),
    /// The neo_ark_power_plant_2 instance of the room-effect library's
    /// RoomFx_SparkBurstTask: a spark burst: a flash plus jittered sparks (non-zero
    /// arg) or two dimming rings (zero arg) for seven ticks; the room stores it in slot
    /// gRoomEffectSparkBurstId, read on impact by the Pawn/Rook GOLEM grenade
    /// (golem_pawn_rook_bullet_fly.inc.c; grenade-launcher golems actor_05600/05700).
    EFFECT_NEO_ARK_POWER_PLANT_2_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x214),
    /// neo_ark_savanna_zone's copy of the RoomFx spark burst: spawns a flash, then
    /// sprays jittered sparks or draws widening rings for seven ticks; the room stores
    /// it in slot gRoomEffectSparkBurstId, which the grenade-launcher Pawn/Rook golems spawn where a
    /// grenade lands (golemPawnRookBulletFly).
    EFFECT_NEO_ARK_SAVANNA_ZONE_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x215),
    /// Spark burst (RoomFx_SparkBurstTask): a flash then a spray of jittered sparks, or
    /// two widening rings, over seven ticks; stored in gRoomEffectSparkBurstId, which the grenade
    /// golems (actor_05600/05700) spawn when a shot ends.
    EFFECT_NEO_ARK_SOUTH_PROMENADE_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x216),
    /// neo_ark_shrine's copy of the shared spark burst (RoomFx_SparkBurstTask): spawns
    /// impact flash 0x60076, then sprays sparks/smoke or draws expanding rings for
    /// seven ticks; the room stores it in gRoomEffectSparkBurstId (spark-burst slot, spawned by
    /// golemPawnRookBulletFly when a golem bullet ends its flight).
    EFFECT_NEO_ARK_SHRINE_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x217),
    /// shelter_b6_nursery's copy of RoomFx_SparkBurstTask (a flash then sprayed sparks
    /// or a widening ring for seven ticks), stored in gRoomEffectSparkBurstId, which the grenade-
    /// launcher GOLEM's bullet spawns where it hits.
    EFFECT_SHELTER_B6_NURSERY_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x218),
    /// The neo_ark_bridge instance of the room-effect library's RoomFx_SparkBurstTask:
    /// a spark burst: a flash plus jittered sparks (non-zero arg) or two dimming rings
    /// (zero arg) for seven ticks; the room stores it in slot gRoomEffectSparkBurstId, read on
    /// impact by the Pawn/Rook GOLEM grenade (golem_pawn_rook_bullet_fly.inc.c;
    /// grenade-launcher golems actor_05600/05700).
    EFFECT_NEO_ARK_BRIDGE_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x219),
    /// neo_ark_pyramid's copy of the RoomFx spark burst: spawns a flash, then sprays
    /// jittered sparks or draws widening rings for seven ticks; the room stores it in
    /// slot gRoomEffectSparkBurstId, which the grenade-launcher Pawn/Rook golems spawn where a
    /// grenade lands (golemPawnRookBulletFly).
    EFFECT_NEO_ARK_PYRAMID_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x21A),
    /// Flying spark (_roomVisualEffectsFlyingSparkTask): a textured square that moves each tick
    /// by 0xCC/0x1000 of its initial target displacement, with a 20-tick lifetime; stored in
    /// gRoomEffectFlyingSparkId, which the room's glow disc spawns at random player joints.
    EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x21B),
    /// shelter_b1_south_maintenance_walkway's copy of the shared glowing disc
    /// (RoomFx_GlowDiscTask) that grows, spawns the gRoomEffectFlyingSparkId flying sparks at the
    /// player, then drifts away inside an expanding ring; the room stores it in
    /// gRoomEffectGlowDiscId (glow-disc slot, spawned by actor_02400).
    EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x21C),
    /// shelter_b1_south_maintenance_walkway's copy of _roomVisualEffectsFlyingOrangeBurstTask (an
    /// orange disc/glow with an expanding ring), stored in gRoomEffectOrangeBurst2Id, which the
    /// Amoeba's projectile spawns when it expires or hits.
    EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x21D),
    /// The shelter_b1_storeroom instance of the room-effect library's
    /// RoomFx_GlowDiscTask: a growing glowing disc that every fourth tick spawns the
    /// gRoomEffectFlyingSparkId flying spark at a random player joint, then drifts away fading inside
    /// an expanding ring; the room stores it in slot gRoomEffectGlowDiscId, read by actor_02400
    /// (fireball library), which keeps hold of the spawned task.
    EFFECT_SHELTER_B1_STOREROOM_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x21E),
    /// shelter_b1_north_maintenance_walkway's copy of the RoomFx glowing disc: grows,
    /// sends flying sparks (slot gRoomEffectFlyingSparkId) to random joints of the player, then
    /// drifts away inside an expanding ring; the room stores it in slot gRoomEffectGlowDiscId,
    /// which actor_02400 spawns and holds when its second body is hit.
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x21F),
    /// Glowing disc (RoomFx_GlowDiscTask) that grows, emits gRoomEffectFlyingSparkId flying sparks at
    /// the player, flickers and drifts away inside a ring; stored in gRoomEffectGlowDiscId, which
    /// actor_02400 spawns.
    EFFECT_SHELTER_B1_SLEEPING_QUARTERS_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x220),
    /// shelter_b2_south_maintenance_walkway's copy of the shared glowing disc
    /// (RoomFx_GlowDiscTask) that grows, spawns the gRoomEffectFlyingSparkId flying sparks at the
    /// player, then drifts away inside an expanding ring; the room stores it in
    /// gRoomEffectGlowDiscId (glow-disc slot, spawned by actor_02400).
    EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x221),
    /// shelter_b2_operating_room's copy of RoomFx_GlowDiscTask (a growing glowing disc
    /// that pulls gRoomEffectFlyingSparkId sparks from the player's joints), stored in gRoomEffectGlowDiscId,
    /// which the Amoeba holds while it grows.
    EFFECT_SHELTER_B2_OPERATING_ROOM_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x222),
    /// The shelter_b3_elevator_hall instance of the room-effect library's
    /// RoomFx_GlowDiscTask: a growing glowing disc that every fourth tick spawns the
    /// gRoomEffectFlyingSparkId flying spark at a random player joint, then drifts away fading inside
    /// an expanding ring; the room stores it in slot gRoomEffectGlowDiscId, read by actor_02400
    /// (fireball library), which keeps hold of the spawned task.
    EFFECT_SHELTER_B3_ELEVATOR_HALL_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x223),
    /// shelter_b4_upper_sewer's copy of the RoomFx glowing disc: grows, sends flying
    /// sparks (slot gRoomEffectFlyingSparkId) to random joints of the player, then drifts away inside
    /// an expanding ring; the room stores it in slot gRoomEffectGlowDiscId, which actor_02400
    /// spawns and holds when its second body is hit.
    EFFECT_SHELTER_B4_UPPER_SEWER_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x224),
    /// Glowing disc (RoomFx_GlowDiscTask) that grows, emits gRoomEffectFlyingSparkId flying sparks at
    /// the player, flickers and drifts away inside a ring; stored in gRoomEffectGlowDiscId, which
    /// actor_02400 spawns.
    EFFECT_SHELTER_B4_RESERVOIR_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x225),
    /// shelter_b4_water_supply's copy of the shared glowing disc (RoomFx_GlowDiscTask)
    /// that grows, spawns the gRoomEffectFlyingSparkId flying sparks at the player, then drifts away
    /// inside an expanding ring; the room stores it in gRoomEffectGlowDiscId (glow-disc slot,
    /// spawned by actor_02400).
    EFFECT_SHELTER_B4_WATER_SUPPLY_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x226),
    /// neo_ark_pavilion's copy of RoomFx_GlowDiscTask (a growing glowing disc that
    /// pulls gRoomEffectFlyingSparkId sparks from the player's joints), stored in gRoomEffectGlowDiscId, which
    /// the Amoeba holds while it grows.
    EFFECT_NEO_ARK_PAVILION_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x227),
    /// The neo_ark_garden instance of the room-effect library's RoomFx_GlowDiscTask: a
    /// growing glowing disc that every fourth tick spawns the gRoomEffectFlyingSparkId flying spark
    /// at a random player joint, then drifts away fading inside an expanding ring; the
    /// room stores it in slot gRoomEffectGlowDiscId, read by actor_02400 (fireball library), which
    /// keeps hold of the spawned task.
    EFFECT_NEO_ARK_GARDEN_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x228),
    /// shelter_b1_storeroom's copy of the RoomFx flying spark: an animated textured square that
    /// takes a fixed step toward its initial target position during a 20-tick lifetime; the room
    /// stores it in slot gRoomEffectFlyingSparkId, spawned by the room glow-disc effect at
    /// random player joints.
    EFFECT_SHELTER_B1_STOREROOM_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x229),
    /// Flying spark (_roomVisualEffectsFlyingSparkTask): a textured square that moves each tick
    /// by 0xCC/0x1000 of its initial target displacement, with a 20-tick lifetime; stored in
    /// gRoomEffectFlyingSparkId, which the room's glow disc spawns at random player joints.
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x22A),
    /// shelter_b1_sleeping_quarters's copy of the shared spark
    /// (_roomVisualEffectsFlyingSparkTask) that flies from its frame toward a target frame as an
    /// animated textured square for 20 ticks; the room stores it in gRoomEffectFlyingSparkId
    /// (flying-spark slot, spawned by RoomFx glow discs at random player joints).
    EFFECT_SHELTER_B1_SLEEPING_QUARTERS_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x22B),
    /// shelter_b2_south_maintenance_walkway's copy of _roomVisualEffectsFlyingSparkTask (a
    /// textured spark taking a fixed step from a player joint toward its initial target position
    /// for a 20-tick lifetime), stored in gRoomEffectFlyingSparkId and spawned by the Amoeba's
    /// glow disc.
    EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x22C),
    /// The shelter_b2_operating_room instance of the room-effect library's
    /// _roomVisualEffectsFlyingSparkTask: a spark that flies along the initial displacement
    /// toward a target frame named by the spawn argument, drawn as an animated textured square
    /// for 20 ticks; the room stores it in slot gRoomEffectFlyingSparkId, spawned by the room
    /// GLOW_DISC task (gRoomEffectGlowDiscId) at random player joints.
    EFFECT_SHELTER_B2_OPERATING_ROOM_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x22D),
    /// Elevator-hall animated spark, spawned at player joints by the glow disc.
    /// Uses a fixed initial target displacement for 20 active ticks; selected
    /// through `gRoomEffectFlyingSparkId` while the room overlay is loaded.
    EFFECT_SHELTER_B3_ELEVATOR_HALL_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x22E),
    /// Flying spark (_roomVisualEffectsFlyingSparkTask): a textured square that moves each tick
    /// by 0xCC/0x1000 of its initial target displacement, with a 20-tick lifetime; stored in
    /// gRoomEffectFlyingSparkId, which the room's glow disc spawns at random player joints.
    EFFECT_SHELTER_B4_UPPER_SEWER_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x22F),
    /// shelter_b4_reservoir's copy of the shared spark (_roomVisualEffectsFlyingSparkTask) that
    /// flies from its frame toward a target frame as an animated textured square for 20 ticks;
    /// the room stores it in gRoomEffectFlyingSparkId (flying-spark slot, spawned by RoomFx glow
    /// discs at random player joints).
    EFFECT_SHELTER_B4_RESERVOIR_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x230),
    /// shelter_b4_water_supply's copy of _roomVisualEffectsFlyingSparkTask (a textured spark
    /// taking a fixed step from a player joint toward its initial target position for a 20-tick
    /// lifetime), stored in gRoomEffectFlyingSparkId and spawned by the Amoeba's glow disc.
    EFFECT_SHELTER_B4_WATER_SUPPLY_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x231),
    /// The neo_ark_pavilion instance of the room-effect library's
    /// _roomVisualEffectsFlyingSparkTask: a spark that flies along the initial displacement
    /// toward a target frame named by the spawn argument, drawn as an animated textured square
    /// for 20 ticks; the room stores it in slot gRoomEffectFlyingSparkId, spawned by the room
    /// GLOW_DISC task (gRoomEffectGlowDiscId) at random player joints.
    EFFECT_NEO_ARK_PAVILION_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x232),
    /// neo_ark_garden's copy of the RoomFx flying spark: an animated textured square that takes
    /// a fixed step toward its initial target position during a 20-tick lifetime; the room
    /// stores it in slot gRoomEffectFlyingSparkId, spawned by the room glow-disc effect at
    /// random player joints.
    EFFECT_NEO_ARK_GARDEN_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x233),
    /// Orange burst (_roomVisualEffectsFlyingOrangeBurstTask, this unit's copy of the orange burst);
    /// stored in gRoomEffectOrangeBurst2Id, which actor_02400 spawns.
    EFFECT_SHELTER_B1_STOREROOM_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x234),
    /// shelter_b1_north_maintenance_walkway's copy of the shared orange burst
    /// (_roomVisualEffectsFlyingOrangeBurstTask, the flying-section copy of the orange burst): growing
    /// disc and glow with an expanding ring, then fades; the room stores it in
    /// gRoomEffectOrangeBurst2Id (orange-burst-2 slot, spawned by actor_02400).
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x235),
    /// shelter_b1_sleeping_quarters's copy of _roomVisualEffectsFlyingOrangeBurstTask (an orange
    /// disc/glow with an expanding ring), stored in gRoomEffectOrangeBurst2Id, which the Amoeba's
    /// projectile spawns when it expires or hits.
    EFFECT_SHELTER_B1_SLEEPING_QUARTERS_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x236),
    /// The shelter_b2_s_walkway instance of the room-effect library's
    /// _roomVisualEffectsFlyingOrangeBurstTask: an orange burst (second copy of the helpers): a growing
    /// disc and glow with a wider fading ring behind them; the room stores it in slot
    /// gRoomEffectOrangeBurst2Id, read by actor_02400 (fireball library) when its projectile times out
    /// or hits.
    EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x237),
    /// shelter_b2_operating_room's copy of the RoomFx orange burst (flying section): a
    /// growing disc and glow with an expanding, fading ring; the room stores it in slot
    /// gRoomEffectOrangeBurst2Id, which actor_02400 spawns where its fireball ends.
    EFFECT_SHELTER_B2_OPERATING_ROOM_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x238),
    /// Elevator-hall expanding orange burst with layered glow and a ring that fades first.
    /// Selected through `gRoomEffectOrangeBurst2Id` for fireball endings while
    /// the room overlay is loaded.
    EFFECT_SHELTER_B3_ELEVATOR_HALL_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x239),
    /// shelter_b4_upper_sewer's copy of the shared orange burst
    /// (_roomVisualEffectsFlyingOrangeBurstTask, the flying-section copy of the orange burst): growing
    /// disc and glow with an expanding ring, then fades; the room stores it in
    /// gRoomEffectOrangeBurst2Id (orange-burst-2 slot, spawned by actor_02400).
    EFFECT_SHELTER_B4_UPPER_SEWER_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x23A),
    /// shelter_b4_reservoir's copy of _roomVisualEffectsFlyingOrangeBurstTask (an orange disc/glow with
    /// an expanding ring), stored in gRoomEffectOrangeBurst2Id, which the Amoeba's projectile spawns
    /// when it expires or hits.
    EFFECT_SHELTER_B4_RESERVOIR_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x23B),
    /// The shelter_b4_water_supply instance of the room-effect library's
    /// _roomVisualEffectsFlyingOrangeBurstTask: an orange burst (second copy of the helpers): a growing
    /// disc and glow with a wider fading ring behind them; the room stores it in slot
    /// gRoomEffectOrangeBurst2Id, read by actor_02400 (fireball library) when its projectile times out
    /// or hits.
    EFFECT_SHELTER_B4_WATER_SUPPLY_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x23C),
    /// neo_ark_pavilion's copy of the RoomFx orange burst (flying section): a growing
    /// disc and glow with an expanding, fading ring; the room stores it in slot
    /// gRoomEffectOrangeBurst2Id, which actor_02400 spawns where its fireball ends.
    EFFECT_NEO_ARK_PAVILION_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x23D),
    /// Orange burst (_roomVisualEffectsFlyingOrangeBurstTask, this unit's copy of the orange burst);
    /// stored in gRoomEffectOrangeBurst2Id, which actor_02400 spawns.
    EFFECT_NEO_ARK_GARDEN_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x23E),
    /// mine_cavern's copy of the shared orange burst (_roomVisualEffectsHaloOrangeBurstTask): growing
    /// disc and glow with an expanding dimmer ring, then fades; the room stores it in
    /// gRoomEffectOrangeBurstId (orange-burst slot, read by actor_00300).
    EFFECT_MINE_CAVERN_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x23F),
    /// mine_secret_passage's copy of _roomVisualEffectsHaloOrangeBurstTask (an orange disc/glow with
    /// an expanding ring), stored in gRoomEffectOrangeBurstId, which the Brain Stinger's fireball
    /// spawns when it expires or hits.
    EFFECT_MINE_SECRET_PASSAGE_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x240),
    /// mine_secret_passage's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_MINE_SECRET_PASSAGE_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x241),
    /// mine_secret_passage's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_MINE_SECRET_PASSAGE_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x242),
    /// Spark emitter (RoomFx_SparkEmitterTask): for 0x14 ticks spawns gRoomEffectMoteId
    /// particles outward along a turning heading; stored in gRoomEffectSparkEmitterId, which
    /// actor_00300 spawns in its heal step.
    EFFECT_MINE_SECRET_PASSAGE_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x243),
    /// mine_cavern's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_MINE_CAVERN_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x244),
    /// shelter_b1_elevator_hall's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B1_ELEVATOR_HALL_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x245),
    /// shelter_b1_storeroom's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B1_STOREROOM_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x246),
    /// shelter_b1_north_maintenance_walkway's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x247),
    /// shelter_b1_main_corridor's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B1_MAIN_CORRIDOR_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x248),
    /// shelter_b1_transfer_tunnel's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B1_TRANSFER_TUNNEL_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x249),
    /// shelter_b2_elevator_hall's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B2_ELEVATOR_HALL_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x24A),
    /// shelter_b2_north_maintenance_walkway's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x24B),
    /// shelter_b2_pod_access_tunnel's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x24C),
    /// shelter_b3_elevator_hall's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B3_ELEVATOR_HALL_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x24D),
    /// shelter_b4_upper_sewer's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_SHELTER_B4_UPPER_SEWER_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x24E),
    /// neo_ark_north_promenade's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_NEO_ARK_NORTH_PROMENADE_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x24F),
    /// mine_cavern's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_MINE_CAVERN_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x250),
    /// shelter_b1_elevator_hall's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B1_ELEVATOR_HALL_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x251),
    /// shelter_b1_storeroom's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B1_STOREROOM_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x252),
    /// shelter_b1_north_maintenance_walkway's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x253),
    /// shelter_b1_main_corridor's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B1_MAIN_CORRIDOR_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x254),
    /// shelter_b1_transfer_tunnel's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B1_TRANSFER_TUNNEL_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x255),
    /// shelter_b2_elevator_hall's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B2_ELEVATOR_HALL_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x256),
    /// shelter_b2_north_maintenance_walkway's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x257),
    /// shelter_b2_pod_access_tunnel's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x258),
    /// shelter_b3_elevator_hall's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B3_ELEVATOR_HALL_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x259),
    /// shelter_b4_upper_sewer's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_SHELTER_B4_UPPER_SEWER_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x25A),
    /// neo_ark_north_promenade's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_NEO_ARK_NORTH_PROMENADE_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x25B),
    /// Orange burst (_roomVisualEffectsHaloOrangeBurstTask): disc and glow growing inside an expanding
    /// ring, then fading; stored in gRoomEffectOrangeBurstId, which actor_00300 spawns where its
    /// fireball ends.
    EFFECT_SHELTER_B1_ELEVATOR_HALL_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x25C),
    /// shelter_b1_storeroom's copy of the shared orange burst (_roomVisualEffectsHaloOrangeBurstTask):
    /// growing disc and glow with an expanding dimmer ring, then fades; the room stores
    /// it in gRoomEffectOrangeBurstId (orange-burst slot, read by actor_00300).
    EFFECT_SHELTER_B1_STOREROOM_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x25D),
    /// shelter_b1_north_maintenance_walkway's copy of _roomVisualEffectsHaloOrangeBurstTask (an orange
    /// disc/glow with an expanding ring), stored in gRoomEffectOrangeBurstId, which the Brain
    /// Stinger's fireball spawns when it expires or hits.
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x25E),
    /// The shelter_b1_main_corridor instance of the room-effect library's
    /// _roomVisualEffectsHaloOrangeBurstTask: an orange burst: a growing disc and glow with a wider
    /// dimmer ring expanding and fading behind them; the room stores it in slot
    /// gRoomEffectOrangeBurstId, read by actor_00300 (fireball library) when its fireball times out,
    /// hits or is blocked.
    EFFECT_SHELTER_B1_MAIN_CORRIDOR_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x25F),
    /// shelter_b1_transfer_tunnel's copy of the RoomFx orange burst (halo section): a
    /// growing disc and glow with an expanding, fading ring; the room stores it in slot
    /// gRoomEffectOrangeBurstId, which actor_00300 spawns where its fireball ends.
    EFFECT_SHELTER_B1_TRANSFER_TUNNEL_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x260),
    /// Orange burst (_roomVisualEffectsHaloOrangeBurstTask): disc and glow growing inside an expanding
    /// ring, then fading; stored in gRoomEffectOrangeBurstId, which actor_00300 spawns where its
    /// fireball ends.
    EFFECT_SHELTER_B2_ELEVATOR_HALL_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x261),
    /// shelter_b2_north_maintenance_walkway's copy of the shared orange burst
    /// (_roomVisualEffectsHaloOrangeBurstTask): growing disc and glow with an expanding dimmer ring,
    /// then fades; the room stores it in gRoomEffectOrangeBurstId (orange-burst slot, read by
    /// actor_00300).
    EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x262),
    /// shelter_b2_pod_access_tunnel's copy of _roomVisualEffectsHaloOrangeBurstTask (an orange
    /// disc/glow with an expanding ring), stored in gRoomEffectOrangeBurstId, which the Brain
    /// Stinger's fireball spawns when it expires or hits.
    EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x263),
    /// The shelter_b3_elevator_hall instance of the room-effect library's
    /// _roomVisualEffectsHaloOrangeBurstTask: an orange burst: a growing disc and glow with a wider
    /// dimmer ring expanding and fading behind them; the room stores it in slot
    /// gRoomEffectOrangeBurstId, read by actor_00300 (fireball library) when its fireball times out,
    /// hits or is blocked.
    EFFECT_SHELTER_B3_ELEVATOR_HALL_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x264),
    /// shelter_b4_upper_sewer's copy of the RoomFx orange burst (halo section): a
    /// growing disc and glow with an expanding, fading ring; the room stores it in slot
    /// gRoomEffectOrangeBurstId, which actor_00300 spawns where its fireball ends.
    EFFECT_SHELTER_B4_UPPER_SEWER_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x265),
    /// Orange burst (_roomVisualEffectsHaloOrangeBurstTask): disc and glow growing inside an expanding
    /// ring, then fading; stored in gRoomEffectOrangeBurstId, which actor_00300 spawns where its
    /// fireball ends.
    EFFECT_NEO_ARK_NORTH_PROMENADE_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x266),
    /// mine_cavern's copy of the shared spark emitter (RoomFx_SparkEmitterTask): for
    /// 0x14 ticks spawns the gRoomEffectMoteId mote effect outward along a turning heading;
    /// the room stores it in gRoomEffectSparkEmitterId (spark emitter slot, read by actor_00300).
    EFFECT_MINE_CAVERN_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x267),
    /// shelter_b1_elevator_hall's copy of RoomFx_SparkEmitterTask (for 0x14 ticks
    /// sprays the gRoomEffectMoteId mote outwards), stored in gRoomEffectSparkEmitterId, which the Brain
    /// Stinger spawns when it restores 0x64 HP.
    EFFECT_SHELTER_B1_ELEVATOR_HALL_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x268),
    /// The shelter_b1_storeroom instance of the room-effect library's
    /// RoomFx_SparkEmitterTask: a spark emitter that for 0x14 ticks spins its heading
    /// and throws the gRoomEffectMoteId ember outward; the room stores it in slot gRoomEffectSparkEmitterId,
    /// read by actor_00300 when it heals itself (+100 HP).
    EFFECT_SHELTER_B1_STOREROOM_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x269),
    /// shelter_b1_north_maintenance_walkway's copy of the RoomFx spark emitter: for
    /// 0x14 ticks spawns slot-gRoomEffectMoteId motes flying outward on a turning heading; the
    /// room stores it in slot gRoomEffectSparkEmitterId, which actor_00300 spawns when it heals itself
    /// by 0x64 HP.
    EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x26A),
    /// Spark emitter (RoomFx_SparkEmitterTask): for 0x14 ticks spawns gRoomEffectMoteId
    /// particles outward along a turning heading; stored in gRoomEffectSparkEmitterId, which
    /// actor_00300 spawns in its heal step.
    EFFECT_SHELTER_B1_MAIN_CORRIDOR_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x26B),
    /// shelter_b1_transfer_tunnel's copy of the shared spark emitter
    /// (RoomFx_SparkEmitterTask): for 0x14 ticks spawns the gRoomEffectMoteId mote effect
    /// outward along a turning heading; the room stores it in gRoomEffectSparkEmitterId (spark emitter
    /// slot, read by actor_00300).
    EFFECT_SHELTER_B1_TRANSFER_TUNNEL_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x26C),
    /// shelter_b2_elevator_hall's copy of RoomFx_SparkEmitterTask (for 0x14 ticks
    /// sprays the gRoomEffectMoteId mote outwards), stored in gRoomEffectSparkEmitterId, which the Brain
    /// Stinger spawns when it restores 0x64 HP.
    EFFECT_SHELTER_B2_ELEVATOR_HALL_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x26D),
    /// The shelter_b2_n_walkway instance of the room-effect library's
    /// RoomFx_SparkEmitterTask: a spark emitter that for 0x14 ticks spins its heading
    /// and throws the gRoomEffectMoteId ember outward; the room stores it in slot gRoomEffectSparkEmitterId,
    /// read by actor_00300 when it heals itself (+100 HP).
    EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x26E),
    /// shelter_b2_pod_access_tunnel's copy of the RoomFx spark emitter: for 0x14 ticks
    /// spawns slot-gRoomEffectMoteId motes flying outward on a turning heading; the room
    /// stores it in slot gRoomEffectSparkEmitterId, which actor_00300 spawns when it heals itself by
    /// 0x64 HP.
    EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x26F),
    /// Spark emitter (RoomFx_SparkEmitterTask): for 0x14 ticks spawns gRoomEffectMoteId
    /// particles outward along a turning heading; stored in gRoomEffectSparkEmitterId, which
    /// actor_00300 spawns in its heal step.
    EFFECT_SHELTER_B3_ELEVATOR_HALL_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x270),
    /// shelter_b4_upper_sewer's copy of the shared spark emitter
    /// (RoomFx_SparkEmitterTask): for 0x14 ticks spawns the gRoomEffectMoteId mote effect
    /// outward along a turning heading; the room stores it in gRoomEffectSparkEmitterId (spark emitter
    /// slot, read by actor_00300).
    EFFECT_SHELTER_B4_UPPER_SEWER_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x271),
    /// neo_ark_north_promenade's copy of RoomFx_SparkEmitterTask (for 0x14 ticks sprays
    /// the gRoomEffectMoteId mote outwards), stored in gRoomEffectSparkEmitterId, which the Brain Stinger
    /// spawns when it restores 0x64 HP.
    EFFECT_NEO_ARK_NORTH_PROMENADE_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x272),
    /// Fire blast: orange fade quad, an orange point light, a flame disc and a ring of
    /// 0x60275 flames plus 0x60274; actor_521100 spawns it at the player's part 12
    /// during its grab attack.
    EFFECT_DILAPIDATED_HOUSE_FIRE_BLAST = EFFECT_ID(EFFECT_TASK_BANK, 0x273),
    /// Expanding, fading flame cone (glowDrawFlameCone), the dilapidated house's copy
    /// of the pyrokinesis cone; spawned by the room's fire blast 0x60273 with a
    /// ring of 0x60275 flames.
    EFFECT_DRYFIELD_DILAPIDATED_HOUSE_FLAME_CONE = EFFECT_ID(EFFECT_TASK_BANK, 0x274),
    /// Flame ring (glowDrawFlameRing) that expands 0x80 and fades 8 per frame until
    /// gone; a ring of them is spawned and reparented by the room's flame burst
    /// 0x60273, which actor_521100 fires on the player.
    EFFECT_DILAPIDATED_HOUSE_FLAME_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x275),
    /// shelter_b1_control_room's copy of the shared glowing disc (RoomFx_GlowDiscTask)
    /// that grows, spawns the gRoomEffectFlyingSparkId flying sparks at the player, then drifts away
    /// inside an expanding ring; the room stores it in gRoomEffectGlowDiscId (glow-disc slot,
    /// spawned by actor_02400).
    EFFECT_SHELTER_B1_CONTROL_ROOM_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x276),
    /// shelter_b1_control_room's copy of _roomVisualEffectsFlyingSparkTask (a textured spark
    /// taking a fixed step from a player joint toward its initial target position for a 20-tick
    /// lifetime), stored in gRoomEffectFlyingSparkId and spawned by the Amoeba's glow disc.
    EFFECT_SHELTER_B1_CONTROL_ROOM_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x277),
    /// The shelter_b1_control_room instance of the room-effect library's
    /// _roomVisualEffectsFlyingOrangeBurstTask: an orange burst (second copy of the helpers): a growing
    /// disc and glow with a wider fading ring behind them; the room stores it in slot
    /// gRoomEffectOrangeBurst2Id, read by actor_02400 (fireball library) when its projectile times out
    /// or hits.
    EFFECT_SHELTER_B1_CONTROL_ROOM_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x278),
    /// shelter_b1_control_room_access_tunnel's copy of the RoomFx glowing disc: grows,
    /// sends flying sparks (slot gRoomEffectFlyingSparkId) to random joints of the player, then
    /// drifts away inside an expanding ring; the room stores it in slot gRoomEffectGlowDiscId,
    /// which actor_02400 spawns and holds when its second body is hit.
    EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x279),
    /// Flying spark (_roomVisualEffectsFlyingSparkTask): a textured square that moves each tick
    /// by 0xCC/0x1000 of its initial target displacement, with a 20-tick lifetime; stored in
    /// gRoomEffectFlyingSparkId, which the room's glow disc spawns at random player joints.
    EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x27A),
    /// shelter_b1_control_room_access_tunnel's copy of the shared orange burst
    /// (_roomVisualEffectsFlyingOrangeBurstTask, the flying-section copy of the orange burst): growing
    /// disc and glow with an expanding ring, then fades; the room stores it in
    /// gRoomEffectOrangeBurst2Id (orange-burst-2 slot, spawned by actor_02400).
    EFFECT_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x27B),
    /// shelter_b2_breeding_room's copy of RoomFx_GlowDiscTask (a growing glowing disc
    /// that pulls gRoomEffectFlyingSparkId sparks from the player's joints), stored in gRoomEffectGlowDiscId,
    /// which the Amoeba holds while it grows.
    EFFECT_SHELTER_B2_BREEDING_ROOM_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x27C),
    /// The shelter_b2_breeding_room instance of the room-effect library's
    /// _roomVisualEffectsFlyingSparkTask: a spark that flies along the initial displacement
    /// toward a target frame named by the spawn argument, drawn as an animated textured square
    /// for 20 ticks; the room stores it in slot gRoomEffectFlyingSparkId, spawned by the room
    /// GLOW_DISC task (gRoomEffectGlowDiscId) at random player joints.
    EFFECT_SHELTER_B2_BREEDING_ROOM_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x27D),
    /// shelter_b2_breeding_room's copy of the RoomFx orange burst (flying section): a
    /// growing disc and glow with an expanding, fading ring; the room stores it in slot
    /// gRoomEffectOrangeBurst2Id, which actor_02400 spawns where its fireball ends.
    EFFECT_SHELTER_B2_BREEDING_ROOM_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x27E),
    /// Glowing disc (RoomFx_GlowDiscTask) that grows, emits gRoomEffectFlyingSparkId flying sparks at
    /// the player, flickers and drifts away inside a ring; stored in gRoomEffectGlowDiscId, which
    /// actor_02400 spawns.
    EFFECT_NEO_ARK_SUBMARINE_TUNNEL_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x27F),
    /// neo_ark_submarine_tunnel's copy of the shared spark (_roomVisualEffectsFlyingSparkTask)
    /// that flies from its frame toward a target frame as an animated textured square for 20
    /// ticks; the room stores it in gRoomEffectFlyingSparkId (flying-spark slot, spawned by
    /// RoomFx glow discs at random player joints).
    EFFECT_NEO_ARK_SUBMARINE_TUNNEL_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x280),
    /// neo_ark_submarine_tunnel's copy of _roomVisualEffectsFlyingOrangeBurstTask (an orange disc/glow
    /// with an expanding ring), stored in gRoomEffectOrangeBurst2Id, which the Amoeba's projectile
    /// spawns when it expires or hits.
    EFFECT_NEO_ARK_SUBMARINE_TUNNEL_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x281),
    /// dryfield_motel_balcony's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_DRYFIELD_MOTEL_BALCONY_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x282),
    /// dryfield_motel_balcony's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_DRYFIELD_MOTEL_BALCONY_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x283),
    /// Orange burst (_roomVisualEffectsHaloOrangeBurstTask): disc and glow growing inside an expanding
    /// ring, then fading; stored in gRoomEffectOrangeBurstId, which actor_00300 spawns where its
    /// fireball ends.
    EFFECT_DRYFIELD_MOTEL_BALCONY_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x284),
    /// dryfield_motel_balcony's copy of the shared spark emitter
    /// (RoomFx_SparkEmitterTask): for 0x14 ticks spawns the gRoomEffectMoteId mote effect
    /// outward along a turning heading; the room stores it in gRoomEffectSparkEmitterId (spark emitter
    /// slot, read by actor_00300).
    EFFECT_DRYFIELD_MOTEL_BALCONY_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x285),
    /// dryfield_night_main_street's animated mote, rising and brightening or drifting steadily before fading.
    ///
    /// The room selects this `_roomVisualEffectsMoteTask` instance through
    /// `gRoomEffectMoteId` for fireball embers and emitted sparks.
    EFFECT_DRYFIELD_NIGHT_MAIN_STREET_MOTE = EFFECT_ID(EFFECT_TASK_BANK, 0x286),
    /// dryfield_night_main_street's expanding tinted halo and shrinking ring, followed by a fading star.
    ///
    /// The room selects this `_roomVisualEffectsHaloTask` instance through
    /// `gRoomEffectHaloId` for the Brain Stinger's charge and MP-drain effects.
    EFFECT_DRYFIELD_NIGHT_MAIN_STREET_HALO = EFFECT_ID(EFFECT_TASK_BANK, 0x287),
    /// dryfield_night_main_street's copy of the RoomFx orange burst (halo section): a
    /// growing disc and glow with an expanding, fading ring; the room stores it in slot
    /// gRoomEffectOrangeBurstId, which actor_00300 spawns where its fireball ends.
    EFFECT_DRYFIELD_NIGHT_MAIN_STREET_ORANGE_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x288),
    /// Spark emitter (RoomFx_SparkEmitterTask): for 0x14 ticks spawns gRoomEffectMoteId
    /// particles outward along a turning heading; stored in gRoomEffectSparkEmitterId, which
    /// actor_00300 spawns in its heal step.
    EFFECT_DRYFIELD_NIGHT_MAIN_STREET_SPARK_EMITTER = EFFECT_ID(EFFECT_TASK_BANK, 0x289),
    /// dryfield_toilet's copy of the shared glowing disc (RoomFx_GlowDiscTask) that
    /// grows, spawns the gRoomEffectFlyingSparkId flying sparks at the player, then drifts away
    /// inside an expanding ring; the room stores it in gRoomEffectGlowDiscId (glow-disc slot,
    /// spawned by actor_02400).
    EFFECT_DRYFIELD_TOILET_GLOW_DISC = EFFECT_ID(EFFECT_TASK_BANK, 0x28A),
    /// dryfield_toilet's copy of _roomVisualEffectsFlyingSparkTask (a textured spark flying from
    /// a player joint to the target frame over 20 ticks), stored in gRoomEffectFlyingSparkId and
    /// spawned by the Amoeba's glow disc.
    EFFECT_DRYFIELD_TOILET_FLYING_SPARK = EFFECT_ID(EFFECT_TASK_BANK, 0x28B),
    /// The dryfield_toilet instance of the room-effect library's
    /// _roomVisualEffectsFlyingOrangeBurstTask: an orange burst (second copy of the helpers): a growing
    /// disc and glow with a wider fading ring behind them; the room stores it in slot
    /// gRoomEffectOrangeBurst2Id, read by actor_02400 (fireball library) when its projectile times out
    /// or hits.
    EFFECT_DRYFIELD_TOILET_ORANGE_BURST_2 = EFFECT_ID(EFFECT_TASK_BANK, 0x28C),
    /// acropolis_cafeteria's copy of the RoomFx flash: ramps up two fans and a
    /// shrinking ring with a fade quad at its peak, then fades out through a star; the
    /// room stores it in slot gRoomEffectFlashId, which the Pawn/Rook golem library spawns at
    /// body part 4 for its scream (golemPawnRookSilenceScream).
    EFFECT_ACROPOLIS_CAFETERIA_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x28D),
    /// Twin trail (room_visual_effects_trail_task): two points offset from the anchor
    /// recorded in rings of eight and drawn as a fading beam; stored in gRoomEffectTwinTrailId,
    /// which the beam-sword golems (actor_02000/02300) spawn at model part 7.
    EFFECT_ACROPOLIS_CAFETERIA_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x28E),
    /// acropolis_cafeteria's copy of the shared spark burst (RoomFx_SparkBurstTask):
    /// spawns impact flash 0x60076, then sprays sparks/smoke or draws expanding rings
    /// for seven ticks; the room stores it in gRoomEffectSparkBurstId (spark-burst slot, spawned by
    /// golemPawnRookBulletFly when a golem bullet ends its flight).
    EFFECT_ACROPOLIS_CAFETERIA_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x28F),
    /// acropolis_forked_road's copy of _roomVisualEffectsFlashTask (fans and a shrinking ring
    /// ramping to a coloured screen fade), stored in gRoomEffectFlashId, which the Pawn/Rook
    /// GOLEM's silence-scream state spawns on its part 4.
    EFFECT_ACROPOLIS_FORKED_ROAD_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x290),
    /// The acropolis_forked_road instance of the room-effect library's
    /// room_visual_effects_trail_task.inc.c: a twin-trail beam: two rings of eight
    /// frames recording two points offset from the anchor, drawn as a beam between
    /// them; the room stores it in slot gRoomEffectTwinTrailId, read by the Pawn/Rook GOLEM
    /// delayed-effect child at part 7 (golem_pawn_rook_delayed_effect_tick.inc.c; the
    /// beam-sword golems actor_02000/02300 reference this slot).
    EFFECT_ACROPOLIS_FORKED_ROAD_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x291),
    /// acropolis_forked_road's copy of the RoomFx spark burst: spawns a flash, then
    /// sprays jittered sparks or draws widening rings for seven ticks; the room stores
    /// it in slot gRoomEffectSparkBurstId, which the grenade-launcher Pawn/Rook golems spawn where a
    /// grenade lands (golemPawnRookBulletFly).
    EFFECT_ACROPOLIS_FORKED_ROAD_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x292),
    /// Flash (_roomVisualEffectsFlashTask): fans and an inward ring ramping up, a fade quad at the
    /// peak, then a star fade-out; stored in gRoomEffectFlashId, which golems
    /// (actor_02300/05700) spawn at part 4 in their scream sequence.
    EFFECT_DRYFIELD_MAIN_STREET_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x293),
    /// dryfield_main_street's copy of the shared twin trail
    /// (room_visual_effects_trail_task): two rings of eight frames drawn as a fading
    /// gouraud beam; the room stores it in gRoomEffectTwinTrailId (twin-trail slot, spawned by the
    /// Pawn/Rook golem library at joint 7 via golemPawnRookDelayedEffectTick).
    EFFECT_DRYFIELD_MAIN_STREET_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x294),
    /// dryfield_main_street's copy of RoomFx_SparkBurstTask (a flash then sprayed
    /// sparks or a widening ring for seven ticks), stored in gRoomEffectSparkBurstId, which the
    /// grenade-launcher GOLEM's bullet spawns where it hits.
    EFFECT_DRYFIELD_MAIN_STREET_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x295),
    /// The dryfield_back_street instance of the room-effect library's _roomVisualEffectsFlashTask:
    /// a flash: two fans and an inward-shrinking ring ramping up over spawnArg1 ticks,
    /// peaking with a coloured fade quad, then fading through a star; the room stores
    /// it in slot gRoomEffectFlashId, read by the Pawn/Rook GOLEM silence-scream state
    /// (golem_pawn_rook_silence_scream.inc.c, part 4).
    EFFECT_DRYFIELD_BACK_STREET_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x296),
    /// dryfield_back_street's copy of the RoomFx twin trail: two eight-slot rings of
    /// frames drawn as a fading beam behind an anchor; the room stores it in slot
    /// gRoomEffectTwinTrailId, which the beam-sword Pawn/Rook golems spawn at body part 7
    /// (golemPawnRookDelayedEffectTick).
    EFFECT_DRYFIELD_BACK_STREET_TWIN_TRAIL = EFFECT_ID(EFFECT_TASK_BANK, 0x297),
    /// Spark burst (RoomFx_SparkBurstTask): a flash then a spray of jittered sparks, or
    /// two widening rings, over seven ticks; stored in gRoomEffectSparkBurstId, which the grenade
    /// golems (actor_05600/05700) spawn when a shot ends.
    EFFECT_DRYFIELD_BACK_STREET_SPARK_BURST = EFFECT_ID(EFFECT_TASK_BANK, 0x298),
    /// Yellow expanding arc and ring at the player that fades after nine ticks (only
    /// the newest instance survives); spawned by actor_205200 when its attack seizes
    /// the player.
    EFFECT_SHELTER_B6_CORRIDOR_PLAYER_HIT_RING = EFFECT_ID(EFFECT_TASK_BANK, 0x299),
    /// Gunblade charge-up/blast flash for its three shot grades: spawns four grade-
    /// coloured effects and sparks, then a spinning ring with crossing arcs and a full-
    /// screen fade; spawned by the gunblade's fire state.
    EFFECT_GUNBLADE_CHARGE_FLASH = EFFECT_ID(EFFECT_TASK_BANK, 0x29A),
    /// Bank 8 slot 5 (Gp_EffAttachTask37 with a caller-set TMD): a severed body-part
    /// model (head/arm/leg/ear) flung when an enemy bursts; the caller sets
    /// D_800626EC[5].data.model first.
    EFFECT_BURST_BODY_PART_BANK8 = EFFECT_ID(8, 0x005),
    /// Bank-10 model effect: a body-part model (TmdSource the actor stores in
    /// D_80114B34[5]) flung off when an enemy bursts apart, e.g. the Desert Chaser's
    /// legs, torso and head.
    EFFECT_BURST_BODY_PART_BANK10 = EFFECT_ID(10, 0x005),
};

#endif // GAMEPLAY_EFFECT_IDS_H
