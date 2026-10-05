/* Part of the Pyke flame library; see pyke_flame.h. */

#ifndef PYKE_FLAME_REDRAW_UPDATES_COORD
#define PYKE_FLAME_REDRAW_UPDATES_COORD 0
#endif

/// Per-frame task for one flying flame. `Task::spawnArg2` is the
/// `EffectWork` holding its velocity (`move`), age (`age`), width (`scale`)
/// and spin (`angle`); `Task::extra` reaches the coordinate it flies on.
/// Everything stops on cancellation (`gRoomEffectState->effectControl >=
/// 4`); with nonzero control below that threshold the flame is only redrawn.
///
/// - State 0 allocates the `PykeFlameBody`, aims the flame by rotating
///   `(0, spawnArg1 - rand(0..0x3F), 0)` through the coordinate's own matrix,
///   seeds the width and spin, links the body and falls through.
/// - State 1 flies the flame, redraws it, and on a random third of the frames
///   traces the ground under it for a splash. A category-3 contact
///   (`Gp_CountRec18Hi`, high halfword 0x30000) or living past 0x14 frames
///   releases it; hitting geometry (`func_800DE7CC`) switches to state 2
///   with a fresh velocity.
/// - State 2 coasts on that velocity with a fast-widening flame until it is
///   0x15 frames old.
static inline void pykeFlameTask(Task* task)
{
    GfxCoord       ground;
    SVECTOR        after;
    SVECTOR        before;
    GfxCoord*      coord;
    EffectWork*    work;
    PykeFlameBody* flame;
    s32            effectControl;
    u32            ang0;
    u32            ang1;
    u32            ang2;
    u32            ang3;

    flame         = (PykeFlameBody*)task->work;
    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (task->state != 0) {
            worldCollisionUnlinkBody(&flame->body);
        }
        effectKillTask(work, task);
        return;
    }
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
#if PYKE_FLAME_REDRAW_UPDATES_COORD
        actorRenderComposeCoord(coord);
#endif
        pykeFlameDrawBlob(MATRIX_TRANS(&coord->workm),
                          (work->age >> 1) + 1, work->scale, work->angle);
        return;
    }
    work->age = work->age + 1;
    switch (task->state) {
        case 0:
            flame = memCalloc(sizeof(PykeFlameBody), 0);
            if (flame == NULL) {
                work->age = 0;
                return;
            }
            task->exitCallback = pykeFlameRelease;
            /* The three halfwords are the SVECTOR `gte_rtv0` rotates in
               place, so `field_14` has to be cleared after the random pitch is
               written to `field_12`, not alongside `field_10`. */
            work->move.vx   = 0;
            ang0            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = ang0;
            work->move.vy   = (u16)task->spawnArg1.value - ((ang0 >> 16) & 0x3F);
            work->move.vz   = 0;
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            work->scale                  = (u16)task->spawnArg1.value + 0x180;
            ang1                         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle                  = (ang1 >> 16) & 0xFFF;
            task->state                  = 1;
            task->work                   = flame;
            flame->body.coord            = coord;
            flame->body.context.contacts = flame->contacts;
            flame->body.key              = PYKE_FLAME_KEY;
            flame->body.radius           = work->scale >> 1;
            gRandomLcgState              = ang1;
            flame->body.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &flame->body);
            // The allocation already zeroed the entry; LAST terminates the table.
            flame->contacts[0].flags = WORLD_COLLISION_CONTACT_LAST;
            flame->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            /* fallthrough */
        case 1:
            work->scale         = work->scale + 0x10;
            work->move.vy       = work->move.vy + 8;
            before.vx           = coord->workm.t[0];
            before.vy           = coord->workm.t[1];
            before.vz           = coord->workm.t[2];
            coord->coord.t[0]  += work->move.vx;
            coord->coord.t[1]  += work->move.vy;
            coord->coord.t[2]  += work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            after.vx = coord->workm.t[0];
            after.vy = coord->workm.t[1];
            after.vz = coord->workm.t[2];
            pykeFlameDrawBlob(MATRIX_TRANS(&coord->workm),
                              (work->age >> 1) + 1, work->scale,
                              work->angle);
            ang2            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = ang2;
            if ((u16)((ang2 >> 16) % 3) == 0 && gRoomEffectState->groundTraceEnabled != 0 &&
                Gp_TraceGroundCoord(coord, &ground) == 1) {
                pykeFlameDrawSplash(MATRIX_TRANS(&ground.workm), (s16)((work->scale * 2) / 3));
            }
            if (Gp_CountRec18Hi(flame->body.context.contacts, 0x30000) != 0) {
                worldCollisionUnlinkBody(&flame->body);
                effectKillTask(work, task);
                return;
            }
            if (func_800DE7CC(&after, &before, NULL, NULL) == 1) {
                worldCollisionUnlinkBody(&flame->body);
                task->state     = 2;
                work->move.vx   = (u32)rcos(work->angle) >> 8;
                work->move.vy   = (u32)rsin(work->angle) >> 8;
                ang3            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = ang3;
                work->move.vz   = (u32)rsin((ang3 >> 16) & 0xFFF) >> 8;
                return;
            }
            if (work->age >= 0x15) {
                worldCollisionUnlinkBody(&flame->body);
                effectKillTask(work, task);
                return;
            }
            worldCollisionClearContacts(flame->contacts);
            return;
        case 2:
            work->scale         = work->scale + 0x40;
            coord->coord.t[0]  += work->move.vx;
            coord->coord.t[1]  += work->move.vy;
            coord->coord.t[2]  += work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            pykeFlameDrawBlob(MATRIX_TRANS(&coord->workm),
                              (work->age >> 1) + 1, work->scale,
                              work->angle);
            if (work->age >= 0x15) {
                effectKillTask(work, task);
            }
            break;
    }
}
