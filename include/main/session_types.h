#ifndef MAIN_SESSION_TYPES_H
#define MAIN_SESSION_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/world_collision_types.h"

#include "main/coord.h"

struct GpActorD4;
struct AnimationRecord;
struct GpAnimSet;
struct GpLinkNode;
struct Task;

/// Location selector shared by the live session, saved state and world lookups.
///
/// The six unsigned bytes select parts of the location. `view` is
/// a room-local slot; a lookup maps it to the camera/image index. `variant`
/// selects the area's placement and resource layout and mirrors the saved
/// per-area variant after synchronization. Each layout supplies a placement
/// list and its associated resources.
///
/// Active stage, area, room, view and warp IDs are 1-based. Stage 0 means no
/// active stage; the other upper bounds depend on the selected loaded tables.
/// Each lookup requires only the components it reads to be initialized and in
/// range. The key is byte-aligned; packed word reads require a word-aligned
/// instance. Whole location-cell transfers use the eight-byte `GameLoc`.
typedef struct {
    u8 view;    // 1-based view slot within the room, mapped to a camera/image index
    u8 room;    // 1-based room within the area
    u8 area;    // 1-based area and CDF folder within the stage
    u8 stage;   // Stage ID (0 no active stage, 1..5 active stages)
    u8 warp;    // 1-based arrival record within the area's warp table
    u8 variant; // Placement/resource layout within the area (1 default)
} GameLocationKey;
STATIC_ASSERT_SIZEOF(GameLocationKey, 0x6);

/// Eight-byte location cell shared by the live session and saved state.
///
/// Whole-cell copies preserve the two bytes following the six-byte key;
/// their role is unproven. The cell is byte-aligned. Stream lookups borrow a
/// byte view of the cell and read only its leading view and room bytes;
/// a local copy can replace the view byte with a stream identifier.
typedef struct {
    GameLocationKey loc;          // Place key; leading view and room also select streams
    u8              unknown_6[2]; // Retained by whole-cell copies; role unproven
} GameLoc;
STATIC_ASSERT_SIZEOF(GameLoc, 8);

