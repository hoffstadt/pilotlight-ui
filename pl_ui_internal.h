/*
   pl_ui_internal.h
   - FORWARD COMPATIBILITY NOT GUARANTEED
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] helper macros
// [SECTION] defines
// [SECTION] forward declarations
// [SECTION] context
// [SECTION] enums
// [SECTION] math
// [SECTION] stretchy buffer internal
// [SECTION] stretchy buffer
// [SECTION] internal structs
// [SECTION] plUiStorage
// [SECTION] plUiWindow
// [SECTION] plUiContext
// [SECTION] internal api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UI_INTERNAL_H
#define PL_UI_INTERNAL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint32_t
#include <stdlib.h>  // malloc, free
#include <string.h>  // memset, memmove
#include <stdbool.h> // bool
#include <stdarg.h>  // arg vars
#include <stdio.h>   // vsprintf
#include <assert.h>
#include <math.h>
#include "pl_ui.h"

//-----------------------------------------------------------------------------
// [SECTION] helper macros
//-----------------------------------------------------------------------------

#define plu_sprintf sprintf
#define plu_vsprintf vsprintf
#define plu_vnsprintf vsnprintf

#ifndef PL_UI_ASSERT
    #include <assert.h>
    #define PL_UI_ASSERT(x) assert((x))
#endif

// stb
#undef STB_TEXTEDIT_STRING
#undef STB_TEXTEDIT_CHARTYPE
#define STB_TEXTEDIT_STRING           plUiInputTextState
#define STB_TEXTEDIT_CHARTYPE         plUiWChar
#define STB_TEXTEDIT_GETWIDTH_NEWLINE (-1.0f)
#define STB_TEXTEDIT_UNDOSTATECOUNT   99
#define STB_TEXTEDIT_UNDOCHARCOUNT    999
#include "stb_textedit.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

// Helper: Unicode defines
#define PL_UNICODE_CODEPOINT_INVALID 0xFFFD     // Invalid Unicode code point (standard value).
#define PL_UNICODE_CODEPOINT_MAX     0xFFFF     // Maximum Unicode code point supported by this build.

#define PLU_PI_2 1.57079632f // pi/2
#define PLU_2PI  6.28318530f // pi

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plUiStyle          plUiStyle;
typedef struct _plUiColorScheme    plUiColorScheme;
typedef struct _plUiWindow         plUiWindow;
typedef struct _plUiTabBar         plUiTabBar;
typedef struct _plUiPrevItemData   plUiPrevItemData;
typedef struct _plUiNextWindowData plUiNextWindowData;
typedef struct _plUiTempWindowData plUiTempWindowData;
typedef struct _plUiStorage        plUiStorage;
typedef struct _plUiStorageEntry   plUiStorageEntry;
typedef struct _plUiInputTextState   plUiInputTextState;

// enums
typedef int plUiNextWindowFlags;
typedef int plUiWindowFlags;
typedef int plUiAxis;
typedef int plUiLayoutRowEntryType;
typedef int plUiLayoutSystemType;
typedef int plDebugLogFlags;

//-----------------------------------------------------------------------------
// [SECTION] context
//-----------------------------------------------------------------------------

extern plUiContext* gptCtx;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plDebugLogFlags_
{
    PL_UI_DEBUG_LOG_FLAGS_NONE            = 0,
    PL_UI_DEBUG_LOG_FLAGS_EVENT_IO        = 1 << 0,
    PL_UI_DEBUG_LOG_FLAGS_EVENT_ACTIVE_ID = 1 << 1,
};

enum plUiAxis_
{
    PL_UI_AXIS_NONE = -1,
    PL_UI_AXIS_X    =  0,
    PL_UI_AXIS_Y    =  1,
};

enum plUiWindowFlags_
{
    PL_UI_WINDOW_FLAGS_NONE         = 0,
    PL_UI_WINDOW_FLAGS_NO_TITLE_BAR = 1 << 0,
    PL_UI_WINDOW_FLAGS_NO_RESIZE    = 1 << 1,
    PL_UI_WINDOW_FLAGS_NO_MOVE      = 1 << 2,
    PL_UI_WINDOW_FLAGS_NO_COLLAPSE  = 1 << 3,
    PL_UI_WINDOW_FLAGS_AUTO_SIZE    = 1 << 4,
    PL_UI_WINDOW_FLAGS_CHILD_WINDOW = 1 << 5,
    PL_UI_WINDOW_FLAGS_TOOLTIP      = 1 << 6,
};

enum _plUiNextWindowFlags
{
    PL_NEXT_WINDOW_DATA_FLAGS_NONE          = 0,
    PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS       = 1 << 0,
    PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE      = 1 << 1,
    PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED = 1 << 2,   
};

enum plUiLayoutRowEntryType_
{
    PL_UI_LAYOUT_ROW_ENTRY_TYPE_NONE,
    PL_UI_LAYOUT_ROW_ENTRY_TYPE_VARIABLE,
    PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC,
    PL_UI_LAYOUT_ROW_ENTRY_TYPE_STATIC
};

enum plUiLayoutSystemType_
{
    PL_UI_LAYOUT_SYSTEM_TYPE_NONE,
    PL_UI_LAYOUT_SYSTEM_TYPE_DYNAMIC,
    PL_UI_LAYOUT_SYSTEM_TYPE_STATIC,
    PL_UI_LAYOUT_SYSTEM_TYPE_SPACE,
    PL_UI_LAYOUT_SYSTEM_TYPE_ARRAY,
    PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE,
    PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX
};

enum plUiInputTextFlags_ // borrowed from "Dear ImGui"
{
    PL_UI_INPUT_TEXT_FLAGS_NONE                    = 0,
    PL_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL           = 1 << 0,   // allow 0123456789.+-*/
    PL_UI_INPUT_TEXT_FLAGS_CHARS_HEXADECIMAL       = 1 << 1,   // allow 0123456789ABCDEFabcdef
    PL_UI_INPUT_TEXT_FLAGS_CHARS_UPPERCASE         = 1 << 2,   // turn a..z into A..Z
    PL_UI_INPUT_TEXT_FLAGS_CHARS_NO_BLANK          = 1 << 3,   // filter out spaces, tabs
    PL_UI_INPUT_TEXT_FLAGS_AUTO_SELECT_ALL         = 1 << 4,   // select entire text when first taking mouse focus
    PL_UI_INPUT_TEXT_FLAGS_ENTER_RETURNS_TRUE      = 1 << 5,   // return 'true' when Enter is pressed (as opposed to every time the value was modified). Consider looking at the IsItemDeactivatedAfterEdit() function.
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_COMPLETION     = 1 << 6,   // callback on pressing TAB (for completion handling)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_HISTORY        = 1 << 7,   // callback on pressing Up/Down arrows (for history handling)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_ALWAYS         = 1 << 8,   // callback on each iteration. User code may query cursor position, modify text buffer.
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_CHAR_FILTER    = 1 << 9,   // callback on character inputs to replace or discard them. Modify 'EventChar' to replace or discard, or return 1 in callback to discard.
    PL_UI_INPUT_TEXT_FLAGS_ALLOW_TAB_INPUT         = 1 << 10,  // pressing TAB input a '\t' character into the text field
    PL_UI_INPUT_TEXT_FLAGS_CTRL_ENTER_FOR_NEW_LINE = 1 << 11,  // in multi-line mode, unfocus with Enter, add new line with Ctrl+Enter (default is opposite: unfocus with Ctrl+Enter, add line with Enter).
    PL_UI_INPUT_TEXT_FLAGS_NO_HORIZONTAL_SCROLL    = 1 << 12,  // disable following the cursor horizontally
    PL_UI_INPUT_TEXT_FLAGS_ALWAYS_OVERWRITE        = 1 << 13,  // overwrite mode
    PL_UI_INPUT_TEXT_FLAGS_READ_ONLY               = 1 << 14,  // read-only mode
    PL_UI_INPUT_TEXT_FLAGS_PASSWORD                = 1 << 15,  // password mode, display all characters as '*'
    PL_UI_INPUT_TEXT_FLAGS_NO_UNDO_REDO            = 1 << 16,  // disable undo/redo. Note that input text owns the text data while active, if you want to provide your own undo/redo stack you need e.g. to call ClearActiveID().
    PL_UI_INPUT_TEXT_FLAGS_CHARS_SCIENTIFIC        = 1 << 17,  // allow 0123456789.+-*/eE (Scientific notation input)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_RESIZE         = 1 << 18,  // callback on buffer capacity changes request (beyond 'buf_size' parameter value), allowing the string to grow. Notify when the string wants to be resized (for string types which hold a cache of their Size). You will be provided a new BufSize in the callback and NEED to honor it. (see misc/cpp/imgui_stdlib.h for an example of using this)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_EDIT           = 1 << 19,  // callback on any edit (note that InputText() already returns true on edit, the callback is useful mainly to manipulate the underlying buffer while focus is active)
    PL_UI_INPUT_TEXT_FLAGS_ESCAPE_CLEARS_ALL       = 1 << 20,  // escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
    
    // internal
    PL_UI_INPUT_TEXT_FLAGS_MULTILINE      = 1 << 21,  // escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
    PL_UI_INPUT_TEXT_FLAGS_NO_MARK_EDITED = 1 << 22,  // escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
};

