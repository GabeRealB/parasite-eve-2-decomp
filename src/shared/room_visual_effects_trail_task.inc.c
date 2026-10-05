/* Function body for the overlay's twin-trail task entry (Task* task).
 * GCC 2.8.1 changes argument spills when this particular body is inlined.
 * Including it in the entry preserves the matched code without extra calls or
 * register/volatile/assembly workarounds. Private drawing helpers remain in
 * the other room_visual_effects fragments. */

/// A twin trail. The first tick allocates sixteen coordinate frames, eight for
/// each trail, and seeds them all from the two points offset from the anchor,
/// so both trails start collapsed. Each later tick re-places the two points,
/// records them in the next slot of each ring of eight and draws the trails
/// between the rings as a beam. The work block is released once the tick count
/// reaches the spawn argument. It idles while the room's event state is 2 or
/// more.

GfxCoord    coord;
GfxCoord*   coords;
GfxCoord*   objCoord;
GfxCoord*   dst;
EffectWork* work;
SVECTOR*    vec;
s32         i;

coords   = task->work;
work     = (EffectWork*)task->spawnArg2.pointer;
objCoord = task->extra.coordBody->coord;

if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
    work->age++;
    switch (task->state) {
        case 0:
            coords = memCalloc(sizeof(GfxCoord[16]), 0);
            if (coords == NULL) {
                work->age = 0;
                return;
            }
            task->work             = coords;
            objCoord->parent       = work->parent;
            objCoord->coord.t[0]   = RoomFx_TrailOffsets[0].vx;
            objCoord->coord.t[1]   = RoomFx_TrailOffsets[0].vy;
            objCoord->coord.t[2]   = RoomFx_TrailOffsets[0].vz;
            objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(objCoord);
            task->state        = 1;
            coord.parent       = work->parent;
            vec                = &RoomFx_TrailOffsets[1];
            coord.coord.t[0]   = vec->vx;
            coord.coord.t[1]   = vec->vy;
            coord.coord.t[2]   = vec->vz;
            coord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&coord);
            for (i = 0; i < 8; i++) {
                dst         = &coords[i];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[i + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
            }
            break;

        case 1:
            objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(objCoord);
            coord.parent = work->parent;
            {
                SVECTOR* edge    = &RoomFx_TrailOffsets[1];
                coord.coord.t[0] = edge->vx;
                coord.coord.t[1] = edge->vy;
                coord.coord.t[2] = edge->vz;
            }
            coord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&coord);
            dst         = &coords[work->age & 7];
            dst->parent = &gGfxViewCoord;
            dst->workm  = objCoord->workm;
            gte_SetRotMatrix(&objCoord->workm);
            gte_SetTransMatrix(&objCoord->workm);
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
            dst         = &coords[(work->age & 7) + 8];
            dst->parent = &gGfxViewCoord;
            dst->workm  = coord.workm;
            gte_SetRotMatrix(&coord.workm);
            gte_SetTransMatrix(&coord.workm);
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
            for (i = 0; i < 8; i++) {
                dst               = &coords[i];
                dst->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(dst);
                dst               = &coords[i + 8];
                dst->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(dst);
            }
            _roomVisualEffectsDrawTwinTrail(coords, &coords[8], work->age & 7, 0x123);
            if (work->age == task->spawnArg1.value && work->age != 0) {
                effectKillTask(work, task);
            }
            break;
    }
}
