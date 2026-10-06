#ifndef GAMEPLAY_WORLD_COORDS_H
#define GAMEPLAY_WORLD_COORDS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/enemy.h"
#include "gameplay/light.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Number of directly indexed transient point-light slots.
enum { WORLD_COORDINATE_TRANSIENT_LIGHT_COUNT = 8 };

/// Shared transient point lights contributing alongside the room's authored lights.
///
/// Storage lives with the gameplay overlay and is indexed from 0 through
/// `WORLD_COORDINATE_TRANSIENT_LIGHT_COUNT - 1`. Writers select slots directly;
/// there is no allocation or exclusive reservation, and a later writer can
/// replace a light that is still active.
///
/// Slot 0 serves muzzle, impact and parasite-energy flashes; slot 1 serves
/// weapon and impact lights; slot 2 serves actor, fireball and room glows;
/// slot 3 serves actor lights. Room effects also use slots 4-7, energy balls
/// use 4-6, and the plaza beam effects wrap their indices across all eight.
/// These assignments overlap rather than partitioning the pool by owner.
///
/// Gameplay and room-light initialization disable all entries and parent their
/// transforms to the persistent view coordinate. Activation, placement and
/// countdown behavior follow `WorldCoordTransientPointLight`; expiry retains
/// each record for the next writer.
extern WorldCoordTransientPointLight gWorldCoordTransientPointLights[WORLD_COORDINATE_TRANSIENT_LIGHT_COUNT];

/// Rebuilds a model's lighting from room and active transient lights at a world position.
///
/// `worldPosition` supplies three readable, word-aligned signed 32-bit xyz values
/// in world units, such as a VECTOR/VECTOR3 or a MATRIX translation. Reads exactly
/// 12 bytes and retains no pointer. `model` borrows writable lightMtx and colorMtx
/// matrices; source transforms and the current view must already be composed.
/// The view-relative offset is narrowed to signed 16 bits before rotation.
///
/// Use firstLightIndex = 0 and lightCount = 3 for the full query. Nonnegative
/// indices must satisfy firstLightIndex + lightCount <= 3. The coefficient loops
/// visit [firstLightIndex, lightCount), preserving the original nonzero-start
/// behavior. A missing room collection, zero count or an end outside 0..3 returns
/// without writes. Otherwise all nine colour coefficients are cleared, and the
/// request is reduced recursively until enough room/active transient sources
/// exist; zero sources leave that clear but retain the previous ambient term.
/// Only selected direction rows are replaced; other direction rows are retained.
///
/// Positive contributions are ranked by RGB score and falloff. The cutoff source
/// supplies ambient when firstLightIndex is zero, followed by the ambient override
/// or the current view's RGB minima. Optional Q12 RGB scales affect coefficient
/// rows alone. Queries update source attenuation and may capture diagnostic ranks.
/// Requires 124 scratch bytes plus up to 68 nested bytes, released before return;
/// changes GTE state. Output matrices and input must be disjoint from scratch.
void worldCoordSetModelLighting(const TmdObject* model, const void* worldPosition, s32 firstLightIndex, s32 lightCount);

/// Rebuilds an actor's model lighting and applies its current colour mode.
///
/// `enemy` must own a live task/model with writable light and colour matrices.
/// `worldPosition` supplies three readable, word-aligned signed 32-bit xyz
/// values in world units, including VECTOR, VECTOR3 or matrix-translation storage;
/// only those 12 bytes are read and no pointer is retained. The trailing
/// arguments are ignored; callers pass zero for both. Coordinate composition
/// and the sampled frame follow `worldCoordSetModelLighting`.
///
/// Only the colour matrix's nine light coefficients are remapped or blended;
/// its ambient translation comes from the lighting query. A positive signed
/// `colorBlend` weights the previous mode by countdown/16 and the current mode
/// by the remainder. It decrements after blending only while actors are running.
/// Nonpositive signed countdowns select the current mode directly. A hit flash
/// is consumed by the first eligible remap, before remapping the previous mode.
///
/// With sceneUpdatesPaused exactly 1, work runs only for a model whose active
/// drawing is enabled and whose buffer is non-NULL. Other pause values allow
/// the update. Requires 48 scratch bytes plus the lighting query's nested
/// reservations, released before return; changes GTE state. Matrices and the
/// sample storage must be disjoint from those scratch reservations.
void worldCoordUpdateActorColor(Enemy* enemy, const void* worldPosition, s32 unusedArg2, s32 unusedArg3);