/// Live play-state object shared by main and every overlay.
///
/// One BSS instance is pointed to by `gGameSession`. It holds the current
/// place in the world, the tasks the session keeps by slot, remapped pad
/// buttons, and the flags that cutscenes, view loads and death/restart share.
/// New game, load and reset zero the whole object, which pins the size at 0x13C.
typedef struct {
    s8           deathVariant;     // (0 none, 1/2 which death cutscene file); nonzero blocks resume and menu
    s8           eventState;       // 0 idle; nonzero blocks player-dir handling and room scripts
    u8           uiOpen;           // 1 while a UI overlay is up; enables d-pad auto-repeat
    byte         unknown_3;
    GameLoc      at4;              // current place in the world
    struct Task* ptrSlots[16];     // tasks the session keeps by slot
    u8           applySaveVariant; // 1: next area load restores the saved placement variant
    u8           viewReady;        // (0 loading/transitioning, 1 current view finished loading)
    u16          field_4E;
    byte         unknown_50[2];
    s16          viewDirty;      // nonzero: respawn the view from the save
    byte         unknown_54[4];
    u16          pad;            // remapped buttons this frame
    u16          padPrev;        // remapped buttons last frame
    u16          padTrig;        // remapped buttons newly pressed this frame
    u8           field_5E;
    u8           evtSkipped;     // 1 after a forced script skip; nonzero ends overlay and timed waits early
    byte         unknown_60[4];
    u8           freezeRoomObjs; // nonzero: skip room-object state dispatch
    u8           field_65;
    u8           cutsceneHold;   // 1 during scripted sequences: alternate item menu, player hold
    byte         areaSetupDone;  // 0 first area setup skips warp-arrival SFX/flag and latches to 1
    u8           hideHud;        // 1: suppress item prompt, HUD, and target cursor
    u8           flowFlags;      // bit0 skip ending bank-load; bit1 skip area-enter bank-load; bit2 ending spawn 3 vs 2; bit3 area-enter spawn 3 vs 1; bit6 hide weapon with bit7; bit7 PE re-equip
    byte         unknown_6A[0xA];
    u16          sprtVariant;    // 1-based sprite-table / CdCmd param2[0] variant; USA forces 1
    s16          roomObjsDirty;  // nonzero: relink room objects on the next room-obj tick
    s16          loadedStage;    // last stage whose CD was enqueued
    byte         unknown_7A[2];
    s16          field_7C;
    s16          field_7E;
    s16          field_80;
    byte         unknown_82[0x9A];
    s16          loadedWeaponFamily;  // last-loaded player weapon-anim family; -1 forces a CD refresh
    s16          loadedConfigSet;     // last-loaded player config-set; paired with loadedWeaponFamily
    s16          sceneClock;          // frame countdown for timed scene scripts
    s16          waterY;              // water surface world Y
    u8           companionType;       // (0 none, 1/2/3)
    u8           companionVariant;    // addend within the companion ally-id family
    u8           field_126;
    u8           suppressDeathChecks; // nonzero: skip player/companion-down handling
    u8           restartMode;         // (0, 1 companion-1 down, 3 special, 4 companion-3 down, 0xFF ending load)
    u8           loadedSndId;         // last sound-file id already queued; skip re-enqueue when unchanged
    s16          bossPartsHpSum;      // sum of living boss-part HP; scales later enemy spawn HP
    u8           field_12C;
    s8           areaBgmCountdown;    // frames before area BGM on the death path; 0x7F holds without counting
    u8           field_12E;
    u8           deathRestartDelay;   // frames the play-clock waits before BGM/restart after death
    byte         spawnPhase[2];       // (0 idle, 1 armed, 2 done) per spawn-controller slot
    u8           field_132;
    byte         field_133;
    byte         eventRoomIndex; // 0-based room echoed into event replies
    u8           field_135;
    u8           enemyCullZone;  // 1..16 index into the enemy axis-limit table; 0 disables
    byte         skipEventIntro; // nonzero: skip intro spawns
    byte         unknown_138;
    s8           hudShakeY;      // signed HUD vertical shake amplitude (pixels x 3)
    u8           dirActionBusy;  // 1 while a direction/cap action is in flight; blocks HUD
    u8           padScriptFlags; // bit0 hold, bit1 lerp, bit7 run pad scripts during battle freeze
} GameSession;
STATIC_ASSERT_SIZEOF(GameSession, 0x13C);
STATIC_ASSERT(OFFSET_OF(GameSession, at4) == 4, GameSession_at4);
STATIC_ASSERT(OFFSET_OF(GameSession, at4.loc) == 4, GameSession_at4_loc);
STATIC_ASSERT(OFFSET_OF(GameSession, ptrSlots) == 0xC, GameSession_ptrSlots);

/// Link record of an enemy work object: the entry the lock-on system keeps for
/// an actor it is allowed to track. Every object that carries one embeds it at
/// +0x10, so the owner is a fixed subtraction away from any node and the actor
/// behind a node reached from the list can be recovered.
///
/// Two walks run over the list each frame: the transform pass reprojects every
/// tracked actor's position, and the lock-on scan picks which of them an actor
/// slot aims at. `state.b.flags` is the node's own state, written by whoever
/// owns the object; `targeted` and `onList` are the positions the tracking
/// helpers maintain, and what the reticle and the on-screen markers read. The
/// list walkers read the three bytes as one word, `state.word`, and test the
/// flags through it.
typedef struct GpLinkNode {
    struct GpLinkNode* next; // Next node in the list; NULL at the tail
    union {
        struct {
            u8 flags;    // Object state: 0x01 not lockable, 0x04 reproject while not lockable, 0x08 no HP readout
            u8 targeted; // Non-zero while an actor slot is locked onto this node
            u8 onList;   // Non-zero while the node hangs on the list
        } b;
        u32 word;
    } state;
} GpLinkNode;
STATIC_ASSERT_SIZEOF(GpLinkNode, 0x8);

