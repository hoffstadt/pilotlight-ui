// pilotlight ui, v0.1.0 WIP

// library version
#define PL_UI_VERSION    "0.1.0"
#define PL_UI_VERSION_NUM 000100

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums & flags
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UI_H
#define PL_UI_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t
#include <stdarg.h>  // va list
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~structs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// basic types
typedef struct _plUiContext  plUiContext; // (opaque structure)
typedef struct _plUiClipper  plUiClipper;
typedef struct _plIO         plIO;
typedef struct _plInputEvent plInputEvent;
typedef struct _plKeyData    plKeyData;

// drawing
typedef struct _plDrawLayer   plDrawLayer;   // layer for out of order drawing(opaque structure)
typedef struct _plDrawVertex  plDrawVertex;  // single vertex (2D pos + uv + color)
typedef struct _plDrawList    plDrawList;    // collection of draw layers for a specific target (opaque structure)
typedef struct _plDrawCommand plDrawCommand; // single draw call (opaque structure)

// fonts
typedef struct _plFontChar       plFontChar;       // internal for now (opaque structure)
typedef struct _plFontGlyph      plFontGlyph;      // internal for now (opaque structure)
typedef struct _plFontCustomRect plFontCustomRect; // internal for now (opaque structure)
typedef struct _plFontPrepData   plFontPrepData;   // internal for now (opaque structure)
typedef struct _plFontRange      plFontRange;      // a range of characters
typedef struct _plFont           plFont;           // a single font with a specific size and config
typedef struct _plFontConfig     plFontConfig;     // configuration for loading a single font
typedef struct _plFontAtlas      plFontAtlas;      // atlas for multiple fonts

// character types
typedef uint16_t plWChar;

// plTextureID: used to represent texture for renderer backend
typedef void* plTextureId;

// math
typedef union  _plVec2 plVec2;
typedef union  _plVec4 plVec4;
typedef struct _plRect plRect;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~flags & enums~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// flags
typedef int plKeyChord;

// enums
typedef int plUiConditionFlags;
typedef int plUiLayoutRowType;
typedef int plUiInputTextFlags;
typedef int plKey;
typedef int plMouseButton;
typedef int plCursor;
typedef int plInputEventType;
typedef int plInputEventSource;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// context creation & access
plUiContext*   pl_create_ui_context (void);
void           pl_destroy_ui_context(void);
void           pl_set_ui_context    (plUiContext* ptCtx); // must be set when crossing DLL boundary
plUiContext*   pl_get_ui_context    (void);
plIO*          pl_get_io            (void);

// render data
plDrawList*    pl_get_draw_list      (plUiContext* ptContext);
plDrawList*    pl_get_debug_draw_list(plUiContext* ptContext);

// main
void           pl_new_frame(void); // start a new pilotlight ui frame, this should be the first command before calling any commands below
void           pl_end_frame(void); // ends pilotlight ui frame, automatically called by pl_ui_render()
void           pl_render   (void); // submits draw layers, you can then submit the ptDrawlist & ptDebugDrawlist from context

// tools
void           pl_debug(bool* pbOpen);
void           pl_style(bool* pbOpen);
void           pl_demo (bool* pbOpen);

// styling
void           pl_set_dark_theme(void);

// fonts
void           pl_set_default_font(plFont* ptFont);
plFont*        pl_get_default_font(void);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~windows~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// windows
// - only call "pl_ui_end_window()" if "pl_ui_begin_window()" returns true (its call automatically if false)
// - passing a valid pointer to pbOpen will show a red circle that will turn pbOpen false when clicked
// - "pl_ui_end_window()" will return false if collapsed or clipped
// - if you use autosize, make sure at least 1 row has a static component (or the window will grow unbounded)
bool           pl_begin_window(const char* pcName, bool* pbOpen, bool bAutoSize);
void           pl_end_window  (void);

