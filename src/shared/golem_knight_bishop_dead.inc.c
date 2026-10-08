#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Refreshes a corpse's root composition, room lighting/tint and ground shadow.
///
/// `root` is coordinate 0 of `task`'s live model; its work, enemy spawn argument,
/// ancestors and lighting/scratch/GTE state must remain initialized. Marks root
/// and part 3 dirty, composes only the root, then updates tint and draws from
/// part 3's cached X/Z with the root's cached Y. A pending tint may be consumed
/// and zero shadow shade becomes the no-shadow sentinel. Advances no animation,
/// allocates no retained resource and borrows both pointers only for the call.
static inline void _golemKnightBishopRefreshCorpsePresentation(Task* task, GfxCoord* root)
{
    root->composeStamp                      = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);
    _golemKnightBishopUpdateTintInline(task);
    _golemKnightBishopDrawShadowInline(task);
}

/// Detaches a newly dead GOLEM or presents a corpse restored from its saved pose.
///
/// `enemy` belongs to `task`, whose GOLEM rig/model remain live. Entry step 0
/// unlinks targeting and the ground, hurt and strike bodies, releases battle
/// rewards and saves the downed pose. Step 1 emits a flash every fourth
/// animation frame at local Y offsets -511..0. Restored corpses enter step 2,
/// capture/fade once and remain at step 3 without flashes. Grab release and
/// animation continue in running mode, followed by composition, tint and shadow.
///
/// Paused mode refreshes presentation only; hidden mode suppresses drawing.
/// Both early exits retain one `SVECTOR` scratch reservation until the next
/// main-loop scratch reset, as in the original; the running path releases it.
static void _golemKnightBishopDeadState(Enemy* enemy, Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_CORPSE_DETACH            = 0,
        GOLEM_KNIGHT_BISHOP_CORPSE_FLASHES           = 1,
        GOLEM_KNIGHT_BISHOP_CORPSE_RESTORE           = 2,
        GOLEM_KNIGHT_BISHOP_CORPSE_RESTORED          = 3,
        GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_FRAME_MASK  = 3,
        GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_OFFSET_MASK = 511,
        GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_SIZE        = 1024,
    };
    SVECTOR*               flashOffset;
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    u32                    randomDraw;
    s16                    lyingAnimation;

    work        = task->work;
    root        = &task->extra.tmd->coords[0];
    flashOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _golemKnightBishopRefreshCorpsePresentation(task, root);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_CORPSE_DETACH:
            // Retire combat participation before saving the persistent corpse pose.
            enemy->recs = NULL;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->groundBody);
            worldCollisionUnlinkBody(&work->hurtBody);
            worldCollisionUnlinkBody(&work->strikeBody);
            sceneReleaseBattleRefWithRewards(task, work->actorId);
            lyingAnimation = GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT;
            if (work->downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                lyingAnimation = GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND;
            }
            work->anim        = lyingAnimation;
            work->step        = GOLEM_KNIGHT_BISHOP_CORPSE_FLASHES;
            enemy->spawnState = work->downedPose;
            areaSaveEnemyPose(enemy);
            break;
        case GOLEM_KNIGHT_BISHOP_CORPSE_FLASHES:
            if (!(work->animFrame & GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_FRAME_MASK)) {
                flashOffset->vx = 0;
                flashOffset->vz = 0;
                randomDraw      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                flashOffset->vy = -((randomDraw >> 16) & GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_OFFSET_MASK);
                gRandomLcgState = randomDraw;
                effectSpawn(EFFECT_FLASH_BURST, &task->extra.tmd->coords[3], GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_SIZE, flashOffset);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_CORPSE_RESTORE:
            _golemKnightBishopQueueFrameCapture(&task->extra.tmd->coords[3], GOLEM_KNIGHT_BISHOP_FRAME_CAPTURE_BIAS);
            _golemKnightBishopUpdateAppearance(task);
            modelLightingSetLayerMaterials(work->translucency);
            work->step = GOLEM_KNIGHT_BISHOP_CORPSE_RESTORED;
            break;
    }
    _golemKnightBishopTickGrabRelease(task);
    _golemKnightBishopTickAnimInline(task);
    _golemKnightBishopRefreshCorpsePresentation(task, root);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