/// What a kind-4 `GpObj` holds in `ctx.dir`. `dir` is the facing vector written
/// there each frame; `field_8` is the `WorldCollisionContact` table the object's contacts are
/// recorded in.
///
/// Declared main-side because `GameActor` embeds them by value.
typedef struct _GpObjDirRec {
    /* 0x0 */ SVECTOR                dir;
    /* 0x8 */ WorldCollisionContact* field_8;
} GpObjDirRec;
STATIC_ASSERT_SIZEOF(GpObjDirRec, 0xC);

/// The collision shape a body carries: the segment between two local
/// endpoints, the radius at each, and the table the contacts it makes are
/// recorded in. A body whose kind bits name this shape reaches it through its
/// `GpObj` context.
///
/// Both endpoints are offsets under the body's own position in its own frame,
/// so the shape travels with it. The grid passes sweep the segment between the
/// two, and the pair passes test the cylinder that the segment and the two
/// radii describe, which is a capsule while the radii are equal and tapers
/// between them when they are not.
///
/// The table is the shape's link to its owner: it is sized and cleared when the
/// shape is set up, and the collision passes fill it as contacts are made. A
/// weapon re-arms the shape as it unfolds, pushing one endpoint out along its
/// reach and widening that end's radius with the spread.
///
/// Declared main-side because `GameActor` embeds one by value; the work blocks
/// that carry the same shape reach it through this declaration.
typedef struct {
    /* 0x00 */ SVECTOR                end0;       // one end of the segment, offset under the body
    /* 0x08 */ SVECTOR                end1;       // the other end
    /* 0x10 */ s16                    end0Radius; // radius at `end0`
    /* 0x12 */ s16                    end1Radius; // radius at `end1`; equal to `end0Radius` unless tapered
    /* 0x14 */ WorldCollisionContact* recs;       // table the contacts this body makes are recorded in
} GpActorD4Rec;
STATIC_ASSERT_SIZEOF(GpActorD4Rec, 0x18);