// window utilities
plDrawLayer*   pl_get_window_fg_drawlayer(void); // returns current window foreground drawlist (call between pl_ui_begin_window(...) & pl_ui_end_window(...))
plDrawLayer*   pl_get_window_bg_drawlayer(void); // returns current window background drawlist (call between pl_ui_begin_window(...) & pl_ui_end_window(...))
plVec2         pl_get_cursor_pos         (void); // returns current cursor position (where the next widget will start drawing)

// child windows
// - only call "pl_ui_end_child()" if "pl_ui_begin_child()" returns true (its call automatically if false)
// - self-contained window with scrolling & clipping
bool           pl_begin_child(const char* pcName);
void           pl_end_child  (void);

// tooltips
// - window that follows the mouse (usually used in combination with "pl_ui_was_last_item_hovered()")
void           pl_begin_tooltip(void);
void           pl_end_tooltip  (void);

// window utilities
// - refers to current window (between "pl_ui_begin_window()" & "pl_ui_end_window()")
plVec2         pl_get_window_pos       (void);
plVec2         pl_get_window_size      (void);
plVec2         pl_get_window_scroll    (void);
plVec2         pl_get_window_scroll_max(void);
void           pl_set_window_scroll    (plVec2 tScroll);

// window manipulation
// - call before "pl_ui_begin_window()"
void           pl_set_next_window_pos     (plVec2 tPos, plUiConditionFlags tCondition);
void           pl_set_next_window_size    (plVec2 tSize, plUiConditionFlags tCondition);
void           pl_set_next_window_collapse(bool bCollapsed, plUiConditionFlags tCondition);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~widgets~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// main
bool           pl_button          (const char* pcText);
bool           pl_selectable      (const char* pcText, bool* bpValue);
bool           pl_checkbox        (const char* pcText, bool* pbValue);
bool           pl_radio_button    (const char* pcText, int* piValue, int iButtonValue);
void           pl_image           (plTextureId tTexture, plVec2 tSize);
void           pl_image_ex        (plTextureId tTexture, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor);
bool           pl_invisible_button(const char* pcText, plVec2 tSize);
void           pl_dummy           (plVec2 tSize);

// plotting
void           pl_progress_bar(float fFraction, plVec2 tSize, const char* pcOverlay);

// text
void           pl_text          (const char* pcFmt, ...);
void           pl_text_v        (const char* pcFmt, va_list args);
void           pl_color_text    (plVec4 tColor, const char* pcFmt, ...);
void           pl_color_text_v  (plVec4 tColor, const char* pcFmt, va_list args);
void           pl_labeled_text  (const char* pcLabel, const char* pcFmt, ...);
void           pl_labeled_text_v(const char* pcLabel, const char* pcFmt, va_list args);

// input
bool           pl_input_text     (const char* pcLabel, char* pcBuffer, size_t szBufferSize);
bool           pl_input_text_hint(const char* pcLabel, const char* pcHint, char* pcBuffer, size_t szBufferSize);
bool           pl_input_float    (const char* pcLabel, float* fValue, const char* pcFormat);
bool           pl_input_int      (const char* pcLabel, int* iValue);

// sliders
bool           pl_slider_float  (const char* pcLabel, float* pfValue, float fMin, float fMax);
bool           pl_slider_float_f(const char* pcLabel, float* pfValue, float fMin, float fMax, const char* pcFormat);
bool           pl_slider_int    (const char* pcLabel, int* piValue, int iMin, int iMax);
bool           pl_slider_int_f  (const char* pcLabel, int* piValue, int iMin, int iMax, const char* pcFormat);

// drag sliders
bool           pl_drag_float  (const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax);
bool           pl_drag_float_f(const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax, const char* pcFormat);

// trees
// - only call "pl_ui_tree_pop()" if "pl_ui_tree_node()" returns true (its call automatically if false)
// - only call "pl_ui_end_collapsing_header()" if "pl_ui_collapsing_header()" returns true (its call automatically if false)
bool           pl_collapsing_header    (const char* pcText);
void           pl_end_collapsing_header(void);
bool           pl_tree_node            (const char* pcText);
bool           pl_tree_node_f          (const char* pcFmt, ...);
bool           pl_tree_node_v          (const char* pcFmt, va_list args);
void           pl_tree_pop             (void);

