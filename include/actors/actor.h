#ifndef INCLUDE_ACTORS_ACTOR_H
#define INCLUDE_ACTORS_ACTOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/// Scratch-stack block of an actor's push out of its world contacts.
///
/// A push reserves one block, has its contact records resolved into `delta`,
/// moves the actor's coordinate frame by the whole units of that correction
/// and releases the block before returning. X and Z then take one more unit
/// away from zero wherever the correction leaves a fraction, rounding the
/// applied step outward. Nothing clears the block when it is reserved;
/// `moved` is set explicitly and `delta` is the resolver's to write. The
/// pushes that report `moved` read it after the release, while the bytes are
/// still intact.
typedef struct {
    WorldCollisionDelta delta; // Correction resolved from the contact records, in signed 16.16 units
    s32                 moved; // 1 when the X or Z correction is nonzero, so the push displaced the actor horizontally; 0 otherwise
} ActorContactPushScratch;
STATIC_ASSERT_SIZEOF(ActorContactPushScratch, 0x14);

/// Scratch-stack block of an actor's length-limited push out of its world
/// contacts.
///
/// A push reserves one block, has its contact records resolved into `delta`
/// and applies the whole units of that correction as `step`: Y first, limited
/// in size by the package, in some only in a room that has an
/// `ActorHeightClamp` row, then X and Z, shortened to the package's limit
/// when their length reaches it. X and Z then take one more unit away from
/// zero wherever the correction leaves a fraction. The block is released
/// before the push returns `moved`, which it reads while the bytes are still
/// intact. Nothing clears the block when it is reserved; `moved` is set
/// explicitly and `delta` is the resolver's to write.
typedef struct {
    WorldCollisionDelta delta;      // Correction resolved from the contact records, in signed 16.16 units
    SVECTOR             step;       // Whole units of `delta`, then the limited step added to the coordinate; `vy` is zeroed before X and Z are shortened, `pad` is never written
    s32                 stepLength; // Length of `step` across X and Z, world units
    s32                 moved;      // 1 when the X or Z correction is nonzero, so the push displaced the actor horizontally; 0 otherwise
} ActorContactCappedPushScratch;
STATIC_ASSERT_SIZEOF(ActorContactCappedPushScratch, 0x20);

/// Scratch-stack block of one limb shadow: a subtractive textured quad laid
/// under the segment between two parts of an actor's model.
///
/// Everything up to the projection is in world space, the frame under the
/// view coordinate. The quad lies in one plane: most drawers put it on the
/// floor, giving every point the shadow's height as `vy`, while a drawer
/// whose shadow falls on a wall takes X and Y from the parts and gives every
/// point the wall's depth as `vz`. `corners` are in GPU quad strip order: 0
/// and 1 either side of the first part, 2 and 3 either side of the second,
/// each pair pushed outwards along the segment by half its length, so the
/// quad is twice as long as the segment. `screenCorners`, `depthCue` and
/// `flag` are `RotTransPers4`'s outputs for those four corners.
///
/// A drawer reserves one block for a segment and releases it once the quad is
/// queued; nothing in it outlives the call.
typedef struct {
    MATRIX  firstMatrix;      // First part's transform relative to the view coordinate; only its translation is read
    MATRIX  secondMatrix;     // Second part's transform relative to the view coordinate; only its translation is read
    SVECTOR firstPos;         // First part's position, with the plane's coordinate in place of its own; `pad` is never written
    SVECTOR secondPos;        // Second part's position, with the plane's coordinate in place of its own; `pad` is never written
    SVECTOR corners[4];       // The quad's corners; `pad` is never written
    long    screenCorners[4]; // Projected corners: screen X in bits 0..15, Y in bits 16..31, copied whole into the primitive
    long    depthCue;         // Depth-cueing interpolation value of the projection; never read
    long    flag;             // GTE FLAG word of the projection; a set bit 31 drops the quad
    s32     depth;            // Last corner's screen Z / 4, which picks the ordering-table entry
} ActorLimbShadowScratch;
STATIC_ASSERT_SIZEOF(ActorLimbShadowScratch, 0x8C);

/// The scratch-stack block of a model rescale, which multiplies a per-axis
/// scale into a coordinate's rotation.
///
/// One block serves one rescale: it is reserved, an identity rotation is
/// written into it and scaled, and it is released once that has been
/// multiplied into the coordinate. The multiply changes the coordinate's
/// rotation only. A routine that rescales every frame first sets the
/// coordinate from an unscaled matrix, so the scale does not compound.
typedef struct {
    GfxMatrix matrix; // Identity rotation, written word-wise, then scaled in place; its translation is never set or read
    VECTOR    scale;  // Factor for each axis, 4096 = 1.0
} ActorScaleScratch;
STATIC_ASSERT_SIZEOF(ActorScaleScratch, 0x30);

/// The scratch-stack block a spawned child's root coordinate is placed with.
///
/// A projectile's spawn state starts its root as the transform of the
/// coordinate it is launched from, moves it by an offset along that
/// transform's axes, and then turns it by a rotation applied in its own frame.
/// One block serves one placement and is released by the spawn state.
typedef struct {
    SVECTOR operand;  // short vector being worked on: the launch offset along the root's axes, then the Euler angles of the turn (0x1000 to a full turn)
    VECTOR  offset;   // `operand` carried through the root's rotation, added to its translation; once that is done a spawn state may reuse it for an offset of its own
    MATRIX  rotation; // the turn built from `operand`, multiplied onto the root's rotation a column at a time; its translation is never set or read
} ActorChildPlaceScratch;
STATIC_ASSERT_SIZEOF(ActorChildPlaceScratch, 0x38);

/// The scratch-stack block of a turn multiplied onto a rotation: the Euler
/// angles of the turn and the rotation matrix built from them.
///
/// One block serves one turn. The angles are written, `RotMatrix` builds
/// `rotation` from them, and that is multiplied onto the rotation being turned
/// a column at a time, which turns it in its own frame. The rotation turned is
/// a model's root coordinate or a matrix the owner keeps in its work block.
typedef struct {
    SVECTOR angles;   // Euler angles of the turn, 4096 per turn; `pad` is never written. A routine that spawns an effect once the turn is done borrows it for the effect's offset from its parent coordinate
    MATRIX  rotation; // The turn as a rotation matrix; its translation is never set or read
} ActorEulerTurnScratch;
STATIC_ASSERT_SIZEOF(ActorEulerTurnScratch, 0x28);

/// Scratch-stack block of a uniform matrix scale that takes the translation
/// along: the rotation is scaled by `ScaleMatrix`, the translation on the GTE.
///
/// The matrix scaled this way is a model's light-colour matrix, which is
/// rebuilt every frame: an enemy that is appearing scales it by a factor
/// that rises from 0 frame by frame, so the model starts black and
/// brightens. One block serves one scale; the routine releases it before it
/// returns.
typedef struct {
    VECTOR  scale;       // The factor on each axis, 4096 = 1.0, handed to `ScaleMatrix`; `pad` is never written
    SVECTOR translation; // Low 16 bits of the matrix's translation, multiplied by the factor in place and written back; `pad` is never written
} ActorScaleMatrixScratch;
STATIC_ASSERT_SIZEOF(ActorScaleMatrixScratch, 0x18);

/// The scratch-stack block of an enemy routine that aims at a target or turns
/// the model: the ground offset to the target, and the short vector the
/// routine hands on by address.
///
/// A routine reserves one block on entry whether it needs one member or both.
/// The target is the player or a waypoint of the enemy's own; its bearing is
/// `ratan2(delta.vx, delta.vz)`, taken from the low 16 bits of each component,
/// and its distance the square root of their squares. Nothing carries over
/// from one call to the next.
typedef struct {
    VECTOR  delta; // Target position minus the actor's, world units, with `vy` set to zero; `pad` is never written
    SVECTOR rot;   // Euler angles a coordinate's rotation is rebuilt from, 4096 per turn; routines that spawn an effect instead borrow it for the effect's offset from its parent coordinate
} ActorFaceScratch;
STATIC_ASSERT_SIZEOF(ActorFaceScratch, 0x18);