/// Selects an actor's target light-colour mode and starts a transition when it changes.
///
/// Only colorMode's low two bits are used (0 default, 1 weighted, 2 black,
/// 3 tint). A change saves the old current mode as the previous mode, preserves
/// the upper flag nibble and resets colorBlend to 16 updating frames. Repeating
/// the current mode leaves the packed modes and countdown untouched. The enemy
/// must be writable; matrices change later in `worldCoordUpdateActorColor`.
void worldCoordSetActorColorMode(Enemy* enemy, s32 colorMode);

/// Returns the signed attenuation depth for spatial sound at a coordinate's local origin.
///
/// Subtracts the current view's projection distance from `coord->workm.t[2]`,
/// clamps to [-32767, 32767] game-coordinate units and shifts right by eight,
/// rounding negative values down. One depth unit spans 256 game-coordinate
/// units; the result is in [-128, 127]. Negative is nearer than the projection
/// plane, positive is farther, and zero supplies no sound attenuation.
///
/// `coord` must be live with its local-to-view matrix already composed. Reads
/// the cached matrix without updating it; changes no node, scratch or GTE state.
/// `worldCoordGetOriginAudioPan` supplies the corresponding spatial pan offset.
s32 worldCoordGetOriginAudioDepth(const GfxCoord* coord);

/// Returns the spatial sound pan offset for a coordinate node's local origin.
///
/// Projects with `coord->workm` and the current GTE projection settings, clamps
/// screen X to [-160, 159] pixels and divides by ten, rounding toward zero.
/// The result is in [-16, 15]: negative pans left, positive pans right and
/// zero leaves the sound's base pan unchanged. Sound events apply three SPU
/// pan steps per offset unit. A GTE summary error also returns zero.
///
/// `coord` must be live with its local-to-view matrix already composed. Requires
/// an initialized scratch stack with room for one projection record (24 bytes),
/// released before return. Replaces the GTE rotation/translation matrices and
/// projection results; it does not change the node or update its matrix.
s32 worldCoordGetOriginAudioPan(const GfxCoord* coord);

/// Copies an ambient RGB override for subsequent model-light queries, or disables it.
///
/// `ambientColor` is a readable complete SVECTOR; vx/vy/vz are signed ambient
/// RGB levels in the colour matrix's translation units. The entire eight-byte
/// value is copied, including the unused final halfword; no pointer is retained.
/// NULL disables the override and retains the stored value. This replaces both
/// the cutoff-derived ambient colour and the room view's minimum levels, while
/// leaving the directional colour coefficients unchanged. Room-light binding
/// also disables the override. Storage lasts with the gameplay overlay.
void worldCoordSetAmbientColorOverride(const SVECTOR* ambientColor);

/// Sets the RGB ambient term added when lighting a model's vertices.
///
/// `model->colorMtx` must be writable and remain live. Signed 16-bit r/g/b
/// values are extended into its translation; 0x1000 is full channel intensity.
/// The nine directional colour coefficients are preserved. A later lighting
/// query may replace this ambient term; the matrix remains caller-owned.
void worldCoordSetModelAmbientColor(const TmdObject* model, s16 r, s16 g, s16 b);

/// Initializes and composes the current room's authored and transient lights.
///
/// State 0 parents all room lights to the view, builds cone-axis rotations,
/// disables every transient slot, and advances to the recurring state. Every
/// call composes the view and active lights, excluding the view ancestor from
/// the light caches; it does not age transient lifetimes. Missing room lights
/// kill the task. Loaded room arrays and their authored counts must stay live.
/// Requires 28 local scratch bytes plus called helpers' reservations; changes
/// coordinate caches and GTE state. Registered in resident task bank 1, slot 0xF.
void worldCoordUpdateRoomLightsTask(Task* task);

/// Dispatches player and companion lighting initialization or per-frame updates.
///
/// `task->state` must be 0 (bind matrices) or 1 (update); dispatch has no bounds
/// check. Initialization advances to 1 and performs that frame's update, or kills
/// the task when room lights are missing. Player/companion models and borrowed
/// child-model matrix storage must stay live. Updates also support the light-probe
/// diagnostic. Registered in resident task bank 1, slot 0x10; gameplay must be loaded.
void worldCoordPlayerLightingTask(Task* task);

/// Runs a task's current exit callback as its frame handler.
///
/// `task` and its exit handler must be live, and the gameplay implementation
/// must remain loaded. Teardown follows `taskCallExit`; a custom handler may
/// retain the task or release it before returning. Registered in the resident
/// task-descriptor table, so this entry has external linkage.
void taskRunExitCallbackTask(Task* task);

#endif // GAMEPLAY_WORLD_COORDS_H