// tabs & tab bars
// - only call "pl_ui_end_tab_bar()" if "pl_ui_begin_tab_bar()" returns true (its call automatically if false)
// - only call "pl_ui_end_tab()" if "pl_ui_begin_tab()" returns true (its call automatically if false)
bool           pl_begin_tab_bar(const char* pcText);
void           pl_end_tab_bar  (void);
bool           pl_begin_tab    (const char* pcText);
void           pl_end_tab      (void);

// misc.
void           pl_separator       (void);
void           pl_vertical_spacing(void);
void           pl_indent          (float fIndent);
void           pl_unindent        (float fIndent);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~clipper~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// - clipper based on "Dear ImGui"'s ImGuiListClipper (https://github.com/ocornut/imgui)
// - Used for large numbers of evenly spaced rows.
// - Without Clipper:
//       for(uint32_t i = 0; i < QUANTITY; i++)
//           ptUi->text("%i", i);
// - With Clipper:
//       plUiClipper tClipper = {QUANTITY};
//       while(ptUi->step_clipper(&tClipper))
//           for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
//               ptUi->text("%i", i);
bool           pl_step_clipper(plUiClipper* ptClipper);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~layout systems~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// - layout systems are based on "Nuklear" (https://github.com/Immediate-Mode-UI/Nuklear)
// - 6 different layout strategies
// - default layout strategy is system 2 with uWidgetCount=1 & fWidth=300 & fHeight=0
// - setting fHeight=0 will cause the row height to be equal to the minimal height of the maximum height widget

// layout system 1
// - provides each widget with the same horizontal space and grows dynamically with the parent window
// - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
void           pl_layout_dynamic(float fHeight, uint32_t uWidgetCount);

// layout system 2
// - provides each widget with the same horizontal pixel widget and does not grow with the parent window
// - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
void           pl_layout_static(float fHeight, float fWidth, uint32_t uWidgetCount);

// layout system 3
// - allows user to change the width per widget
// - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then fWidth is pixel width
// - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth is a ratio of the available width
// - does not wrap
void           pl_layout_row_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount);
void           pl_layout_row_push (float fWidth);
void           pl_layout_row_end  (void);

// layout system 4
// - same as layout system 3 but the "array" form
// - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
void           pl_layout_row(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios);

// layout system 5
// - similar to a flexbox
// - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
void           pl_layout_template_begin        (float fHeight);
void           pl_layout_template_push_dynamic (void);         // can go to minimum widget width if not enough space (10 pixels)
void           pl_layout_template_push_variable(float fWidth); // variable width with min pixel width of fWidth but can grow bigger if enough space
void           pl_layout_template_push_static  (float fWidth); // static pixel width of fWidth
void           pl_layout_template_end          (void);

// layout system 6
// - allows user to place widgets freely
// - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then fWidth/fHeight is pixel width/height
// - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth/fHeight is a ratio of the available width/height (for pl_ui_layout_space_begin())
// - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth is a ratio of the available width & fHeight is a ratio of fHeight given to "pl_ui_layout_space_begin()" (for pl_ui_layout_space_push())
void          pl_layout_space_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount);
void          pl_layout_space_push (float fX, float fY, float fWidth, float fHeight);
void          pl_layout_space_end  (void);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~state query~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool          pl_was_last_item_hovered(void);
bool          pl_was_last_item_active (void);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~io~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// keyboard
bool         pl_is_key_down           (plKey tKey);
bool         pl_is_key_pressed        (plKey tKey, bool bRepeat);
bool         pl_is_key_released       (plKey tKey);
int          pl_get_key_pressed_amount(plKey tKey, float fRepeatDelay, float fRate);