/// The scratch-stack block of a yaw rebuild, which replaces a coordinate's
/// rotation with a turn about Y through the heading it already has, scaled
/// along each axis.
///
/// One block serves one rebuild: it is reserved, the rotation is built and
/// scaled in it, and it is released once the nine rotation terms have been
/// copied to the coordinate. Whatever pitch and roll the coordinate had are
/// discarded, and its translation is left alone.
typedef struct {
    MATRIX rotation; // Turn about Y by `yaw`, then scaled in place; its translation is never set or read
    VECTOR scale;    // Factor for each axis, 4096 = 1.0
    s16    yaw;      // Heading read from the coordinate's Z axis before the rebuild, 4096 units per turn
} ActorScaleRotScratch;
STATIC_ASSERT_SIZEOF(ActorScaleRotScratch, 0x34);

/// The scratch-stack block of a state body that turns an actor a limited step
/// toward a target each frame: the offset to the target and the yaw worked out
/// from it.
///
/// One block serves one frame of one state, and nothing carries over to the
/// next. The target is a waypoint, the place the actor spawned or the player.
/// A body that goes on to watch for the player fills both members again with
/// the player's offset and its bearing off the actor's heading, and a body may
/// take the block for that test alone. Yaws are 4096 units per turn, and a
/// wrapped one lies in [-0x800, 0x800].
typedef struct {
    SVECTOR delta;          // Target's position minus the actor's, world units; `vy` is zero unless the target is the player, and only `vx` and `vz` give the bearing and the range. A body that steers round obstacles lends it to the steer, which leaves the displacement it moved the actor by; nothing reads that back
    s16     angle;          // Wrapped turn from the actor's heading to the target, then that turn limited to the body's step, then the limited turn added to the heading: the yaw the actor's rotation is rebuilt around
    byte    unknown_A[0x2]; // Reserved with the block and never accessed; role unproven
} ActorTurnScratch;
STATIC_ASSERT_SIZEOF(ActorTurnScratch, 0xC);

/// The scratch-stack block of a state body that steers an actor by where the
/// player is: the offset to the player and the yaws worked out from it.
///
/// One block serves one frame of one state. Every body fills `delta` and
/// `turn`. Only a body that also weighs which way the player is looking fills
/// `playerYaw` and `yawFromPlayer`, and only one that keeps the turn to the
/// player apart from the heading it sets uses `heading`; the words a body does
/// not use are left as the scratch stack had them. Yaws are 4096 units per
/// turn, and a wrapped one lies in [-0x800, 0x800].
typedef struct {
    SVECTOR delta;         // Player's position minus the actor's, world units; a sidestep then overwrites it with the displacement it moves the actor by that frame
    s16     playerYaw;     // Heading the player faces
    s16     yawFromPlayer; // Bearing from the player to the actor, wrapped: the reverse of `delta`'s. Next to `playerYaw` when the player faces the actor, more than a quarter turn off it when the actor is behind
    s16     turn;          // Wrapped turn from the actor's heading to the player. A body turning at the player limits it and adds the actor's heading, or replaces it with that heading outright, which leaves the heading it sets; a sidestep works out the heading it moves along here instead
    s16     heading;       // Heading the actor is given where `turn` has to survive: a limited turn that keeps the player off to one side, added to the actor's heading, or the heading a turn-around starts from
} ActorChaseScratch;
STATIC_ASSERT_SIZEOF(ActorChaseScratch, 0x10);

/// Scratch-stack block of an enemy's hit intake, which runs every frame the
/// enemy has health left.
///
/// The intake looks for a damaging contact, kind 0x20000, in the enemy's
/// contact tables. When it finds one it works out where the hit landed
/// relative to the facing, spawns the hit effect, knocks the root back from
/// the hit unless the enemy is already down, and rolls the damage for the
/// player's range, scaled by a critical roll and by a hit from behind. The
/// reaction is picked from the attack's kind and `hitYaw`. The
/// damage-over-time tick that follows reuses `damage` alone. Nothing carries
/// over from one frame to the next. Angles are 4096ths of a turn.
typedef struct {
    MATRIX  towardHit;      // Root's local matrix turned about its own Y by `hitYaw`, whose Z axis the knockback runs along; set only when the hit knocks the enemy back
    VECTOR  toPlayer;       // Player's position minus the root's; never read back, and `pad` is never written
    SVECTOR hitOffset;      // `hitPos` minus the root's composed translation; `vx` and `vz` give the hit's bearing. A knockback then replaces it with the step it adds to the root, against `towardHit`'s Z axis; `pad` is never written
    SVECTOR hitPos;         // Point of the contact found, replaced by the player's position when `hitKey` has bit 15 set; `pad` is never written
    s32     hitKey;         // Key of the contact found: the kind over the attack's packed id; 0 when no table holds a damaging contact
    s32     damage;         // Damage of the hit: the roll for the range, quadrupled by a critical roll and doubled by a hit from behind; then the damage of the over-time tick
    s32     playerDistance; // Length of `toPlayer`, the range the damage is rolled for
    s16     hitYaw;         // Bearing of `hitOffset` off the enemy's facing, wrapped to [-0x800, 0x800]
    s16     critical;       // 1 when the critical roll succeeded, else 0; a critical hit staggers the enemy whatever the damage
    s16     criticalEffect; // Spawn argument of the critical-hit effect (-1 none spawned, 0 a critical roll, 4 a doubled hit that dealt damage)
} ActorHitScratch;
STATIC_ASSERT_SIZEOF(ActorHitScratch, 0x54);

/// Value an `ActorBodyPushScratch::marks` slot takes at the record that ends
/// the walk early.
enum { ACTOR_BODY_PUSH_MARK_END = 0x7FFE };

/// Scratch-stack block of an actor's push away from the bodies it touches.
///
/// A push reserves one block and walks the actor's contact table up to its
/// capacity or the first record with no key. Each record whose key is of a
/// player character's kind (0x10000) or an enemy's (0x30000) yields an offset
/// away from that body, shortened to the package's limit when its XZ length
/// reaches it, and a fraction of that offset is added to the actor's root.
/// The block is released before the push returns `hit`, which it reads while
/// the bytes are still intact. Nothing clears the block when it is reserved.
typedef struct {
    SVECTOR offset;       // Offset away from the current record's body, in the view coordinate's frame; `vy` is zeroed before it is shortened
    SVECTOR position;     // World position of the actor's second coordinate, which the offsets are measured from
    s32     kind;         // Kind bits of the current record's key
    s32     offsetLength; // Length of `offset` across X and Z, world units
    s16     recordIndex;  // Contact record the walk is on
    s16     hit;          // 1 once a record that counts was met, 0 otherwise; a package decides which kinds count
    s16     marks[12];    // One slot per contact record. Only the slot of a keyless record is written, with `ACTOR_BODY_PUSH_MARK_END`, and nothing reads any; role otherwise unproven
} ActorBodyPushScratch;
STATIC_ASSERT_SIZEOF(ActorBodyPushScratch, 0x34);

/// Scratch-stack block of an enemy's per-frame update, in which the world
/// position of one of its model parts is worked out.
///
/// The update reserves one block before it runs the enemy's state. Afterwards
/// it zeroes `position` and carries it from the part's frame up the parent
/// chain to the view coordinate, which leaves the part's origin in world
/// space. That is where the enemy's body is that frame: the update copies it
/// out as the centre of the hit sphere, as the newest entry of the history
/// of body positions and as the position the enemy record publishes. An
/// update that releases the block first still reads `position` afterwards,
/// while the bytes are intact.
typedef struct {
    byte    unknown_0[0x10]; // Reserved with the block and never accessed; role unproven
    SVECTOR position;        // Zeroed, then carried from the part's frame into the view coordinate's: the part's origin, world units; `pad` is never written
} ActorPartPositionScratch;
STATIC_ASSERT_SIZEOF(ActorPartPositionScratch, 0x18);

