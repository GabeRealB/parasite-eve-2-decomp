/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the anchored dangle pose and advances to its sway.
///
/// Requires live model/work storage at the dangle entry sub-state. Anchors
/// part 6 at anchorPos through the frame callback and requests a normal-rate
/// cut to clip 7. The caller applies animation and the anchor after this step.
static void _madChaserDangleStart(Task* task)
{
    enum { MAD_CHASER_DANGLE_CLIP = 7 };
    MadChaserWork* work;
    MadChaserWork* requestWork;

    work                     = task->work;
    work->anchored           = 1;
    requestWork              = task->work;
    requestWork->animRate    = ANIMATION_RATE_ONE;
    requestWork->animId      = MAD_CHASER_DANGLE_CLIP;
    requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    work->subState           = work->subState + 1;
}

/// Replaces the root's nine Q12 rotation coefficients for the anchored sway.
///
/// Both matrices must be live through the call. Copies only the 3x3 basis;
/// translation and the matrix's alignment halfword are retained. The caller
/// is responsible for invalidating coordinate composition.
static inline void _madChaserDangleSwayCopyRotation(MATRIX* rootMatrix, const MATRIX* rotationMatrix)
{
    rootMatrix->m[0][0] = rotationMatrix->m[0][0];
    rootMatrix->m[0][1] = rotationMatrix->m[0][1];
    rootMatrix->m[0][2] = rotationMatrix->m[0][2];
    rootMatrix->m[1][0] = rotationMatrix->m[1][0];
    rootMatrix->m[1][1] = rotationMatrix->m[1][1];
    rootMatrix->m[1][2] = rotationMatrix->m[1][2];
    rootMatrix->m[2][0] = rotationMatrix->m[2][0];
    rootMatrix->m[2][1] = rotationMatrix->m[2][1];
    rootMatrix->m[2][2] = rotationMatrix->m[2][2];
}

/// Sways the anchored root until a hit releases it into the fall.
///
/// Requires live work/model storage. Replaces root rotation with X pitch then
/// Y heading, retaining translation. Pitch uses 4096 angle units per turn:
/// a 64-frame sine cycle of amplitude 512 about -1024. Only hitTaken == 1
/// releases the anchor, consumes the hit latch, saves pitch and zeros the
/// signed-halfword fall motion before advancing. The caller marks composition
/// dirty and applies the part-6 anchor while it remains enabled.
static void _madChaserDangleSway(Task* task)
{
    enum {
        MAD_CHASER_DANGLE_PHASE_SHIFT      = 6,
        MAD_CHASER_DANGLE_SWAY_SCALE       = 16,
        MAD_CHASER_DANGLE_SWAY_SCALE_SHIFT = 7,
    };
    MadChaserWork* work;
    GfxCoord*      root;
    MATRIX         rotation;
    MATRIX*        rotationMatrix;
    MATRIX*        rootMatrix;
    s16            pitch;

    work           = task->work;
    root           = task->extra.tmd->coords;
    rotationMatrix = &rotation;
    gfxSetRotIdentity(rotationMatrix);
    pitch = ((rsin(work->frameCount << MAD_CHASER_DANGLE_PHASE_SHIFT) * MAD_CHASER_DANGLE_SWAY_SCALE) >> MAD_CHASER_DANGLE_SWAY_SCALE_SHIFT) - ACTOR_TRANSFORM_ANGLE_TURN / 4;
    RotMatrixX(pitch, rotationMatrix);
    RotMatrixY(work->rotation.vy, rotationMatrix);
    rootMatrix = &root->coord;
    _madChaserDangleSwayCopyRotation(rootMatrix, rotationMatrix);
    if (work->hitTaken == 1) {
        work->hitTaken  = 0;
        work->anchored  = 0;
        work->moveAccel = 0;
        work->moveSpeed = 0;
        work->fallPitch = pitch;
        work->subState++;
    }
}