/// One animation slot: the playback state of one model part's animation.
///
/// The slot walks the keyframe records of a single track, blending the pose it
/// has reached (`curSet`/`curRec`) into the one it is heading for
/// (`nextSet`/`nextRec`), which takes that keyframe's `duration` as the length
/// of the segment. `timeLeft` counts the segment down under `rate` and is the
/// blend's numerator, `timeSpan` its denominator. Records that are control
/// entries rather than poses are not shown: the walk follows them and reports
/// what it did in `flags`.
///
/// A slot's track and its transform are separate: `trackIndex` names the track
/// it reads and `mtxIndex` the coordinate it writes, the same part unless a
/// caller pairs a slot with another part's track. Slots sit in the array
/// `GpAnimCtx.slots` points at - `GameActor.field_438` is the array the actor's
/// own context is given - and the tick helpers recover that array as
/// `slot - slot->trackIndex`, so a slot following another part's track cannot
/// be ticked through a pointer alone.
///
/// Either keyframe may instead be a pose the caller supplies, kept per slot in
/// the context's pose buffer and marked by the 0x7FFF sentinel; `bufPose` says
/// one of the two is that kind.
///
/// Declared main-side because `GameActor` embeds the array by value; the actor
/// work blocks that carry the same slots reach the type through this
/// declaration.
typedef struct {
    /* 0x00 */ u16                curSet;      // set of the keyframe the slot has reached; 0x7FFF takes the pose from the context's pose buffer
    /* 0x02 */ u16                curRec;      // that keyframe's record index
    /* 0x04 */ u16                nextSet;     // set of the keyframe it is heading for; 0x7FFF as in `curSet`
    /* 0x06 */ u16                nextRec;     // that keyframe's record index
    /* 0x08 */ byte               pad_8;
    /* 0x09 */ u8                 rate;        // segment advance per tick in 16ths of a frame (0x10 one frame), read signed, so a negative rate runs the segment backwards
    /* 0x0A */ u8                 field_A;     // role unproven: written 0 by the walk, never read
    /* 0x0B */ u8                 poseKind;    // Track encoding cached from the initial keyframe's flags low nibble; selects `GpAnimSet.poseBanks`
    /* 0x0C */ s16                timeLeft;    // frames left in the segment, in 16ths; it runs past zero until the walk catches up
    /* 0x0E */ u16                timeSpan;    // Segment duration in sixteenths of a frame, from `AnimationRecord.durationFrames` or a requested blend; the blend denominator
    /* 0x10 */ u16                flags;       // bit 0 the walk took the clip's end, bit 1 it followed a control entry, bit 8 the clip has ended and settled on its last pose
    /* 0x12 */ u16                field_12;    // role unproven: written 0 by every initialiser, never read
    /* 0x14 */ u8                 mtxIndex;    // `GpAnimCtx.coords` entry the slot writes: the model part whose transform it drives
    /* 0x15 */ u8                 trackIndex;  // track the slot reads: the model part whose keyframes it follows
    /* 0x16 */ u8                 atEnd;       // the clip has run to its end: the slot holds its last pose and does not advance
    /* 0x17 */ u8                 bufPose;     // the pose came from the context's pose buffer rather than a pose bank
    /* 0x18 */ SVECTOR            bufRotDelta; // Euler angles of the rotation from the previous buffered pose to the current, applied while both ticks are buffered
    /* 0x20 */ struct GpAnimSet** sets;        // the animation set table `curSet` and `nextSet` index (the context's)
    /* 0x24 */ byte               pad_24[4];
} GpAnimSlot;
STATIC_ASSERT_SIZEOF(GpAnimSlot, 0x28);