// mouse
bool         pl_is_mouse_down          (plMouseButton tButton);
bool         pl_is_mouse_clicked       (plMouseButton tButton, bool bRepeat);
bool         pl_is_mouse_released      (plMouseButton tButton);
bool         pl_is_mouse_double_clicked(plMouseButton tButton);
bool         pl_is_mouse_dragging      (plMouseButton tButton, float fThreshold);
bool         pl_is_mouse_hovering_rect (plVec2 minVec, plVec2 maxVec);
void         pl_reset_mouse_drag_delta (plMouseButton tButton);
plVec2       pl_get_mouse_drag_delta   (plMouseButton tButton, float fThreshold);
plVec2       pl_get_mouse_pos          (void);
float        pl_get_mouse_wheel        (void);
bool         pl_is_mouse_pos_valid     (plVec2 tPos);
void         pl_set_mouse_cursor       (plCursor tCursor);

// input functions
plKeyData*   pl_get_key_data          (plKey tKey);
void         pl_add_key_event         (plKey tKey, bool bDown);
void         pl_add_text_event        (uint32_t uChar);
void         pl_add_text_event_utf16  (uint16_t uChar);
void         pl_add_text_events_utf8  (const char* pcText);
void         pl_add_mouse_pos_event   (float fX, float fY);
void         pl_add_mouse_button_event(int iButton, bool bDown);
void         pl_add_mouse_wheel_event (float fX, float fY);

void         pl_clear_input_characters(void);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// setup
void         pl_register_drawlist   (plDrawList* ptDrawlist);
plDrawLayer* pl_request_layer       (plDrawList* ptDrawlist, const char* pcName);
void         pl_return_layer        (plDrawLayer* ptLayer);

// per frame
void pl_submit_layer(plDrawLayer* ptLayer);

