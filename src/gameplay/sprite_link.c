#include "gameplay/loading.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/collision.h"
#include "hud_sprites.h"
#include "loading.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "world_collision.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

/// Texture-page bits retained from a source when building the GPU draw-mode word.
enum { SPRITE_SOURCE_TEXTURE_PAGE_MASK = 0x9FF };

/// Each allocated primitive has the same packet layout as an emitted sprite.
typedef GpTpageSprt GpSprtPrim;

GpSprtPrim* Gp_SprtCursor;

/// Dual-buffer primitive list heads, indexed by `gDisplayState.drawBuffer`.
/// Allocated by `Gp_AllocSprtLists`; `Gp_SprtLists[1]` is the second half of
/// the same block.
extern GpSprtPrim* Gp_SprtLists[];

static void Gp_EmitSprts(SpriteSource* sources, SpriteBatch* batch);

static void Gp_SetSprtShadeBits(s32 arg0);

static void Gp_LinkRoomObjects(Task* task);

static s32 Gp_ViewSprtCmdEmpty(void);

static void func_800AD024(void);

static void Gp_LinkSprtCmd(SpriteSource* sources, SpriteBatch* batch);

static void func_800AD620(Task* task);

static void func_800AD65C(Task* task);

GpSprtPrim* Gp_SprtLists[2] = {
    NULL,
    NULL,
};

void Gp_LinkViewSprts(void)
{
    GameLocationKey* sess;
    s32              view;
    DisplayState*    ds;
    GpSprtPrim**     table;
    GpSprtTbl*       tbl;
    GpSprtRec*       recs;
    SpriteBatch*     batch;
    SpriteSource*    sources;

    sess          = &gGameSession->location.loc;
    view          = Gp_GetViewIndex();
    table         = Gp_SprtLists;
    ds            = &gDisplayState;
    Gp_SprtCursor = table[ds->drawBuffer];
    tbl           = Gp_SprtTables[sess->stage - 1];
    recs          = tbl->field_0[sess->area - 1];
    batch         = recs[(u8)view - 1].field_4;
    sources       = recs[(u8)view - 1].field_0.elements;
    // The first count selects decoded strips or the cached sprite background.
    if (batch->spriteCount == 0) {
        batch++;
    } else {
        ds->control.flags.imageSource = DISPLAY_IMAGE_NONE;
    }
    if (batch->firstSprite != SPRITE_BATCH_END) {
        do {
            if (batch->skipCachedPackets == 0) {
                Gp_LinkSprtCmd(sources, batch);
            }
            batch++;
        } while (batch->firstSprite != SPRITE_BATCH_END);
    }
}

static void Gp_EmitSprts(SpriteSource* sources, SpriteBatch* batch)
{
    u32             i;
    GpTpageSprt*    dest;
    SpriteSource*   texturePageSource;
    SpriteSource*   source;
    DisplayState*   ds;
    u32             maskHi;
    u32             mask;
    GpSpritePacket* sprt;
    u32             tpage;

    i                 = 0;
    dest              = gGpuPrimCursor;
    texturePageSource = sources + batch->firstSprite;
    gGpuPrimCursor    = dest + batch->spriteCount;
    if (batch->spriteCount != 0) {
        ds     = &gDisplayState;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        source = texturePageSource;
        do {
            sprt = &dest->sprt;
            // Copy source geometry and texture state, then link the merged packet by depth.
            if ((source->codeFlags & SPRITE_SOURCE_RAW_TEXTURE) == 0) {
                sprt->packed.color = GPU_PRIMITIVE_COLOR_WORD(source, 0);
            }
            tpage = texturePageSource->tpage;
            setlen(&dest->tpage, 1);
            setlen(&sprt->fields, 4);
            setcode(&sprt->fields, 0x64);
            dest->tpage.code[0] = 0xE1000000 | (tpage & SPRITE_SOURCE_TEXTURE_PAGE_MASK);
            MargePrim(dest, &sprt->fields);
            sprt->fields.code    |= source->codeFlags;
            sprt->packed.uv       = source->uv.packed;
            sprt->fields.clut     = source->clut;
            sprt->packed.position = GPU_PRIMITIVE_XY_WORD(source, 0);
            i++;
            sprt->packed.size = source->size.packed;
            texturePageSource++;
            dest->tpage.tag = (dest->tpage.tag & maskHi) | (*GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)source->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) & mask);
            *GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)source->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) =
                (*GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)source->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) & maskHi) | ((u32)dest & mask);
            dest++;
            source++;
        } while (i < batch->spriteCount);
    }
}

