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

SpriteDrawModePacket* Gp_SprtCursor;

/// Dual-buffer primitive list heads, indexed by `gDisplayState.drawBuffer`.
/// Allocated by `Gp_AllocSprtLists`; `Gp_SprtLists[1]` is the second half of
/// the same block.
extern SpriteDrawModePacket* Gp_SprtLists[];

static void Gp_EmitSprts(SpriteSource* sources, SpriteBatch* batch);

static void _spriteSetViewRawTexture(s32 rawTexture);

static void Gp_LinkRoomObjects(Task* task);

static s32 _spriteViewUsesImageStrips(void);

static void _spriteQueueViewDrawAreas(void);

static void _spriteLinkCachedBatch(const SpriteSource* sources, const SpriteBatch* batch);

static void _spriteInitViewBackgroundTask(Task* task);

static void func_800AD65C(Task* task);

SpriteDrawModePacket* Gp_SprtLists[2] = {
    NULL,
    NULL,
};

/// Borrows the selected sprite-view descriptor through the live room view map.
///
/// Requires loaded stage/area/room directories and a valid nonzero mapped byte
/// within the selected area's view array. The returned descriptor borrows the
/// room overlay's lifetime; the map is read again on each call.
static inline SpriteView* _spriteGetCurrentView(void)
{
    GameSession*     session;
    GameLocationKey* location;
    ViewIndexTable*  viewIndexTable;
    u8***            areaViewMaps;
    u8**             roomViewMaps;
    u8*              viewMap;
    u8               mappedViewIndex;
    SpriteAreaTable* spriteTable;
    SpriteView**     spriteAreaViews;
    SpriteView*      areaViews;

    session         = gGameSession;
    location        = &session->location.loc;
    viewIndexTable  = Gp_ViewIndexTables[location->stage - 1];
    areaViewMaps    = viewIndexTable->viewMaps;
    roomViewMaps    = areaViewMaps[location->area - 1];
    viewMap         = roomViewMaps[location->room - 1];
    mappedViewIndex = viewMap[location->view - 1];
    spriteTable     = Gp_SprtTables[location->stage - 1];
    spriteAreaViews = spriteTable->areaViews;
    areaViews       = spriteAreaViews[location->area - 1];
    return &areaViews[mappedViewIndex - 1];
}

/// Reserves and encodes one clip command in the current frame's packet arena.
static inline DR_AREA* _spriteCreateDrawAreaPacket(RECT* rect)
{
    DR_AREA* packet;

    packet         = gGpuPrimCursor;
    gGpuPrimCursor = packet + 1;
    SetDrawArea(packet, rect);
    return packet;
}

void spriteLinkViewCachedPackets(void)
{
    GameLocationKey*       location;
    s32                    mappedViewIndex;
    DisplayState*          display;
    SpriteDrawModePacket** packetBuffers;
    SpriteAreaTable*       spriteTable;
    SpriteView*            areaViews;
    SpriteBatch*           batch;
    SpriteSource*          sources;

    location        = &gGameSession->location.loc;
    mappedViewIndex = viewGetMappedIndex();
    packetBuffers   = Gp_SprtLists;
    display         = &gDisplayState;
    Gp_SprtCursor   = packetBuffers[display->drawBuffer];
    spriteTable     = Gp_SprtTables[location->stage - 1];
    areaViews       = spriteTable->areaViews[location->area - 1];
    batch           = areaViews[(u8)mappedViewIndex - 1].batches;
    sources         = areaViews[(u8)mappedViewIndex - 1].sources.elements;
    // A zero first count keeps image strips and skips the first batch before testing its end marker.
    if (batch->spriteCount == 0) {
        batch++;
    } else {
        display->control.flags.imageSource = DISPLAY_IMAGE_NONE;
    }
    if (batch->firstSprite != SPRITE_BATCH_END) {
        do {
            if (batch->skipCachedPackets == 0) {
                _spriteLinkCachedBatch(sources, batch);
            }
            batch++;
        } while (batch->firstSprite != SPRITE_BATCH_END);
    }
}