// drawing
void pl_add_line               (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec4 tColor, float fThickness);
void pl_add_lines              (plDrawLayer* ptLayer, plVec2* atPoints, uint32_t uCount, plVec4 tColor, float fThickness);
void pl_add_text               (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
void pl_add_text_ex            (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
void pl_add_text_clipped       (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap);
void pl_add_text_clipped_ex    (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
void pl_add_triangle           (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness);
void pl_add_triangle_filled    (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor);
void pl_add_rect               (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness);
void pl_add_rect_filled        (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor);
void pl_add_rect_rounded       (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness, float fRadius, uint32_t uSegments);
void pl_add_rect_rounded_filled(plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fRadius, uint32_t uSegments);
void pl_add_quad               (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness);
void pl_add_quad_filled        (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor);
void pl_add_circle             (plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments, float fThickness);
void pl_add_circle_filled      (plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments);
void pl_add_image              (plDrawLayer* ptLayer, plTextureId tTexture, plVec2 tPMin, plVec2 tPMax);
void pl_add_image_ex           (plDrawLayer* ptLayer, plTextureId tTexture, plVec2 tPMin, plVec2 tPMax, plVec2 tUvMin, plVec2 tUvMax, plVec4 tColor);
void pl_add_bezier_quad        (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
void pl_add_bezier_cubic       (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

// fonts
void   pl_build_font_atlas        (plFontAtlas* ptAtlas);
void   pl_cleanup_font_atlas      (plFontAtlas* ptAtlas);
void   pl_add_default_font        (plFontAtlas* ptAtlas);
void   pl_add_font_from_file_ttf  (plFontAtlas* ptAtlas, plFontConfig tConfig, const char* pcFile);
void   pl_add_font_from_memory_ttf(plFontAtlas* ptAtlas, plFontConfig tConfig, void* pData);
plVec2 pl_calculate_text_size     (plFont* ptFont, float fSize, const char* pcText, float fWrap);
plVec2 pl_calculate_text_size_ex  (plFont* ptFont, float fSize, const char* pcText, const char* pcTextEnd, float fWrap);
plRect pl_calculate_text_bb       (plFont* ptFont, float fSize, plVec2 tP, const char* pcText, float fWrap);
plRect pl_calculate_text_bb_ex    (plFont* ptFont, float fSize, plVec2 tP, const char* pcText, const char* pcTextEnd, float fWrap);

// clipping
void          pl_push_clip_rect_pt(plDrawList* ptDrawlist, const plRect* ptRect);
void          pl_push_clip_rect   (plDrawList* ptDrawlist, plRect tRect, bool bAccumulate);
void          pl_pop_clip_rect    (plDrawList* ptDrawlist);
const plRect* pl_get_clip_rect    (plDrawList* ptDrawlist);

//-----------------------------------------------------------------------------
// [SECTION] enums & flags
//-----------------------------------------------------------------------------

enum plUiConditionFlags_
{
    PL_UI_COND_NONE   = 0,
    PL_UI_COND_ALWAYS = 1 << 0,
    PL_UI_COND_ONCE   = 1 << 1
};

enum plUiLayoutRowType_
{
    PL_UI_LAYOUT_ROW_TYPE_NONE, // don't use (internal only)
    PL_UI_LAYOUT_ROW_TYPE_DYNAMIC,
    PL_UI_LAYOUT_ROW_TYPE_STATIC
};


enum plMouseButton_
{
    PL_MOUSE_BUTTON_LEFT   = 0,
    PL_MOUSE_BUTTON_RIGHT  = 1,
    PL_MOUSE_BUTTON_MIDDLE = 2,
    PL_MOUSE_BUTTON_COUNT  = 5    
};

enum plKey_
{
    PL_KEY_NONE = 0,
    PL_KEY_TAB,
    PL_KEY_LEFT_ARROW,
    PL_KEY_RIGHT_ARROW,
    PL_KEY_UP_ARROW,
    PL_KEY_DOWN_ARROW,
    PL_KEY_PAGE_UP,
    PL_KEY_PAGE_DOWN,
    PL_KEY_HOME,
    PL_KEY_END,
    PL_KEY_INSERT,
    PL_KEY_DELETE,
    PL_KEY_BACKSPACE,
    PL_KEY_SPACE,
    PL_KEY_ENTER,
    PL_KEY_ESCAPE,
    PL_KEY_LEFT_CTRL,
    PL_KEY_LEFT_SHIFT,
    PL_KEY_LEFT_ALT,
    PL_KEY_LEFT_SUPER,
    PL_KEY_RIGHT_CTRL,
    PL_KEY_RIGHT_SHIFT,
    PL_KEY_RIGHT_ALT,
    PL_KEY_RIGHT_SUPER,
    PL_KEY_MENU,
    PL_KEY_0, PL_KEY_1, PL_KEY_2, PL_KEY_3, PL_KEY_4, PL_KEY_5, PL_KEY_6, PL_KEY_7, PL_KEY_8, PL_KEY_9,
    PL_KEY_A, PL_KEY_B, PL_KEY_C, PL_KEY_D, PL_KEY_E, PL_KEY_F, PL_KEY_G, PL_KEY_H, PL_KEY_I, PL_KEY_J,
    PL_KEY_K, PL_KEY_L, PL_KEY_M, PL_KEY_N, PL_KEY_O, PL_KEY_P, PL_KEY_Q, PL_KEY_R, PL_KEY_S, PL_KEY_T,
    PL_KEY_U, PL_KEY_V, PL_KEY_W, PL_KEY_X, PL_KEY_Y, PL_KEY_Z,
    PL_KEY_F1, PL_KEY_F2, PL_KEY_F3,PL_KEY_F4, PL_KEY_F5, PL_KEY_F6, PL_KEY_F7, PL_KEY_F8, PL_KEY_F9,
    PL_KEY_F10, PL_KEY_F11, PL_KEY_F12, PL_KEY_F13, PL_KEY_F14, PL_KEY_F15, PL_KEY_F16, PL_KEY_F17,
    PL_KEY_F18, PL_KEY_F19, PL_KEY_F20, PL_KEY_F21, PL_KEY_F22, PL_KEY_F23, PL_KEY_F24,
    PL_KEY_APOSTROPHE,    // '
    PL_KEY_COMMA,         // ,
    PL_KEY_MINUS,         // -
    PL_KEY_PERIOD,        // .
    PL_KEY_SLASH,         // /
    PL_KEY_SEMICOLON,     // ;
    PL_KEY_EQUAL,         // =
    PL_KEY_LEFT_BRACKET,  // [
    PL_KEY_BACKSLASH,     // \ (this text inhibit multiline comment caused by backslash)
    PL_KEY_RIGHT_BRACKET, // ]
    PL_KEY_GRAVE_ACCENT,  // `
    PL_KEY_CAPS_LOCK,
    PL_KEY_SCROLL_LOCK,
    PL_KEY_NUM_LOCK,
    PL_KEY_PRINT_SCREEN,
    PL_KEY_PAUSE,
    PL_KEY_KEYPAD_0, PL_KEY_KEYPAD_1, PL_KEY_KEYPAD_2, PL_KEY_KEYPAD_3, 
    PL_KEY_KEYPAD_4, PL_KEY_KEYPAD_5, PL_KEY_KEYPAD_6, PL_KEY_KEYPAD_7, 
    PL_KEY_KEYPAD_8, PL_KEY_KEYPAD_9,
    PL_KEY_KEYPAD_DECIMAL,
    PL_KEY_KEYPAD_DIVIDE,
    PL_KEY_KEYPAD_MULTIPLY,
    PL_KEY_KEYPAD_SUBTRACT,
    PL_KEY_KEYPAD_ADD,
    PL_KEY_KEYPAD_ENTER,
    PL_KEY_KEYPAD_EQUAL,
    
    PL_KEY_RESERVED_MOD_CTRL, PL_KEY_RESERVED_MOD_SHIFT, PL_KEY_RESERVED_MOD_ALT, PL_RESERVED_KEY_MOD_SUPER,
    PL_KEY_COUNT, // no valid plKey_ is ever greater than this value

    PL_KEY_MOD_NONE     = 0,
    PL_KEY_MOD_CTRL     = 1 << 12, // ctrl
    PL_KEY_MOD_SHIFT    = 1 << 13, // shift
    PL_KEY_MOD_ALT      = 1 << 14, // option/menu
    PL_KEY_MOD_SUPER    = 1 << 15, // cmd/super/windows
    PL_KEY_MOD_SHORTCUT = 1 << 11, // alias for Ctrl (non-macOS) _or_ super (macOS)
    PL_KEY_MOD_MASK_    = 0xF800   // 5 bits
};

enum plCursor_
{
    PL_MOUSE_CURSOR_NONE  = -1,
    PL_MOUSE_CURSOR_ARROW =  0,
    PL_MOUSE_CURSOR_TEXT_INPUT,
    PL_MOUSE_CURSOR_RESIZE_ALL,
    PL_MOUSE_CURSOR_RESIZE_NS,
    PL_MOUSE_CURSOR_RESIZE_EW,
    PL_MOUSE_CURSOR_RESIZE_NESW,
    PL_MOUSE_CURSOR_RESIZE_NWSE,
    PL_MOUSE_CURSOR_HAND,
    PL_MOUSE_CURSOR_NOT_ALLOWED,
    PL_MOUSE_CURSOR_COUNT
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

#ifndef PL_MATH_DEFINED
#define PL_MATH_DEFINED

typedef union _plVec2
{
    struct { float x, y; };
    struct { float r, g; };
    struct { float u, v; };
    float d[2];
} plVec2;

typedef struct _plRect
{
    plVec2 tMin;
    plVec2 tMax;
} plRect;

typedef union _plVec3
{
    struct { float x, y, z; };
    struct { float r, g, b; };
    struct { float u, v, __; };
    struct { plVec2 xy; float ignore0_; };
    struct { plVec2 rg; float ignore1_; };
    struct { plVec2 uv; float ignore2_; };
    struct { float ignore3_; plVec2 yz; };
    struct { float ignore4_; plVec2 gb; };
    struct { float ignore5_; plVec2 v__; };
    float d[3];
} plVec3;

typedef union _plVec4
{
    struct
    {
        union
        {
            plVec3 xyz;
            struct{ float x, y, z;};
        };

        float w;
    };
    struct
    {
        union
        {
            plVec3 rgb;
            struct{ float r, g, b;};
        };
        float a;
    };
    struct
    {
        plVec2 xy;
        float ignored0_, ignored1_;
    };
    struct
    {
        float ignored2_;
        plVec2 yz;
        float ignored3_;
    };
    struct
    {
        float ignored4_, ignored5_;
        plVec2 zw;
    };
    float d[4];
} plVec4;

#endif // PL_MATH_DEFINED

typedef struct _plUiClipper
{
    uint32_t uItemCount;
    uint32_t uDisplayStart;
    uint32_t uDisplayEnd;
    float    _fItemHeight;
    float    _fStartPosY;
} plUiClipper;

typedef struct _plKeyData
{
    bool  bDown;
    float fDownDuration;
    float fDownDurationPrev;
} plKeyData;

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

typedef struct _plDrawVertex
{
    float    pos[2];
    float    uv[2];
    uint32_t uColor;
} plDrawVertex;

typedef struct _plFontRange
{
    int         firstCodePoint;
    uint32_t    charCount;
    plFontChar* ptrFontChar; // offset into parent font's char data
} plFontRange;

typedef struct _plFontConfig
{
    float        fontSize;
    plFontRange* sbRanges;
    int*         sbIndividualChars;

    // BITMAP
    uint32_t vOverSampling;
    uint32_t hOverSampling;

    // SDF
    bool          sdf;
    int           sdfPadding;
    unsigned char onEdgeValue;
    float         sdfPixelDistScale;
} plFontConfig;

typedef struct _plFont
{
    plFontConfig config;
    plFontAtlas* parentAtlas;
    float        lineSpacing;
    float        ascent;
    float        descent;
    
    uint32_t*    sbCodePoints; // glyph index lookup based on codepoint
    plFontGlyph* sbGlyphs;     // glyphs
    plFontChar*  sbCharData;
} plFont;

typedef struct _plFontAtlas
{
    plFont*           sbFonts;
    plFontCustomRect* sbCustomRects;
    unsigned char*    pixelsAsAlpha8;
    unsigned char*    pixelsAsRGBA32;
    uint32_t          atlasSize[2];
    float             whiteUv[2];
    bool              dirty;
    int               glyphPadding;
    size_t            pixelDataSize;
    plFontCustomRect* whiteRect;
    plTextureId       texture;
    plFontPrepData*   _sbPrepData;
} plFontAtlas;

typedef struct _plDrawList
{
    plDrawLayer**  sbSubmittedLayers;
    plDrawLayer**  sbLayerCache;
    plDrawLayer**  sbLayersCreated;
    plDrawCommand* sbDrawCommands;
    plDrawVertex*  sbVertexBuffer;
    uint32_t       indexBufferByteSize;
    uint32_t       layersCreated;
    plRect*        sbClipStack;
} plDrawList;

typedef struct _plFontCustomRect
{
    uint32_t       width;
    uint32_t       height;
    uint32_t       x;
    uint32_t       y;
    unsigned char* bytes;
} plFontCustomRect;

typedef struct _plDrawCommand
{
    uint32_t    vertexOffset;
    uint32_t    indexOffset;
    uint32_t    elementCount;
    uint32_t    layer;
    plTextureId textureId;
    plRect      tClip;
    bool        sdf;
} plDrawCommand;

typedef struct _plDrawLayer
{
    const char*     name;
    plDrawList*     drawlist;
    plDrawCommand*  sbCommandBuffer;
    uint32_t*       sbIndexBuffer;
    plVec2*         sbPath;
    uint32_t        vertexCount;
    plDrawCommand*  _lastCommand;
} plDrawLayer;

typedef struct _plFontChar
{
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
    float    xOff;
    float    yOff;
    float    xAdv;
    float    xOff2;
    float    yOff2;
} plFontChar;

typedef struct _plFontGlyph
{
    float x0;
    float y0;
    float u0;
    float v0;
    float x1;
    float y1;
    float u1;
    float v1;
    float xAdvance;
    float leftBearing;  
} plFontGlyph;

typedef struct _plIO
{

    //------------------------------------------------------------------
    // Configuration
    //------------------------------------------------------------------

    float fDeltaTime;
    float fMouseDragThreshold;      // default 6.0f
    float fMouseDoubleClickTime;    // default 0.3f seconds
    float fMouseDoubleClickMaxDist; // default 6.0f
    float fKeyRepeatDelay;          // default 0.275f
    float fKeyRepeatRate;           // default 0.050f
    float afMainViewportSize[2];
    float afMainFramebufferScale[2];
    void* pUserData;

    // miscellaneous options
    bool bConfigMacOSXBehaviors;

    //------------------------------------------------------------------
    // platform functions
    //------------------------------------------------------------------

    void* pBackendPlatformData;
    void* pBackendRendererData;

    // access OS clipboard
    // (default to use native Win32 clipboard on Windows, otherwise uses a private clipboard. Override to access OS clipboard on other architectures)
    const char* (*get_clipboard_text_fn)(void* pUserData);
    void        (*set_clipboard_text_fn)(void* pUserData, const char* pcText);
    void*       pClipboardUserData;
    char*       sbcClipboardData;

    //------------------------------------------------------------------
    // Output
    //------------------------------------------------------------------

    double   dTime;
    float    fFrameRate; // rough estimate(rolling average of fDeltaTime over 120 frames)
    bool     bViewportSizeChanged;
    bool     bViewportMinimized;
    uint64_t ulFrameCount;

    // new
    plKeyChord tKeyMods;
    bool       bWantCaptureMouse;
    bool       bWantCaptureKeyboard;
    bool       bWantTextInput;
    bool       bKeyCtrl;  // Keyboard modifier down: Control
    bool       bKeyShift; // Keyboard modifier down: Shift
    bool       bKeyAlt;   // Keyboard modifier down: Alt
    bool       bKeySuper; // Keyboard modifier down: Cmd/Super/Windows

    // [INTERNAL]
    bool          _bOverflowInUse;
    plInputEvent  _atInputEvents[64];
    plInputEvent* _sbtInputEvents;
    uint32_t      _uInputEventSize;
    uint32_t      _uInputEventCapacity;
    uint32_t      _uInputEventOverflowCapacity;
    plWChar*      _sbInputQueueCharacters;
    plWChar       _tInputQueueSurrogate; 

    // main input state
    plVec2 _tMousePos;
    bool   _abMouseDown[5];
    int    _iMouseButtonsDown;
    float  _fMouseWheel;
    float  _fMouseWheelH;

    // mouse cursor
    plCursor tCurrentCursor;
    plCursor tNextCursor;
    bool     bCursorChanged;

    // other state
    plKeyData _tKeyData[PL_KEY_COUNT];
    plVec2    _tLastValidMousePos;
    plVec2    _tMouseDelta;
    plVec2    _tMousePosPrev;              // previous mouse position
    plVec2    _atMouseClickedPos[5];       // position when clicked
    double    _adMouseClickedTime[5];      // time of last click
    bool      _abMouseClicked[5];          // mouse went from !down to down
    bool      _abMouseOwned[5];
    uint32_t  _auMouseClickedCount[5];     // 
    uint32_t  _auMouseClickedLastCount[5]; // 
    bool      _abMouseReleased[5];         // mouse went from down to !down
    float     _afMouseDownDuration[5];     // duration mouse button has been down (0.0f == just clicked)
    float     _afMouseDownDurationPrev[5]; // previous duration of mouse button down
    float     _afMouseDragMaxDistSqr[5];   // squared max distance mouse traveled from clicked position

    // frame rate calcs
    float _afFrameRateSecPerFrame[120];
    int   _iFrameRateSecPerFrameIdx;
    int   _iFrameRateSecPerFrameCount;
    float _fFrameRateSecPerFrameAccum;

} plIO;

#endif // PL_UI_H