static void Gp_SetSprtShadeBits(s32 arg0)
{
    GameLocationKey* sess;
    s32              view;
    GpSprtPrim*      prim;
    GpSprtTbl*       tbl;
    GpSprtRec*       recs;
    SpriteBatch*     batch;
    u32              i;

    sess          = &gGameSession->location.loc;
    view          = Gp_GetViewIndex();
    Gp_SprtCursor = Gp_SprtLists[gDisplayState.drawBuffer];
    tbl           = Gp_SprtTables[sess->stage - 1];
    recs          = tbl->field_0[sess->area - 1];
    batch         = recs[(u8)view - 1].field_4;
    prim          = Gp_SprtCursor;
    if (batch->firstSprite != SPRITE_BATCH_END) {
        do {
            if (batch->skipCachedPackets == 0) {
                if (Gp_SprtLists[0] != NULL) {
                    for (i = 0; i < batch->spriteCount; i++) {
                        if (arg0 != 0) {
                            prim->sprt.fields.code |= 1;
                        } else {
                            prim->sprt.fields.code &= ~1;
                        }
                        prim++;
                    }
                }
            }
            batch++;
        } while (batch->firstSprite != SPRITE_BATCH_END);
    }
}

void Gp_AllocSprtLists(void)
{
    GameLocationKey* sess;
    u8               view;
    union {
        u32         address;
        GpSprtPrim* records;
    } count;
    s32             i;
    GpSprtRec*      recs;
    SpriteBatch*    batch;
    SpriteSource*   sources;
    SpriteSource*   source;
    s32             bufIdx;
    GpTpageSprt*    buf[2];
    GpTpageSprt*    dest;
    GpSpritePacket* sprt;
    u32             tpage;

    sess          = &gGameSession->location.loc;
    count.address = 0;
    view          = Gp_GetViewIndex();
    recs          = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    batch         = recs[view - 1].field_4;
    sources       = recs[view - 1].field_0.elements;
    while (batch->firstSprite != SPRITE_BATCH_END) {
        count.address += batch->spriteCount;
        batch++;
    }
    count.address *= 0x38;
    if (count.address == 0) {
        Gp_SprtLists[0] = NULL;
        return;
    }
    Gp_SprtLists[0] = memCalloc(count.address, 1);
    if (Gp_SprtLists[0] == NULL) {
        return;
    }
    count.address >>= 1;
    /* The byte count becomes the PS1 address of the second packet buffer. */
    {
        union {
            GpSprtPrim* records;
            u32         address;
        } half;
        half.records    = Gp_SprtLists[0];
        count.address  += half.address;
        Gp_SprtLists[1] = count.records;
        buf[0]          = Gp_SprtLists[0];
        buf[1]          = count.records;
    }
    for (batch = recs[view - 1].field_4; batch->firstSprite != SPRITE_BATCH_END; batch++) {
        if (batch->skipCachedPackets != 0) {
            continue;
        }
        for (bufIdx = 0; bufIdx < 2; bufIdx++) {
            // Snapshot the source range into both buffers with raw texture enabled.
            source = sources + batch->firstSprite;
            for (i = 0; i < batch->spriteCount; i++) {
                dest               = buf[bufIdx];
                sprt               = &dest->sprt;
                sprt->packed.color = GPU_PACK_COLOR_WORD(0, 0x80, 0, 0);
                setlen(&dest->tpage, 1);
                tpage = source->tpage;
                setlen(&dest->sprt.fields, 4);
                setcode(&dest->sprt.fields, 0x64 | SPRITE_SOURCE_RAW_TEXTURE);
                dest->tpage.code[0] = 0xE1000000 | (tpage & SPRITE_SOURCE_TEXTURE_PAGE_MASK);
                MargePrim(dest, &sprt->fields);
                sprt->fields.code    |= source->codeFlags;
                sprt->packed.uv       = source->uv.packed;
                sprt->fields.clut     = source->clut;
                sprt->packed.position = GPU_PRIMITIVE_XY_WORD(source, 0);
                sprt->packed.size     = source->size.packed;
                source++;
                buf[bufIdx]++;
            }
        }
    }
}