static void Gp_EmitSprts(SpriteSource* sources, SpriteBatch* batch)
{
    u32                   i;
    SpriteDrawModePacket* dest;
    SpriteSource*         texturePageSource;
    SpriteSource*         source;
    DisplayState*         ds;
    u32                   maskHi;
    u32                   mask;
    SpritePacket*         packet;
    u32                   tpage;

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
            packet = &dest->sprite;
            // Copy source geometry and texture state, then link the merged packet by depth.
            if ((source->codeFlags & SPRITE_SOURCE_RAW_TEXTURE) == 0) {
                packet->packed.color = GPU_PRIMITIVE_COLOR_WORD(source, 0);
            }
            tpage = texturePageSource->tpage;
            setlen(&dest->drawMode, 1);
            setlen(&packet->sprt, 4);
            setcode(&packet->sprt, 0x64);
            dest->drawMode.code[0] = 0xE1000000 | (tpage & SPRITE_SOURCE_TEXTURE_PAGE_MASK);
            MargePrim(dest, &packet->sprt);
            packet->sprt.code      |= source->codeFlags;
            packet->packed.uv       = source->uv.packed;
            packet->sprt.clut       = source->clut;
            packet->packed.position = GPU_PRIMITIVE_XY_WORD(source, 0);
            i++;
            packet->packed.size = source->size.packed;
            texturePageSource++;
            dest->drawMode.tag = (dest->drawMode.tag & maskHi) | (*GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)source->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) & mask);
            *GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)source->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) =
                (*GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)source->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) & maskHi) | ((u32)dest & mask);
            dest++;
            source++;
        } while (i < batch->spriteCount);
    }
}