/// Large object pointed to by Task::work for the slot-3 game object
/// (gameGetPtrSlot(3)). Sparse fields used by Display_SpawnFromMode.
typedef struct _GameActor {
    /* 0x000 */ s32                     field_0;  // per-frame X velocity (Gp_PlayerMode2State3)
    /* 0x004 */ s32                     field_4;  // per-frame Y velocity
    /* 0x008 */ s32                     field_8;  // per-frame Z velocity
    /* 0x00C */ byte                    pad_C[4];
    /* 0x010 */ s32                     field_10; // copy of GfxCoord.coord.t[0]
    /* 0x014 */ s32                     field_14; // copy of GfxCoord.coord.t[1]
    /* 0x018 */ s32                     field_18; // copy of GfxCoord.coord.t[2]
    /* 0x01C */ byte                    pad_1C[4];
    /* 0x020 */ s32                     field_20; // copied from Gp_SetActorDest arg2
    /* 0x024 */ s32                     field_24;
    /* 0x028 */ s32                     field_28;
    /* 0x02C */ byte                    pad_2C[4];
    /* 0x030 */ VECTOR                  field_30; // push-back dir (func_80109BB4); low halves go to a scratch SVECTOR
    /* 0x040 */ s32                     field_40;
    /* 0x044 */ s32                     field_44;
    /* 0x048 */ s32                     field_48;
    /* 0x04C */ byte                    pad_4C[4];
    /* 0x050 */ s16                     field_50; // SVECTOR.vx; func_80104D68 / RotMatrix
    /* 0x052 */ s16                     field_52; // facing angle (lh); func_8010BCF4 / func_80103E7C
    /* 0x054 */ s16                     field_54; // SVECTOR.vz; func_80104D68 / RotMatrix
    /* 0x056 */ byte                    pad_56[2];
    /* 0x058 */ s16                     field_58;
    /* 0x05A */ byte                    pad_5A[2];
    /* 0x05C */ s16                     field_5C; // pitch; Gp_AimPitchToLockAlt
    /* 0x05E */ byte                    pad_5E[2];
    /* 0x060 */ s16                     field_60;
    /* 0x062 */ byte                    pad_62[2];
    /* 0x064 */ s16                     field_64; // pitch; Gp_AimPitchToLockAlt
    /* 0x066 */ byte                    pad_66[2];
    /* 0x068 */ s16                     field_68;
    /* 0x06A */ s16                     field_6A;        // aim/look yaw offset; func_8010BE5C
    /* 0x06C */ byte                    pad_6C[4];
    /* 0x070 */ s16                     field_70;        // pitch-like angle; Gp_AimPitchRec
    /* 0x072 */ byte                    pad_72[6];
    /* 0x078 */ s16                     field_78;        // pitch; Gp_AimPitchDirect
    /* 0x07A */ byte                    pad_7A[6];
    /* 0x080 */ s16                     field_80;        // copied from func_80104F5C arg2
    /* 0x082 */ s16                     field_82;        // target facing angle; func_80104E00 / Gp_PlayerMode2State2
    /* 0x084 */ byte                    pad_84[4];
    /* 0x088 */ GpObjDirRec             field_88[3];     // `ctx.dir` of the `GpObj` nodes at `field_AC`, `field_CC` and `field_EC`
    /* 0x0AC */ byte                    field_AC[0x20];  // first of the five `GpObj` nodes at 0xAC..0x14C
    /* 0x0CC */ byte                    field_CC[0x20];  // `GpObj` node
    /* 0x0EC */ byte                    field_EC[0x20];  // `GpObj` node
    /* 0x10C */ byte                    field_10C[0x18]; // `GpObj` node; `field_124` is its key
    /* 0x124 */ u32                     field_124;
    /* 0x128 */ byte                    pad_128[2];
    /* 0x12A */ u16                     field_12A;       // that node's flags
    /* 0x12C */ byte                    field_12C[0x20];
    /* 0x14C */ GpActorD4Rec            field_14C;       // the actor's own collision shape; the weapon attach re-arms it
    /* 0x164 */ byte                    pad_164[0x18];
    /* 0x17C */ WorldCollisionContact   field_17C[18];   // Gp_ClearRec18Occupied / func_801041B4
    /* 0x32C */ WorldCollisionContact   field_32C[6];    // Gp_AttachActorObj / Gp_InitRec18Table
    /* 0x3BC */ WorldCollisionContact   aimContacts[1];  // Single result for the aiming capsule
    /* 0x3D4 */ GfxCoord                field_3D4;       // copy of the attached model's root coordinate; the frame of the body at `field_10C`
    /* 0x424 */ byte                    field_424[0x14]; // GpAnimCtx overlay; Gp_AnimTickIndex
    /* 0x438 */ GpAnimSlot              field_438[19];   // the actor's animation slots, the array its `GpAnimCtx` walks
    /* 0x730 */ byte                    pad_730[0x10];
    /* 0x740 */ byte                    pad_740[0x68];
    /* 0x7A8 */ byte                    field_7A8; // addr taken as func_800B3F84 arg3
    /* 0x7A9 */ byte                    pad_7A9[0x163];
    /* 0x90C */ struct GpLinkNode*      field_90C;
    /* 0x910 */ struct GpActorD4*       field_910;
    /* 0x914 */ struct Task*            field_914;
    /* 0x918 */ struct Task*            field_918;
    /* 0x91C */ struct Task*            field_91C;
    /* 0x920 */ struct Task*            field_920;
    /* 0x924 */ struct Task*            field_924;
    struct GpAnimSet**                  animationSets; // Borrowed set table used by the player's or companion's animation slots
    /* 0x92C */ struct AnimationRecord* field_92C;     // last Gp_AnimGetRec result (Gp_PlayerNormalState5)
    /* 0x930 */ s32                     field_930;     // sw from Gp_MsgPlayerDirFacing; addr taken by func_801011D0
    /* 0x934 */ s32                     field_934;
    /* 0x938 */ s16                     field_938;     // number of animation slots the actor walks (init 0x13)
    /* 0x93A */ u16                     field_93A;     // Gp_WeaponIdBase[field_22-1] + field_21
    /* 0x93C */ u16                     field_93C;
    /* 0x93E */ s16                     field_93E;
    /* 0x940 */ s16                     field_940;
    /* 0x942 */ s16                     field_942;
    /* 0x944 */ s16                     field_944;
    /* 0x946 */ s16                     field_946;
    /* 0x948 */ s16                     field_948;
    /* 0x94A */ s16                     field_94A;
    /* 0x94C */ s16                     field_94C;
    /* 0x94E */ s16                     field_94E;
    /* 0x950 */ s16                     field_950;
    /* 0x952 */ s16                     field_952;
    /* 0x954 */ u16                     field_954;
    /* 0x956 */ u16                     field_956;
    /* 0x958 */ s16                     field_958;
    /* 0x95A */ u16                     field_95A;
    /* 0x95C */ u16                     field_95C;
    /* 0x95E */ u16                     field_95E;
    /* 0x960 */ u16                     field_960;
    /* 0x962 */ u16                     field_962;
    /* 0x964 */ u16                     field_964; // previous field_962
    /* 0x966 */ u16                     field_966;
    /* 0x968 */ u16                     field_968; // released buttons: field_964 & ~field_962
    /* 0x96A */ u16                     field_96A; // set to 0xF89A by func_8010615C
    /* 0x96C */ s16                     field_96C;
    /* 0x96E */ s16                     field_96E;
    /* 0x970 */ s16                     field_970;
    /* 0x972 */ u8                      field_972;
    /* 0x973 */ s8                      field_973;
    /* 0x974 */ s8                      field_974;
    /* 0x975 */ s8                      field_975;
    /* 0x976 */ s8                      field_976;
    /* 0x977 */ s8                      field_977;
    /* 0x978 */ s8                      field_978;
    /* 0x979 */ s8                      field_979; // countdown; `func_mongoose_8011D1D8` loads 0xB
    /* 0x97A */ u8                      field_97A;
    /* 0x97B */ s8                      field_97B;
    /* 0x97C */ s8                      field_97C;
    /* 0x97D */ u8                      field_97D;
    /* 0x97E */ s8                      field_97E;
    /* 0x97F */ s8                      field_97F;
    /* 0x980 */ byte                    pad_980;
    /* 0x981 */ s8                      field_981;
    /* 0x982 */ s8                      field_982;
    /* 0x983 */ u8                      field_983;
    /* 0x984 */ u8                      field_984;
    /* 0x985 */ u8                      field_985;
    /* 0x986 */ s8                      field_986;
    /* 0x987 */ u8                      field_987; // texture upload seq A (func_801030CC / D_80112E74)
    /* 0x988 */ u8                      field_988; // field_987 delay; reload 4 after each upload
    /* 0x989 */ u8                      field_989; // field_987 frame index
    /* 0x98A */ u8                      field_98A; // texture upload seq B (func_801030CC / D_80112EB4)
    /* 0x98B */ u8                      field_98B; // field_98A delay; reload 8 after each upload
    /* 0x98C */ u8                      field_98C; // field_98A frame index
    /* 0x98D */ u8                      field_98D;
    /* 0x98E */ u8                      field_98E;
    /* 0x98F */ s8                      field_98F; // cleared by Gp_SpawnWeaponEff
    /* 0x990 */ u8                      field_990;
    /* 0x991 */ s8                      field_991; // func_80109374 requires 0 to write field_97D = 1
    /* 0x992 */ u8                      field_992; // Gp_PlayerWorkState1: func_801011D0 result when field_984 & 1
    /* 0x993 */ u8                      field_993;
} GameActor;
STATIC_ASSERT_SIZEOF(GameActor, 0x994);

#endif // MAIN_SESSION_TYPES_H