static void Gp_LinkRoomObjects(Task* task)
{
    GameLocationKey*        sess;
    GpRoomObjRec*           recs;
    WorldCollisionGrid*     grid;
    WorldCollisionTrigger*  list1;
    WorldCollisionTrigger*  list2;
    WorldCollisionOccluder* occluders;
    s32                     i;

    sess = &gGameSession->location.loc;
    Gp_LoadStageView();
    Gp_GridParams = NULL;
    Gp_ClearObj4AList(1);
    Gp_ClearObj4AList(0);
    Gp_ClearObj3AList(0);
    recs = Gp_RoomObjTables[sess->stage - 1]->field_0[sess->area - 1];
    if (recs != NULL) {
        grid      = recs[sess->room - 1].field_0;
        list1     = recs[sess->room - 1].field_4;
        list2     = recs[sess->room - 1].field_8;
        occluders = recs[sess->room - 1].field_C;
        if (grid != NULL) {
            // Bind the room mesh to the current view before publishing it.
            grid->viewCoord = &gGfxViewCoord;
            Gp_GridParams   = grid;
        }
        if (list1 != NULL) {
            for (i = 0;; i++) {
                list1[i].coord = &gGfxViewCoord;
                Gp_LinkObj4A(1, &list1[i]);
                list1[i].flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                if (list1[i].flags & WORLD_COLLISION_TRIGGER_LAST) {
                    break;
                }
            }
        }
        if (list2 != NULL) {
            for (i = 0;; i++) {
                list2[i].coord = &gGfxViewCoord;
                Gp_LinkObj4A(0, &list2[i]);
                list2[i].flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                if (list2[i].flags & WORLD_COLLISION_TRIGGER_LAST) {
                    break;
                }
            }
        }
        if (occluders != NULL) {
            for (i = 0;; i++) {
                Gp_LinkObj3A(0, &occluders[i]);
                occluders[i].flags |= WORLD_COLLISION_OCCLUDER_ENABLED;
                if (occluders[i].flags & WORLD_COLLISION_OCCLUDER_LAST) {
                    break;
                }
            }
        }
    }
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&gGfxViewCoord);
}

s8 Gp_FindViewIndex(s32 arg0)
{
    s16              idx;
    GameLocationKey* sess;
    s16              limit;
    u8*              bytes;

    idx   = 0;
    sess  = &gGameSession->location.loc;
    limit = Gp_ViewCountTables[sess->stage - 1]->field_0[sess->area - 1][sess->room - 1].prefix.packed;
    bytes = Gp_ViewIndexTables[sess->stage - 1]->field_0[sess->area - 1][sess->room - 1];
    if (limit > 0) {
        do {
            if (bytes[idx] == (u8)arg0) {
                return idx + 1;
            }
            idx++;
        } while (idx < limit);
    }
    return 0;
}

static s32 Gp_ViewSprtCmdEmpty(void)
{
    GameSession*     session;
    GameLocationKey* sess;
    GpSprtTbl**      tbl68;
    s32              i;
    GpViewIndexTbl*  tbl;
    u8***            mid;
    u8**             inner;
    u8*              bytes;
    u8               idx;
    GpSprtTbl*       tbl2;
    GpSprtRec**      mid2;
    GpSprtRec*       recs;

    session = gGameSession;
    tbl68   = Gp_SprtTables;
    sess    = &session->location.loc;
    i       = sess->stage - 1;
    tbl68   = &tbl68[i];
    tbl     = Gp_ViewIndexTables[i];
    mid     = tbl->field_0;
    inner   = mid[sess->area - 1];
    bytes   = inner[sess->room - 1];
    idx     = bytes[sess->view - 1];
    tbl2    = *tbl68;
    mid2    = tbl2->field_0;
    recs    = mid2[sess->area - 1];
    return recs[idx - 1].field_4->spriteCount == 0;
}

static void func_800AD024(void)
{
    RECT             rect;
    GameSession*     session;
    GameLocationKey* sess;
    GpViewIndexTbl*  tbl;
    u8***            mid;
    u8**             inner;
    u8*              bytes;
    u8               idx;
    GpSprtTbl*       tbl2;
    GpSprtRec**      mid2;
    GpSprtRec*       recs;
    SpriteDrawArea*  drawArea;
    DR_AREA*         prim;

    session  = gGameSession;
    sess     = &session->location.loc;
    tbl      = Gp_ViewIndexTables[sess->stage - 1];
    mid      = tbl->field_0;
    inner    = mid[sess->area - 1];
    bytes    = inner[sess->room - 1];
    idx      = bytes[sess->view - 1];
    tbl2     = Gp_SprtTables[sess->stage - 1];
    mid2     = tbl2->field_0;
    recs     = mid2[sess->area - 1];
    drawArea = recs[idx - 1].field_8;
    if (drawArea != NULL) {
        for (; drawArea->restoreDepth != SPRITE_DRAW_AREA_END; drawArea++) {
            // Apply the view clip before depth-sorted drawing begins.
            rect = drawArea->clipRect;
            if (gDisplayState.drawBuffer != 0) {
                rect.y += 0x110;
            }
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            SetDrawArea(prim, &rect);
            addPrim(&gGpuCurrentOt[GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt)], prim);
            // Restore the full draw buffer at this record's sorting boundary.
            if (gDisplayState.drawBuffer != 0) {
                rect.y = 0x110;
            } else {
                rect.y = 0;
            }
            rect.x         = 0;
            rect.w         = 0x140;
            rect.h         = 0xF0;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            SetDrawArea(prim, &rect);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)drawArea->restoreDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), prim);
        }
    }
}