//-----------------------------------------------------------------------------
// [SECTION] math
//-----------------------------------------------------------------------------

#define plu_create_vec2(XARG, YARG)                  (plVec2){(XARG), (YARG)}
#define plu_create_vec3(XARG, YARG, ZARG)            (plVec3){(XARG), (YARG), (ZARG)}
#define plu_create_vec4(XARG, YARG, ZARG, WARG)      (plVec4){(XARG), (YARG), (ZARG), (WARG)}
#define plu_create_mat4_diag(XARG, YARG, ZARG, WARG) (plMat4){.x11 = (XARG), .x22 = (YARG), .x33 = (ZARG), .x44 = (WARG)}
#define plu_create_mat4_cols(XARG, YARG, ZARG, WARG) (plMat4){.col[0] = (XARG), .col[1] = (YARG), .col[2] = (ZARG), .col[3] = (WARG)}
#define plu_create_rect_vec2(XARG, YARG)             (plRect){.tMin = (XARG), .tMax = (YARG)}
#define plu_create_rect(XARG, YARG, ZARG, WARG)      (plRect){.tMin = {.x = (XARG), .y = (YARG)}, .tMax = {.x = (ZARG), .y = (WARG)}}

#define plu_max(Value1, Value2) ((Value1) > (Value2) ? (Value1) : (Value2))
#define plu_min(Value1, Value2) ((Value1) > (Value2) ? (Value2) : (Value1))

static inline float    plu_maxf    (float fValue1, float fValue2)            { return fValue1 > fValue2 ? fValue1 : fValue2; }
static inline float    plu_minf    (float fValue1, float fValue2)            { return fValue1 > fValue2 ? fValue2 : fValue1; }
static inline uint32_t plu_minu    (uint32_t uValue1, uint32_t uValue2)      { return uValue1 > uValue2 ? uValue2 : uValue1; }
static inline int      plu_clampi  (int iMin, int iValue, int iMax)          { if (iValue < iMin) return iMin; else if (iValue > iMax) return iMax; return iValue; }
static inline float    plu_clampf  (float fMin, float fValue, float fMax)    { if (fValue < fMin) return fMin; else if (fValue > fMax) return fMax; return fValue; }

// unary ops
static inline plVec2 plu_floor_vec2       (plVec2 tVec)                             { return plu_create_vec2(floorf(tVec.x), floorf(tVec.y));}
static inline plVec2 plu_clamp_vec2       (plVec2 tMin, plVec2 tValue, plVec2 tMax) { return plu_create_vec2(plu_clampf(tMin.x, tValue.x, tMax.x), plu_clampf(tMin.y, tValue.y, tMax.y));}
static inline plVec2 plu_min_vec2        (plVec2 tValue0, plVec2 tValue1)           { return plu_create_vec2(plu_minf(tValue0.x, tValue1.x), plu_minf(tValue0.y, tValue1.y));}
static inline plVec2 plu_max_vec2        (plVec2 tValue0, plVec2 tValue1)           { return plu_create_vec2(plu_maxf(tValue0.x, tValue1.x), plu_maxf(tValue0.y, tValue1.y));}

// binary ops
static inline plVec2 plu_add_vec2        (plVec2 tVec1, plVec2 tVec2) { return plu_create_vec2(tVec1.x + tVec2.x, tVec1.y + tVec2.y); }
static inline plVec2 plu_sub_vec2        (plVec2 tVec1, plVec2 tVec2) { return plu_create_vec2(tVec1.x - tVec2.x, tVec1.y - tVec2.y); }
static inline plVec2 plu_mul_vec2        (plVec2 tVec1, plVec2 tVec2) { return plu_create_vec2(tVec1.x * tVec2.x, tVec1.y * tVec2.y); }
static inline plVec2 plu_mul_vec2_scalarf(plVec2 tVec, float fValue)  { return plu_create_vec2(fValue * tVec.x, fValue * tVec.y); }