/// Scratch-stack block of the projection that finds how deep one of an
/// actor's coordinates lies in the ordering table.
///
/// The projection reserves one block, sends the coordinate's own origin
/// through the coordinate's composed matrix with one perspective transform and
/// stores the four results. Only the flag word and the depth are used: an
/// enemy that samples the scene behind it queues its frame capture at the
/// depth left in `otz`, a caller's bias behind the coordinate. The block is
/// released before the projection returns.
typedef struct {
    SVECTOR origin;    // Point projected: zero on all three axes, the coordinate's own origin; `pad` is never written
    s32     screenPos; // Projected screen position, X in bits 0..15 and Y in bits 16..31; stored, never read
    s32     depthCue;  // Depth-cueing interpolation value of the projection; stored, never read
    s32     flag;      // GTE FLAG word of the projection; a set bit 31 marks a failed projection, which zeroes `otz`
    s32     otz;       // Origin's screen Z / 4 as stored, then a sixteenth of that plus the caller's bias: the ordering-table depth the capture is queued at
} ActorOriginDepthScratch;
STATIC_ASSERT_SIZEOF(ActorOriginDepthScratch, 0x18);

/// Scratch-stack block of a camera-facing sprite: one textured quad built in
/// screen space around the projected root of an actor.
///
/// The drawer stages the root's world position in `corners[0]`, projects it
/// with one perspective transform and drops the sprite when `otz` is under
/// 20. It then lays the four corners out around the projected centre, a size
/// divided by `otz` away from it on each axis, and copies them into the
/// primitive. One block serves one sprite and is released before the drawer
/// returns.
typedef struct {
    SVECTOR corners[4];   // [0] first holds the root's world position, narrowed to s16, and in a drawer that rolls the sprite the roll's angles; then the quad's corners in screen pixels: top left, top right, bottom left, bottom right. A rolled sprite's are offsets from the centre until each is turned and has the centre added
    s32     screenCentre; // Projected root: screen X in bits 0..15, Y in bits 16..31
    s32     otz;          // Root's screen Z / 4: the divisor of the sprite's size, and the ordering-table depth of the quad
} ActorScreenQuadScratch;
STATIC_ASSERT_SIZEOF(ActorScreenQuadScratch, 0x28);

/// Scratch-stack block of a contact step that keeps a single vector: the
/// correction resolved from an actor's contact records.
///
/// A step reserves one block, has the push-back of the room's collision grid
/// resolved from the records into `delta`, and adds the whole units of that
/// correction to the actor's root. A step that goes on to take hits reuses
/// `delta` for the offset from the root to the player, whose length is the
/// range the hit's damage is worked out for. The block is released before
/// the step returns, and nothing in it carries over to the next frame.
///
/// No step touches the bytes either side of `delta`, so the size is that of
/// the reservation alone and what else the block was laid out to hold is
/// unproven.
typedef struct {
    byte                unknown_0[0x20]; // Reserved with the block and never accessed; role unproven
    WorldCollisionDelta delta;           // Correction resolved from the contact records, in signed 16.16 units; then the player's position minus the root's, in whole world units
    byte                unknown_30[0x8]; // Reserved with the block and never accessed; role unproven
} ActorContactDeltaScratch;
STATIC_ASSERT_SIZEOF(ActorContactDeltaScratch, 0x38);

/// Scratch-stack block of a contact step that keeps a single vector, in the
/// larger of the two reservations such a step makes.
///
/// It is used exactly as `ActorContactDeltaScratch` is: the step has the
/// push-back of the room's collision grid resolved from its contact records
/// into `delta`, adds the whole units of that correction to the actor's root,
/// and a step that goes on to take hits reuses `delta` for the offset from
/// the root to the player. The block is released before the step returns.
///
/// The two blocks differ only in how many bytes follow `delta`, none of
/// which a step touches, so what the longer tail was laid out to hold is
/// unproven.
typedef struct {
    byte                unknown_0[0x20];  // Reserved with the block and never accessed; role unproven
    WorldCollisionDelta delta;            // Correction resolved from the contact records, in signed 16.16 units; then the player's position minus the root's, in whole world units, on X and Z or on all three axes as the step measures its range
    byte                unknown_30[0x18]; // Reserved with the block and never accessed; role unproven
} ActorContactDeltaWideScratch;
STATIC_ASSERT_SIZEOF(ActorContactDeltaWideScratch, 0x48);

/// Scratch-stack block of an enemy's contact pass, which ends by pushing the
/// enemy out of the body it overlaps most.
///
/// The pass is the one `ActorOverlapPushScratch` serves, in a block that
/// opens as `ActorContactDeltaScratch` does. It has the push-back of the
/// room's collision grid resolved from the grid contacts into `delta` and
/// adds the whole units of that correction to the root, then walks the body's
/// contact records, reusing `delta` for each. A damaging contact, kind
/// 0x20000, takes the offset to the attacking player, whose length is the
/// range the damage is worked out for. A contact with a body the package
/// yields to -- a player's, kind 0x10000, or an enemy's, kind 0x30000 --
/// takes the offset from that body's centre; the overlap is the record's
/// summed radii less that offset's length. The deepest overlap leaves its
/// direction in `normal` and `pushDirection`, and once the walk is over the
/// root is moved that deep along `pushDirection` on X and Z. The block is
/// released before the pass returns.
///
/// No pass touches the leading bytes or the halfword after each of the last
/// two members, so what they were laid out to hold is unproven.
typedef struct {
    byte                unknown_0[0x20]; // Reserved with the block and never accessed; role unproven
    WorldCollisionDelta delta;           // Correction resolved from the grid contacts, in signed 16.16 units; then, in whole world units, the offset to the attacker or from the centre of the body being tested
    VECTOR              normal;          // `delta` of the deepest overlap met so far, normalised: away from that body, 4096 = 1.0
    VECTOR              pushDirection;   // `normal` turned into the frame of the collision grid's coordinate; the root is pushed along its X and Z
    s16                 gridNormalX;     // X of the first grid contact's push-back direction, 4096 = 1.0; staged by a pass that turns its enemy to face the wall it met, whose yaw is the reverse of this direction's bearing
    byte                unknown_52[0x2]; // Reserved with the block and never accessed; role unproven
    s16                 gridNormalZ;     // Z of the same direction
    byte                unknown_56[0x2]; // Reserved with the block and never accessed; role unproven
} ActorContactOverlapPushScratch;
STATIC_ASSERT_SIZEOF(ActorContactOverlapPushScratch, 0x58);

/// Scratch-stack block of a small enemy's contact pass, which ends by pushing
/// the enemy out of the body it overlaps most.
///
/// The pass reserves one block, has the push-back of the room's collision
/// grid resolved from the grid sphere's contact records into `delta`, and
/// adds the whole units of that correction to the root. It then walks the
/// hit sphere's records, reusing `delta` for each. A damaging contact, kind
/// 0x20000, takes the offset to the attacking player, whose length is the
/// range the damage is worked out for. A contact with a player's body, kind
/// 0x10000, or an enemy's, kind 0x30000, takes the offset from that body's
/// centre; the overlap is the record's summed radii less that offset's
/// length. The deepest overlap leaves its direction in `normal` and
/// `pushDirection`, and once the walk is over the root is moved that deep
/// along `pushDirection` on X and Z. The block is released before the pass
/// returns.
typedef struct {
    WorldCollisionDelta delta;         // Correction resolved from the grid contacts, in signed 16.16 units; then, in whole world units, the offset to the attacker or from the centre of the body being tested
    VECTOR              normal;        // `delta` of the deepest overlap met so far, normalised: away from that body, 4096 = 1.0
    VECTOR              pushDirection; // `normal` turned into the frame of the collision grid's coordinate; the root is pushed along its X and Z
} ActorOverlapPushScratch;
STATIC_ASSERT_SIZEOF(ActorOverlapPushScratch, 0x30);