s32 Gp_GetViewIndex(void)
{
    GameSession*     session;
    GameLocationKey* sess;
    GpViewIndexTbl*  tbl;
    u8***            mid;
    u8**             inner;
    u8*              bytes;

    session = gGameSession;
    sess    = &session->location.loc;
    tbl     = Gp_ViewIndexTables[sess->stage - 1];
    mid     = tbl->field_0;
    inner   = mid[sess->area - 1];
    bytes   = inner[sess->room - 1];
    return bytes[sess->view - 1];
}

void* Gp_GetViewSprtExtra(void)
{
    GameSession*     session;
    GameLocationKey* sess;
    GpViewIndexTbl*  tbl;
    u8***            mid;
    u8**             inner;
    u8*              bytes;
    u8               idx;
    GpSprtTbl*       tbl2;
    GpSprtRec**      mid2;
    GpSprtRec*       recs;

    session = gGameSession;
    sess    = &session->location.loc;
    tbl     = Gp_ViewIndexTables[sess->stage - 1];
    mid     = tbl->field_0;
    inner   = mid[sess->area - 1];
    bytes   = inner[sess->room - 1];
    idx     = bytes[sess->view - 1];
    tbl2    = Gp_SprtTables[sess->stage - 1];
    mid2    = tbl2->field_0;
    recs    = mid2[sess->area - 1];
    return recs[idx - 1].field_8;
}

void Gp_RoomObjState1(Task* task)
{
    if (task->spawnArg1.value != gGameSession->location.loc.view) {
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&gGfxViewCoord);
        task->spawnArg1.value = gGameSession->location.loc.view;
    }
    if (gGameSession->roomObjsDirty != 0) {
        Gp_LinkRoomObjects(task);
        gGameSession->roomObjsDirty = 0;
    }
    func_800AD024();
}

static void Gp_LinkSprtCmd(SpriteSource* sources, SpriteBatch* batch)
{
    u32           i;
    GpSprtPrim*   prim;
    SpriteSource* source;

    if (Gp_SprtLists[0] == NULL) {
        return;
    }
    prim   = Gp_SprtCursor;
    source = sources + batch->firstSprite;
    // Hiding a batch preserves its packet positions for later batches.
    for (i = 0; i < batch->spriteCount; prim++, i++, source++) {
        if (batch->hidden == 0) {
            addPrim(&gGpuCurrentOt[((u32)source->depth << gDisplayState.otDepthShift) >> 4 & 0x3FF], prim);
        }
    }
    Gp_SprtCursor = prim;
}

void func_800AD50C(Task* task)
{
    TaskFuncTable3 funcs;

    funcs = Gp_RoomObjStates;
    if (gGameSession->freezeRoomObjs == 0) {
        funcs.funcs[task->state](task);
    } else {
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
    }
}

void Gp_AllocSprtListsTask(Task* task)
{
    Gp_AllocSprtLists();
    taskKill(task);
}

void func_800AD5B8(Task* task)
{
    TaskFunc funcs[2] = { func_800AD620, func_800AD65C };

    if (gGameSession->freezeRoomObjs == 0) {
        funcs[task->state](task);
    }
}

static void func_800AD620(Task* task)
{
    s32 val;

    val = Gp_ViewSprtCmdEmpty();
    do {
        gDisplayState.control.flags.imageSource = val;
    } while (0);
    task->state++;
}

static void func_800AD65C(Task* task)
{
    DisplayState* ds;
    s32           val;

    ds = &gDisplayState;
    if ((ds->displayOwner != DISPLAY_OWNER_TASK) && (ds->skipDraw == 0)) {
        Gp_LinkViewSprts();
    } else {
        val                                     = Gp_ViewSprtCmdEmpty();
        gDisplayState.control.flags.imageSource = val;
    }
}