/// Releases the ordinary-death target and fades its positional alert cry.
///
/// Requires a live enemy, model and task-owned Mad Chaser work. Releases this
/// enemy's alert claim and unlinks its target node, retaining battle rewards,
/// collision bodies and resource storage. A blast reaction hides the model,
/// resets stateFrames and selects death behavior 7 at sub-state zero; other
/// reactions advance from behavior 0 to the reward/settle step.
static void _madChaserDeathReleaseTarget(Task* task)
{
    enum {
        MAD_CHASER_DEATH_CRY_FADE_UPDATES     = 15,
        MAD_CHASER_DEATH_BLAST_PAUSE_STATE    = 7,
        MAD_CHASER_DEATH_SOUND_INSTANCE_SHIFT = 8,
    };
    Enemy*         enemy;
    MadChaserWork* work;
    TmdObject*     model;
    MadChaserWork* stateWork;

    enemy = task->spawnArg2.pointer;
    model = task->extra.tmd;
    work  = task->work;
    sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_DEATH_SOUND_INSTANCE_SHIFT) | SOUND_MAD_CHASER_ALERT_CRY, MAD_CHASER_DEATH_CRY_FADE_UPDATES);
    _madChaserSetAlertHold(task, 0);
    worldTargetUnlinkNode(&enemy->node);
    if (work->hitReaction == MAD_CHASER_HIT_REACTION_BLAST) {
        // A lethal blast skips settling and shrinking before the burst.
        work->stateFrames   = 0;
        model->flags        = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        stateWork           = task->work;
        stateWork->state    = MAD_CHASER_DEATH_BLAST_PAUSE_STATE;
        stateWork->subState = 0;
        return;
    }
    work->state = work->state + 1;
}

/// Requests a four-frame normal-rate blend into a death settle clip.
///
/// Requires initialized task-owned animation storage and a valid settle clip.
/// Reloads the live request work without advancing playback or behavior.
static inline void _madChaserDeathBlendSettle(Task* task, s16 settleClip)
{
    enum { MAD_CHASER_DEATH_SETTLE_BLEND_FRAMES = 4 };
    MadChaserWork* requestWork = task->work;

    requestWork->animBlendFrames = MAD_CHASER_DEATH_SETTLE_BLEND_FRAMES;
    requestWork->animRate        = ANIMATION_RATE_ONE;
    requestWork->animId          = settleClip;
    requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
}

/// Credits the dying enemy's rewards and starts its stance-appropriate settle pose.
///
/// Requires a live enemy, model and initialized nine-slot animation/work storage
/// in ordinary-death behavior 1. The current clip must be in 1..19 for every
/// carrier's one-based settle mapping. Clip 8 chooses clip 5 before the first
/// leap, clip 6 afterwards; other clips use that mapping. Releases one battle
/// hold with rewards, requests a four-frame normal-rate blend, ticks slots 1..8
/// and advances to behavior 2. All task, model and clip storage remain live.
static void _madChaserDeathSettle(Task* task)
{
    enum {
        MAD_CHASER_DEATH_LEAP_CLIP           = 8,
        MAD_CHASER_DEATH_LOW_SETTLE_CLIP     = 5,
        MAD_CHASER_DEATH_UPRIGHT_SETTLE_CLIP = 6,
    };
    MadChaserWork* work;
    s16            currentClip;
    s16            settleClip;

    work = task->work;
    // Reward release precedes selection and the first tick of the death pose.
    sceneReleaseBattleRefWithRewards(task, 0);
    currentClip = work->animId;
    if (currentClip == MAD_CHASER_DEATH_LEAP_CLIP) {
        if (work->hasLeaped == 0) {
            _madChaserDeathBlendSettle(task, MAD_CHASER_DEATH_LOW_SETTLE_CLIP);
        } else {
            _madChaserDeathBlendSettle(task, MAD_CHASER_DEATH_UPRIGHT_SETTLE_CLIP);
        }
    } else {
        settleClip = _gMadChaserSettleAnims[currentClip - 1];
        _madChaserDeathBlendSettle(task, settleClip);
    }
    _madChaserTickAnim(task);
    work->state++;
}