/// Scratch-stack block of an enemy's knockback of the player, a scripted
/// sequence the enemy's task steps once a tick.
///
/// Each tick of the sequence reserves one block and releases it before
/// returning; what has to last from tick to tick is kept in the enemy's work
/// block. The first tick decides whether the player is struck from behind,
/// starts the matching animation of the package's own set on the player and
/// spawns the hit effect. For sixteen ticks after that the player is placed a
/// step further from the enemy, turned to face it or away from it as first
/// decided, and a second animation follows. The player's task reads
/// `playerAnim` and `playerPlacement` while the message that carries each is
/// dispatched, and keeps neither address.
typedef struct {
    AnimationPlayRequest playerAnim;      // Animation of the package's player set the player's task is told to install and play
    ActorTransform       playerPlacement; // Where `GAME_ACTOR_MESSAGE_PLACE` puts the player that tick: 100 units further along `pushDirection` on X and Z, at height 0, with yaw toward the enemy or away from it
    VECTOR               toPlayer;        // Player's position minus the enemy's root, world units; `vy` is zero on the tick that tests which side the player is struck from, and `pad` is never written
    SVECTOR              pushDirection;   // `toPlayer` normalised, 4096 = 1.0: the way the player is pushed. The first tick borrows it for the hit effect's offset from the player's coordinate instead; `pad` is never written
} ActorPlayerKnockbackScratch;
STATIC_ASSERT_SIZEOF(ActorPlayerKnockbackScratch, 0x44);

/// Scratch-stack block of an enemy's hold on the player, which the player
/// breaks by pressing buttons.
///
/// The enemy fills `buttonPressHold` and sends it with
/// `GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES`; a player that accepts the hold
/// answers 0. The enemy then fills `playerAnim` with an animation of the
/// package's own player set and sends it with
/// `ANIMATION_MESSAGE_REPLACE_AND_PLAY`. A block is reserved for one tick and
/// released before the routine returns, so an enemy that changes the held
/// player's animation on a later tick fills `playerAnim` again. The player's
/// task reads each member while its message is dispatched and keeps neither
/// address.
typedef struct {
    GameActorButtonPressHold buttonPressHold; // Payload of the hold request; only `pressCount`, the presses that break the hold, is written
    AnimationPlayRequest     playerAnim;      // Animation of the package's player set the held player is told to install and play
} ActorPlayerHoldScratch;
STATIC_ASSERT_SIZEOF(ActorPlayerHoldScratch, 0x2C);

/// Scratch-stack block of a projectile placed at an offset from the enemy
/// that fires it.
///
/// The projectile's spawn handler reserves one block, fills `offset` with the
/// package's fixed offset in the frame of the parent's root part and has it
/// turned by that part's own rotation into `rotated`. The projectile's root
/// then takes the parent root's matrix, with `rotated` added to its
/// translation, and hangs off the view coordinate. Nothing reads the block
/// after that. The handler releases it as it ends, except where the
/// projectile's work block cannot be allocated: that path returns with the
/// block still reserved.
typedef struct {
    SVECTOR offset;  // Where the projectile starts, in the frame of the parent's root part; `pad` is never written
    VECTOR  rotated; // `offset` turned by the parent root's local rotation, with no translation applied: what is added to the parent root's translation. `pad` is never written
} ActorOffsetScratch;
STATIC_ASSERT_SIZEOF(ActorOffsetScratch, 0x18);

/// Scratch-stack block of the hit check of an enemy that turns toward what
/// hit it.
///
/// The check reserves one block a frame and looks through the enemy's hit
/// contacts for the first damaging one, kind 0x20000, stopping at a record
/// with no key. When it finds one it rolls the damage, works out which way
/// the hit lies from the model's facing, hands that turn and the key to the
/// package's reaction and takes the damage off the enemy's health. The block
/// is released before the check returns. Angles are 4096ths of a turn.
typedef struct {
    SVECTOR hitOffset; // `hitPos` minus the root's composed translation; `vx` and `vz` give the hit's bearing. The root's translation is staged here first; `pad` is never written
    SVECTOR hitPos;    // Point of the contact found; `pad` is never written
    s32     hitKey;    // Key of the contact found: the kind over the attack's packed id; 0 when no record holds a damaging contact
    u16     damage;    // Damage rolled for the attack at range 0
    s16     hitYaw;    // Bearing of `hitOffset` off the model's facing, wrapped to [-0x800, 0x800]
} ActorHitTakenScratch;
STATIC_ASSERT_SIZEOF(ActorHitTakenScratch, 0x18);

/// The scratch-pad block the bearing of one coordinate from another is
/// worked out in.
///
/// The bearing is the yaw of the measured coordinate as the reference
/// coordinate sees it, so both members describe the reference: the offset to
/// the measured point, and the rotation that carries that offset out of world
/// axes into the reference's own. The block is reserved either on its own or
/// as the top of an `ActorRangeBearingScratch`, and holds nothing a caller
/// needs once the angle is taken; a caller that also wants a range reuses
/// `delta` for it.
typedef struct {
    SVECTOR delta;           // The measured point's offset from the reference, each component cut to 16 bits: along world axes as stored, along the reference's own once rotated in place. `pad` is never written
    byte    unknown_8[0x18]; // Reserved with the block and never accessed; role unproven
    MATRIX  inverseRotation; // Transpose of the reference's world rotation, 4.12 fixed point; the translation is never written or read
} ActorBearingScratch;
STATIC_ASSERT_SIZEOF(ActorBearingScratch, 0x40);

/// The scratch-pad block of a target measured from a coordinate, in range and
/// in bearing.
///
/// The range is the length of `offset` over X and Z; its Y is stored and not
/// read. No user touches the bytes before `offset` or between it and
/// `bearing`, so what they hold is unproven.
typedef struct {
    byte                pad_0[0x20];
    VECTOR              offset;  // The target's position less the coordinate's, in the space both local transforms map into
    byte                pad_30[0xC];
    ActorBearingScratch bearing; // Where the target's bearing in the coordinate's own frame is worked out
} ActorRangeBearingScratch;
STATIC_ASSERT_SIZEOF(ActorRangeBearingScratch, 0x7C);

/// Work block of a task that draws one model with lighting of its own and
/// does nothing else with it.
///
/// The task allocates the block on its first tick, clears it and keeps it at
/// `Task::work`. The block supplies the two matrices the task's model does not
/// own, which the room's lights are worked into each tick at the model's
/// position, and remembers the task it was spawned for.
typedef struct {
    MATRIX light;  // Light matrix the model's `TmdObject::lightMtx` points at
    MATRIX color;  // Colour matrix the model's `TmdObject::colorMtx` points at
    Task*  parent; // Task this one was spawned for, taken from `Task::spawnArg2`, and made a child of
} ActorLitWork;
STATIC_ASSERT_SIZEOF(ActorLitWork, 0x44);

/// Ends an `ActorZone` table, in the `id` of its last entry.
enum { ACTOR_ZONE_END = -1 };

/// One entry of a zone table: a rectangle of a room's floor and the id a
/// lookup returns for a point inside it.
///
/// A lookup takes the first entry containing the point, both far edges
/// included, and answers 0 when none does, so 0 is never a zone's id. An
/// enemy looks up its own root and the player's to tell which part of the
/// room each stands in. Coordinates are world units.
typedef struct {
    s16 x;     // Near corner along world X
    s16 z;     // Near corner along world Z
    s16 width; // Extent along X
    s16 depth; // Extent along Z
    s16 id;    // Value returned for a point inside, or `ACTOR_ZONE_END`
} ActorZone;
STATIC_ASSERT_SIZEOF(ActorZone, 0xA);

/// One tuning of a Stranger: the four values the enemy keeps in its work
/// block from the moment it is set up.
///
/// A Stranger package defines three tunings, and the low four bits of the
/// spawn argument pick one: 2 the first, 1 the third, anything else the
/// second. Each member is copied to the member of the work block that has
/// the same role, and the enemy reads only that copy afterwards.
typedef struct {
    s16  downFramesBase; // Ticks the downed state lasts, before a random few more
    s16  sidestepAngle;  // Angle between the bearing to the player and the direction a sidestep moves in, 4096 units per turn
    s16  sidestepDelay;  // Ticks a chase must have run before it sidesteps, raised by half the sidesteps made since the last hit or grab
    s16  noticeRadius;   // Distance at which a dormant or patrolling Stranger notices the player, world units
    byte unknown_8[0x4]; // Never read, and zero in every table; role unproven
} ActorStrangerVariant;
STATIC_ASSERT_SIZEOF(ActorStrangerVariant, 0xC);