/// Enables or disables raw texture on the current buffer's cached view sprites.
///
/// Nonzero `rawTexture` ignores packet RGB; zero enables colour modulation.
/// Requires live view resources and initialized cached packets. Excluded batches
/// consume no packets, while hidden batches are updated too. Only the selected
/// draw buffer changes; the other buffer and source flags retain their values.
/// Leaves the published cached-packet cursor at the selected buffer's start.
static void _spriteSetViewRawTexture(s32 rawTexture)
{
    GameLocationKey*      location;
    s32                   mappedViewIndex;
    SpriteDrawModePacket* packet;
    SpriteAreaTable*      spriteTable;
    SpriteView*           areaViews;
    SpriteBatch*          batch;
    u32                   spriteIndex;

    location        = &gGameSession->location.loc;
    mappedViewIndex = viewGetMappedIndex();
    Gp_SprtCursor   = Gp_SprtLists[gDisplayState.drawBuffer];
    spriteTable     = Gp_SprtTables[location->stage - 1];
    areaViews       = spriteTable->areaViews[location->area - 1];
    batch           = areaViews[(u8)mappedViewIndex - 1].batches;
    packet          = Gp_SprtCursor;
    if (batch->firstSprite != SPRITE_BATCH_END) {
        do {
            if (batch->skipCachedPackets == 0) {
                if (Gp_SprtLists[0] != NULL) {
                    for (spriteIndex = 0; spriteIndex < batch->spriteCount; spriteIndex++) {
                        if (rawTexture != 0) {
                            packet->sprite.sprt.code |= SPRITE_SOURCE_RAW_TEXTURE;
                        } else {
                            packet->sprite.sprt.code &= ~SPRITE_SOURCE_RAW_TEXTURE;
                        }
                        packet++;
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
        u32                   address;
        SpriteDrawModePacket* records;
    } count;
    s32                   i;
    SpriteView*           recs;
    SpriteBatch*          batch;
    SpriteSource*         sources;
    SpriteSource*         source;
    s32                   bufIdx;
    SpriteDrawModePacket* buf[2];
    SpriteDrawModePacket* dest;
    SpritePacket*         packet;
    u32                   tpage;

    sess          = &gGameSession->location.loc;
    count.address = 0;
    view          = viewGetMappedIndex();
    recs          = Gp_SprtTables[sess->stage - 1]->areaViews[sess->area - 1];
    batch         = recs[view - 1].batches;
    sources       = recs[view - 1].sources.elements;
    while (batch->firstSprite != SPRITE_BATCH_END) {
        count.address += batch->spriteCount;
        batch++;
    }
    count.address *= 2 * sizeof(SpriteDrawModePacket); // one packet per sprite in each of the two lists
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
            SpriteDrawModePacket* records;
            u32                   address;
        } half;
        half.records    = Gp_SprtLists[0];
        count.address  += half.address;
        Gp_SprtLists[1] = count.records;
        buf[0]          = Gp_SprtLists[0];
        buf[1]          = count.records;
    }
    for (batch = recs[view - 1].batches; batch->firstSprite != SPRITE_BATCH_END; batch++) {
        if (batch->skipCachedPackets != 0) {
            continue;
        }
        for (bufIdx = 0; bufIdx < 2; bufIdx++) {
            // Snapshot the source range into both buffers with raw texture enabled.
            source = sources + batch->firstSprite;
            for (i = 0; i < batch->spriteCount; i++) {
                dest                 = buf[bufIdx];
                packet               = &dest->sprite;
                packet->packed.color = GPU_PACK_COLOR_WORD(0, 0x80, 0, 0);
                setlen(&dest->drawMode, 1);
                tpage = source->tpage;
                setlen(&dest->sprite.sprt, 4);
                setcode(&dest->sprite.sprt, 0x64 | SPRITE_SOURCE_RAW_TEXTURE);
                dest->drawMode.code[0] = 0xE1000000 | (tpage & SPRITE_SOURCE_TEXTURE_PAGE_MASK);
                MargePrim(dest, &packet->sprt);
                packet->sprt.code      |= source->codeFlags;
                packet->packed.uv       = source->uv.packed;
                packet->sprt.clut       = source->clut;
                packet->packed.position = GPU_PRIMITIVE_XY_WORD(source, 0);
                packet->packed.size     = source->size.packed;
                source++;
                buf[bufIdx]++;
            }
        }
    }
}

static void Gp_LinkRoomObjects(Task* task)
{
    GameLocationKey*                   sess;
    const WorldCollisionRoomResources* roomResources;
    WorldCollisionGrid*                grid;
    WorldCollisionTrigger*             viewBoundaryTriggers;
    WorldCollisionTrigger*             actionTriggers;
    WorldCollisionOccluder*            occluders;
    s32                                i;

    sess = &gGameSession->location.loc;
    Gp_LoadStageView();
    Gp_GridParams = NULL;
    Gp_ClearObj4AList(1);
    Gp_ClearObj4AList(0);
    Gp_ClearObj3AList(0);
    roomResources = Gp_RoomObjTables[sess->stage - 1]->areaRooms[sess->area - 1];
    if (roomResources != NULL) {
        grid                 = roomResources[sess->room - 1].grid;
        viewBoundaryTriggers = roomResources[sess->room - 1].viewBoundaryTriggers;
        actionTriggers       = roomResources[sess->room - 1].actionTriggers;
        occluders            = roomResources[sess->room - 1].occluders;
        if (grid != NULL) {
            // Bind the room mesh to the current view before publishing it.
            grid->viewCoord = &gGfxViewCoord;
            Gp_GridParams   = grid;
        }
        if (viewBoundaryTriggers != NULL) {
            for (i = 0;; i++) {
                viewBoundaryTriggers[i].coord = &gGfxViewCoord;
                Gp_LinkObj4A(1, &viewBoundaryTriggers[i]);
                viewBoundaryTriggers[i].flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                if (viewBoundaryTriggers[i].flags & WORLD_COLLISION_TRIGGER_LAST) {
                    break;
                }
            }
        }
        if (actionTriggers != NULL) {
            for (i = 0;; i++) {
                actionTriggers[i].coord = &gGfxViewCoord;
                Gp_LinkObj4A(0, &actionTriggers[i]);
                actionTriggers[i].flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                if (actionTriggers[i].flags & WORLD_COLLISION_TRIGGER_LAST) {
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
    actorRenderComposeCoord(&gGfxViewCoord);
}

s8 Gp_FindViewIndex(s32 arg0)
{
    s16              idx;
    GameLocationKey* sess;
    ViewCount        viewCount;
    u8*              viewMap;

    idx       = 0;
    sess      = &gGameSession->location.loc;
    viewCount = Gp_ViewCountTables[sess->stage - 1]->viewCounts[sess->area - 1][sess->room - 1];
    viewMap   = Gp_ViewIndexTables[sess->stage - 1]->viewMaps[sess->area - 1][sess->room - 1];
    if (viewCount > 0) {
        do {
            if (viewMap[idx] == (u8)arg0) {
                return idx + 1;
            }
            idx++;
        } while (idx < viewCount);
    }
    return 0;
}

/// Returns 1 to select decoded-image strips, or 0 to suppress that background.
///
/// Tests only the mapped view's first batch count, including a terminal first
/// record; it does not count sprites or test the whole list for emptiness.
/// Requires valid loaded stage/area/room/view indices and a first batch record.
static s32 _spriteViewUsesImageStrips(void)
{
    GameSession*      session;
    GameLocationKey*  location;
    SpriteAreaTable** stageSpriteEntry;
    s32               stageIndex;
    ViewIndexTable*   viewIndexTable;
    u8***             areaViewMaps;
    u8**              roomViewMaps;
    u8*               viewMap;
    u8                mappedViewIndex;
    SpriteAreaTable*  spriteTable;
    SpriteView**      spriteAreaViews;
    SpriteView*       areaViews;

    session          = gGameSession;
    stageSpriteEntry = Gp_SprtTables;
    location         = &session->location.loc;
    stageIndex       = location->stage - 1;
    stageSpriteEntry = &stageSpriteEntry[stageIndex];
    viewIndexTable   = Gp_ViewIndexTables[stageIndex];
    areaViewMaps     = viewIndexTable->viewMaps;
    roomViewMaps     = areaViewMaps[location->area - 1];
    viewMap          = roomViewMaps[location->room - 1];
    mappedViewIndex  = viewMap[location->view - 1];
    spriteTable      = *stageSpriteEntry;
    spriteAreaViews  = spriteTable->areaViews;
    areaViews        = spriteAreaViews[location->area - 1];
    return areaViews[mappedViewIndex - 1].batches->spriteCount == 0;
}

/// Queues the current view's clipping areas and depth-sorted full-buffer restores.
///
/// Each nonterminal draw-area record consumes two `DR_AREA` packets from the
/// current frame arena. Coordinates are pixels in the fixed 320x240 view;
/// buffer 1 adds its 272-row VRAM origin. The clip is linked at the last depth
/// tag, then the full-buffer restore at the scaled, masked `restoreDepth`.
/// Requires live view resources, sufficient packet storage and a 1024-tag OT.
static void _spriteQueueViewDrawAreas(void)
{
    enum {
        SPRITE_VIEW_WIDTH_PIXELS    = 320,
        SPRITE_VIEW_HEIGHT_PIXELS   = 240,
        SPRITE_VIEW_BUFFER_Y_STRIDE = 272
    };
    RECT            rect;
    SpriteDrawArea* drawArea;
    DR_AREA*        drawAreaPacket;

    drawArea = _spriteGetCurrentView()->drawAreas;
    if (drawArea != NULL) {
        for (; drawArea->restoreDepth != SPRITE_DRAW_AREA_END; drawArea++) {
            // Apply the view clip before depth-sorted drawing begins.
            rect = drawArea->clipRect;
            if (gDisplayState.drawBuffer != 0) {
                rect.y += SPRITE_VIEW_BUFFER_Y_STRIDE;
            }
            drawAreaPacket = _spriteCreateDrawAreaPacket(&rect);
            addPrim(&gGpuCurrentOt[GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt)], drawAreaPacket);
            // Restore the full draw buffer at this record's sorting boundary.
            if (gDisplayState.drawBuffer != 0) {
                rect.y = SPRITE_VIEW_BUFFER_Y_STRIDE;
            } else {
                rect.y = 0;
            }
            rect.x         = 0;
            rect.w         = SPRITE_VIEW_WIDTH_PIXELS;
            rect.h         = SPRITE_VIEW_HEIGHT_PIXELS;
            drawAreaPacket = _spriteCreateDrawAreaPacket(&rect);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)drawArea->restoreDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), drawAreaPacket);
        }
    }
}

s32 viewGetMappedIndex(void)
{
    const GameSession*     session;
    const GameLocationKey* location;
    u8***                  areaViewMaps;
    u8**                   roomViewMaps;
    const u8*              viewMap;

    session      = gGameSession;
    location     = &session->location.loc;
    areaViewMaps = Gp_ViewIndexTables[location->stage - 1]->viewMaps;
    roomViewMaps = areaViewMaps[location->area - 1];
    viewMap      = roomViewMaps[location->room - 1];
    return viewMap[location->view - 1];
}

SpriteDrawArea* spriteGetViewDrawAreas(void)
{
    return _spriteGetCurrentView()->drawAreas;
}

void Gp_RoomObjState1(Task* task)
{
    if (task->spawnArg1.value != gGameSession->location.loc.view) {
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        task->spawnArg1.value = gGameSession->location.loc.view;
    }
    if (gGameSession->roomObjsDirty != 0) {
        Gp_LinkRoomObjects(task);
        gGameSession->roomObjsDirty = 0;
    }
    _spriteQueueViewDrawAreas();
}

/// Links one included batch from the cached packet cursor at current source depths.
///
/// `firstSprite` and `spriteCount` select a valid source range in elements.
/// Requires initialized packets and a 1024-tag depth-sorted ordering table.
/// Hidden batches advance the cursor without linking; a missing allocation
/// leaves the cursor intact. The caller skips `skipCachedPackets` batches.
static void _spriteLinkCachedBatch(const SpriteSource* sources, const SpriteBatch* batch)
{
    u32                   spriteIndex;
    SpriteDrawModePacket* packet;
    const SpriteSource*   source;

    if (Gp_SprtLists[0] == NULL) {
        return;
    }
    packet = Gp_SprtCursor;
    source = sources + batch->firstSprite;
    // Hiding a batch preserves its packet positions for later batches.
    for (spriteIndex = 0; spriteIndex < batch->spriteCount; packet++, spriteIndex++, source++) {
        if (batch->hidden == 0) {
            addPrim(&gGpuCurrentOt[((u32)source->depth << gDisplayState.otDepthShift) >> 4 & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)], packet);
        }
    }
    Gp_SprtCursor = packet;
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
    TaskFunc funcs[2] = { _spriteInitViewBackgroundTask, func_800AD65C };

    if (gGameSession->freezeRoomObjs == 0) {
        funcs[task->state](task);
    }
}

/// Selects the mapped view's initial background source and advances to linking.
///
/// The first batch count selects decoded strips (zero) or no image (nonzero).
/// Called in state 0 by the unfrozen view-sprite task; advances to state 1.
static void _spriteInitViewBackgroundTask(Task* task)
{
    s32 useImageStrips;

    useImageStrips                          = _spriteViewUsesImageStrips();
    gDisplayState.control.flags.imageSource = useImageStrips;
    task->state++;
}

static void func_800AD65C(Task* task)
{
    DisplayState* ds;
    s32           val;

    ds = &gDisplayState;
    if ((ds->displayOwner != DISPLAY_OWNER_TASK) && (ds->skipDraw == 0)) {
        spriteLinkViewCachedPackets();
    } else {
        val                                     = _spriteViewUsesImageStrips();
        gDisplayState.control.flags.imageSource = val;
    }
}
