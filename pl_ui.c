/*
   pl_ui.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] context
// [SECTION] enums
// [SECTION] internal api
// [SECTION] stb_text mess
// [SECTION] public api implementations
// [SECTION] internal api implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_ui_internal.h"
#include <float.h> // FLT_MAX

//-----------------------------------------------------------------------------
// [SECTION] context
//-----------------------------------------------------------------------------

plUiContext* gptCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

typedef enum
{
    PL_INPUT_EVENT_TYPE_NONE = 0,
    PL_INPUT_EVENT_TYPE_MOUSE_POS,
    PL_INPUT_EVENT_TYPE_MOUSE_WHEEL,
    PL_INPUT_EVENT_TYPE_MOUSE_BUTTON,
    PL_INPUT_EVENT_TYPE_KEY,
    PL_INPUT_EVENT_TYPE_TEXT,
    
    PL_INPUT_EVENT_TYPE_COUNT
} _plInputEventType;

typedef enum
{
    PL_INPUT_EVENT_SOURCE_NONE = 0,
    PL_INPUT_EVENT_SOURCE_MOUSE,
    PL_INPUT_EVENT_SOURCE_KEYBOARD,
    
    PL_INPUT_EVENT_SOURCE_COUNT
} _plInputEventSource;

//-----------------------------------------------------------------------------
// [SECTION] internal api 
//-----------------------------------------------------------------------------

static void          pl__update_events(void);
static void          pl__update_mouse_inputs(void);
static void          pl__update_keyboard_inputs(void);
static int           pl__calc_typematic_repeat_amount(float fT0, float fT1, float fRepeatDelay, float fRepeatRate);
static plInputEvent* pl__get_last_event(plInputEventType tType, int iButtonOrKey);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plKeyData*
pl_get_key_data(plKey tKey)
{
    if(tKey & PL_KEY_MOD_MASK_)
    {
        if     (tKey == PL_KEY_MOD_CTRL)  tKey = PL_KEY_RESERVED_MOD_CTRL;
        else if(tKey == PL_KEY_MOD_SHIFT) tKey = PL_KEY_RESERVED_MOD_SHIFT;
        else if(tKey == PL_KEY_MOD_ALT)   tKey = PL_KEY_RESERVED_MOD_ALT;
        else if(tKey == PL_KEY_MOD_SUPER) tKey = PL_RESERVED_KEY_MOD_SUPER;
        else if(tKey == PL_KEY_MOD_SHORTCUT) tKey = (gptCtx->tIO.bConfigMacOSXBehaviors ? PL_RESERVED_KEY_MOD_SUPER : PL_KEY_RESERVED_MOD_CTRL);
    }
    assert(tKey > PL_KEY_NONE && tKey < PL_KEY_COUNT && "Key not valid");
    return &gptCtx->tIO._tKeyData[tKey];
}

void
pl_add_key_event(plKey tKey, bool bDown)
{
    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_KEY, (int)tKey);
    if(ptLastEvent && ptLastEvent->bKeyDown == bDown)
        return;

    const plInputEvent tEvent = {
        .tType    = PL_INPUT_EVENT_TYPE_KEY,
        .tSource  = PL_INPUT_EVENT_SOURCE_KEYBOARD,
        .tKey     = tKey,
        .bKeyDown = bDown
    };
    plu_sb_push(gptCtx->tIO._sbtInputEvents, tEvent);
}

void
pl_add_text_event(uint32_t uChar)
{
    const plInputEvent tEvent = {
        .tType    = PL_INPUT_EVENT_TYPE_TEXT,
        .tSource  = PL_INPUT_EVENT_SOURCE_KEYBOARD,
        .uChar     = uChar
    };
    plu_sb_push(gptCtx->tIO._sbtInputEvents, tEvent);
}

void
pl_add_text_event_utf16(uint16_t uChar)
{
    if (uChar == 0 && gptCtx->tIO._tInputQueueSurrogate == 0)
        return;

    if ((uChar & 0xFC00) == 0xD800) // High surrogate, must save
    {
        if (gptCtx->tIO._tInputQueueSurrogate != 0)
            pl_add_text_event(0xFFFD);
        gptCtx->tIO._tInputQueueSurrogate = uChar;
        return;
    }

    plUiWChar cp = uChar;
    if (gptCtx->tIO._tInputQueueSurrogate != 0)
    {
        if ((uChar & 0xFC00) != 0xDC00) // Invalid low surrogate
        {
            pl_add_text_event(0xFFFD);
        }
        else
        {
            cp = 0xFFFD; // Codepoint will not fit in ImWchar
        }

        gptCtx->tIO._tInputQueueSurrogate = 0;
    }
    pl_add_text_event((uint32_t)cp);
}

void
pl_add_text_events_utf8(const char* pcText)
{
    while(*pcText != 0)
    {
        uint32_t uChar = 0;
        pcText += plu_text_char_from_utf8(&uChar, pcText, NULL);
        pl_add_text_event(uChar);
    }
}

void
pl_add_mouse_pos_event(float fX, float fY)
{

    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_MOUSE_POS, (int)(fX + fY));
    if(ptLastEvent && ptLastEvent->fPosX == fX && ptLastEvent->fPosY == fY)
        return;

    const plInputEvent tEvent = {
        .tType    = PL_INPUT_EVENT_TYPE_MOUSE_POS,
        .tSource  = PL_INPUT_EVENT_SOURCE_MOUSE,
        .fPosX    = fX,
        .fPosY    = fY
    };
    plu_sb_push(gptCtx->tIO._sbtInputEvents, tEvent);
}

void
pl_add_mouse_button_event(int iButton, bool bDown)
{

    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_MOUSE_BUTTON, iButton);
    if(ptLastEvent && ptLastEvent->bMouseDown == bDown)
        return;

    const plInputEvent tEvent = {
        .tType      = PL_INPUT_EVENT_TYPE_MOUSE_BUTTON,
        .tSource    = PL_INPUT_EVENT_SOURCE_MOUSE,
        .iButton    = iButton,
        .bMouseDown = bDown
    };
    plu_sb_push(gptCtx->tIO._sbtInputEvents, tEvent);
}

void
pl_add_mouse_wheel_event(float fX, float fY)
{

    const plInputEvent tEvent = {
        .tType   = PL_INPUT_EVENT_TYPE_MOUSE_WHEEL,
        .tSource = PL_INPUT_EVENT_SOURCE_MOUSE,
        .fWheelX = fX,
        .fWheelY = fY
    };
    plu_sb_push(gptCtx->tIO._sbtInputEvents, tEvent);
}

void
pl_clear_input_characters(void)
{
    plu_sb_reset(gptCtx->tIO._sbInputQueueCharacters);
}

bool
pl_is_key_down(plKey tKey)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    return ptData->bDown;
}

bool
pl_is_key_pressed(plKey tKey, bool bRepeat)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    if (!ptData->bDown) // In theory this should already be encoded as (DownDuration < 0.0f), but testing this facilitates eating mechanism (until we finish work on input ownership)
        return false;
    const float fT = ptData->fDownDuration;
    if (fT < 0.0f)
        return false;

    bool bPressed = (fT == 0.0f);
    if (!bPressed && bRepeat)
    {
        const float fRepeatDelay = gptCtx->tIO.fKeyRepeatDelay;
        const float fRepeatRate = gptCtx->tIO.fKeyRepeatRate;
        bPressed = (fT > fRepeatDelay) && pl_get_key_pressed_amount(tKey, fRepeatDelay, fRepeatRate) > 0;
    }

    if (!bPressed)
        return false;
    return true;
}

bool
pl_is_key_released(plKey tKey)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    if (ptData->fDownDurationPrev < 0.0f || ptData->bDown)
        return false;
    return true;
}

int
pl_get_key_pressed_amount(plKey tKey, float fRepeatDelay, float fRate)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    if (!ptData->bDown) // In theory this should already be encoded as (DownDuration < 0.0f), but testing this facilitates eating mechanism (until we finish work on input ownership)
        return 0;
    const float fT = ptData->fDownDuration;
    return pl__calc_typematic_repeat_amount(fT - gptCtx->tIO.fDeltaTime, fT, fRepeatDelay, fRate);
}

bool
pl_is_mouse_down(plMouseButton tButton)
{
    return gptCtx->tIO._abMouseDown[tButton];
}

bool
pl_is_mouse_clicked(plMouseButton tButton, bool bRepeat)
{
    if(!gptCtx->tIO._abMouseDown[tButton])
        return false;
    const float fT = gptCtx->tIO._afMouseDownDuration[tButton];
    if(fT == 0.0f)
        return true;
    if(bRepeat && fT > gptCtx->tIO.fKeyRepeatDelay)
        return pl__calc_typematic_repeat_amount(fT - gptCtx->tIO.fDeltaTime, fT, gptCtx->tIO.fKeyRepeatDelay, gptCtx->tIO.fKeyRepeatRate) > 0;
    return false;
}

bool
pl_is_mouse_released(plMouseButton tButton)
{
    return gptCtx->tIO._abMouseReleased[tButton];
}

bool
pl_is_mouse_double_clicked(plMouseButton tButton)
{
    return gptCtx->tIO._auMouseClickedCount[tButton] == 2;
}

bool
pl_is_mouse_dragging(plMouseButton tButton, float fThreshold)
{
    if(!gptCtx->tIO._abMouseDown[tButton])
        return false;
    if(fThreshold < 0.0f)
        fThreshold = gptCtx->tIO.fMouseDragThreshold;
    return gptCtx->tIO._afMouseDragMaxDistSqr[tButton] >= fThreshold * fThreshold;
}

bool
pl_is_mouse_hovering_rect(plVec2 minVec, plVec2 maxVec)
{
    const plVec2 tMousePos = gptCtx->tIO._tMousePos;
    return ( tMousePos.x >= minVec.x && tMousePos.y >= minVec.y && tMousePos.x <= maxVec.x && tMousePos.y <= maxVec.y);
}

void
pl_reset_mouse_drag_delta(plMouseButton tButton)
{
    gptCtx->tIO._atMouseClickedPos[tButton] = gptCtx->tIO._tMousePos;
}

plVec2
pl_get_mouse_drag_delta(plMouseButton tButton, float fThreshold)
{
    if(fThreshold < 0.0f)
        fThreshold = gptCtx->tIO.fMouseDragThreshold;
    if(gptCtx->tIO._abMouseDown[tButton] || gptCtx->tIO._abMouseReleased[tButton])
    {
        if(gptCtx->tIO._afMouseDragMaxDistSqr[tButton] >= fThreshold * fThreshold)
        {
            if(pl_is_mouse_pos_valid(gptCtx->tIO._tMousePos) && pl_is_mouse_pos_valid(gptCtx->tIO._atMouseClickedPos[tButton]))
                return plu_sub_vec2(gptCtx->tIO._tLastValidMousePos, gptCtx->tIO._atMouseClickedPos[tButton]);
        }
    }
    
    return plu_create_vec2(0.0f, 0.0f);
}

plVec2
pl_get_mouse_pos(void)
{
    return gptCtx->tIO._tMousePos;
}

float
pl_get_mouse_wheel(void)
{
    return gptCtx->tIO._fMouseWheel;
}

bool
pl_is_mouse_pos_valid(plVec2 tPos)
{
    return tPos.x > -FLT_MAX && tPos.y > -FLT_MAX;
}

void
pl_set_mouse_cursor(plMouseCursor tCursor)
{
    gptCtx->tIO.tNextCursor = tCursor;
    gptCtx->tIO.bCursorChanged = true;
}

plUiContext*
pl_create_context(void)
{
    static plUiContext tContext = {0};
    gptCtx = &tContext;
    memset(gptCtx, 0, sizeof(plUiContext));
    gptCtx->ptDrawlist = pl_memory_alloc(sizeof(plDrawList));
    gptCtx->ptDebugDrawlist = pl_memory_alloc(sizeof(plDrawList));
    memset(gptCtx->ptDrawlist, 0, sizeof(plDrawList));
    memset(gptCtx->ptDebugDrawlist, 0, sizeof(plDrawList));
    pl_register_drawlist(gptCtx->ptDrawlist);
    pl_register_drawlist(gptCtx->ptDebugDrawlist);
    gptCtx->ptBgLayer = pl_request_layer(gptCtx->ptDrawlist, "plui Background");
    gptCtx->ptFgLayer = pl_request_layer(gptCtx->ptDrawlist, "plui Foreground");
    gptCtx->ptDebugLayer = pl_request_layer(gptCtx->ptDebugDrawlist, "ui debug");
    gptCtx->tTooltipWindow.ptBgLayer = pl_request_layer(gptCtx->ptDrawlist, "plui Tooltip Background");
    gptCtx->tTooltipWindow.ptFgLayer = pl_request_layer(gptCtx->ptDrawlist, "plui Tooltip Foreground");

    gptCtx->tIO.fMouseDoubleClickTime    = 0.3f;
    gptCtx->tIO.fMouseDoubleClickMaxDist = 6.0f;
    gptCtx->tIO.fMouseDragThreshold      = 6.0f;
    gptCtx->tIO.fKeyRepeatDelay          = 0.275f;
    gptCtx->tIO.fKeyRepeatRate           = 0.050f;
    
    gptCtx->tIO.afMainFramebufferScale[0] = 1.0f;
    gptCtx->tIO.afMainFramebufferScale[1] = 1.0f;

    gptCtx->tIO.tCurrentCursor = PL_MOUSE_CURSOR_ARROW;
    gptCtx->tIO.tNextCursor = gptCtx->tIO.tCurrentCursor;
    gptCtx->tIO.afMainViewportSize[0] = 500.0f;
    gptCtx->tIO.afMainViewportSize[1] = 500.0f;
    gptCtx->tIO.bViewportSizeChanged = true;

    gptCtx->tFrameBufferScale.x = 1.0f;
    gptCtx->tFrameBufferScale.y = 1.0f;
    pl_set_dark_theme();
    return gptCtx;
}

void
pl_destroy_context(void)
{
    for(uint32_t i = 0; i < plu_sb_size(gptCtx->sbptWindows); i++)
    {
        plu_sb_free(gptCtx->sbptWindows[i]->tStorage.sbtData);
        plu_sb_free(gptCtx->sbptWindows[i]->sbuTempLayoutIndexSort);
        plu_sb_free(gptCtx->sbptWindows[i]->sbtTempLayoutSort);
        plu_sb_free(gptCtx->sbptWindows[i]->sbtRowStack);
        plu_sb_free(gptCtx->sbptWindows[i]->sbtChildWindows);
        plu_sb_free(gptCtx->sbptWindows[i]->sbtRowTemplateEntries);
        pl_memory_free(gptCtx->sbptWindows[i]);
    }

    for(uint32_t i = 0u; i < plu_sb_size(gptCtx->sbDrawlists); i++)
    {
        plDrawList* drawlist = gptCtx->sbDrawlists[i];
        for(uint32_t j = 0; j < plu_sb_size(drawlist->sbtSubmittedLayers); j++)
        {
            plu_sb_free(drawlist->sbtSubmittedLayers[j]->sbtCommandBuffer);
            plu_sb_free(drawlist->sbtSubmittedLayers[j]->sbuIndexBuffer);   
            plu_sb_free(drawlist->sbtSubmittedLayers[j]->sbtPath);  
        }

        for(uint32_t j = 0; j < plu_sb_size(drawlist->sbtLayersCreated); j++)
        {
            free(drawlist->sbtLayersCreated[j]);
        }
        plu_sb_free(drawlist->sbtDrawCommands);
        plu_sb_free(drawlist->sbtVertexBuffer);
        plu_sb_free(drawlist->sbtLayerCache);
        plu_sb_free(drawlist->sbtLayersCreated);
        plu_sb_free(drawlist->sbtSubmittedLayers);   
        plu_sb_free(drawlist->sbtClipStack);
    }
    plu_sb_free(gptCtx->sbDrawlists);

    pl_memory_free(gptCtx->ptDrawlist);
    pl_memory_free(gptCtx->ptDebugDrawlist);
    plu_sb_free(gptCtx->tWindows.sbtData);
    plu_sb_free(gptCtx->sbptWindows);
    plu_sb_free(gptCtx->sbtTabBars);
    plu_sb_free(gptCtx->sbptFocusedWindows);
    plu_sb_free(gptCtx->sbuIdStack);

    memset(gptCtx, 0, sizeof(plUiContext));
    gptCtx = NULL;
}

void
pl_set_context(plUiContext* ptCtx)
{
    gptCtx = ptCtx;
}

plUiContext*
pl_get_context(void)
{
    return gptCtx;
}

plIO*
pl_get_io(void)
{
    return &gptCtx->tIO;
}

plDrawList*
pl_get_draw_list(plUiContext* ptContext)
{
    if(ptContext == NULL)
        ptContext = gptCtx;
    return ptContext->ptDrawlist;
}

plDrawList*
pl_get_debug_draw_list(plUiContext* ptContext)
{
    if(ptContext == NULL)
        ptContext = gptCtx;
    return ptContext->ptDebugDrawlist;
}

void
pl_new_frame(void)
{

    // track click ownership
    for(uint32_t i = 0; i < 5; i++)
    {
        if(gptCtx->tIO._abMouseClicked[i])
        {
            gptCtx->tIO._abMouseOwned[i] = (gptCtx->ptHoveredWindow != NULL);
        }
    }

    // update IO structure
    gptCtx->tIO.dTime += (double)gptCtx->tIO.fDeltaTime;
    gptCtx->tIO.ulFrameCount++;
    gptCtx->tIO.bViewportSizeChanged = false;
    gptCtx->tIO.bWantTextInput = false;
    gptCtx->tIO.bWantCaptureMouse = false;
    gptCtx->tIO.bWantCaptureKeyboard = false;

    // calculate frame rate
    gptCtx->tIO._fFrameRateSecPerFrameAccum += gptCtx->tIO.fDeltaTime - gptCtx->tIO._afFrameRateSecPerFrame[gptCtx->tIO._iFrameRateSecPerFrameIdx];
    gptCtx->tIO._afFrameRateSecPerFrame[gptCtx->tIO._iFrameRateSecPerFrameIdx] = gptCtx->tIO.fDeltaTime;
    gptCtx->tIO._iFrameRateSecPerFrameIdx = (gptCtx->tIO._iFrameRateSecPerFrameIdx + 1) % 120;
    gptCtx->tIO._iFrameRateSecPerFrameCount = plu_max(gptCtx->tIO._iFrameRateSecPerFrameCount, 120);
    gptCtx->tIO.fFrameRate = FLT_MAX;
    if(gptCtx->tIO._fFrameRateSecPerFrameAccum > 0)
        gptCtx->tIO.fFrameRate = ((float) gptCtx->tIO._iFrameRateSecPerFrameCount) / gptCtx->tIO._fFrameRateSecPerFrameAccum;

    pl__update_events();
    pl__update_keyboard_inputs();
    pl__update_mouse_inputs();

    // reset drawlists
    for(uint32_t i = 0u; i < plu_sb_size(gptCtx->sbDrawlists); i++)
    {
        plDrawList* drawlist = gptCtx->sbDrawlists[i];

        drawlist->uIndexBufferByteSize = 0u;
        plu_sb_reset(drawlist->sbtDrawCommands);
        plu_sb_reset(drawlist->sbtVertexBuffer);

        // reset submitted layers
        for(uint32_t j = 0; j < plu_sb_size(drawlist->sbtSubmittedLayers); j++)
        {
            plu_sb_reset(drawlist->sbtSubmittedLayers[j]->sbtCommandBuffer);
            plu_sb_reset(drawlist->sbtSubmittedLayers[j]->sbuIndexBuffer);   
            plu_sb_reset(drawlist->sbtSubmittedLayers[j]->sbtPath);  
            drawlist->sbtSubmittedLayers[j]->uVertexCount = 0u;
            drawlist->sbtSubmittedLayers[j]->_ptLastCommand = NULL;
        }
        plu_sb_reset(drawlist->sbtSubmittedLayers);       
    }

    gptCtx->frameCount++;

    if(pl_is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        gptCtx->uNextActiveId = gptCtx->uActiveId;
}

void
pl_end_frame(void)
{

    const plVec2 tMousePos = pl_get_mouse_pos();

    // update state id's from previous frame
    gptCtx->uHoveredId = gptCtx->uNextHoveredId;
    gptCtx->uActiveId = gptCtx->uNextActiveId;
    gptCtx->tIO.bWantCaptureKeyboard = gptCtx->bWantCaptureKeyboardNextFrame;
    gptCtx->tIO.bWantCaptureMouse = gptCtx->bWantCaptureMouseNextFrame || gptCtx->tIO._abMouseOwned[0] || gptCtx->uActiveId != 0 || gptCtx->ptMovingWindow != NULL;

    // null starting state
    gptCtx->bActiveIdJustActivated = false;
    gptCtx->bWantCaptureMouseNextFrame = false;
    gptCtx->ptHoveredWindow = NULL;
    gptCtx->ptActiveWindow = NULL;
    gptCtx->ptWheelingWindow = NULL;
    gptCtx->uNextHoveredId = 0u;
    gptCtx->uNextActiveId = 0u;
    gptCtx->tPrevItemData.bHovered = false;
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;
    gptCtx->tNextWindowData.tCollapseCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tPosCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tSizeCondition = PL_UI_COND_NONE;

    if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        gptCtx->bWantCaptureMouseNextFrame = false;
        gptCtx->ptMovingWindow = NULL;
        gptCtx->ptSizingWindow = NULL;
        gptCtx->ptScrollingWindow = NULL;
        gptCtx->ptActiveWindow = NULL;
    }

    // reset active window
    if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        gptCtx->uActiveWindowId = 0;

    // submit windows in display order
    plu_sb_reset(gptCtx->sbptWindows);
    for(uint32_t i = 0; i < plu_sb_size(gptCtx->sbptFocusedWindows); i++)
    {
        plUiWindow* ptRootWindow = gptCtx->sbptFocusedWindows[i];

        if(ptRootWindow->bActive)
            pl_submit_window(ptRootWindow);

        // adjust window size if outside viewport
        if(ptRootWindow->tPos.x > gptCtx->tIO.afMainViewportSize[0])
            ptRootWindow->tPos.x = gptCtx->tIO.afMainViewportSize[0] - ptRootWindow->tSize.x / 2.0f;

        if(ptRootWindow->tPos.y > gptCtx->tIO.afMainViewportSize[1])
        {
            ptRootWindow->tPos.y = gptCtx->tIO.afMainViewportSize[1] - ptRootWindow->tSize.y / 2.0f;
            ptRootWindow->tPos.y = plu_maxf(ptRootWindow->tPos.y, 0.0f);
        }
    }

    // move newly activated window to front of focus order
    if(gptCtx->bActiveIdJustActivated)
    {
        plu_sb_top(gptCtx->sbptFocusedWindows)->uFocusOrder = gptCtx->ptActiveWindow->uFocusOrder;
        gptCtx->ptActiveWindow->uFocusOrder = plu_sb_size(gptCtx->sbptFocusedWindows) - 1;
        plu_sb_del_swap(gptCtx->sbptFocusedWindows, plu_sb_top(gptCtx->sbptFocusedWindows)->uFocusOrder);
        plu_sb_push(gptCtx->sbptFocusedWindows, gptCtx->ptActiveWindow);
    }

    // scrolling window
    if(gptCtx->ptWheelingWindow)
    {
        gptCtx->ptWheelingWindow->tScroll.y -= pl_get_mouse_wheel() * 10.0f;
        gptCtx->ptWheelingWindow->tScroll.y = plu_clampf(0.0f, gptCtx->ptWheelingWindow->tScroll.y, gptCtx->ptWheelingWindow->tScrollMax.y);
    }

    // moving window
    if(gptCtx->ptMovingWindow && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f) && !(gptCtx->ptMovingWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_MOVE))
    {

        if(tMousePos.x > 0.0f && tMousePos.x < gptCtx->tIO.afMainViewportSize[0])
            gptCtx->ptMovingWindow->tPos.x = gptCtx->ptMovingWindow->tPos.x + pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).x;

        if(tMousePos.y > 0.0f && tMousePos.y < gptCtx->tIO.afMainViewportSize[1])
            gptCtx->ptMovingWindow->tPos.y = gptCtx->ptMovingWindow->tPos.y + pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).y;  

        // clamp x
        gptCtx->ptMovingWindow->tPos.x = plu_maxf(gptCtx->ptMovingWindow->tPos.x, -gptCtx->ptMovingWindow->tSize.x / 2.0f);   
        gptCtx->ptMovingWindow->tPos.x = plu_minf(gptCtx->ptMovingWindow->tPos.x, gptCtx->tIO.afMainViewportSize[0] - gptCtx->ptMovingWindow->tSize.x / 2.0f);

        // clamp y
        gptCtx->ptMovingWindow->tPos.y = plu_maxf(gptCtx->ptMovingWindow->tPos.y, 0.0f);   
        gptCtx->ptMovingWindow->tPos.y = plu_minf(gptCtx->ptMovingWindow->tPos.y, gptCtx->tIO.afMainViewportSize[1] - 50.0f);

        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    gptCtx->tIO._fMouseWheel = 0.0f;
    gptCtx->tIO._fMouseWheelH = 0.0f;
    plu_sb_reset(gptCtx->tIO._sbInputQueueCharacters);
}

void
pl_render(void)
{
    pl_submit_layer(gptCtx->ptBgLayer);
    for(uint32_t i = 0; i < plu_sb_size(gptCtx->sbptWindows); i++)
    {
        if(gptCtx->sbptWindows[i]->uHideFrames == 0)
        {
            pl_submit_layer(gptCtx->sbptWindows[i]->ptBgLayer);
            pl_submit_layer(gptCtx->sbptWindows[i]->ptFgLayer);
        }
        else
        {
            gptCtx->sbptWindows[i]->uHideFrames--;
        }
    }
    pl_submit_layer(gptCtx->tTooltipWindow.ptBgLayer);
    pl_submit_layer(gptCtx->tTooltipWindow.ptFgLayer);
    pl_submit_layer(gptCtx->ptFgLayer);
    pl_submit_layer(gptCtx->ptDebugLayer);

    pl_end_frame();
}

void
pl_set_dark_theme(void)
{
    // styles
    gptCtx->tStyle.fTitlePadding            = 10.0f;
    gptCtx->tStyle.fFontSize                = 13.0f;
    gptCtx->tStyle.fWindowHorizontalPadding = 5.0f;
    gptCtx->tStyle.fWindowVerticalPadding   = 5.0f;
    gptCtx->tStyle.fIndentSize              = 15.0f;
    gptCtx->tStyle.fScrollbarSize           = 10.0f;
    gptCtx->tStyle.fSliderSize              = 12.0f;
    gptCtx->tStyle.tItemSpacing  = (plVec2){8.0f, 4.0f};
    gptCtx->tStyle.tInnerSpacing = (plVec2){4.0f, 4.0f};
    gptCtx->tStyle.tFramePadding = (plVec2){4.0f, 4.0f};

    // colors
    gptCtx->tColorScheme.tTitleActiveCol      = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCol          = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCollapsedCol = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tColorScheme.tWindowBgColor       = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    gptCtx->tColorScheme.tWindowBorderColor   = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tChildBgColor        = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    gptCtx->tColorScheme.tButtonCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tButtonHoveredCol    = (plVec4){0.61f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tButtonActiveCol     = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tTextCol             = (plVec4){1.00f, 1.00f, 1.00f, 1.00f};
    gptCtx->tColorScheme.tProgressBarCol      = (plVec4){0.90f, 0.70f, 0.00f, 1.00f};
    gptCtx->tColorScheme.tCheckmarkCol        = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tFrameBgCol          = (plVec4){0.23f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tFrameBgHoveredCol   = (plVec4){0.26f, 0.59f, 0.98f, 0.40f};
    gptCtx->tColorScheme.tFrameBgActiveCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.67f};
    gptCtx->tColorScheme.tHeaderCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tHeaderHoveredCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.80f};
    gptCtx->tColorScheme.tHeaderActiveCol     = (plVec4){0.26f, 0.59f, 0.98f, 1.00f};
    gptCtx->tColorScheme.tScrollbarBgCol      = (plVec4){0.05f, 0.05f, 0.05f, 0.85f};
    gptCtx->tColorScheme.tScrollbarHandleCol  = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tScrollbarFrameCol   = (plVec4){0.00f, 0.00f, 0.00f, 0.00f};
    gptCtx->tColorScheme.tScrollbarActiveCol  = gptCtx->tColorScheme.tButtonActiveCol;
    gptCtx->tColorScheme.tScrollbarHoveredCol = gptCtx->tColorScheme.tButtonHoveredCol;
}

void
pl_push_theme_color(plUiColor tColor, const plVec4* ptColor)
{
    const plUiColorStackItem tPrevItem = {
        .tIndex = tColor,
        .tColor = gptCtx->tColorScheme.atColors[tColor]
    };
    gptCtx->tColorScheme.atColors[tColor] = *ptColor;
    plu_sb_push(gptCtx->sbtColorStack, tPrevItem);
}

void
pl_pop_theme_color(uint32_t uCount)
{
    for(uint32_t i = 0; i < uCount; i++)
    {
        const plUiColorStackItem tPrevItem = plu_sb_last(gptCtx->sbtColorStack);
        gptCtx->tColorScheme.atColors[tPrevItem.tIndex] = tPrevItem.tColor;
        plu_sb_pop(gptCtx->sbtColorStack);
    }
}

void
pl_set_default_font(plFont* ptFont)
{
    gptCtx->ptFont = ptFont;
}

plFont*
pl_get_default_font(void)
{
    return gptCtx->ptFont;
}

bool
pl_begin_window(const char* pcName, bool* pbOpen, plUiWindowFlags tFlags)
{
    bool bResult = pl_begin_window_ex(pcName, pbOpen, tFlags);

    static const float pfRatios[] = {300.0f};
    if(bResult)
    {
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
    }
    else
        pl_end_window();
    return bResult;
}

void
pl_end_window(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    float fTitleBarHeight = ptWindow->tTempData.fTitleBarHeight;

    // set content sized based on last frames maximum cursor position
    if(ptWindow->bVisible)
    {
        // cursor max pos - start pos + padding
        ptWindow->tContentSize = plu_add_vec2(
            (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, 
            plu_sub_vec2(ptWindow->tTempData.tCursorMaxPos, ptWindow->tTempData.tCursorStartPos)
        
        );
    }
    ptWindow->tScrollMax = plu_sub_vec2(ptWindow->tContentSize, (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});
    
    // clamp scrolling max
    ptWindow->tScrollMax = plu_max_vec2(ptWindow->tScrollMax, (plVec2){0});
    ptWindow->bScrollbarX = ptWindow->tScrollMax.x > 0.0f;
    ptWindow->bScrollbarY = ptWindow->tScrollMax.y > 0.0f;

    if(ptWindow->bScrollbarX && ptWindow->bScrollbarY)
    {
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;
    }
    else if(!ptWindow->bScrollbarY)
        ptWindow->tScroll.y = 0;
    else if(!ptWindow->bScrollbarX)
        ptWindow->tScroll.x = 0;

    const bool bScrollBarsPresent = ptWindow->bScrollbarX || ptWindow->bScrollbarY;

    // clamp window size to min/max
    ptWindow->tSize = plu_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    // autosized non collapsed
    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE && !ptWindow->bCollapsed)
    {

        const plRect tBgRect = plu_calculate_rect(
            (plVec2){ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight},
            (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});

        // ensure window doesn't get too small
        ptWindow->tSize.x = ptWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding * 2.0f;
        ptWindow->tSize.y = fTitleBarHeight + ptWindow->tContentSize.y + gptCtx->tStyle.fWindowVerticalPadding;
        
        // clamp window size to min/max
        ptWindow->tSize = plu_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

        ptWindow->tOuterRect = plu_calculate_rect(ptWindow->tPos, ptWindow->tSize);
        ptWindow->tOuterRectClipped = ptWindow->tOuterRect;
        
        // remove scissor rect
        pl_pop_clip_rect(gptCtx->ptDrawlist);

        // draw background
        pl_add_rect_filled(ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tColorScheme.tWindowBgColor);

        ptWindow->tFullSize = ptWindow->tSize;
    }

    // regular window non collapsed
    else if(!ptWindow->bCollapsed)
    {
        plUiWindow* ptParentWindow = ptWindow->ptParentWindow;

        plRect tBgRect = plu_calculate_rect(
            (plVec2){ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight},
            (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});

        plRect tParentBgRect = ptParentWindow->tOuterRect;

        if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
        {
            tBgRect = plu_rect_clip(&tBgRect, &tParentBgRect);
        }

        pl_pop_clip_rect(gptCtx->ptDrawlist);

        const uint32_t uResizeHash = ptWindow->uId + 1;
        const uint32_t uWestResizeHash = uResizeHash + 1;
        const uint32_t uEastResizeHash = uResizeHash + 2;
        const uint32_t uNorthResizeHash = uResizeHash + 3;
        const uint32_t uSouthResizeHash = uResizeHash + 4;
        const uint32_t uVerticalScrollHash = uResizeHash + 5;
        const uint32_t uHorizonatalScrollHash = uResizeHash + 6;
        const float fRightSidePadding = ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fBottomPadding = ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fHoverPadding = 4.0f;

        // draw background
        pl_add_rect_filled(ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tColorScheme.tWindowBgColor);

        // vertical scroll bar
        if(ptWindow->bScrollbarY)
            pl_render_scrollbar(ptWindow, uVerticalScrollHash, PL_UI_AXIS_Y);

        // horizontal scroll bar
        if(ptWindow->bScrollbarX)
            pl_render_scrollbar(ptWindow, uHorizonatalScrollHash, PL_UI_AXIS_X);

        const plVec2 tTopLeft = plu_rect_top_left(&ptWindow->tOuterRect);
        const plVec2 tBottomLeft = plu_rect_bottom_left(&ptWindow->tOuterRect);
        const plVec2 tTopRight = plu_rect_top_right(&ptWindow->tOuterRect);
        const plVec2 tBottomRight = plu_rect_bottom_right(&ptWindow->tOuterRect);

        // resizing grip
        if (!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_RESIZE))
        {
            {
                const plVec2 tCornerTopLeftPos = plu_add_vec2(tBottomRight, (plVec2){-15.0f, -15.0f});
                const plVec2 tCornerTopPos = plu_add_vec2(tBottomRight, (plVec2){0.0f, -15.0f});
                const plVec2 tCornerLeftPos = plu_add_vec2(tBottomRight, (plVec2){-15.0f, 0.0f});

                const plRect tBoundingBox = plu_calculate_rect(tCornerTopLeftPos, (plVec2){15.0f, 15.0f});
                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uResizeHash)
                {
                    pl_add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.99f, 0.02f, 0.10f, 1.0f});
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
                }
                else if(gptCtx->uHoveredId == uResizeHash)
                {
                    pl_add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.66f, 0.02f, 0.10f, 1.0f});
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
                }
                else
                {
                    pl_add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.33f, 0.02f, 0.10f, 1.0f});   
                }
            }

            // east border
            {

                plRect tBoundingBox = plu_calculate_rect(tTopRight, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
                tBoundingBox = plu_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, 0.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uEastResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uEastResizeHash)
                {
                    pl_add_line(ptWindow->ptFgLayer, tTopRight, tBottomRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
                else if(gptCtx->uHoveredId == uEastResizeHash)
                {
                    pl_add_line(ptWindow->ptFgLayer, tTopRight, tBottomRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
            }

            // west border
            {
                plRect tBoundingBox = plu_calculate_rect(tTopLeft, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
                tBoundingBox = plu_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, 0.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uWestResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uWestResizeHash)
                {
                    pl_add_line(ptWindow->ptFgLayer, tTopLeft, tBottomLeft, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
                else if(gptCtx->uHoveredId == uWestResizeHash)
                {
                    pl_add_line(ptWindow->ptFgLayer, tTopLeft, tBottomLeft, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
            }

            // north border
            {
                plRect tBoundingBox = {tTopLeft, (plVec2){tTopRight.x - 15.0f, tTopRight.y}};
                tBoundingBox = plu_rect_expand_vec2(&tBoundingBox, (plVec2){0.0f, fHoverPadding / 2.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uNorthResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uNorthResizeHash)
                {
                    pl_add_line(ptWindow->ptFgLayer, tTopLeft, tTopRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
                else if(gptCtx->uHoveredId == uNorthResizeHash)
                {
                    pl_add_line(ptWindow->ptFgLayer, tTopLeft, tTopRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
            }

            // south border
            {
                plRect tBoundingBox = {tBottomLeft, (plVec2){tBottomRight.x - 15.0f, tBottomRight.y}};
                tBoundingBox = plu_rect_expand_vec2(&tBoundingBox, (plVec2){0.0f, fHoverPadding / 2.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uSouthResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uSouthResizeHash)
                {
                    pl_add_line(ptWindow->ptFgLayer, tBottomLeft, tBottomRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
                else if(gptCtx->uHoveredId == uSouthResizeHash)
                {
                    pl_add_line(ptWindow->ptFgLayer, tBottomLeft, tBottomRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
            }
        }

        // draw border
        pl_add_rect(ptWindow->ptFgLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax, gptCtx->tColorScheme.tWindowBorderColor, 1.0f);

        // handle corner resizing
        if(pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
        {
            const plVec2 tMousePos = pl_get_mouse_pos();

            if(gptCtx->uActiveId == uResizeHash)
            {  
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize = plu_sub_vec2(tMousePos, ptWindow->tPos);
                ptWindow->tSize = plu_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tScroll = plu_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle east resizing
            else if(gptCtx->uActiveId == uEastResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.x = tMousePos.x - ptWindow->tPos.x;
                ptWindow->tSize = plu_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tScroll = plu_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle west resizing
            else if(gptCtx->uActiveId == uWestResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.x = tTopRight.x - tMousePos.x;  
                ptWindow->tSize = plu_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tPos.x = tTopRight.x - ptWindow->tSize.x;
                ptWindow->tScroll = plu_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle north resizing
            else if(gptCtx->uActiveId == uNorthResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.y = tBottomRight.y - tMousePos.y;  
                ptWindow->tSize = plu_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tPos.y = tBottomRight.y - ptWindow->tSize.y;
                ptWindow->tScroll = plu_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle south resizing
            else if(gptCtx->uActiveId == uSouthResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.y = tMousePos.y - ptWindow->tPos.y;
                ptWindow->tSize = plu_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tScroll = plu_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle vertical scrolling with scroll bar
            else if(gptCtx->uActiveId == uVerticalScrollHash)
            {
                gptCtx->ptScrollingWindow = ptWindow;

                if(tMousePos.y > ptWindow->tPos.y && tMousePos.y < ptWindow->tPos.y + ptWindow->tSize.y)
                {
                    const float fScrollConversion = roundf(ptWindow->tContentSize.y / ptWindow->tSize.y);
                    ptWindow->tScroll.y += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).y * fScrollConversion;
                    ptWindow->tScroll.y = plu_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
                    pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
            }

            // handle horizontal scrolling with scroll bar
            else if(gptCtx->uActiveId == uHorizonatalScrollHash)
            {
                gptCtx->ptScrollingWindow = ptWindow;

                if(tMousePos.x > ptWindow->tPos.x && tMousePos.x < ptWindow->tPos.x + ptWindow->tSize.x)
                {
                    const float fScrollConversion = roundf(ptWindow->tContentSize.x / ptWindow->tSize.x);
                    ptWindow->tScroll.x += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fScrollConversion;
                    ptWindow->tScroll.x = plu_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);
                    pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
            }
        }
        gptCtx->ptCurrentWindow->tFullSize = ptWindow->tSize;
    }

    gptCtx->ptCurrentWindow = NULL;
    plu_sb_pop(gptCtx->sbuIdStack);
}

plDrawLayer*
pl_get_window_fg_drawlayer(void)
{
    PL_UI_ASSERT(gptCtx->ptCurrentWindow && "no current window");
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->ptFgLayer;
}

plDrawLayer*
pl_get_window_bg_drawlayer(void)
{
    PL_UI_ASSERT(gptCtx->ptCurrentWindow && "no current window");
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->ptBgLayer; 
}

plVec2
pl_get_cursor_pos(void)
{
    return pl__ui_get_cursor_pos();
}

bool
pl_begin_child(const char* pcName)
{
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptParentWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_calculate_item_size(200.0f);

    const plUiWindowFlags tFlags = 
        PL_UI_WINDOW_FLAGS_CHILD_WINDOW |
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    pl_set_next_window_size(tWidgetSize, PL_UI_COND_ALWAYS);
    bool bValue =  pl_begin_window_ex(pcName, NULL, tFlags);

    static const float pfRatios[] = {300.0f};
    if(bValue)
    {
        plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
        ptWindow->tMinSize = plu_min_vec2(ptWindow->tMinSize, tWidgetSize);
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
    }
    else
    {
        pl_end_child();
    }
    return bValue;
}

void
pl_end_child(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiWindow* ptParentWindow = ptWindow->ptParentWindow;

    // set content sized based on last frames maximum cursor position
    if(ptWindow->bVisible)
    {
        ptWindow->tContentSize = plu_add_vec2(
            (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, 
            plu_sub_vec2(ptWindow->tTempData.tCursorMaxPos, ptWindow->tTempData.tCursorStartPos)
        
        );
    }
    ptWindow->tScrollMax = plu_sub_vec2(ptWindow->tContentSize, ptWindow->tSize);
    
    // clamp scrolling max
    ptWindow->tScrollMax = plu_max_vec2(ptWindow->tScrollMax, (plVec2){0});
    ptWindow->bScrollbarX = ptWindow->tScrollMax.x > 0.0f;
    ptWindow->bScrollbarY = ptWindow->tScrollMax.y > 0.0f;

    if(ptWindow->bScrollbarX && ptWindow->bScrollbarY)
    {
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;
    }

    // clamp window size to min/max
    ptWindow->tSize = plu_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    plRect tParentBgRect = ptParentWindow->tOuterRect;
    const plRect tBgRect = plu_rect_clip(&ptWindow->tOuterRect, &tParentBgRect);

    pl_pop_clip_rect(gptCtx->ptDrawlist);

    const uint32_t uVerticalScrollHash = plu_str_hash("##scrollright", 0, plu_sb_top(gptCtx->sbuIdStack));
    const uint32_t uHorizonatalScrollHash = plu_str_hash("##scrollbottom", 0, plu_sb_top(gptCtx->sbuIdStack));

    // draw background
    pl_add_rect_filled(ptParentWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tColorScheme.tWindowBgColor);

    // vertical scroll bar
    if(ptWindow->bScrollbarY)
        pl_render_scrollbar(ptWindow, uVerticalScrollHash, PL_UI_AXIS_Y);

    // horizontal scroll bar
    if(ptWindow->bScrollbarX)
        pl_render_scrollbar(ptWindow, uHorizonatalScrollHash, PL_UI_AXIS_X);

    // handle vertical scrolling with scroll bar
    if(gptCtx->uActiveId == uVerticalScrollHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {
        const float fScrollConversion = roundf(ptWindow->tContentSize.y / ptWindow->tSize.y);
        gptCtx->ptScrollingWindow = ptWindow;
        gptCtx->uNextHoveredId = uVerticalScrollHash;
        ptWindow->tScroll.y += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).y * fScrollConversion;
        ptWindow->tScroll.y = plu_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    // handle horizontal scrolling with scroll bar
    else if(gptCtx->uActiveId == uHorizonatalScrollHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {
        const float fScrollConversion = roundf(ptWindow->tContentSize.x / ptWindow->tSize.x);
        gptCtx->ptScrollingWindow = ptWindow;
        gptCtx->uNextHoveredId = uHorizonatalScrollHash;
        ptWindow->tScroll.x += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fScrollConversion;
        ptWindow->tScroll.x = plu_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    ptWindow->tFullSize = ptWindow->tSize;
    plu_sb_pop(gptCtx->sbuIdStack);
    gptCtx->ptCurrentWindow = ptParentWindow;

    pl_advance_cursor(ptWindow->tSize.x, ptWindow->tSize.y);
}

void
pl_begin_tooltip(void)
{
    plUiWindow* ptWindow = &gptCtx->tTooltipWindow;

    ptWindow->tContentSize = plu_add_vec2(
        (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding},
        plu_sub_vec2(
            ptWindow->tTempData.tCursorMaxPos, 
            ptWindow->tTempData.tCursorStartPos)
    );

    memset(&ptWindow->tTempData, 0, sizeof(plUiTempWindowData));

    ptWindow->tFlags |= 
        PL_UI_WINDOW_FLAGS_TOOLTIP |
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_AUTO_SIZE | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    // place window at mouse position
    const plVec2 tMousePos = pl_get_mouse_pos();
    ptWindow->tTempData.tCursorStartPos = plu_add_vec2(tMousePos, (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});
    ptWindow->tPos = tMousePos;
    ptWindow->tTempData.tRowPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tMousePos.x);
    ptWindow->tTempData.tRowPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tMousePos.y);

    const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y };
    const plVec2 tEndClip = { ptWindow->tSize.x, ptWindow->tSize.y };
    pl_push_clip_rect(gptCtx->ptDrawlist, plu_calculate_rect(tStartClip, tEndClip), false);

    ptWindow->ptParentWindow = gptCtx->ptCurrentWindow;
    gptCtx->ptCurrentWindow = ptWindow;

    static const float pfRatios[] = {300.0f};
    pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
}

void
pl_end_tooltip(void)
{
    plUiWindow* ptWindow = &gptCtx->tTooltipWindow;

    ptWindow->tSize.x = ptWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding;
    ptWindow->tSize.y = ptWindow->tContentSize.y;

    pl_add_rect_filled(ptWindow->ptBgLayer,
        ptWindow->tPos, 
        plu_add_vec2(ptWindow->tPos, ptWindow->tSize), gptCtx->tColorScheme.tWindowBgColor);

    pl_pop_clip_rect(gptCtx->ptDrawlist);
    gptCtx->ptCurrentWindow = ptWindow->ptParentWindow;
}

plVec2
pl_get_window_pos(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tPos;
}

plVec2
pl_get_window_size(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tSize;
}

plVec2
pl_get_window_scroll(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tScroll;
}

plVec2
pl_get_window_scroll_max(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tScrollMax;
}

void
pl_set_window_scroll(plVec2 tScroll)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    if(ptWindow->tScrollMax.x >= tScroll.x)
        ptWindow->tScroll.x = tScroll.x;

    if(ptWindow->tScrollMax.y >= tScroll.y)
        ptWindow->tScroll.y = tScroll.y;
}

void
pl_set_next_window_pos(plVec2 tPos, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tPos = tPos;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS;
    gptCtx->tNextWindowData.tPosCondition = tCondition;
}

void
pl_set_next_window_size(plVec2 tSize, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tSize = tSize;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE;
    gptCtx->tNextWindowData.tSizeCondition = tCondition;
}

void
pl_set_next_window_collapse(bool bCollapsed, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.bCollapsed = bCollapsed;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED;
    gptCtx->tNextWindowData.tCollapseCondition = tCondition;    
}

bool
pl_step_clipper(plUiClipper* ptClipper)
{
    if(ptClipper->uItemCount == 0)
        return false;
        
    if(ptClipper->uDisplayStart == 0 && ptClipper->uDisplayEnd == 0)
    {
        ptClipper->uDisplayStart = 0;
        ptClipper->uDisplayEnd = 1;
        ptClipper->_fItemHeight = 0.0f;
        ptClipper->_fStartPosY = pl__ui_get_cursor_pos().y;
        return true;
    }
    else if (ptClipper->_fItemHeight == 0.0f)
    {
        ptClipper->_fItemHeight = pl__ui_get_cursor_pos().y - ptClipper->_fStartPosY;
        if(ptClipper->_fStartPosY < pl_get_window_pos().y)
            ptClipper->uDisplayStart = (uint32_t)((pl_get_window_pos().y - ptClipper->_fStartPosY) / ptClipper->_fItemHeight);
        ptClipper->uDisplayEnd = ptClipper->uDisplayStart + (uint32_t)(pl_get_window_size().y / ptClipper->_fItemHeight) + 1;
        ptClipper->uDisplayEnd = plu_minu(ptClipper->uDisplayEnd, ptClipper->uItemCount) + 1;
        if(ptClipper->uDisplayStart > 0)
            ptClipper->uDisplayStart--;

        if(ptClipper->uDisplayEnd > ptClipper->uItemCount)
            ptClipper->uDisplayEnd = ptClipper->uItemCount;
        
        if(ptClipper->uDisplayStart > 0)
        {
            for(uint32_t i = 0; i < gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.uColumns; i++)
                pl_advance_cursor(0.0f, (float)ptClipper->uDisplayStart * ptClipper->_fItemHeight);
        }
        ptClipper->uDisplayStart++;
        return true;
    }
    else
    {
        if(ptClipper->uDisplayEnd < ptClipper->uItemCount)
        {
            for(uint32_t i = 0; i < gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.uColumns; i++)
                pl_advance_cursor(0.0f, (float)(ptClipper->uItemCount - ptClipper->uDisplayEnd) * ptClipper->_fItemHeight);
        }

        ptClipper->uDisplayStart = 0;
        ptClipper->uDisplayEnd = 0;
        ptClipper->_fItemHeight = 0.0f;
        ptClipper->_fStartPosY = 0.0f;
        ptClipper->uItemCount = 0;
        return false;
    }
}

void
pl_layout_dynamic(float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_DYNAMIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_DYNAMIC,
        .uColumns         = uWidgetCount,
        .fWidth           = 1.0f / (float)uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_static(float fHeight, float fWidth, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_STATIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_STATIC,
        .uColumns         = uWidgetCount,
        .fWidth           = fWidth
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_row_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX,
        .uColumns         = uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_row_push(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_UI_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptCurrentRow->fWidth = fWidth;
}

void
pl_layout_row_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_UI_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptWindow->tTempData.tCursorMaxPos.x = plu_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = plu_maxf(ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_row(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ARRAY,
        .uColumns         = uWidgetCount,
        .pfSizesOrRatios  = pfSizesOrRatios
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_template_begin(float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_NONE,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE,
        .uColumns         = 0,
        .uEntryStartIndex = plu_sb_size(ptWindow->sbtRowTemplateEntries)
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_template_push_dynamic(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->uDynamicEntryCount++;
    plu_sb_add(ptWindow->sbtRowTemplateEntries);
    plu_sb_back(ptWindow->sbtRowTemplateEntries).tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC;
    plu_sb_back(ptWindow->sbtRowTemplateEntries).fWidth = 0.0f;
    ptCurrentRow->uColumns++;
}

void
pl_layout_template_push_variable(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->uVariableEntryCount++;
    ptCurrentRow->fWidth += fWidth;
    plu_sb_push(ptWindow->sbuTempLayoutIndexSort, ptCurrentRow->uColumns);
    plu_sb_add(ptWindow->sbtRowTemplateEntries);
    plu_sb_back(ptWindow->sbtRowTemplateEntries).tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_VARIABLE;
    plu_sb_back(ptWindow->sbtRowTemplateEntries).fWidth = fWidth;
    ptCurrentRow->uColumns++;
    ptWindow->tTempData.fTempMinWidth += fWidth;
}

void
pl_layout_template_push_static(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->fWidth += fWidth;
    ptCurrentRow->uStaticEntryCount++;
    plu_sb_add(ptWindow->sbtRowTemplateEntries);
    plu_sb_back(ptWindow->sbtRowTemplateEntries).tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_STATIC;
    plu_sb_back(ptWindow->sbtRowTemplateEntries).fWidth = fWidth;
    ptCurrentRow->uColumns++;
    ptWindow->tTempData.fTempStaticWidth += fWidth;
    ptWindow->tTempData.fTempMinWidth += fWidth;
}

void
pl_layout_template_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_UI_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE);
    ptWindow->tTempData.tCursorMaxPos.x = plu_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = plu_maxf(ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // total available width minus padding/spacing
    float fWidthAvailable = 0.0f;
    if(ptWindow->bScrollbarY)
        fWidthAvailable = (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - 2.0f - gptCtx->tStyle.fScrollbarSize - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
    else
        fWidthAvailable = (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);

    // simplest cast, not enough room, so nothing left to distribute to dynamic widths
    if(ptWindow->tTempData.fTempMinWidth >= fWidthAvailable)
    {
        for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
        {
            plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + i];
            if(ptEntry->tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC)
                ptEntry->fWidth = 0.0f;
        }
    }
    else if((ptCurrentRow->uDynamicEntryCount + ptCurrentRow->uVariableEntryCount) != 0)
    {

        // sort large to small (slow bubble sort, should replace later)
        bool bSwapOccured = true;
        while(bSwapOccured)
        {
            if(ptCurrentRow->uVariableEntryCount == 0)
                break;
            bSwapOccured = false;
            for(uint32_t i = 0; i < ptCurrentRow->uVariableEntryCount - 1; i++)
            {
                const uint32_t ii = ptWindow->sbuTempLayoutIndexSort[i];
                const uint32_t jj = ptWindow->sbuTempLayoutIndexSort[i + 1];
                
                plUiLayoutRowEntry* ptEntry0 = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ii];
                plUiLayoutRowEntry* ptEntry1 = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + jj];

                if(ptEntry0->fWidth < ptEntry1->fWidth)
                {
                    ptWindow->sbuTempLayoutIndexSort[i] = jj;
                    ptWindow->sbuTempLayoutIndexSort[i + 1] = ii;
                    bSwapOccured = true;
                }
            }
        }

        // add dynamic to the end
        if(ptCurrentRow->uDynamicEntryCount > 0)
        {

            // dynamic entries appended to the end so they will be "sorted" from the get go
            for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
            {
                plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + i];
                if(ptEntry->tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC)
                    plu_sb_push(ptWindow->sbuTempLayoutIndexSort, i);
            }
        }

        // organize into levels
        float fCurrentWidth = -10000.0f;
        for(uint32_t i = 0; i < ptCurrentRow->uVariableEntryCount; i++)
        {
            const uint32_t ii = ptWindow->sbuTempLayoutIndexSort[i];
            plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ii];

            if(ptEntry->fWidth == fCurrentWidth)
            {
                plu_sb_back(ptWindow->sbtTempLayoutSort).uCount++;
            }
            else
            {
                const plUiLayoutSortLevel tNewSortLevel = {
                    .fWidth      = ptEntry->fWidth,
                    .uCount      = 1,
                    .uStartIndex = i
                };
                plu_sb_push(ptWindow->sbtTempLayoutSort, tNewSortLevel);
                fCurrentWidth = ptEntry->fWidth;
            }
        }

        // add dynamic to the end
        if(ptCurrentRow->uDynamicEntryCount > 0)
        {
            const plUiLayoutSortLevel tInitialSortLevel = {
                .fWidth      = 0.0f,
                .uCount      = ptCurrentRow->uDynamicEntryCount,
                .uStartIndex = ptCurrentRow->uVariableEntryCount
            };
            plu_sb_push(ptWindow->sbtTempLayoutSort, tInitialSortLevel);
        }

        // calculate left over width
        float fExtraWidth = fWidthAvailable - ptWindow->tTempData.fTempMinWidth;

        // distribute to levels
        const uint32_t uLevelCount = plu_sb_size(ptWindow->sbtTempLayoutSort);
        if(uLevelCount == 1)
        {
            plUiLayoutSortLevel tCurrentSortLevel = plu_sb_pop(ptWindow->sbtTempLayoutSort);
            const float fDistributableWidth = fExtraWidth / (float)tCurrentSortLevel.uCount;
            for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uCount; i++)
            {
                plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                ptEntry->fWidth += fDistributableWidth;
            }
        }
        else
        {
            while(fExtraWidth > 0.0f)
            {
                plUiLayoutSortLevel tCurrentSortLevel = plu_sb_pop(ptWindow->sbtTempLayoutSort);

                if(plu_sb_size(ptWindow->sbtTempLayoutSort) == 0) // final
                {
                    const float fDistributableWidth = fExtraWidth / (float)tCurrentSortLevel.uCount;
                    for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uStartIndex + tCurrentSortLevel.uCount; i++)
                    {
                        plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                        ptEntry->fWidth += fDistributableWidth;
                    }
                    break;
                }
                    
                const float fDelta = plu_sb_back(ptWindow->sbtTempLayoutSort).fWidth - tCurrentSortLevel.fWidth;
                const float fTotalOwed = fDelta * (float)tCurrentSortLevel.uCount;
                
                if(fTotalOwed < fExtraWidth) // perform operations
                {
                    for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uStartIndex + tCurrentSortLevel.uCount; i++)
                    {
                        plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                        ptEntry->fWidth += fDelta;
                    }
                    plu_sb_back(ptWindow->sbtTempLayoutSort).uCount += tCurrentSortLevel.uCount;
                    fExtraWidth -= fTotalOwed;
                }
                else // do the best we can
                {
                    const float fDistributableWidth = fExtraWidth / (float)tCurrentSortLevel.uCount;
                    for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uStartIndex + tCurrentSortLevel.uCount; i++)
                    {
                        plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                        ptEntry->fWidth += fDistributableWidth;
                    }
                    fExtraWidth = 0.0f;
                }
            }
        }

    }

    plu_sb_reset(ptWindow->sbuTempLayoutIndexSort);
    plu_sb_reset(ptWindow->sbtTempLayoutSort);
    ptWindow->tTempData.fTempMinWidth = 0.0f;
    ptWindow->tTempData.fTempStaticWidth = 0.0f;
}

void
pl_layout_space_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fHeight : 1.0f,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_SPACE,
        .uColumns         = uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_space_push(float fX, float fY, float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    PL_UI_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptCurrentRow->fHorizontalOffset = ptCurrentRow->tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fX * ptWindow->tSize.x : fX;
    ptCurrentRow->fVerticalOffset = fY * ptCurrentRow->fSpecifiedHeight;
    ptCurrentRow->fWidth = fWidth;
    ptCurrentRow->fHeight = fHeight * ptCurrentRow->fSpecifiedHeight;
}

void
pl_layout_space_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_UI_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptWindow->tTempData.tCursorMaxPos.x = plu_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = plu_maxf(ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

bool
pl_was_last_item_hovered(void)
{
    return gptCtx->tPrevItemData.bHovered;
}

bool
pl_was_last_item_active(void)
{
    return gptCtx->tPrevItemData.bActive;
}

int
pl_get_int(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == plu_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return iDefaultValue;
    return ptIterator->iValue;
}

float
pl_get_float(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == plu_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return fDefaultValue;
    return ptIterator->fValue;
}

bool
pl_get_bool(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return pl_get_int(ptStorage, uKey, bDefaultValue ? 1 : 0) != 0;
}

void*
pl_get_ptr(plUiStorage* ptStorage, uint32_t uKey)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == plu_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return NULL;
    return ptIterator->pValue;    
}

int*
pl_get_int_ptr(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == plu_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        plu_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .iValue = iDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->iValue;
}

float*
pl_get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == plu_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        plu_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .fValue = fDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->fValue;
}

bool*
pl_get_bool_ptr(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return (bool*)pl_get_int_ptr(ptStorage, uKey, bDefaultValue ? 1 : 0);
}

void**
pl_get_ptr_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == plu_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        plu_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .pValue = pDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->pValue;
}

void
pl_set_int(plUiStorage* ptStorage, uint32_t uKey, int iValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == plu_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        plu_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .iValue = iValue}));
        return;
    }
    ptIterator->iValue = iValue;
}

void
pl_set_float(plUiStorage* ptStorage, uint32_t uKey, float fValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == plu_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        plu_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .fValue = fValue}));
        return;
    }
    ptIterator->fValue = fValue;
}

void
pl_set_bool(plUiStorage* ptStorage, uint32_t uKey, bool bValue)
{
    pl_set_int(ptStorage, uKey, bValue ? 1 : 0);
}

void
pl_set_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == plu_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        plu_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .pValue = pValue}));
        return;
    }
    ptIterator->pValue = pValue;    
}

const char*
pl_find_renderered_text_end(const char* pcText, const char* pcTextEnd)
{
    const char* pcTextDisplayEnd = pcText;
    if (!pcTextEnd)
        pcTextEnd = (const char*)-1;

    while (pcTextDisplayEnd < pcTextEnd && *pcTextDisplayEnd != '\0' && (pcTextDisplayEnd[0] != '#' || pcTextDisplayEnd[1] != '#'))
        pcTextDisplayEnd++;
    return pcTextDisplayEnd;
}

bool
pl_is_item_hoverable(const plRect* ptBox, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(gptCtx->ptHoveredWindow != gptCtx->ptCurrentWindow)
        return false;

    // hovered & active ID must be some value but not ours
    if(gptCtx->uHoveredId != 0 && gptCtx->uHoveredId != uHash)
        return false;

    // active ID must be not used or ours
    if(!(gptCtx->uActiveId == uHash || gptCtx->uActiveId == 0))
        return false;

    return pl_is_mouse_hovering_rect(ptBox->tMin, ptBox->tMax);
}

bool
pl_is_item_hoverable_circle(plVec2 tP, float fRadius, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(gptCtx->ptHoveredWindow != gptCtx->ptCurrentWindow)
        return false;

    // hovered & active ID must be some value but not ours
    if(gptCtx->uHoveredId != 0 && gptCtx->uHoveredId != uHash)
        return false;

    // active ID must be not used or ours
    if(!(gptCtx->uActiveId == uHash || gptCtx->uActiveId == 0))
        return false;
        
    return pl_does_circle_contain_point(tP, fRadius, pl_get_mouse_pos());
}

void
pl_ui_add_text(plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    pl_add_text_ex(ptLayer, ptFont, fSize, (plVec2){roundf(tP.x), roundf(tP.y)}, tColor, pcText, pl_find_renderered_text_end(pcText, pcTextEnd), fWrap);
}

void
pl_add_clipped_text(plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    pl_add_text_clipped_ex(ptLayer, ptFont, fSize, (plVec2){roundf(tP.x + 0.5f), roundf(tP.y + 0.5f)}, tMin, tMax, tColor, pcText, pl_find_renderered_text_end(pcText, pcTextEnd), fWrap);   
}

plVec2
pl_ui_calculate_text_size(plFont* font, float size, const char* text, float wrap)
{
    const char* pcTextEnd = text + strlen(text);
    return pl_calculate_text_size_ex(font, size, text, pl_find_renderered_text_end(text, pcTextEnd), wrap);  
}

bool
pl_does_triangle_contain_point(plVec2 p0, plVec2 p1, plVec2 p2, plVec2 point)
{
    bool b1 = ((point.x - p1.x) * (p0.y - p1.y) - (point.y - p1.y) * (p0.x - p1.x)) < 0.0f;
    bool b2 = ((point.x - p2.x) * (p1.y - p2.y) - (point.y - p2.y) * (p1.x - p2.x)) < 0.0f;
    bool b3 = ((point.x - p0.x) * (p2.y - p0.y) - (point.y - p0.y) * (p2.x - p0.x)) < 0.0f;
    return ((b1 == b2) && (b2 == b3));
}

plUiStorageEntry*
pl_lower_bound(plUiStorageEntry* sbtData, uint32_t uKey)
{
    plUiStorageEntry* ptFirstEntry = sbtData;
    uint32_t uCount = plu_sb_size(sbtData);
    while (uCount > 0)
    {
        uint32_t uCount2 = uCount >> 1;
        plUiStorageEntry* ptMiddleEntry = ptFirstEntry + uCount2;
        if(ptMiddleEntry->uKey < uKey)
        {
            ptFirstEntry = ++ptMiddleEntry;
            uCount -= uCount2 + 1;
        }
        else
            uCount = uCount2;
    }

    return ptFirstEntry;
}

bool
pl_begin_window_ex(const char* pcName, bool* pbOpen, plUiWindowFlags tFlags)
{
    plUiWindow* ptWindow = NULL;                          // window we are working on
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow; // parent window if there any

    // generate hashed ID
    const uint32_t uWindowID = plu_str_hash(pcName, 0, ptParentWindow ? plu_sb_top(gptCtx->sbuIdStack) : 0);
    plu_sb_push(gptCtx->sbuIdStack, uWindowID);

    // title text & title bar sizes
    const plVec2 tTextSize = pl_ui_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcName, 0.0f);
    float fTitleBarHeight = (tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    // see if window already exist in storage
    ptWindow = pl_get_ptr(&gptCtx->tWindows, uWindowID);

    // new window needs to be created
    if(ptWindow == NULL)
    {
        // allocate new window
        ptWindow = pl_memory_alloc(sizeof(plUiWindow));
        memset(ptWindow, 0, sizeof(plUiWindow));
        ptWindow->uId                     = uWindowID;
        ptWindow->pcName                  = pcName;
        ptWindow->tPos                    = (plVec2){ 200.0f, 200.0f};
        ptWindow->tMinSize                = (plVec2){ 200.0f, 200.0f};
        ptWindow->tMaxSize                = (plVec2){ 10000.0f, 10000.0f};
        ptWindow->tSize                   = (plVec2){ 500.0f, 500.0f};
        ptWindow->ptBgLayer               = pl_request_layer(gptCtx->ptDrawlist, pcName);
        ptWindow->ptFgLayer               = pl_request_layer(gptCtx->ptDrawlist, pcName);
        ptWindow->tPosAllowableFlags      = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tSizeAllowableFlags     = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tCollapseAllowableFlags = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->ptParentWindow          = (tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? ptParentWindow : ptWindow;
        ptWindow->uFocusOrder             = plu_sb_size(gptCtx->sbptFocusedWindows);
        ptWindow->tFlags                  = PL_UI_WINDOW_FLAGS_NONE;

        // add to focused windows if not a child
        if(!(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW))
        {
            plu_sb_push(gptCtx->sbptFocusedWindows, ptWindow);
            ptWindow->ptRootWindow = ptWindow;
        }
        else
            ptWindow->ptRootWindow = ptParentWindow->ptRootWindow;

        // add window to storage
        pl_set_ptr(&gptCtx->tWindows, uWindowID, ptWindow);
    }

    // seen this frame (obviously)
    ptWindow->bActive = true;
    ptWindow->tFlags = tFlags;

    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {

        plUiLayoutRow* ptCurrentRow = &ptParentWindow->tTempData.tCurrentLayoutRow;
        const plVec2 tStartPos   = pl__ui_get_cursor_pos();

        // set window position to parent window current cursor
        ptWindow->tPos = tStartPos;

        plu_sb_push(ptParentWindow->sbtChildWindows, ptWindow);
    }

    // reset per frame window temporary data
    memset(&ptWindow->tTempData, 0, sizeof(plUiTempWindowData));
    plu_sb_reset(ptWindow->sbtChildWindows);
    plu_sb_reset(ptWindow->sbtRowStack);
    plu_sb_reset(ptWindow->sbtRowTemplateEntries);

    // clamp window size to min/max
    ptWindow->tSize = plu_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    // should window collapse
    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED)
    {
        if(ptWindow->tCollapseAllowableFlags & gptCtx->tNextWindowData.tCollapseCondition)
        {
            ptWindow->bCollapsed = true;
            ptWindow->tCollapseAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    // position & size
    const plVec2 tMousePos = pl_get_mouse_pos();
    plVec2 tStartPos = ptWindow->tPos;

    // next window calls
    bool bWindowSizeSet = false;
    bool bWindowPosSet = false;
    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS)
    {
        bWindowPosSet = ptWindow->tPosAllowableFlags & gptCtx->tNextWindowData.tPosCondition;
        if(bWindowPosSet)
        {
            tStartPos = gptCtx->tNextWindowData.tPos;
            ptWindow->tPos = tStartPos;
            ptWindow->tPosAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE)
    {
        bWindowSizeSet = ptWindow->tSizeAllowableFlags & gptCtx->tNextWindowData.tSizeCondition;
        if(bWindowSizeSet)
        {
            ptWindow->tSize = gptCtx->tNextWindowData.tSize;
            if(ptWindow->tSize.x < 0.0f && ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) 
                ptWindow->tSize.x = -(ptWindow->tPos.x - ptParentWindow->tPos.x) + ptParentWindow->tSize.x - ptWindow->tSize.x - (ptParentWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f) - (ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f);
            if(ptWindow->tSize.y < 0.0f && ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) 
                ptWindow->tSize.y = -(ptWindow->tPos.y - ptParentWindow->tPos.y) + ptParentWindow->tSize.y - ptWindow->tSize.y - (ptParentWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f) - (ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f);
            
            ptWindow->tSizeAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    if(ptWindow->bCollapsed)
        ptWindow->tSize = (plVec2){ptWindow->tSize.x, fTitleBarHeight};

    // updating outer rect here but autosized windows do so again in pl_end_window(..)
    ptWindow->tOuterRect = plu_calculate_rect(ptWindow->tPos, ptWindow->tSize);
    ptWindow->tOuterRectClipped = ptWindow->tOuterRect;
    ptWindow->tInnerRect = ptWindow->tOuterRect;

    // remove scrollbars from inner rect
    if(ptWindow->bScrollbarX)
        ptWindow->tInnerRect.tMax.y -= gptCtx->tStyle.fScrollbarSize + 2.0f;
    if(ptWindow->bScrollbarY)
        ptWindow->tInnerRect.tMax.x -= gptCtx->tStyle.fScrollbarSize + 2.0f;

    // decorations
    if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR)) // has title bar
    {

        ptWindow->tInnerRect.tMin.y += fTitleBarHeight;

        // draw title bar
        plVec4 tTitleColor;
        if(ptWindow->uId == gptCtx->uActiveWindowId)
            tTitleColor = gptCtx->tColorScheme.tTitleActiveCol;
        else if(ptWindow->bCollapsed)
            tTitleColor = gptCtx->tColorScheme.tTitleBgCollapsedCol;
        else
            tTitleColor = gptCtx->tColorScheme.tTitleBgCol;
        pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, plu_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x, fTitleBarHeight}), tTitleColor);

        // draw title text
        const plVec2 titlePos = plu_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x / 2.0f - tTextSize.x / 2.0f, gptCtx->tStyle.fTitlePadding});
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, titlePos, gptCtx->tColorScheme.tTextCol, pcName, 0.0f);

        // draw close button
        const float fTitleBarButtonRadius = 8.0f;
        float fTitleButtonStartPos = fTitleBarButtonRadius * 2.0f;
        if(pbOpen)
        {
            plVec2 tCloseCenterPos = plu_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
            fTitleButtonStartPos += fTitleBarButtonRadius * 2.0f + gptCtx->tStyle.tItemSpacing.x;
            if(pl_does_circle_contain_point(tCloseCenterPos, fTitleBarButtonRadius, tMousePos) && gptCtx->ptHoveredWindow == ptWindow)
            {
                pl_add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 12);
                if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false)) gptCtx->uActiveId = 1;
                else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT)) *pbOpen = false;       
            }
            else
                pl_add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.0f, 0.0f, 1.0f}, 12);
        }

        if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_COLLAPSE))
        {
            // draw collapse button
            plVec2 tCollapsingCenterPos = plu_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
            fTitleButtonStartPos += fTitleBarButtonRadius * 2.0f + gptCtx->tStyle.tItemSpacing.x;

            if(pl_does_circle_contain_point(tCollapsingCenterPos, fTitleBarButtonRadius, tMousePos) &&  gptCtx->ptHoveredWindow == ptWindow)
            {
                pl_add_circle_filled(ptWindow->ptFgLayer, tCollapsingCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 12);

                if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
                {
                    gptCtx->uActiveId = 2;
                }
                else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
                {
                    ptWindow->bCollapsed = !ptWindow->bCollapsed;
                    if(!ptWindow->bCollapsed)
                    {
                        ptWindow->tSize = ptWindow->tFullSize;
                        if(tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE)
                            ptWindow->uHideFrames = 2;
                    }
                }
            }
            else
                pl_add_circle_filled(ptWindow->ptFgLayer, tCollapsingCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.5f, 0.0f, 1.0f}, 12);
        }

    }
    else
        fTitleBarHeight = 0.0f;

    // remove padding for inner clip rect
    ptWindow->tInnerClipRect = plu_rect_expand_vec2(&ptWindow->tInnerRect, (plVec2){-gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});

    if(!ptWindow->bCollapsed)
    {
        const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight };

        const plVec2 tInnerClip = { 
            ptWindow->tSize.x - (ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f),
            ptWindow->tSize.y - fTitleBarHeight - (ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f)
        };

        if(plu_sb_size(gptCtx->ptDrawlist->sbtClipStack) > 0)
        {
            ptWindow->tInnerClipRect = plu_rect_clip_full(&ptWindow->tInnerClipRect, &plu_sb_back(gptCtx->ptDrawlist->sbtClipStack));
            ptWindow->tOuterRectClipped = plu_rect_clip_full(&ptWindow->tOuterRectClipped, &plu_sb_back(gptCtx->ptDrawlist->sbtClipStack));
        }
        pl_push_clip_rect(gptCtx->ptDrawlist, ptWindow->tInnerClipRect, false);
    }

    // update cursors
    ptWindow->tTempData.tCursorStartPos.x = gptCtx->tStyle.fWindowHorizontalPadding + tStartPos.x - ptWindow->tScroll.x;
    ptWindow->tTempData.tCursorStartPos.y = gptCtx->tStyle.fWindowVerticalPadding + tStartPos.y + fTitleBarHeight - ptWindow->tScroll.y;
    ptWindow->tTempData.tRowPos = ptWindow->tTempData.tCursorStartPos;
    ptWindow->tTempData.tCursorStartPos = plu_floor_vec2(ptWindow->tTempData.tCursorStartPos);
    ptWindow->tTempData.fTitleBarHeight = fTitleBarHeight;

    // reset next window flags
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;

    gptCtx->ptCurrentWindow = ptWindow;
    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        ptWindow->bVisible = plu_rect_overlaps_rect(&ptWindow->tInnerClipRect, &ptParentWindow->tInnerClipRect);
        return ptWindow->bVisible && !plu_rect_is_inverted(&ptWindow->tInnerClipRect);
    }

    ptWindow->bVisible = true;
    return !ptWindow->bCollapsed;
}

void
pl_render_scrollbar(plUiWindow* ptWindow, uint32_t uHash, plUiAxis tAxis)
{
    const plRect tParentBgRect = ptWindow->ptParentWindow->tOuterRect;
    if(tAxis == PL_UI_AXIS_X)
    {
        const float fRightSidePadding = ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.x = plu_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);

        const float fScrollbarHandleSize  = plu_maxf(5.0f, floorf((ptWindow->tSize.x - fRightSidePadding) * ((ptWindow->tSize.x - fRightSidePadding) / (ptWindow->tContentSize.x))));
        const float fScrollbarHandleStart = floorf((ptWindow->tSize.x - fRightSidePadding - fScrollbarHandleSize) * (ptWindow->tScroll.x/(ptWindow->tScrollMax.x)));
        
        const plVec2 tStartPos = plu_add_vec2(ptWindow->tPos, (plVec2){fScrollbarHandleStart, ptWindow->tSize.y - gptCtx->tStyle.fScrollbarSize - 2.0f});
        
        plRect tScrollBackground = {
            plu_add_vec2(ptWindow->tPos, (plVec2){0.0f, ptWindow->tSize.y - gptCtx->tStyle.fScrollbarSize - 2.0f}),
            plu_add_vec2(ptWindow->tPos, (plVec2){ptWindow->tSize.x - fRightSidePadding, ptWindow->tSize.y - 2.0f})
        };

        if(plu_rect_overlaps_rect(&tParentBgRect, &tScrollBackground))
        {
            const plVec2 tFinalSize = {fScrollbarHandleSize, gptCtx->tStyle.fScrollbarSize};
            plRect tHandleBox = plu_calculate_rect(tStartPos, tFinalSize);
            tScrollBackground = plu_rect_clip(&tScrollBackground, &ptWindow->tOuterRectClipped);
            tHandleBox = plu_rect_clip(&tHandleBox, &ptWindow->tOuterRectClipped);

            pl_add_rect_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tColorScheme.tScrollbarBgCol);

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);   
            if(gptCtx->uActiveId == uHash)
                pl_add_rect_filled(ptWindow->ptBgLayer, tStartPos, plu_add_vec2(tStartPos, tFinalSize), gptCtx->tColorScheme.tScrollbarActiveCol);
            else if(gptCtx->uHoveredId == uHash)
                pl_add_rect_filled(ptWindow->ptBgLayer, tStartPos, plu_add_vec2(tStartPos, tFinalSize), gptCtx->tColorScheme.tScrollbarHoveredCol);
            else
                pl_add_rect_filled(ptWindow->ptBgLayer, tStartPos, plu_add_vec2(tStartPos, tFinalSize), gptCtx->tColorScheme.tScrollbarHandleCol);
        }
    }
    else if(tAxis == PL_UI_AXIS_Y)
    {
          
        const float fBottomPadding = ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fTopPadding = (ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.y = plu_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);

        const float fScrollbarHandleSize  = plu_maxf(5.0f, floorf((ptWindow->tSize.y - fTopPadding - fBottomPadding) * ((ptWindow->tSize.y - fTopPadding - fBottomPadding) / (ptWindow->tContentSize.y))));
        const float fScrollbarHandleStart = floorf((ptWindow->tSize.y - fTopPadding - fBottomPadding - fScrollbarHandleSize) * (ptWindow->tScroll.y / (ptWindow->tScrollMax.y)));

        const plVec2 tStartPos = plu_add_vec2(ptWindow->tPos, (plVec2){ptWindow->tSize.x - gptCtx->tStyle.fScrollbarSize - 2.0f, fTopPadding + fScrollbarHandleStart});
        
        plRect tScrollBackground = plu_calculate_rect(plu_add_vec2(ptWindow->tPos, 
            (plVec2){ptWindow->tSize.x - gptCtx->tStyle.fScrollbarSize - 2.0f, fTopPadding}),
            (plVec2){gptCtx->tStyle.fScrollbarSize, ptWindow->tSize.y - fBottomPadding});

        if(plu_rect_overlaps_rect(&tParentBgRect, &tScrollBackground))
        {    

            const plVec2 tFinalSize = {gptCtx->tStyle.fScrollbarSize, fScrollbarHandleSize};
            plRect tHandleBox = plu_calculate_rect(tStartPos, tFinalSize);
            tScrollBackground = plu_rect_clip(&tScrollBackground, &ptWindow->tOuterRectClipped);
            tHandleBox = plu_rect_clip(&tHandleBox, &ptWindow->tOuterRectClipped);

            // scrollbar background
            pl_add_rect_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tColorScheme.tScrollbarBgCol);

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);

            // scrollbar handle
            if(gptCtx->uActiveId == uHash) 
                pl_add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tColorScheme.tScrollbarActiveCol);
            else if(gptCtx->uHoveredId == uHash) 
                pl_add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tColorScheme.tScrollbarHoveredCol);
            else
                pl_add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tColorScheme.tScrollbarHandleCol);
        }
    }
}

plVec2
pl_calculate_item_size(float fDefaultHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    float fHeight = ptCurrentRow->fHeight;

    if(fHeight == 0.0f)
        fHeight = fDefaultHeight;

    if(ptCurrentRow->tSystemType ==  PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE)
    {
        const plUiLayoutRowEntry* ptCurrentEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptCurrentRow->uCurrentColumn];
        const plVec2 tWidgetSize = { ptCurrentEntry->fWidth, fHeight};
        return tWidgetSize;
    }
    else
    {
        // when passed array of sizes/ratios, override
        if(ptCurrentRow->pfSizesOrRatios)
            ptCurrentRow->fWidth = ptCurrentRow->pfSizesOrRatios[ptCurrentRow->uCurrentColumn];

        float fWidth = ptCurrentRow->fWidth;

        if(ptCurrentRow->tType ==  PL_UI_LAYOUT_ROW_TYPE_DYNAMIC) // width was a ratio
        {
            if(ptWindow->bScrollbarY)
                fWidth *= (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - 2.0f - gptCtx->tStyle.fScrollbarSize - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
            else
                fWidth *= (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
        }

        const plVec2 tWidgetSize = { fWidth, fHeight};
        return tWidgetSize;
    }
}

void
pl_advance_cursor(float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    ptCurrentRow->uCurrentColumn++;
    
    ptCurrentRow->fMaxWidth = plu_maxf(ptCurrentRow->fHorizontalOffset + fWidth, ptCurrentRow->fMaxWidth);
    ptCurrentRow->fMaxHeight = plu_maxf(ptCurrentRow->fMaxHeight, ptCurrentRow->fVerticalOffset + fHeight);

    // not yet at end of row
    if(ptCurrentRow->uCurrentColumn < ptCurrentRow->uColumns)
        ptCurrentRow->fHorizontalOffset += fWidth + gptCtx->tStyle.tItemSpacing.x;

    // automatic wrap
    if(ptCurrentRow->uCurrentColumn == ptCurrentRow->uColumns && ptCurrentRow->tSystemType != PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x = plu_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x);
        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y = plu_maxf(ptWindow->tTempData.tRowPos.y, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y);   

        // reset
        ptCurrentRow->uCurrentColumn = 0;
        ptCurrentRow->fMaxWidth = 0.0f;
        ptCurrentRow->fMaxHeight = 0.0f;
        ptCurrentRow->fHorizontalOffset = ptCurrentRow->fRowStartX + ptWindow->tTempData.fExtraIndent;
        ptCurrentRow->fVerticalOffset = 0.0f;
    }

    // passed end of row
    if(ptCurrentRow->uCurrentColumn > ptCurrentRow->uColumns && ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        PL_UI_ASSERT(false);
    }
}

void
pl_submit_window(plUiWindow* ptWindow)
{
    ptWindow->bActive = false; // no longer active (for next frame)

    const size_t ulCurrentFrame = gptCtx->tIO.ulFrameCount;
    // const plVec2 tMousePos = pl_get_mouse_pos();
    const float fTitleBarHeight = gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    const plRect tTitleBarHitRegion = {
        .tMin = {ptWindow->tPos.x + 2.0f, ptWindow->tPos.y + 2.0f},
        .tMax = {ptWindow->tPos.x + ptWindow->tSize.x - 2.0f, ptWindow->tPos.y + fTitleBarHeight}
    };

    plRect tBoundBox = ptWindow->tOuterRectClipped;

    // add padding for resizing from borders
    if(!(ptWindow->tFlags & (PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_AUTO_SIZE)))
        tBoundBox = plu_rect_expand(&tBoundBox, 2.0f);

    // check if window is hovered
    if(pl_is_mouse_hovering_rect(tBoundBox.tMin, tBoundBox.tMax))
    {
        gptCtx->ptHoveredWindow = ptWindow;

        // check if window is activated
        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            gptCtx->ptMovingWindow = NULL;
            gptCtx->uActiveWindowId = ptWindow->ptParentWindow->uId;
            gptCtx->bActiveIdJustActivated = true;
            gptCtx->ptActiveWindow = ptWindow->ptParentWindow;

            gptCtx->tIO._abMouseOwned[PL_MOUSE_BUTTON_LEFT] = true;

            // check if window titlebar is clicked
            if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR) && pl_is_mouse_hovering_rect(tTitleBarHitRegion.tMin, tTitleBarHitRegion.tMax))
                gptCtx->ptMovingWindow = ptWindow;
        }

        // scrolling
        if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE) && pl_get_mouse_wheel() != 0.0f)
            gptCtx->ptWheelingWindow = ptWindow;
    }

    plu_sb_push(gptCtx->sbptWindows, ptWindow);
    for(uint32_t j = 0; j < plu_sb_size(ptWindow->sbtChildWindows); j++)
        pl_submit_window(ptWindow->sbtChildWindows[j]);
}


//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__update_events(void)
{
    const uint32_t uEventCount = plu_sb_size(gptCtx->tIO._sbtInputEvents);
    for(uint32_t i = 0; i < uEventCount; i++)
    {
        plInputEvent* ptEvent = &gptCtx->tIO._sbtInputEvents[i];

        switch(ptEvent->tType)
        {
            case PL_INPUT_EVENT_TYPE_MOUSE_POS:
            {
                PL_UI_DEBUG_LOG_IO("[%Iu] IO Mouse Pos (%0.0f, %0.0f)", gptCtx->frameCount, ptEvent->fPosX, ptEvent->fPosY);

                if(ptEvent->fPosX != -FLT_MAX && ptEvent->fPosY != -FLT_MAX)
                {
                    gptCtx->tIO._tMousePos.x = ptEvent->fPosX;
                    gptCtx->tIO._tMousePos.y = ptEvent->fPosY;
                }
                break;
            }

            case PL_INPUT_EVENT_TYPE_MOUSE_WHEEL:
            {
                PL_UI_DEBUG_LOG_IO("[%Iu] IO Mouse Wheel (%0.0f, %0.0f)", gptCtx->frameCount, ptEvent->fWheelX, ptEvent->fWheelY);
                gptCtx->tIO._fMouseWheelH += ptEvent->fWheelX;
                gptCtx->tIO._fMouseWheel += ptEvent->fWheelY;
                break;
            }

            case PL_INPUT_EVENT_TYPE_MOUSE_BUTTON:
            {
                PL_UI_DEBUG_LOG_IO(ptEvent->bMouseDown ? "[%Iu] IO Mouse Button %i down" : "[%Iu] IO Mouse Button %i up", gptCtx->frameCount, ptEvent->iButton);
                assert(ptEvent->iButton >= 0 && ptEvent->iButton < PL_MOUSE_BUTTON_COUNT);
                gptCtx->tIO._abMouseDown[ptEvent->iButton] = ptEvent->bMouseDown;
                break;
            }

            case PL_INPUT_EVENT_TYPE_KEY:
            {
                if(ptEvent->tKey < PL_KEY_COUNT)
                    PL_UI_DEBUG_LOG_IO(ptEvent->bKeyDown ? "[%Iu] IO Key %i down" : "[%Iu] IO Key %i up", gptCtx->frameCount, ptEvent->tKey);
                plKey tKey = ptEvent->tKey;
                assert(tKey != PL_KEY_NONE);
                plKeyData* ptKeyData = pl_get_key_data(tKey);
                ptKeyData->bDown = ptEvent->bKeyDown;
                break;
            }

            case PL_INPUT_EVENT_TYPE_TEXT:
            {
                PL_UI_DEBUG_LOG_IO("[%Iu] IO Text (U+%08u)", gptCtx->frameCount, (uint32_t)ptEvent->uChar);
                plUiWChar uChar = (plUiWChar)ptEvent->uChar;
                plu_sb_push(gptCtx->tIO._sbInputQueueCharacters, uChar);
                break;
            }

            default:
            {
                assert(false && "unknown input event type");
                break;
            }
        }
    }
    plu_sb_reset(gptCtx->tIO._sbtInputEvents)
}

static void
pl__update_keyboard_inputs(void)
{
    gptCtx->tIO.tKeyMods = 0;
    if (pl_is_key_down(PL_KEY_LEFT_CTRL)  || pl_is_key_down(PL_KEY_RIGHT_CTRL))     { gptCtx->tIO.tKeyMods |= PL_KEY_MOD_CTRL; }
    if (pl_is_key_down(PL_KEY_LEFT_SHIFT) || pl_is_key_down(PL_KEY_RIGHT_SHIFT))    { gptCtx->tIO.tKeyMods |= PL_KEY_MOD_SHIFT; }
    if (pl_is_key_down(PL_KEY_LEFT_ALT)   || pl_is_key_down(PL_KEY_RIGHT_ALT))      { gptCtx->tIO.tKeyMods |= PL_KEY_MOD_ALT; }
    if (pl_is_key_down(PL_KEY_LEFT_SUPER) || pl_is_key_down(PL_KEY_RIGHT_SUPER))    { gptCtx->tIO.tKeyMods |= PL_KEY_MOD_SUPER; }

    gptCtx->tIO.bKeyCtrl  = (gptCtx->tIO.tKeyMods & PL_KEY_MOD_CTRL) != 0;
    gptCtx->tIO.bKeyShift = (gptCtx->tIO.tKeyMods & PL_KEY_MOD_SHIFT) != 0;
    gptCtx->tIO.bKeyAlt   = (gptCtx->tIO.tKeyMods & PL_KEY_MOD_ALT) != 0;
    gptCtx->tIO.bKeySuper = (gptCtx->tIO.tKeyMods & PL_KEY_MOD_SUPER) != 0;

    // Update keys
    for (uint32_t i = 0; i < PL_KEY_COUNT; i++)
    {
        plKeyData* ptKeyData = &gptCtx->tIO._tKeyData[i];
        ptKeyData->fDownDurationPrev = ptKeyData->fDownDuration;
        ptKeyData->fDownDuration = ptKeyData->bDown ? (ptKeyData->fDownDuration < 0.0f ? 0.0f : ptKeyData->fDownDuration + gptCtx->tIO.fDeltaTime) : -1.0f;
    }
}

static void
pl__update_mouse_inputs(void)
{
    if(pl_is_mouse_pos_valid(gptCtx->tIO._tMousePos))
    {
        gptCtx->tIO._tMousePos.x = floorf(gptCtx->tIO._tMousePos.x);
        gptCtx->tIO._tMousePos.y = floorf(gptCtx->tIO._tMousePos.y);
        gptCtx->tIO._tLastValidMousePos = gptCtx->tIO._tMousePos;
    }

    // only calculate data if the current & previous mouse position are valid
    if(pl_is_mouse_pos_valid(gptCtx->tIO._tMousePos) && pl_is_mouse_pos_valid(gptCtx->tIO._tMousePosPrev))
        gptCtx->tIO._tMouseDelta = plu_sub_vec2(gptCtx->tIO._tMousePos, gptCtx->tIO._tMousePosPrev);
    else
    {
        gptCtx->tIO._tMouseDelta.x = 0.0f;
        gptCtx->tIO._tMouseDelta.y = 0.0f;
    }
    gptCtx->tIO._tMousePosPrev = gptCtx->tIO._tMousePos;

    for(uint32_t i = 0; i < PL_MOUSE_BUTTON_COUNT; i++)
    {
        gptCtx->tIO._abMouseClicked[i] = gptCtx->tIO._abMouseDown[i] && gptCtx->tIO._afMouseDownDuration[i] < 0.0f;
        gptCtx->tIO._auMouseClickedCount[i] = 0;
        gptCtx->tIO._abMouseReleased[i] = !gptCtx->tIO._abMouseDown[i] && gptCtx->tIO._afMouseDownDuration[i] >= 0.0f;
        gptCtx->tIO._afMouseDownDurationPrev[i] = gptCtx->tIO._afMouseDownDuration[i];
        gptCtx->tIO._afMouseDownDuration[i] = gptCtx->tIO._abMouseDown[i] ? (gptCtx->tIO._afMouseDownDuration[i] < 0.0f ? 0.0f : gptCtx->tIO._afMouseDownDuration[i] + gptCtx->tIO.fDeltaTime) : -1.0f;

        if(gptCtx->tIO._abMouseClicked[i])
        {

            bool bIsRepeatedClick = false;
            if((float)(gptCtx->tIO.dTime - gptCtx->tIO._adMouseClickedTime[i]) < gptCtx->tIO.fMouseDoubleClickTime)
            {
                plVec2 tDeltaFromClickPos = plu_create_vec2(0.0f, 0.0f);
                if(pl_is_mouse_pos_valid(gptCtx->tIO._tMousePos))
                    tDeltaFromClickPos = plu_sub_vec2(gptCtx->tIO._tMousePos, gptCtx->tIO._atMouseClickedPos[i]);

                if(PLU_VEC2_LENGTH_SQR(tDeltaFromClickPos) < gptCtx->tIO.fMouseDoubleClickMaxDist * gptCtx->tIO.fMouseDoubleClickMaxDist)
                    bIsRepeatedClick = true;
            }

            if(bIsRepeatedClick)
                gptCtx->tIO._auMouseClickedLastCount[i]++;
            else
                gptCtx->tIO._auMouseClickedLastCount[i] = 1;

            gptCtx->tIO._adMouseClickedTime[i] = gptCtx->tIO.dTime;
            gptCtx->tIO._atMouseClickedPos[i] = gptCtx->tIO._tMousePos;
            gptCtx->tIO._afMouseDragMaxDistSqr[i] = 0.0f;
            gptCtx->tIO._auMouseClickedCount[i] = gptCtx->tIO._auMouseClickedLastCount[i];
        }
        else if(gptCtx->tIO._abMouseDown[i])
        {
            const plVec2 tClickPos = plu_sub_vec2(gptCtx->tIO._tLastValidMousePos, gptCtx->tIO._atMouseClickedPos[i]);
            float fDeltaSqrClickPos = PLU_VEC2_LENGTH_SQR(tClickPos);
            gptCtx->tIO._afMouseDragMaxDistSqr[i] = plu_max(fDeltaSqrClickPos, gptCtx->tIO._afMouseDragMaxDistSqr[i]);
        }


    }
}

static int
pl__calc_typematic_repeat_amount(float fT0, float fT1, float fRepeatDelay, float fRepeatRate)
{
    if(fT1 == 0.0f)
        return 1;
    if(fT0 >= fT1)
        return 0;
    if(fRepeatRate <= 0.0f)
        return (fT0 < fRepeatDelay) && (fT1 >= fRepeatDelay);
    
    const int iCountT0 = (fT0 < fRepeatDelay) ? -1 : (int)((fT0 - fRepeatDelay) / fRepeatRate);
    const int iCountT1 = (fT1 < fRepeatDelay) ? -1 : (int)((fT1 - fRepeatDelay) / fRepeatRate);
    const int iCount = iCountT1 - iCountT0;
    return iCount;
}

static plInputEvent*
pl__get_last_event(plInputEventType tType, int iButtonOrKey)
{
    const uint32_t uEventCount = plu_sb_size(gptCtx->tIO._sbtInputEvents);
    for(uint32_t i = 0; i < uEventCount; i++)
    {
        plInputEvent* ptEvent = &gptCtx->tIO._sbtInputEvents[uEventCount - i - 1];
        if(ptEvent->tType != tType)
            continue;
        if(tType == PL_INPUT_EVENT_TYPE_KEY && (int)ptEvent->tKey != iButtonOrKey)
            continue;
        else if(tType == PL_INPUT_EVENT_TYPE_MOUSE_BUTTON && ptEvent->iButton != iButtonOrKey)
            continue;
        else if(tType == PL_INPUT_EVENT_TYPE_MOUSE_POS && (int)(ptEvent->fPosX + ptEvent->fPosY) != iButtonOrKey)
            continue;
        return ptEvent;
    }
    return NULL;
}

void
pl_debug_log(const char* pcFormat, ...)
{
    if(!gptCtx->bLogActive)
        return;
        
    plu_sb_push(gptCtx->sbuLogEntries, plu_sb_size(gptCtx->sbcLogBuffer));

    va_list argptr;
    va_start(argptr, pcFormat);
    plu__sb_vsprintf(&gptCtx->sbcLogBuffer, pcFormat, argptr);
    va_end(argptr);     
}

void*
pl_memory_alloc(size_t szSize)
{
    if(gptCtx)
        gptCtx->uMemoryAllocations++;
    return malloc(szSize);
}

void
pl_memory_free(void* pMemory)
{
    if(gptCtx)
        gptCtx->uMemoryAllocations--;
    free(pMemory);
}