/// One tuning of a Horned Stranger: the values the enemy takes into its work
/// block as it is set up.
///
/// A Horned Stranger package defines three tunings and picks among them by the
/// low four bits of the spawn argument, as a Stranger picks an
/// `ActorStrangerVariant`: 2 the first, 1 the third, anything else the second.
/// One package pairs the second's `downFramesBase` with the third's
/// `sidestepAngle` in that last case. The record is the first three members of
/// `ActorStrangerVariant`, in the same order, and a fourth halfword.
typedef struct {
    s16 downFramesBase; // Ticks the downed state lasts, before a random few more
    s16 sidestepAngle;  // Angle between the bearing to the player and the direction a sidestep moves in, 4096 units per turn
    s16 sidestepDelay;  // Unused counterpart of `ActorStrangerVariant::sidestepDelay`, by its position and its values (3, 5, 7): no Horned Stranger reads it, and the chase that tests such a delay waits a fixed 3 ticks
    s16 unknown_6;      // Never read, and zero in every table; role unproven
} ActorHornedStrangerVariant;
STATIC_ASSERT_SIZEOF(ActorHornedStrangerVariant, 0x8);

/// One room's limits on the height of an actor's root.
///
/// A package that keeps a table of these looks the current room up in it
/// each time it pushes its actor out of the room's collision grid. In a room
/// with a row, the root coordinate's Y translation is brought into
/// [`minY`, `maxY`] once the push has been applied, and the package's fixed
/// offset is then added to it; in any other room the root is left where the
/// push put it. Y grows downward.
typedef struct {
    s16  stage;        // Stage the row applies in, a `GAME_STAGE_` value
    s16  area;         // Area of that stage the row applies in, a `GAME_AREA_` value
    s16  minY;         // Least Y translation the root keeps, world units
    s16  maxY;         // Greatest Y translation the root keeps, world units
    byte unknown_8[8]; // Zero in every row and never read; role unproven
} ActorHeightClamp;
STATIC_ASSERT_SIZEOF(ActorHeightClamp, 0x10);

/// One end of the two-point beat a patrolling enemy walks back and forth.
///
/// An enemy's work block keeps a pair of these and a selector for the one it
/// is walking toward; coming within reach of that one switches it to the
/// other. Setup seeds the pair with where the enemy was placed and a point a
/// fixed step ahead of that along its facing, and an enemy that roams
/// replaces either end later. Both coordinates are the root coordinate's
/// translation in its parent's space, narrowed to 16 bits. No height is kept:
/// the walk is planar.
typedef struct {
    s16 x; // X translation of the point
    s16 z; // Z translation of the point
} ActorPatrolPoint;
STATIC_ASSERT_SIZEOF(ActorPatrolPoint, 4);

/// Animation playback storage for a model of twenty-one parts.
///
/// An enemy's work block embeds one of these for each playback it runs over
/// its model: the animation context, a slot for every part and an encoded-pose
/// entry for every slot. Setup binds `anim` to the two arrays and to the
/// model's part coordinates, so the rig has to stay live, and stay where it
/// is, for as long as the context is used. An owner that mixes two motions
/// keeps a second rig bound to the same model.
///
/// A slot's index is its model part's, and slot `i` uses pose entry `i`. Each
/// entry reserves `ANIMATION_POSE_BUFFER_BYTES` and holds the slot's encoding
/// at its start: `AnimationPackedPose` (12 bytes) or `AnimationPackedRotation`
/// (4 bytes). Playback stores no capacity, so a slot or pose index has to stay
/// below `ARRAY_SIZE(slots)`. Owners start and tick slots 1 to 20; slot 0, the
/// root part's, keeps its place in both arrays and is never started.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model's part coordinates
    AnimationSlot    slots[21];                              // Playback state of the model part at the same index
    u8               poses[21][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose of the slot at the same index
} ActorAnimRig21;
STATIC_ASSERT_SIZEOF(ActorAnimRig21, 0x4AC);