// rect ops
static inline plRect plu_calculate_rect     (plVec2 tStart, plVec2 tSize)                      { return plu_create_rect_vec2(tStart, plu_add_vec2(tStart, tSize));}
static inline float  plu_rect_width         (const plRect* ptRect)                             { return ptRect->tMax.x - ptRect->tMin.x;}
static inline float  plu_rect_height        (const plRect* ptRect)                             { return ptRect->tMax.y - ptRect->tMin.y;}
static inline plVec2 plu_rect_size          (const plRect* ptRect)                             { return plu_sub_vec2(ptRect->tMax, ptRect->tMin);}
static inline plVec2 plu_rect_center        (const plRect* ptRect)                             { return plu_create_vec2((ptRect->tMax.x + ptRect->tMin.x) * 0.5f, (ptRect->tMax.y + ptRect->tMin.y) * 0.5f);}
static inline plVec2 plu_rect_top_left      (const plRect* ptRect)                             { return ptRect->tMin;}
static inline plVec2 plu_rect_top_right     (const plRect* ptRect)                             { return plu_create_vec2(ptRect->tMax.x, ptRect->tMin.y);}
static inline plVec2 plu_rect_bottom_left   (const plRect* ptRect)                             { return plu_create_vec2(ptRect->tMin.x, ptRect->tMax.y);}
static inline plVec2 plu_rect_bottom_right  (const plRect* ptRect)                             { return ptRect->tMax;}
static inline bool   plu_rect_contains_point(const plRect* ptRect, plVec2 tP)                  { return tP.x >= ptRect->tMin.x && tP.y >= ptRect->tMin.y && tP.x < ptRect->tMax.x && tP.y < ptRect->tMax.y; }
static inline bool   plu_rect_contains_rect (const plRect* ptRect0, const plRect* ptRect1)     { return ptRect1->tMin.x >= ptRect0->tMin.x && ptRect1->tMin.y >= ptRect0->tMin.y && ptRect1->tMax.x <= ptRect0->tMax.x && ptRect1->tMax.y <= ptRect0->tMax.y; }
static inline bool   plu_rect_overlaps_rect (const plRect* ptRect0, const plRect* ptRect1)     { return ptRect1->tMin.y <  ptRect0->tMax.y && ptRect1->tMax.y >  ptRect0->tMin.y && ptRect1->tMin.x <  ptRect0->tMax.x && ptRect1->tMax.x > ptRect0->tMin.x; }
static inline bool   plu_rect_is_inverted   (const plRect* ptRect)                             { return ptRect->tMin.x > ptRect->tMax.x || ptRect->tMin.y > ptRect->tMax.y; }
static inline plRect plu_rect_expand        (const plRect* ptRect, float fPadding)             { const plVec2 tMin = plu_create_vec2(ptRect->tMin.x - fPadding, ptRect->tMin.y - fPadding); const plVec2 tMax = plu_create_vec2(ptRect->tMax.x + fPadding, ptRect->tMax.y + fPadding); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_expand_vec2   (const plRect* ptRect, plVec2 tPadding)            { const plVec2 tMin = plu_create_vec2(ptRect->tMin.x - tPadding.x, ptRect->tMin.y - tPadding.y); const plVec2 tMax = plu_create_vec2(ptRect->tMax.x + tPadding.x, ptRect->tMax.y + tPadding.y); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_clip          (const plRect* ptRect0, const plRect* ptRect1)     { const plVec2 tMin = plu_create_vec2(plu_maxf(ptRect0->tMin.x, ptRect1->tMin.x), plu_maxf(ptRect0->tMin.y, ptRect1->tMin.y)); const plVec2 tMax = plu_create_vec2(plu_minf(ptRect0->tMax.x, ptRect1->tMax.x), plu_minf(ptRect0->tMax.y, ptRect1->tMax.y)); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_clip_full     (const plRect* ptRect0, const plRect* ptRect1)     { const plVec2 tMin = plu_clamp_vec2(ptRect1->tMin, ptRect0->tMin, ptRect1->tMax); const plVec2 tMax = plu_clamp_vec2(ptRect1->tMin, ptRect0->tMax, ptRect1->tMax); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_floor         (const plRect* ptRect)                             { const plVec2 tMin = plu_create_vec2( floorf(ptRect->tMin.x), floorf(ptRect->tMin.y)); const plVec2 tMax = plu_create_vec2(floorf(ptRect->tMax.x), floorf(ptRect->tMax.y)); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_translate_vec2(const plRect* ptRect, plVec2 tDelta)              { const plVec2 tMin = plu_create_vec2(ptRect->tMin.x + tDelta.x, ptRect->tMin.y + tDelta.y); const plVec2 tMax = plu_create_vec2(ptRect->tMax.x + tDelta.x, ptRect->tMax.y + tDelta.y); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_translate_x   (const plRect* ptRect, float fDx)                  { const plVec2 tMin = plu_create_vec2(ptRect->tMin.x + fDx, ptRect->tMin.y); const plVec2 tMax = plu_create_vec2(ptRect->tMax.x + fDx, ptRect->tMax.y); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_translate_y   (const plRect* ptRect, float fDy)                  { const plVec2 tMin = plu_create_vec2(ptRect->tMin.x, ptRect->tMin.y + fDy); const plVec2 tMax = plu_create_vec2(ptRect->tMax.x, ptRect->tMax.y + fDy); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_add_point     (const plRect* ptRect, plVec2 tP)                  { const plVec2 tMin = plu_create_vec2(ptRect->tMin.x > tP.x ? tP.x : ptRect->tMin.x, ptRect->tMin.y > tP.y ? tP.y : ptRect->tMin.y); const plVec2 tMax = plu_create_vec2(ptRect->tMax.x < tP.x ? tP.x : ptRect->tMax.x, ptRect->tMax.y < tP.y ? tP.y : ptRect->tMax.y); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_add_rect      (const plRect* ptRect0, const plRect* ptRect1)     { const plVec2 tMin = plu_create_vec2(ptRect0->tMin.x > ptRect1->tMin.x ? ptRect1->tMin.x : ptRect0->tMin.x, ptRect0->tMin.y > ptRect1->tMin.y ? ptRect1->tMin.y : ptRect0->tMin.y); const plVec2 tMax = plu_create_vec2(ptRect0->tMax.x < ptRect1->tMax.x ? ptRect1->tMax.x : ptRect0->tMax.x, ptRect0->tMax.y < ptRect1->tMax.y ? ptRect1->tMax.y : ptRect0->tMax.y); return plu_create_rect_vec2(tMin, tMax);}
static inline plRect plu_rect_move_center   (const plRect* ptRect, float fX, float fY)         { const plVec2 tCurrentCenter = plu_rect_center(ptRect); const float fDx = fX - tCurrentCenter.x; const float fDy = fY - tCurrentCenter.y; const plRect tResult = {{ ptRect->tMin.x + fDx, ptRect->tMin.y + fDy},{ ptRect->tMax.x + fDx, ptRect->tMax.y + fDy}}; return tResult;}
static inline plRect plu_rect_move_center_y (const plRect* ptRect, float fY)                   { const plVec2 tCurrentCenter = plu_rect_center(ptRect); const float fDy = fY - tCurrentCenter.y; const plRect tResult = {{ ptRect->tMin.x, ptRect->tMin.y + fDy},{ ptRect->tMax.x, ptRect->tMax.y + fDy}}; return tResult;}
static inline plRect plu_rect_move_center_x (const plRect* ptRect, float fX)                   { const plVec2 tCurrentCenter = plu_rect_center(ptRect); const float fDx = fX - tCurrentCenter.x; const plRect tResult = { { ptRect->tMin.x + fDx, ptRect->tMin.y}, { ptRect->tMax.x + fDx, ptRect->tMax.y} }; return tResult;}
static inline plRect plu_rect_move_start    (const plRect* ptRect, float fX, float fY)         { const plRect tResult = {{ fX, fY}, { fX + ptRect->tMax.x - ptRect->tMin.x, fY + ptRect->tMax.y - ptRect->tMin.y} }; return tResult;}
static inline plRect plu_rect_move_start_x  (const plRect* ptRect, float fX)                   { const plRect tResult = { { fX, ptRect->tMin.y}, { fX + ptRect->tMax.x - ptRect->tMin.x, ptRect->tMax.y} }; return tResult;}

#define PLU_VEC2_LENGTH_SQR(vec) (((vec).x * (vec).x) + ((vec).y * (vec).y))

//-----------------------------------------------------------------------------
// [SECTION] stretchy buffer internal
//-----------------------------------------------------------------------------

#define plu__sb_header(buf) ((plUiSbHeader_*)(((char*)(buf)) - sizeof(plUiSbHeader_)))
#define plu__sb_may_grow(buf, s, n, m) plu__sb_may_grow_((void**)&(buf), (s), (n), (m))

typedef struct
{
    uint32_t uSize;
    uint32_t uCapacity;
} plUiSbHeader_;

static void
plu__sb_grow(void** ptrBuffer, size_t szElementSize, size_t szNewItems)
{

    plUiSbHeader_* ptOldHeader = plu__sb_header(*ptrBuffer);

    const size_t szNewSize = (ptOldHeader->uCapacity + szNewItems) * szElementSize + sizeof(plUiSbHeader_);
    plUiSbHeader_* ptNewHeader = (plUiSbHeader_*)pl_memory_alloc(szNewSize); //-V592
    memset(ptNewHeader, 0, (ptOldHeader->uCapacity + szNewItems) * szElementSize + sizeof(plUiSbHeader_));
    if(ptNewHeader)
    {
        ptNewHeader->uSize = ptOldHeader->uSize;
        ptNewHeader->uCapacity = ptOldHeader->uCapacity + (uint32_t)szNewItems;
        memcpy(&ptNewHeader[1], *ptrBuffer, ptOldHeader->uSize * szElementSize);
        pl_memory_free(ptOldHeader);
        *ptrBuffer = &ptNewHeader[1];
    }
}

static void
plu__sb_may_grow_(void** ptrBuffer, size_t szElementSize, size_t szNewItems, size_t szMinCapacity)
{
    if(*ptrBuffer)
    {   
        plUiSbHeader_* ptOriginalHeader = plu__sb_header(*ptrBuffer);
        if(ptOriginalHeader->uSize + szNewItems > ptOriginalHeader->uCapacity)
        {
            plu__sb_grow(ptrBuffer, szElementSize, szNewItems);
        }
    }
    else // first run
    {
        const size_t szNewSize = szMinCapacity * szElementSize + sizeof(plUiSbHeader_);
        plUiSbHeader_* ptHeader = (plUiSbHeader_*)pl_memory_alloc(szNewSize);
        memset(ptHeader, 0, szMinCapacity * szElementSize + sizeof(plUiSbHeader_));
        if(ptHeader)
        {
            *ptrBuffer = &ptHeader[1]; 
            ptHeader->uSize = 0u;
            ptHeader->uCapacity = (uint32_t)szMinCapacity;
        }
    }     
}

//-----------------------------------------------------------------------------
// [SECTION] stretchy buffer
//-----------------------------------------------------------------------------

#define plu_sb_capacity(buf) \
    ((buf) ? plu__sb_header((buf))->uCapacity : 0u)

#define plu_sb_size(buf) \
    ((buf) ? plu__sb_header((buf))->uSize : 0u)

#define plu_sb_pop(buf) \
    (buf)[--plu__sb_header((buf))->uSize]

#define plu_sb_pop_n(buf, n) \
    plu__sb_header((buf))->uSize-=(n)

#define plu_sb_top(buf) \
    ((buf)[plu__sb_header((buf))->uSize-1])

#define plu_sb_last(buf) \
    plu_sb_top((buf))

#define plu_sb_free(buf) \
    if((buf)){ pl_memory_free(plu__sb_header(buf));} (buf) = NULL;

#define plu_sb_reset(buf) \
    if((buf)){ plu__sb_header((buf))->uSize = 0u;}

#define plu_sb_back(buf) \
    plu_sb_top((buf))

#define plu_sb_end(buf) \
    ((buf) ? (buf) + plu__sb_header((buf))->uSize : (buf))

#define plu_sb_add_n(buf, n) \
    (plu__sb_may_grow((buf), sizeof(*(buf)), (n), (n)), (n) ? (plu__sb_header(buf)->uSize += (n), plu__sb_header(buf)->uSize - (n)) : plu_sb_size(buf))

#define plu_sb_add(buf) \
    plu_sb_add_n((buf), 1)

#define plu_sb_add_ptr_n(buf, n) \
    (plu__sb_may_grow((buf), sizeof(*(buf)), (n), (n)), (n) ? (plu__sb_header(buf)->uSize += (n), &(buf)[plu__sb_header(buf)->uSize - (n)]) : (buf))

#define plu_sb_add_ptr(buf, n) \
    plu_sb_add_ptr_n((buf), 1)

#define plu_sb_push(buf, v) \
    (plu__sb_may_grow((buf), sizeof(*(buf)), 1, 8), (buf)[plu__sb_header((buf))->uSize++] = (v))

#define plu_sb_reserve(buf, n) \
    (plu__sb_may_grow((buf), sizeof(*(buf)), (n), (n)))

#define plu_sb_resize(buf, n) \
    (plu__sb_may_grow((buf), sizeof(*(buf)), (n), (n)), plu__sb_header((buf))->uSize = (n))

#define plu_sb_del_n(buf, i, n) \
    (memmove(&(buf)[i], &(buf)[(i) + (n)], sizeof *(buf) * (plu__sb_header(buf)->uSize - (n) - (i))), plu__sb_header(buf)->uSize -= (n))

#define plu_sb_del(buf, i) \
    plu_sb_del_n((buf), (i), 1)

#define plu_sb_del_swap(buf, i) \
    ((buf)[i] = plu_sb_last(buf), plu__sb_header(buf)->uSize -= 1)

#define plu_sb_insert_n(buf, i, n) \
    (plu_sb_add_n((buf), (n)), memmove(&(buf)[(i) + (n)], &(buf)[i], sizeof *(buf) * (plu__sb_header(buf)->uSize - (n) - (i))))

#define plu_sb_insert(buf, i, v) \
    (plu_sb_insert_n((buf), (i), 1), (buf)[i] = (v))

static void
plu__sb_vsprintf(char** ppcBuffer, const char* pcFormat, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    int32_t n = vsnprintf(NULL, 0, pcFormat, args2);
    va_end(args2);
    uint32_t an = plu_sb_size(*ppcBuffer);
    plu_sb_resize(*ppcBuffer, an + n + 1);
    vsnprintf(*ppcBuffer + an, n + 1, pcFormat, args);
}

static void
plu__sb_sprintf(char** ppcBuffer, const char* pcFormat, ...)
{
    va_list args;
    va_start(args, pcFormat);
    plu__sb_vsprintf(ppcBuffer, pcFormat, args);
    va_end(args);
}

#define plu_sb_insert(buf, i, v) \
    (plu_sb_insert_n((buf), (i), 1), (buf)[i] = (v))

#define plu_sb_sprintf(buf, pcFormat, ...) \
    plu__sb_sprintf(&(buf), (pcFormat), __VA_ARGS__)

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plInputEvent
{
    plInputEventType   tType;
    plInputEventSource tSource;

    union
    {
        struct // mouse pos event
        {
            float fPosX;
            float fPosY;
        };

        struct // mouse wheel event
        {
            float fWheelX;
            float fWheelY;
        };
        
        struct // mouse button event
        {
            int  iButton;
            bool bMouseDown;
        };

        struct // key event
        {
            plKey tKey;
            bool  bKeyDown;
        };

        struct // text event
        {
            uint32_t uChar;
        };
        
    };

} plInputEvent;

typedef struct _plUiColorScheme
{
    plVec4 tTitleActiveCol;
    plVec4 tTitleBgCol;
    plVec4 tTitleBgCollapsedCol;
    plVec4 tWindowBgColor;
    plVec4 tWindowBorderColor;
    plVec4 tChildBgColor;
    plVec4 tButtonCol;
    plVec4 tButtonHoveredCol;
    plVec4 tButtonActiveCol;
    plVec4 tTextCol;
    plVec4 tProgressBarCol;
    plVec4 tCheckmarkCol;
    plVec4 tFrameBgCol;
    plVec4 tFrameBgHoveredCol;
    plVec4 tFrameBgActiveCol;
    plVec4 tHeaderCol;
    plVec4 tHeaderHoveredCol;
    plVec4 tHeaderActiveCol;
    plVec4 tScrollbarBgCol;
    plVec4 tScrollbarHandleCol;
    plVec4 tScrollbarFrameCol;
    plVec4 tScrollbarActiveCol;
    plVec4 tScrollbarHoveredCol;
} plUiColorScheme;

typedef struct _plUiStyle
{
    // style
    float  fTitlePadding;
    float  fFontSize;
    float  fIndentSize;
    float  fWindowHorizontalPadding;
    float  fWindowVerticalPadding;
    float  fScrollbarSize;
    float  fSliderSize;
    plVec2 tItemSpacing;
    plVec2 tInnerSpacing;
    plVec2 tFramePadding;
} plUiStyle;

typedef struct _plUiTabBar
{
    uint32_t    uId;
    plVec2      tStartPos;
    plVec2      tCursorPos;
    uint32_t    uCurrentIndex;
    uint32_t    uValue;
    uint32_t    uNextValue;
} plUiTabBar;

typedef struct _plUiPrevItemData
{
    bool bHovered;
    bool bActive;
} plUiPrevItemData;

typedef struct _plUiLayoutSortLevel
{
    float    fWidth;
    uint32_t uStartIndex;
    uint32_t uCount;
} plUiLayoutSortLevel;

typedef struct _plUiLayoutRowEntry
{
    plUiLayoutRowEntryType tType;  // entry type (PL_UI_LAYOUT_ROW_ENTRY_TYPE_*)
    float                  fWidth; // widget width (could be relative or absolute)
} plUiLayoutRowEntry;

typedef struct _plUiLayoutRow
{
    plUiLayoutRowType    tType;                // determines if width/height is relative or absolute (PL_UI_LAYOUT_ROW_TYPE_*)
    plUiLayoutSystemType tSystemType;          // this rows layout strategy

    float                fWidth;               // widget width (could be relative or absolute)
    float                fHeight;              // widget height (could be relative or absolute)
    float                fSpecifiedHeight;     // height user passed in
    float                fVerticalOffset;      // used by space layout system (system 6) to temporary offset widget placement
    float                fHorizontalOffset;    // offset where the next widget should start from (usually previous widget + item spacing)
    uint32_t             uColumns;             // number of columns in row
    uint32_t             uCurrentColumn;       // current column
    float                fMaxWidth;            // maximum row width  (to help set max cursor position)
    float                fMaxHeight;           // maximum row height (to help set next row position + max cursor position)
    float                fRowStartX;           // offset where the row starts from (think of a tab bar being the second widget in the row)
    const float*         pfSizesOrRatios;      // size or rations when using array layout system (system 4)
    uint32_t             uStaticEntryCount;    // number of static entries when using template layout system (system 5)
    uint32_t             uDynamicEntryCount;   // number of dynamic entries when using template layout system (system 5)
    uint32_t             uVariableEntryCount;  // number of variable entries when using template layout system (system 5)
    uint32_t             uEntryStartIndex;     // offset into parent window sbtRowTemplateEntries buffer
} plUiLayoutRow;

typedef struct _plUiInputTextState
{
    uint32_t           uId;
    int                iCurrentLengthW;        // widget id owning the text state
    int                iCurrentLengthA;        // we need to maintain our buffer length in both UTF-8 and wchar format. UTF-8 length is valid even if TextA is not.
    plUiWChar*           sbTextW;                // edit buffer, we need to persist but can't guarantee the persistence of the user-provided buffer. so we copy into own buffer.
    char*              sbTextA;                // temporary UTF8 buffer for callbacks and other operations. this is not updated in every code-path! size=capacity.
    char*              sbInitialTextA;         // backup of end-user buffer at the time of focus (in UTF-8, unaltered)
    bool               bTextAIsValid;          // temporary UTF8 buffer is not initially valid before we make the widget active (until then we pull the data from user argument)
    int                iBufferCapacityA;       // end-user buffer capacity
    float              fScrollX;               // horizontal scrolling/offset
    STB_TexteditState  tStb;                   // state for stb_textedit.h
    float              fCursorAnim;            // timer for cursor blink, reset on every user action so the cursor reappears immediately
    bool               bCursorFollow;          // set when we want scrolling to follow the current cursor position (not always!)
    bool               bSelectedAllMouseLock;  // after a double-click to select all, we ignore further mouse drags to update selection
    bool               bEdited;                // edited this frame
    plUiInputTextFlags tFlags;    // copy of InputText() flags. may be used to check if e.g. ImGuiInputTextFlags_Password is set.
} plUiInputTextState;

//-----------------------------------------------------------------------------
// [SECTION] plUiStorage
//-----------------------------------------------------------------------------

typedef struct _plUiStorageEntry
{
    uint32_t uKey;
    union
    {
        int   iValue;
        float fValue;
        void* pValue;
    };
} plUiStorageEntry;

typedef struct _plUiStorage
{
    plUiStorageEntry* sbtData;
} plUiStorage;

//-----------------------------------------------------------------------------
// [SECTION] plUiWindow
//-----------------------------------------------------------------------------

typedef struct _plUiTempWindowData
{
    plVec2               tCursorStartPos;   // position where widgets begin drawing (could be outside window if scrolling)
    plVec2               tCursorMaxPos;     // maximum cursor position (could be outside window if scrolling)
    uint32_t             uTreeDepth;        // current level inside trees
    float                fExtraIndent;      // extra indent added by pl_indent
    plUiLayoutRow        tCurrentLayoutRow; // current layout row to use
    plVec2               tRowPos;           // current row starting position
    float                fAccumRowX;        // additional indent due to a parent (like tab bar) not being the first item in a row

    // template layout system
    float fTempMinWidth;
    float fTempStaticWidth;
} plUiTempWindowData;

typedef struct _plUiWindow
{
    const char*          pcName;
    uint32_t             uId;                     // window Id (=plu_str_hash(pcName))
    plUiWindowFlags      tFlags;                  // plUiWindowFlags, not all honored at the moment
    plVec2               tPos;                    // position of window in viewport
    plVec2               tContentSize;            // size of contents/scrollable client area
    plVec2               tMinSize;                // minimum size of window (default 200,200)
    plVec2               tMaxSize;                // maximum size of window (default 10000,10000)
    plVec2               tSize;                   // full size or title bar size if collapsed
    plVec2               tFullSize;               // used to restore size after uncollapsing
    plVec2               tScroll;                 // current scroll amount (0 < tScroll < tScrollMax)
    plVec2               tScrollMax;              // maximum scroll amount based on last frame content size & adjusted for scroll bars
    plRect               tInnerRect;              // inner rect (excludes titlebar & scrollbars)
    plRect               tOuterRect;              // outer rect (includes everything)
    plRect               tOuterRectClipped;       // outer rect clipped by parent window & viewport
    plRect               tInnerClipRect;          // inner rect clipped by parent window & viewport (includes horizontal padding on each side)
    plUiWindow*          ptParentWindow;          // parent window if child
    plUiWindow*          ptRootWindow;            // root window or self if this is the root window
    bool                 bVisible;                // true if visible (only for child windows at the moment)
    bool                 bActive;                 // window has been "seen" this frame
    bool                 bCollapsed;              // window is currently collapsed
    bool                 bScrollbarX;             // set if horizontal scroll bar is "on"
    bool                 bScrollbarY;             // set if vertical scroll bar is "on"
    plUiTempWindowData   tTempData;               // temporary data reset at the beginning of frame
    plUiWindow**         sbtChildWindows;         // child windows if any (reset every frame)
    plUiLayoutRow*       sbtRowStack;             // row stack for containers to push parents row onto and pop when they exist (reset every frame)
    plUiLayoutSortLevel* sbtTempLayoutSort;       // blah
    uint32_t*            sbuTempLayoutIndexSort;  // blah
    plUiLayoutRowEntry*  sbtRowTemplateEntries;   // row template entries (shared and reset every frame)            
    plDrawLayer*         ptBgLayer;               // background draw layer
    plDrawLayer*         ptFgLayer;               // foreground draw layer
    plUiConditionFlags   tPosAllowableFlags;      // acceptable condition flags for "pl_set_next_window_pos()"
    plUiConditionFlags   tSizeAllowableFlags;     // acceptable condition flags for "pl_set_next_window_size()"
    plUiConditionFlags   tCollapseAllowableFlags; // acceptable condition flags for "pl_set_next_window_collapse()"
    uint8_t              uHideFrames;             // hide window for this many frames (useful for autosizing)
    uint32_t             uFocusOrder;             // display rank
    plUiStorage          tStorage;                // state storage
} plUiWindow;

//-----------------------------------------------------------------------------
// [SECTION] plUiContext
//-----------------------------------------------------------------------------

typedef struct _plUiNextWindowData
{
    plUiNextWindowFlags tFlags;
    plUiConditionFlags  tPosCondition;
    plUiConditionFlags  tSizeCondition;
    plUiConditionFlags  tCollapseCondition;
    plVec2              tPos;
    plVec2              tSize;
    bool                bCollapsed;
} plUiNextWindowData;

typedef struct _plUiContext
{
    plUiStyle       tStyle;
    plUiColorScheme tColorScheme;
    plIO            tIO;
    
    // prev/next state
    plUiNextWindowData tNextWindowData;        // info based on pl_set_next_window_* functions
    plUiPrevItemData   tPrevItemData;          // data for use by pl_was_last_item_* functions
    uint32_t*          sbuIdStack;             // id stack for hashing IDs, container items usually push/pop these

    // widget state
    plUiInputTextState tInputTextState;
    uint32_t           uHoveredId;             // set at the end of the previous frame from uNextHoveredId
    uint32_t           uActiveId;              // set at the end of the previous frame from uNextActiveId
    uint32_t           uNextHoveredId;         // set during current frame (by end of frame, should be last item hovered)
    uint32_t           uNextActiveId;          // set during current frame (by end of frame, should be last item active)

    uint32_t           uActiveWindowId;               // current active window id
    bool               bActiveIdJustActivated;        // window was just activated, so bring it to the front
    bool               bWantCaptureKeyboardNextFrame; // we want the mouse
    bool               bWantCaptureMouseNextFrame;    // wen want the mouse next frame
    
    // windows
    plUiWindow         tTooltipWindow;         // persistent tooltip window (since there can only ever be 1 at a time)
    plUiWindow*        ptCurrentWindow;        // current window we are appending into
    plUiWindow*        ptHoveredWindow;        // window being hovered
    plUiWindow*        ptMovingWindow;         // window being moved
    plUiWindow*        ptSizingWindow;         // window being resized
    plUiWindow*        ptScrollingWindow;      // window being scrolled with mouse
    plUiWindow*        ptWheelingWindow;       // window being scrolled with mouse wheel
    plUiWindow*        ptActiveWindow;         // selected/active window
    plUiWindow**       sbptWindows;            // windows stored in display order (reset every frame and non root windows)
    plUiWindow**       sbptFocusedWindows;     // root windows stored in display order
    plUiStorage        tWindows;               // windows by ID for quick retrieval

    // tabs
    plUiTabBar*        sbtTabBars;             // stretchy-buffer for persistent tab bar data
    plUiTabBar*        ptCurrentTabBar;        // current tab bar being appended to

    // drawing
    plDrawList*        ptDrawlist;             // main ui drawlist
    plDrawList*        ptDebugDrawlist;        // ui debug drawlist (i.e. overlays)
    plFont*            ptFont;                 // current font
    plDrawLayer*       ptBgLayer;              // submitted before window layers
    plDrawLayer*       ptFgLayer;              // submitted after window layers
    plDrawLayer*       ptDebugLayer;           // submitted last

    // drawing context
    plDrawList**   sbDrawlists;
    uint64_t       frameCount;
    plFontAtlas*   fontAtlas;
    plVec2         tFrameBufferScale;

    // logging
    bool            bLogActive;
    char*           sbcLogBuffer;
    uint32_t*       sbuLogEntries; // indices into sbcLogBuffer
    plDebugLogFlags tDebugLogFlags;

    // memory
    uint32_t uMemoryAllocations;

} plUiContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// logging
void pl_debug_log(const char* pcFormat, ...);

#define PL_UI_DEBUG_LOG(...)           pl_debug_log(__VA_ARGS__);
#define PL_UI_DEBUG_LOG_ACTIVE_ID(...) if(gptCtx->tDebugLogFlags & PL_UI_DEBUG_LOG_FLAGS_EVENT_ACTIVE_ID) { pl_debug_log(__VA_ARGS__); }
#define PL_UI_DEBUG_LOG_IO(...)        if(gptCtx->tDebugLogFlags & PL_UI_DEBUG_LOG_FLAGS_EVENT_IO)        { pl_debug_log(__VA_ARGS__); }

const char*          pl_find_renderered_text_end(const char* pcText, const char* pcTextEnd);
void                 pl_ui_add_text             (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
void                 pl_add_clipped_text        (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap);
plVec2               pl_ui_calculate_text_size     (plFont* font, float size, const char* text, float wrap);
static inline float  pl_get_frame_height        (void) { return gptCtx->tStyle.fFontSize + gptCtx->tStyle.tFramePadding.y * 2.0f; }

// collision
static inline bool   pl_does_circle_contain_point  (plVec2 cen, float radius, plVec2 point) { const float fDistanceSquared = powf(point.x - cen.x, 2) + powf(point.y - cen.y, 2); return fDistanceSquared <= radius * radius; }
bool                 pl_does_triangle_contain_point(plVec2 p0, plVec2 p1, plVec2 p2, plVec2 point);
bool                 pl_is_item_hoverable          (const plRect* ptBox, uint32_t uHash);

// layouts
static inline plVec2 pl__ui_get_cursor_pos    (void) { return (plVec2){gptCtx->ptCurrentWindow->tTempData.tRowPos.x + gptCtx->ptCurrentWindow->tTempData.fAccumRowX + gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.fHorizontalOffset + (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize, gptCtx->ptCurrentWindow->tTempData.tRowPos.y + gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.fVerticalOffset};}
plVec2               pl_calculate_item_size(float fDefaultHeight);
void                 pl_advance_cursor     (float fWidth, float fHeight);

// misc
bool pl_begin_window_ex (const char* pcName, bool* pbOpen, plUiWindowFlags tFlags);
void pl_render_scrollbar(plUiWindow* ptWindow, uint32_t uHash, plUiAxis tAxis);
void pl_submit_window   (plUiWindow* ptWindow);

static inline bool   pl__ui_should_render(const plVec2* ptStartPos, const plVec2* ptWidgetSize) { return !(ptStartPos->y + ptWidgetSize->y < gptCtx->ptCurrentWindow->tPos.y || ptStartPos->y > gptCtx->ptCurrentWindow->tPos.y + gptCtx->ptCurrentWindow->tSize.y); }

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~text state system~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static bool        pl__input_text_filter_character(unsigned int* puChar, plUiInputTextFlags tFlags);
static inline void pl__text_state_cursor_anim_reset  (plUiInputTextState* ptState) { ptState->fCursorAnim = -0.30f; }
static inline void pl__text_state_cursor_clamp       (plUiInputTextState* ptState) { ptState->tStb.cursor = plu_min(ptState->tStb.cursor, ptState->iCurrentLengthW); ptState->tStb.select_start = plu_min(ptState->tStb.select_start, ptState->iCurrentLengthW); ptState->tStb.select_end = plu_min(ptState->tStb.select_end, ptState->iCurrentLengthW);}
static inline bool pl__text_state_has_selection      (plUiInputTextState* ptState) { return ptState->tStb.select_start != ptState->tStb.select_end; }
static inline void pl__text_state_clear_selection    (plUiInputTextState* ptState) { ptState->tStb.select_start = ptState->tStb.select_end = ptState->tStb.cursor; }
static inline int  pl__text_state_get_cursor_pos     (plUiInputTextState* ptState) { return ptState->tStb.cursor; }
static inline int  pl__text_state_get_selection_start(plUiInputTextState* ptState) { return ptState->tStb.select_start; }
static inline int  pl__text_state_get_selection_end  (plUiInputTextState* ptState) { return ptState->tStb.select_end; }
static inline void pl__text_state_select_all         (plUiInputTextState* ptState) { ptState->tStb.select_start = 0; ptState->tStb.cursor = ptState->tStb.select_end = ptState->iCurrentLengthW; ptState->tStb.has_preferred_x = 0; }

static inline void pl__text_state_clear_text      (plUiInputTextState* ptState)           { ptState->iCurrentLengthA = ptState->iCurrentLengthW = 0; ptState->sbTextA[0] = 0; ptState->sbTextW[0] = 0; pl__text_state_cursor_clamp(ptState);}
static inline void pl__text_state_free_memory     (plUiInputTextState* ptState)           { plu_sb_free(ptState->sbTextA); plu_sb_free(ptState->sbTextW); plu_sb_free(ptState->sbInitialTextA);}
static inline int  pl__text_state_undo_avail_count(plUiInputTextState* ptState)           { return ptState->tStb.undostate.undo_point;}
static inline int  pl__text_state_redo_avail_count(plUiInputTextState* ptState)           { return STB_TEXTEDIT_UNDOSTATECOUNT - ptState->tStb.undostate.redo_point; }
static void        pl__text_state_on_key_press    (plUiInputTextState* ptState, int iKey);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~widget behavior~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool pl_button_behavior(const plRect* ptBox, uint32_t uHash, bool* pbOutHovered, bool* pbOutHeld);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~storage system~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

plUiStorageEntry*    pl_lower_bound(plUiStorageEntry* sbtData, uint32_t uKey);

int                  pl_get_int      (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float                pl_get_float    (plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool                 pl_get_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void*                pl_get_ptr      (plUiStorage* ptStorage, uint32_t uKey);

int*                 pl_get_int_ptr  (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float*               pl_get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool*                pl_get_bool_ptr (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void**               pl_get_ptr_ptr  (plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue);

void                 pl_set_int      (plUiStorage* ptStorage, uint32_t uKey, int iValue);
void                 pl_set_float    (plUiStorage* ptStorage, uint32_t uKey, float fValue);
void                 pl_set_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bValue);
void                 pl_set_ptr      (plUiStorage* ptStorage, uint32_t uKey, void* pValue);

// (borrowed from Dear ImGui)
// CRC32 needs a 1KB lookup table (not cache friendly)
static const uint32_t gauiCrc32LUT[256] =
{
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
    0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
    0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
    0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
    0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
    0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
    0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
    0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
    0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
    0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
    0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
    0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
};

static uint32_t
plu_str_hash_data(const void* pData, size_t szDataSize, uint32_t uSeed)
{
    uint32_t uCrc = ~uSeed;
    const unsigned char* pucData = (const unsigned char*)pData;
    const uint32_t* puCrc32Lut = gauiCrc32LUT;
    while (szDataSize-- != 0)
        uCrc = (uCrc >> 8) ^ puCrc32Lut[(uCrc & 0xFF) ^ *pucData++];
    return ~uCrc;  
}

static uint32_t
plu_str_hash(const char* pcData, size_t szDataSize, uint32_t uSeed)
{
    uSeed = ~uSeed;
    uint32_t uCrc = uSeed;
    const unsigned char* pucData = (const unsigned char*)pcData;
    const uint32_t* puCrc32Lut = gauiCrc32LUT;
    if (szDataSize != 0)
    {
        while (szDataSize-- != 0)
        {
            unsigned char c = *pucData++;
            if (c == '#' && szDataSize >= 2 && pucData[0] == '#' && pucData[1] == '#')
                uCrc = uSeed;
            uCrc = (uCrc >> 8) ^ puCrc32Lut[(uCrc & 0xFF) ^ c];
        }
    }
    else
    {
        unsigned char c = *pucData++;
        while (c)
        {
            if (c == '#' && pucData[0] == '#' && pucData[1] == '#')
                uCrc = uSeed;
            uCrc = (uCrc >> 8) ^ puCrc32Lut[(uCrc & 0xFF) ^ c];
            c = *pucData;
            pucData++;
            
        }
    }
    return ~uCrc;
}

#define plu_string_min(Value1, Value2) ((Value1) > (Value2) ? (Value2) : (Value1))
static int
plu_text_char_from_utf8(uint32_t* puOutChars, const char* pcInText, const char* pcTextEnd)
{
    static const char lengths[32] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0 };
    static const int masks[]  = { 0x00, 0x7f, 0x1f, 0x0f, 0x07 };
    static const uint32_t mins[] = { 0x400000, 0, 0x80, 0x800, 0x10000 };
    static const int shiftc[] = { 0, 18, 12, 6, 0 };
    static const int shifte[] = { 0, 6, 4, 2, 0 };
    int len = lengths[*(const unsigned char*)pcInText >> 3];
    int wanted = len + (len ? 0 : 1);

    if (pcTextEnd == NULL)
        pcTextEnd = pcInText + wanted; // Max length, nulls will be taken into account.

    // Copy at most 'len' bytes, stop copying at 0 or past pcTextEnd. Branch predictor does a good job here,
    // so it is fast even with excessive branching.
    unsigned char s[4];
    s[0] = pcInText + 0 < pcTextEnd ? pcInText[0] : 0;
    s[1] = pcInText + 1 < pcTextEnd ? pcInText[1] : 0;
    s[2] = pcInText + 2 < pcTextEnd ? pcInText[2] : 0;
    s[3] = pcInText + 3 < pcTextEnd ? pcInText[3] : 0;

    // Assume a four-byte character and load four bytes. Unused bits are shifted out.
    *puOutChars  = (uint32_t)(s[0] & masks[len]) << 18;
    *puOutChars |= (uint32_t)(s[1] & 0x3f) << 12;
    *puOutChars |= (uint32_t)(s[2] & 0x3f) <<  6;
    *puOutChars |= (uint32_t)(s[3] & 0x3f) <<  0;
    *puOutChars >>= shiftc[len];

    // Accumulate the various error conditions.
    int e = 0;
    e  = (*puOutChars < mins[len]) << 6; // non-canonical encoding
    e |= ((*puOutChars >> 11) == 0x1b) << 7;  // surrogate half?
    e |= (*puOutChars > 0xFFFF) << 8;  // out of range?
    e |= (s[1] & 0xc0) >> 2;
    e |= (s[2] & 0xc0) >> 4;
    e |= (s[3]       ) >> 6;
    e ^= 0x2a; // top two bits of each tail byte correct?
    e >>= shifte[len];

    if (e)
    {
        // No bytes are consumed when *pcInText == 0 || pcInText == pcTextEnd.
        // One byte is consumed in case of invalid first byte of pcInText.
        // All available bytes (at most `len` bytes) are consumed on incomplete/invalid second to last bytes.
        // Invalid or incomplete input may consume less bytes than wanted, therefore every byte has to be inspected in s.
        wanted = plu_string_min(wanted, !!s[0] + !!s[1] + !!s[2] + !!s[3]);
        *puOutChars = 0xFFFD;
    }

    return wanted;
}

#endif // PL_UI_INTERNAL_H