/// Caller-owned playback storage for twenty slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 20 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 19.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[20];                              // Playback slot for one driven index
    u8               poses[20][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig20;
STATIC_ASSERT_SIZEOF(ActorAnimRig20, 0x474);

/// Caller-owned playback storage for nineteen slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 19 entries.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[19];                              // Playback slot for one driven index
    u8               poses[19][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig19;
STATIC_ASSERT_SIZEOF(ActorAnimRig19, 0x43C);

/// Caller-owned playback storage for eighteen slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 18 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 17.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[18];                              // Playback slot for one driven index
    u8               poses[18][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig18;
STATIC_ASSERT_SIZEOF(ActorAnimRig18, 0x404);

/// Caller-owned playback storage for fifteen slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 15 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 14.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[15];                              // Playback slot for one driven index
    u8               poses[15][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig15;
STATIC_ASSERT_SIZEOF(ActorAnimRig15, 0x35C);

/// Caller-owned playback storage for six slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 6 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 5.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[6];                              // Playback slot for one driven index
    u8               poses[6][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig6;
STATIC_ASSERT_SIZEOF(ActorAnimRig6, 0x164);

/// Animation playback storage for a model of seven parts.
///
/// A work block embeds one for the playback it runs over its model: the
/// animation context, a slot for every part and an encoded-pose entry for
/// every slot. Setup binds `anim` to the two arrays and to the model's part
/// coordinates, so the rig has to stay live, and stay where it is, for as long
/// as the context is used.
///
/// A slot's index is its model part's, and slot `i` uses pose entry `i`. Each
/// entry reserves `ANIMATION_POSE_BUFFER_BYTES` and holds the slot's encoding
/// at its start: `AnimationPackedPose` (12 bytes) or `AnimationPackedRotation`
/// (4 bytes). Playback stores no capacity, so a slot or pose index has to stay
/// below `ARRAY_SIZE(slots)`. Owners start and tick slots 1 to 6; slot 0, the
/// root part's, keeps its place in both arrays and is never started.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model's part coordinates
    AnimationSlot    slots[7];                              // Playback state of the model part at the same index
    u8               poses[7][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose of the slot at the same index
} ActorAnimRig7;
STATIC_ASSERT_SIZEOF(ActorAnimRig7, 0x19C);

/// Caller-owned playback storage for eight slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 8 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 7, and an owner that drives fewer slots uses the leading
/// entries and leaves the rest untouched.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[8];                              // Playback slot for one driven index
    u8               poses[8][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig8;
STATIC_ASSERT_SIZEOF(ActorAnimRig8, 0x1D4);

/// Caller-owned playback storage for four slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 4 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 3.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[4];                              // Playback slot for one driven index
    u8               poses[4][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig4;
STATIC_ASSERT_SIZEOF(ActorAnimRig4, 0xF4);

/// Animation playback storage for a model of five parts.
///
/// A work block embeds one for the playback it runs over its model: the
/// animation context, a slot for every part and an encoded-pose entry for
/// every slot. Setup binds `anim` to the two arrays and to the model's part
/// coordinates, so the rig has to stay live, and stay where it is, for as long
/// as the context is used.
///
/// A slot's index is its model part's, and slot `i` uses pose entry `i`. Each
/// entry reserves `ANIMATION_POSE_BUFFER_BYTES` and holds the slot's encoding
/// at its start: `AnimationPackedPose` (12 bytes) or `AnimationPackedRotation`
/// (4 bytes). Playback stores no capacity, so a slot or pose index has to stay
/// below `ARRAY_SIZE(slots)`. Owners start and tick slots 1 to 4; slot 0, the
/// root part's, keeps its place in both arrays and is never started.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model's part coordinates
    AnimationSlot    slots[5];                              // Playback state of the model part at the same index
    u8               poses[5][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose of the slot at the same index
} ActorAnimRig5;
STATIC_ASSERT_SIZEOF(ActorAnimRig5, 0x12C);

/// Steps of an enemy's animation, kept in `ActorEnemyState::state`.
///
/// A play request leaves one of the two reseeds pending. The enemy's next
/// update performs it and moves on to `ACTOR_ENEMY_ANIM_TICK`, where it stays
/// until the next request. A block still zeroed has no step, and its update
/// leaves the rig alone.
enum {
    ACTOR_ENEMY_ANIM_BLEND = 1, // Reseed the slots from `animId`, blending from the pose they hold
    ACTOR_ENEMY_ANIM_RESET = 2, // Reseed the slots from `animId` at its start, with no blend
    ACTOR_ENEMY_ANIM_TICK  = 3, // Tick the slots each frame
};

/// The animation request and walk an animated enemy keeps right after its rig.
///
/// A play request stores the clip in `animId` and the reseed to perform in
/// `state`; the update that performs it copies the clip to `appliedAnimId`.
/// Clip ids index the package's own animation table. An enemy that walks
/// keeps the heading it last gave its root coordinate and the frames its walk
/// has left; one that stays where it is placed leaves both zero. No enemy
/// reads or writes the bytes of `pad_A` or `pad_34`, and the block is
/// allocated zeroed.
typedef struct {
    s16  state;         // Step of the animation (0 none, else `ACTOR_ENEMY_ANIM_BLEND`, `_RESET` or `_TICK`)
    s16  appliedAnimId; // Clip the slots were last seeded with; recorded, never read
    s16  animId;        // Clip the last play request selected
    s16  field_6;       // Cleared by each play request and raised once by a view figure's spawn; never read, role unproven
    s16  cueRecord;     // Animation record the enemy last cued a sound for, so a record held for several frames cues once
    byte pad_A[0x28];
    s16  yaw;           // Heading last given the root coordinate, 4096 to a turn
    byte pad_34[0x2];
    s16  travel;        // Frames of forward movement the walk has left
} ActorEnemyState;
STATIC_ASSERT_SIZEOF(ActorEnemyState, 0x38);

/// What follows one clip in a cutscene actor's chain of animations.
///
/// A chain is a table with one link per clip, indexed by the clip that is
/// playing. Each frame the cutscene looks up the playing clip's link: once the
/// clip has been held for `holdFrames` frames, or has finished when that is
/// zero, it starts `nextAnimId` with a blend and looks that clip's link up
/// from then on. A negative `nextAnimId` ends the chain, which the walk
/// reports to its caller. Clips are indices into the animation sets of the
/// model the chain drives: the player's, through a play request, or the
/// cutscene actor's own rig.
typedef struct {
    u16 holdFrames; // Frames the playing clip is held before the next starts; 0 waits for the clip to finish
    s16 nextAnimId; // Clip that follows; negative when none does
} ActorAnimChainLink;
STATIC_ASSERT_SIZEOF(ActorAnimChainLink, 0x4);

/// One pending cue of a cutscene task: what its event script has told one of
/// the scene's actors, or the scene itself, to do.
///
/// A script callback posts a cue by storing `id` and clearing `step`. The
/// cutscene task's handler for that cue acts on it the next time it runs and
/// then clears `id`. A cue that spans several frames starts at its first step
/// and keeps `id` set until its last step is done, and a handler may leave
/// `id` set for an action it repeats every frame.
///
/// A cue is replaced, not queued: posting over one that is still running
/// abandons it, and skipping the scene clears the cues outright. Each package
/// numbers its own cues.
typedef struct {
    u16  id;           // Cue to act on; 0 when none is pending
    u16  step;         // Step reached within a cue that spans several frames; counts up from 0
    s16  counter;      // Frames waited, or distance an actor has been slid, within the current step; zeroed by the step that starts counting, not by posting
    byte unknown_6[2]; // Never accessed; the cues sit 8 bytes apart, so the bytes are the cue's, but nothing shows what they hold
} ActorCutsceneCue;
STATIC_ASSERT_SIZEOF(ActorCutsceneCue, 0x8);

/// Where one cell of an animated sprite's sheet starts: an entry of the table
/// the sprite's frame picks its cell from.
///
/// The coordinates are texels within the texture page the drawer selects. The
/// drawer supplies the cell's size, 32 texels square in every table.
typedef struct {
    u8   u;         // Left texture column of the cell, in texels
    byte unknown_1; // Never read, and zero in every table; role unproven
    u8   v;         // Top texture row of the cell, in texels
    byte unknown_3; // Never read, and zero in every table; role unproven
} ActorSpriteUv;
STATIC_ASSERT_SIZEOF(ActorSpriteUv, 0x4);

/// `ActorModelState::animId` and `ActorModelState::bank` before the first play
/// request: no clip applied, no bank bound.
///
/// Bank indices and clip ids index tables, so it matches none of them: a block
/// seeded with it binds its bank and applies its clip on the first request.
#define ACTOR_MODEL_STATE_NONE (-1)

/// What an actor's model is playing and the matrices it is lit with, kept
/// right after the model's rig in the actor's work block.
///
/// A play request rebinds the rig when its bank differs from `bank` and
/// records the clip it seeds the slots with in `animId`; the handlers that
/// skip a repeated request do so by comparing against `animId`. The work block
/// is allocated zeroed, and the actor's spawn then sets both ids to
/// `ACTOR_MODEL_STATE_NONE`. The model object borrows `light` and `color` for
/// as long as the work block lives.
typedef struct {
    s8     ticking;    // Set once a clip has been applied, never cleared: the slots are ticked each frame and a request may blend from their pose
    s8     animId;     // Clip the slots were last seeded with, within `bank`
    s8     bank;       // Index, in the package's animation bank table, of the bank the rig is bound to
    s8     nextAnimId; // Clip, in bank 0, a walk changes to as it ends; unused by an actor that does not walk
    MATRIX light;      // Light-direction matrix lent to the model object
    MATRIX color;      // Light-colour matrix lent to the model object
} ActorModelState;
STATIC_ASSERT_SIZEOF(ActorModelState, 0x44);

/// Which of its two handlers a scripted walker's tick runs, kept in
/// `ActorWalkState::motion`.
enum {
    ACTOR_WALK_MOTION_IDLE    = 0, // No walk in progress: the package's idle handler runs
    ACTOR_WALK_MOTION_WALKING = 1, // A walk in progress: the handler runs the step `motionStep` selects
};

/// `ActorWalkState::lastDistance` before a walk's first arrival check: larger
/// than any distance the check measures, so that check only records.
#define ACTOR_WALK_DISTANCE_NONE 0x7FFF

/// The walk a scripted walker keeps after its model state: an actor the room
/// script places and sends from point to point.
///
/// A walk request copies the destination's position and rotation into
/// `target` and `targetRot` and sets `motion`; the package's sequence of
/// steps then carries the walk out, and the step that ends it returns
/// `motion` to idle. The steps and what each does are the package's own.
///
/// A walker that moves under `velocity` adds it into `carry` each frame, moves
/// the root coordinate by the integer halves and keeps the fractions, so a
/// velocity below one unit a frame still moves the actor. One whose sequence
/// stops on arrival measures its X and Z distance to `target` each frame and
/// has arrived once neither is smaller than `lastDistance`, which reaching
/// and passing the target both cause.
///
/// No walker's own code reads or writes the fourth words of the two SDK
/// vectors or `pad_2C`, and the block is allocated zeroed.
typedef struct {
    VECTOR  target;       // Destination of the walk, a position the root coordinate's translation is to reach
    VECTOR  velocity;     // Displacement added each frame, in signed 16.16 units; zero while standing
    Fixed16 carry[3];     // X, Y and Z displacement not yet applied; only the fractions survive a frame
    byte    pad_2C[0x4];
    SVECTOR lastDistance; // Absolute X and Z distance to `target` at the last arrival check, `ACTOR_WALK_DISTANCE_NONE` before the first; Y is only seeded
    SVECTOR targetRot;    // Rotation of the destination, 4096 to a turn; the closing turn steers the yaw to `vy`, the other angles are not read
    s16     motion;       // Handler the tick runs (0 `ACTOR_WALK_MOTION_IDLE`, 1 `ACTOR_WALK_MOTION_WALKING`)
    s16     motionStep;   // Step of the package's walk sequence in progress, counted from 0
} ActorWalkState;
STATIC_ASSERT_SIZEOF(ActorWalkState, 0x44);

/// Work block of Kyle Madigan as a room script walks him about, the actor
/// whose code actor_135600 and the second actor of actor_350700 each carry.
///
/// The setup state allocates it zeroed at its full size and keeps it at
/// `Task::work`. It opens as `ActorMotionWalkWork` does, which the play and
/// walk handlers of the actor-motion library run on. After that come the
/// tasks setup spawns for the models attached to the body, each of which the
/// draw-mode message gives the body's own draw flags.
typedef struct {
    ActorAnimRig20  rig;           // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState model;         // Light matrices lent to the body's model object, and what the rig plays
    ActorWalkState  walk;          // Destination, per-frame velocity and step of the walk in progress
    Task*           handTasks[2];  // Tasks drawing the two hand models; which hand each entry holds is the package's choice
    Task*           heldItemTask;  // Task drawing what the body carries: the gun in actor_350700, a model that also draws a quad ahead of itself in actor_135600
    s32             freeCountdown; // Ticks left before the body model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} KyleMadiganWalkerWork;
STATIC_ASSERT_SIZEOF(KyleMadiganWalkerWork, 0x50C);

/// Bearing of `other` from `self`, measured in `self`'s own frame and folded
/// into -0x800..0x800. The offset between the two world positions is written
/// to `blk->delta` and rotated there through `blk->inverseRotation`, the
/// transpose of `self`'s world matrix; the caller owns `blk` and may reuse it
/// afterwards.
static __inline__ s32 actorBearingInFrame(ActorBearingScratch* blk, GfxCoord* self, GfxCoord* other)
{
    s32 angle;

    blk->delta.vx = other->workm.t[0] - self->workm.t[0];
    blk->delta.vy = other->workm.t[1] - self->workm.t[1];
    blk->delta.vz = other->workm.t[2] - self->workm.t[2];
    TransposeMatrix(&self->workm, &blk->inverseRotation);
    gfxRotateSv(&blk->inverseRotation, &blk->delta);
    angle = ratan2(blk->delta.vx, blk->delta.vz);
    if (angle >= 0x801) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    return angle;
}

/// The push that moves `pos` out of the contact record `rec`: how deep `pos`
/// sits inside the record's radius, along the direction from the record's
/// centre carried into grid space. Only X and Z are written.
static __inline__ void actorCalcPush(SVECTOR* pos, WorldCollisionContact* rec, SVECTOR* out)
{
    VECTOR d;
    VECTOR n;
    s32    t;
    s32    pen;

    d.vx = pos->vx - rec->point.vx;
    d.vy = 0;
    d.vz = pos->vz - rec->point.vz;
    pen  = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen  = rec->distance - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = pos->vx - rec->point.vx;
    d.vy = pos->vy - rec->point.vy;
    d.vz = pos->vz - rec->point.vz;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Builds `joint`'s absolute rotation in `out`: its own rotation with each
/// ancestor pre-multiplied in turn, renormalised after every step, up to but
/// not including `stop`. Returns whether the walk reached `stop` rather than
/// the end of the chain.
static __inline__ s32 actorAccumulateRotation(GfxCoord* joint, MATRIX* out, GfxCoord* stop)
{
    MATRIX    matrix;
    GfxCoord* coord;

    coord = joint->parent;
    *out  = joint->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == stop) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(out);
        MatrixNormal(out, &matrix);
        *out  = matrix;
        coord = coord->parent;
    }
}

/// Turns the world-space `rotation` into one relative to `joint`'s parent:
/// accumulates the chain above the parent up to the view coordinate,
/// transposes it and pre-multiplies. Nothing happens when the parent is the
/// view coordinate. Returns `joint`, which callers store through.
static __inline__ GfxCoord* actorLocalizeRotation(GfxCoord* joint, MATRIX* rotation)
{
    MATRIX    matrix;
    MATRIX    normal;
    MATRIX    transposed;
    GfxCoord* coord;
    GfxCoord* view;

    coord = joint->parent;
    if (coord != &gGfxViewCoord) {
        view   = &gGfxViewCoord;
        matrix = coord->coord;
        while (1) {
            coord = coord->parent;
            if (coord == NULL) {
                break;
            }
            if (coord == view) {
                gte_TransposeMatrix(&matrix, &transposed);
                gte_SetRotMatrix(&transposed);
                MulRotMatrix(rotation);
                break;
            }
            gte_SetRotMatrix(&coord->coord);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return joint;
}

/// Carries `out` from the frame of `p` up the parent chain to the view
/// coordinate, leaving it in view space. `out` is written only when the walk
/// reaches the view coordinate.
static __inline__ void actorTransformToView(GfxCoord* p, SVECTOR* out)
{
    SVECTOR   sv;
    VECTOR    vec;
    s32       flag;
    SVECTOR*  svp   = &sv;
    GfxCoord* view  = &gGfxViewCoord;
    VECTOR*   vecp  = &vec;
    s32*      flagp = &flag;
    sv.vx           = out->vx;
    sv.vy           = out->vy;
    sv.vz           = out->vz;
loop:
    if (p->parent != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->parent;
            goto loop;
        }
        out->vx = sv.vx;
        out->vy = sv.vy;
        out->vz = sv.vz;
    }
}

/// Wraps an angle difference into [-0x800, 0x800].
static __inline__ s16 actorNormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

/// The offset from `coord` to the translation of `config`'s coordinate.
static __inline__ void actorConfigPositionDelta(PlayerStatus* config, GfxCoord* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

/// The turn that would face `actor` toward `config`'s coordinate: the bearing
/// of the offset, written to `pos`, less the actor's own heading, wrapped.
static __inline__ s16 actorPositionYaw(Task* actor, SVECTOR* pos, PlayerStatus* config)
{
    GfxCoord* coord;
    s32       angle;
    actorConfigPositionDelta(config, actor->extra.tmd->coords, pos);
    coord = actor->extra.tmd->coords;
    angle = ratan2(pos->vx, pos->vz);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Rebuilds `coord`'s rotation as a turn about Y by its current heading,
/// uniformly scaled by `scale`.
static __inline__ void actorRescaleYaw(GfxCoord* coord, s16 scale)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

/// Steps `coord` `amount` units along its local Z axis unless movement is
/// frozen, staging the direction on the scratch pad.
static __inline__ void actorMoveForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gfxReadMatrixZAxis(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// `actorMoveForward` that also skips the step when `amount` is zero, though
/// it still takes and releases its scratch vector.
static __inline__ void actorMoveForwardNonzero(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gteVec                        = vec;
        if (amount != 0) {
            gfxReadMatrixZAxis(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12();
            gte_stsv(gteVec);
            coord->coord.t[0]  += head[-1].vx;
            coord->coord.t[1]  += vec->vy;
            coord->coord.t[2]  += vec->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// `actorMoveForward` applied to the root coordinate of `task`'s model.
static __inline__ void actorMoveModelForward(Task* task, s16 amount)
{
    GfxCoord* coord;
    SVECTOR*  head;
    SVECTOR*  vec;

    coord = task->extra.tmd->coords;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gfxReadMatrixZAxis(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Rebuilds `coord`'s rotation as a turn about Y by its current heading at
/// unit scale.
static __inline__ void actorResetYaw(GfxCoord* coord)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)blk->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    coord->coord.m[2][2] = (u16)blk->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
}

/// `actorRescaleYaw` with a separate scale on Y.
static __inline__ void actorRescaleYawY(GfxCoord* coord, s32 scale, s16 scaleY)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vx = scale;
    blk->scale.vy = scaleY;
    blk->scale.vz = scale;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

/// `actorMoveForwardNonzero` spelled with the GTE reading the scratch vector
/// under a single name.
static __inline__ void actorStepForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            gfxReadMatrixZAxis(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(vec);
            gte_gpf12();
            gte_stsv(vec);
            coord->coord.t[0]  += head[-1].vx;
            coord->coord.t[1]  += vec->vy;
            coord->coord.t[2]  += vec->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// The first of the leading twelve contact records whose kind is 0x20000:
/// copies its point to `pos` and returns its key, or returns 0 when none is
/// found before the table ends.
static __inline__ s32 actorFindHit(SVECTOR* pos, WorldCollisionContact* records)
{
    s16 i;

    for (i = 0; i < 12; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
        }
    }
    return 0;
}

/// Looks up the current area's placement record from the session location.
static __inline__ AreaVariant* actorGetCurrentAreaRec(void)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;

    sessionKey = &gGameSession->location.loc;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    key.view   = sessionKey->view;
    areaSyncLocationVariant(&key);
    return Gp_GetNestedAreaRec(&key);
}

/// Gives `model` the texture page and palette of the enemy's placement in the
/// current area, and reprocesses its stream when it already has one.
static __inline__ void actorTintModel(TmdObject* model, Enemy* enemy)
{
    AreaVariant*   layout;
    AreaPlacement* place;
    s32            idx;

    idx                      = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    layout                   = actorGetCurrentAreaRec();
    place                    = gpAreaPlaceAt(layout->placements, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// `actorTintModel` for the model carried by the spawned task `spawned`.
static __inline__ void actorTintTask(Task* spawned, Enemy* enemy)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    AreaVariant*     layout;
    AreaPlacement*   place;
    TmdObject*       model;
    s32              idx;

    sessionKey = &gGameSession->location.loc;
    idx        = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    model      = spawned->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    key.view   = sessionKey->view;
    areaSyncLocationVariant(&key);
    layout                   = Gp_GetNestedAreaRec(&key);
    place                    = gpAreaPlaceAt(layout->placements, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// `actorTintModel` for a freshly spawned effect, when the spawn succeeded.
static __inline__ void actorTintEffect(EffectWork* eff, Enemy* enemy)
{
    if (eff != NULL) {
        actorTintModel(eff->task->extra.tmd, enemy);
    }
}

/// The offset from `coord` to the translation of `m`.
static __inline__ void actorMatrixPositionDelta(MATRIX* m, GfxCoord* coord, SVECTOR* pos)
{
    pos->vx = m->t[0] - coord->coord.t[0];
    pos->vy = m->t[1] - coord->coord.t[1];
    pos->vz = m->t[2] - coord->coord.t[2];
}

/// `actorPositionYaw` toward the translation of `m`.
static __inline__ s16 actorMatrixPositionYaw(Task* actor, SVECTOR* pos, MATRIX* m)
{
    GfxCoord* coord;
    s32       angle;

    actorMatrixPositionDelta(m, actor->extra.tmd->coords, pos);
    coord = actor->extra.tmd->coords;
    angle = ratan2(pos->vx, pos->vz);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// The turn from `coord`'s heading to the bearing of the offset (`x`, `z`),
/// wrapped.
static __inline__ s16 actorYawTo(GfxCoord* coord, s16 x, s16 z)
{
    s32 angle;

    angle = ratan2(x, z);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// The turn from `coord`'s heading to the bearing of the offset `dir`,
/// wrapped.
static __inline__ s16 actorViewYaw(GfxCoord* coord, SVECTOR* dir)
{
    s32 angle;

    angle = ratan2(dir->vx, dir->vz);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Combines a movement step with a push along the same axis: the push when
/// there is no step, the step when the two disagree in sign, otherwise the
/// larger in magnitude.
static __inline__ s16 actorPickStep(s16 step, s16 push)
{
    if (step == 0) {
        return push;
    }
    if ((step > 0 && push < 0) || (step < 0 && push > 0)) {
        return step;
    }
    if (step > 0) {
        if (push < step) {
            return step;
        }
        return push;
    }
    if (push < step) {
        return push;
    }
    return step;
}

/// Whether the XZ offset `pos` reaches at least `radius`, worked in a
/// scratch block pushed and popped around the test.
static __inline__ s32 actorOutsideRadius(SVECTOR* pos, s16 radius)
{
    OverlayRangeScratch* head;
    OverlayRangeScratch* scratch;
    head                                      = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    scratch                                   = head - 1;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = scratch;
    scratch->dx                               = pos->vx;
    scratch->dz                               = pos->vz;
    scratch->radius                           = radius;
    scratch->dx                              *= scratch->dx;
    scratch->dz                              *= scratch->dz;
    scratch->radius                          *= scratch->radius;
    SCRATCH_STACK_RELEASE_BLOCK(OverlayRangeScratch);
    return scratch->dx + scratch->dz >= scratch->radius;
}

/// Tells the player task that `ctx` touched it, packing the pair with `mode`.
static __inline__ s32 actorPlayerContactMessage(Enemy* ctx, s32 mode)
{
    Task* player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    return taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(ctx, mode), 0);
}

/// Relights `enemy` for the world position of `coord`.
static __inline__ void actorUpdateColor(Enemy* enemy, GfxCoord* coord)
{
    VECTOR* block                = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);
    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    block->vz                    = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    Gp_UpdateActorColor(enemy, block, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Relights the enemy of `arg0` for the world position of its model's second
/// part.
static __inline__ void actorUpdateModelColor(Task* arg0)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &arg0->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// `actorTransformToView` spelled as a `for` loop with an early return.
static __inline__ void actorLocalToView(GfxCoord* coord, SVECTOR* out)
{
    SVECTOR acc;
    VECTOR  v;
    s32     flag;

    acc.vx = out->vx;
    acc.vy = out->vy;
    acc.vz = out->vz;

    for (;;) {
        if (coord->parent == NULL) {
            return;
        }
        if (coord != &gGfxViewCoord) {
            gte_SetTransMatrix(&coord->coord);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&acc);
            gte_rt();
            gte_stlvnl(&v);
            gte_stflg(&flag);
            acc.vx = v.vx;
            acc.vy = v.vy;
            acc.vz = v.vz;
            coord  = coord->parent;
        } else {
            out->vx = acc.vx;
            out->vy = acc.vy;
            out->vz = acc.vz;
            return;
        }
    }
}

/// `actorAccumulateRotation` stopping at the view coordinate, without the
/// result.
static __inline__ void actorAccumulateToView(GfxCoord* coord, MATRIX* mat)
{
    MATRIX    m;
    GfxCoord* cur;

    cur  = coord->parent;
    *mat = coord->coord;
    while (1) {
        if (cur == NULL) {
            return;
        }
        if (cur == &gGfxViewCoord) {
            return;
        }
        gte_SetRotMatrix(&cur->coord);
        MulRotMatrix(mat);
        MatrixNormal(mat, &m);
        *mat = m;
        cur  = cur->parent;
    }
}

/// Sets up a collision object on `coord` with its record table, position and
/// radius, links it at priority `prio`, and initialises the table as `kind`.
static __inline__ void actorLinkWorkObj(GfxCoord* coord, WorldCollisionBody* obj, WorldCollisionContact* rec,
                                        SVECTOR* pos, s16 field1C, s32 prio, s32 kind)
{
    obj->coord            = coord;
    obj->context.contacts = rec;
    obj->pos.vx           = pos->vx;
    obj->pos.vy           = pos->vy;
    obj->pos.vz           = pos->vz;
    obj->radius           = field1C;
    obj->flags            = 1;
    Gp_LinkObj(prio, obj);
    Gp_InitRec18Table(obj->context.contacts, kind, 0);
}

/// Whether the XZ offset `gap` reaches at least 1000.
static __inline__ s32 actorOutOfReach(SVECTOR* gap)
{
    VECTOR3* v;

    v                             = (VECTOR3*)(SCRATCH_STACK_CURSOR(u8) - sizeof(VECTOR3));
    SCRATCH_STACK_CURSOR(VECTOR3) = v;
    v->vx                         = gap->vx;
    v->vy                         = gap->vz;
    v->vz                         = 1000;
    v->vx                         = v->vx * v->vx;
    v->vy                         = v->vy * v->vy;
    v->vz                         = v->vz * v->vz;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(VECTOR3));

    return v->vx + v->vy >= v->vz;
}

/// The scratch-pad allocation pointer. These two stay inline functions rather
/// than `SCRATCH_STACK_CURSOR` at their call sites: inside an inlined body the
/// pointer's constant address folds into each access, which the callers'
/// code depends on.
static __inline__ u8* actorGetScratchHead(void)
{
    return SCRATCH_STACK_CURSOR(u8);
}

/// Moves the scratch-pad allocation pointer to `head`.
static __inline__ void actorSetScratchHead(void* head)
{
    SCRATCH_STACK_CURSOR(void) = head;
}

/// Wraps an angle into [-0x800, 0x800]; see `overlayWrapAngle`.
static __inline__ s16 actorWrapAngle(s16 angle)
{
    return overlayWrapAngle(angle);
}

#endif // INCLUDE_ACTORS_ACTOR_H
