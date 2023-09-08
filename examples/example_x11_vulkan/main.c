/*
   win32 + vulkan example
*/

/*
Index of this file:
// [SECTION] defines
// [SECTION] macros
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] globals
// [SECTION] entry point
// [SECTION] windows procedure
// [SECTION] implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_VK_KEYPAD_ENTER (VK_RETURN + 256)
#define VK_USE_PLATFORM_XCB_KHR

//-----------------------------------------------------------------------------
// [SECTION] macros
//-----------------------------------------------------------------------------

#ifndef PL_VULKAN
    #include <assert.h>
    #define PL_VULKAN(x) assert(x == VK_SUCCESS)
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_ui.h"
#include "pl_ui_internal.h"
#include "pl_ui_vulkan.h"
#include "vulkan/vulkan.h"

#include <time.h>     // clock_gettime, clock_getres
#include <string.h>   // strlen
#include <stdlib.h>   // free
#include <assert.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h> //xcb_xfixes_query_version, apt install libxcb-xfixes0-dev
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xcb/xcb_cursor.h> // apt install libxcb-cursor-dev, libxcb-cursor0
#include <xcb/xcb_keysyms.h>
#include <X11/XKBlib.h>
#include <sys/stat.h>     // stat, timespec
#include <stdio.h>        // file api
#include <dlfcn.h>        // dlopen, dlsym, dlclose
#include <sys/types.h>
#include <fcntl.h>        // O_RDONLY, O_WRONLY ,O_CREAT
#include <sys/sendfile.h> // sendfile
#include <sys/socket.h>   // sockets
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDevice        plDevice;
typedef struct _plGraphics      plGraphics;
typedef struct _plFrameContext plFrameContext;

// internal helpers
void             render_frame              (void);
void pl_update_mouse_cursor_linux(void);
void pl_linux_procedure          (xcb_generic_event_t* event);
plKey pl__xcb_key_to_pl_key(uint32_t x_keycode);

// clip board
static const char* pl__get_clipboard_text(void* user_data_ctx);
static void        pl__set_clipboard_text(void* pUnused, const char* text);

// vulkan
void setup_vulkan(plGraphics* ptGraphics);
void resize    (plGraphics* ptGraphics);
void cleanup   (plGraphics* ptGraphics);

    // per frame
bool begin_frame    (plGraphics* ptGraphics);
void end_frame      (plGraphics* ptGraphics);
void begin_recording(plGraphics* ptGraphics);
void end_recording  (plGraphics* ptGraphics);

plFrameContext* get_frame_resources(plGraphics* ptGraphics);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVulkanSwapchain
{
    VkSwapchainKHR           tSwapChain;
    VkExtent2D               tExtent;
    VkFramebuffer*           sbtFrameBuffers;
    VkFormat                 tFormat;
    VkFormat                 tDepthFormat;
    uint32_t                 uImageCount;
    VkImage*                 sbtImages;
    VkImageView*             sbtImageViews;
    VkImage                  tColorTexture;
    VkDeviceMemory           tColorTextureMemory;
    VkImageView              tColorTextureView;
    VkImage                  tDepthTexture;
    VkDeviceMemory           tDepthTextureMemory;
    VkImageView              tDepthTextureView;
    uint32_t                 uCurrentImageIndex; // current image to use within the swap chain
    bool                     bVSync;
    VkSampleCountFlagBits    tMsaaSamples;
    VkSurfaceFormatKHR*      sbtSurfaceFormats;

} plVulkanSwapchain;

typedef struct _plFrameGarbage
{
    VkImage*        sbtTextures;
    VkImageView*    sbtTextureViews;
    VkFramebuffer*  sbtFrameBuffers;
    VkDeviceMemory* sbtMemory;
} plFrameGarbage;

typedef struct _plFrameContext
{
    VkSemaphore     tImageAvailable;
    VkSemaphore     tRenderFinish;
    VkFence         tInFlight;
    VkCommandPool   tCmdPool;
    VkCommandBuffer tCmdBuf;
} plFrameContext;

typedef struct _plDevice
{
    VkDevice                                  tLogicalDevice;
    VkPhysicalDevice                          tPhysicalDevice;
    int                                       iGraphicsQueueFamily;
    int                                       iPresentQueueFamily;
    VkQueue                                   tGraphicsQueue;
    VkQueue                                   tPresentQueue;
    VkPhysicalDeviceProperties                tDeviceProps;
    VkPhysicalDeviceMemoryProperties          tMemProps;
    VkPhysicalDeviceMemoryProperties2         tMemProps2;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT tMemBudgetInfo;
    VkDeviceSize                              tMaxLocalMemSize;
    VkPhysicalDeviceFeatures                  tDeviceFeatures;
    bool                                      bSwapchainExtPresent;
    bool                                      bPortabilitySubsetPresent;
    VkCommandPool                             tCmdPool;
    uint32_t                                  uUniformBufferBlockSize;
    uint32_t                                  uCurrentFrame;

	PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTag;
	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
	PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsert;

    // [INTERNAL]
    plFrameGarbage* _sbtFrameGarbage;
} plDevice;

typedef struct _plGraphics
{
    plDevice tDevice;
    VkInstance               tInstance;
    VkDebugUtilsMessengerEXT tDbgMessenger;
    VkSurfaceKHR             tSurface;
    plFrameContext*          sbFrames;
    VkRenderPass             tRenderPass;
    uint32_t                 uFramesInFlight;
    size_t                   szCurrentFrameIndex; // current frame being used
    VkDescriptorPool         tDescriptorPool;
    plVulkanSwapchain        tSwapchain;
} plGraphics;

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// x11 & xcb stuff
Display*              gDisplay       = NULL;
xcb_connection_t*     gConnection    = NULL;
xcb_key_symbols_t*    gKeySyms       = NULL;
xcb_window_t          gWindow;
xcb_screen_t*         gScreen        = NULL;
bool                  gRunning       = true;
xcb_atom_t            gWmProtocols;
xcb_atom_t            gWmDeleteWin;
double                dTime           = 0.0;
double                dFrequency      = 0.0;
xcb_cursor_context_t* ptCursorContext = NULL;
plIO*           gtIO             = NULL;
plUiContext*    gptUiCtx         = NULL;
plGraphics      gtGraphics       = {0};

plDrawList   drawlist;
plDrawLayer* fgDrawLayer;
plDrawLayer* bgDrawLayer;
plFontAtlas  fontAtlas;

static inline double
pl__get_linux_absolute_time(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
    {
        assert(false && "clock_gettime() failed");
    }
    uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
    return (double)nsec_count / dFrequency;    
}

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{

    // os provided apis
    gptUiCtx = pl_create_ui_context();

    // setup & retrieve io context 
    gtIO = pl_get_io();

    // set clipboard functions (may need to move this to OS api)
    // gtIO->set_clipboard_text_fn = pl__set_clipboard_text;
    // gtIO->get_clipboard_text_fn = pl__get_clipboard_text;

    // connect to x
    gDisplay = XOpenDisplay(NULL);

    // turn off auto repeat (we handle this internally)
    XAutoRepeatOff(gDisplay);

    int screen_p = 0;
    gConnection = xcb_connect(NULL, &screen_p);
    if(xcb_connection_has_error(gConnection))
    {
        assert(false && "Failed to connect to X server via XCB.");
    }

    // get data from x server
    const xcb_setup_t* setup = xcb_get_setup(gConnection);

    // loop through screens using iterator
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    
    for (int s = screen_p; s > 0; s--) 
    {
        xcb_screen_next(&it);
    }

    // allocate a XID for the window to be created.
    gWindow = xcb_generate_id(gConnection);

    // after screens have been looped through, assign it.
    gScreen = it.data;

    // register event types.
    // XCB_CW_BACK_PIXEL = filling then window bg with a single colour
    // XCB_CW_EVENT_MASK is required.
    unsigned int event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    // listen for keyboard and mouse buttons
    unsigned int  event_values = 
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    // values to be sent over XCB (bg colour, events)
    unsigned int  value_list[] = {gScreen->black_pixel, event_values};

    // Create the window
    xcb_create_window(
        gConnection,
        XCB_COPY_FROM_PARENT,  // depth
        gWindow,               // window
        gScreen->root,         // parent
        200,                   // x
        200,                   // y
        500,                   // width
        500,                   // height
        0,                     // No border
        XCB_WINDOW_CLASS_INPUT_OUTPUT,  //class
        gScreen->root_visual,
        event_mask,
        value_list);

    // setup timers
    static struct timespec ts;
    if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) 
    {
        assert(false && "clock_getres() failed");
    }
    dFrequency = 1e9/((double)ts.tv_nsec + (double)ts.tv_sec * (double)1e9);
    dTime = pl__get_linux_absolute_time();

    // Notify X for mouse cursor handling
    xcb_discard_reply(gConnection, xcb_xfixes_query_version(gConnection, 4, 0).sequence);

    // Cursor context for looking up cursors for the current X cursor theme
    xcb_cursor_context_new(gConnection, gScreen, &ptCursorContext);

    // Change the title
    xcb_change_property(
        gConnection,
        XCB_PROP_MODE_REPLACE,
        gWindow,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,  // data should be viewed 8 bits at a time
        strlen("Pilot Light (linux)"),
        "Pilot Light (linux)");

    // Tell the server to notify when the window manager
    // attempts to destroy the window.
    xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(
        gConnection,
        0,
        strlen("WM_DELETE_WINDOW"),
        "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
        gConnection,
        0,
        strlen("WM_PROTOCOLS"),
        "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* wm_delete_reply = xcb_intern_atom_reply(
        gConnection,
        wm_delete_cookie,
        NULL);
    xcb_intern_atom_reply_t* wm_protocols_reply = xcb_intern_atom_reply(
        gConnection,
        wm_protocols_cookie,
        NULL);
    gWmDeleteWin = wm_delete_reply->atom;
    gWmProtocols = wm_protocols_reply->atom;

    xcb_change_property(
        gConnection,
        XCB_PROP_MODE_REPLACE,
        gWindow,
        wm_protocols_reply->atom,
        4,
        32,
        1,
        &wm_delete_reply->atom);

    // Map the window to the screen
    xcb_map_window(gConnection, gWindow);

    // Flush the stream
    int stream_result = xcb_flush(gConnection);

    static struct {
        xcb_connection_t* ptConnection;
        xcb_window_t      tWindow;
    } platformData;
    platformData.ptConnection = gConnection;
    platformData.tWindow = gWindow;
    gtIO->pBackendPlatformData = &platformData;

    // get the current key map
    gKeySyms = xcb_key_symbols_alloc(gConnection);

    setup_vulkan(&gtGraphics);

    // setup drawing api
    const plVulkanInit tVulkanInit = {
        .tPhysicalDevice  = gtGraphics.tDevice.tPhysicalDevice,
        .tLogicalDevice   = gtGraphics.tDevice.tLogicalDevice,
        .uImageCount      = gtGraphics.tSwapchain.uImageCount,
        .tRenderPass      = gtGraphics.tRenderPass,
        .tMSAASampleCount = gtGraphics.tSwapchain.tMsaaSamples,
        .uFramesInFlight  = gtGraphics.uFramesInFlight
    };
    pl_initialize_vulkan(&tVulkanInit);

    // create draw list & layers
    pl_register_drawlist(&drawlist);
    bgDrawLayer = pl_request_layer(&drawlist, "Background Layer");
    fgDrawLayer = pl_request_layer(&drawlist, "Foreground Layer");
    
    // create font atlas
    pl_add_default_font(&fontAtlas);
    pl_build_font_atlas(&fontAtlas);
    pl_build_vulkan_font_atlas(&fontAtlas);
    pl_set_default_font(&fontAtlas.sbFonts[0]);

    // main loop
    while (gRunning)
    {

        // Poll for events until null is returned.
        xcb_generic_event_t* event;
        while (event = xcb_poll_for_event(gConnection)) 
            pl_linux_procedure(event);

        if(gtIO->bViewportSizeChanged) //-V547
            resize(&gtGraphics);

        pl_update_mouse_cursor_linux();

        // render frame
        if(gRunning)
            render_frame();
    }

    pl_cleanup_vulkan_font_atlas(&fontAtlas);
    pl_cleanup_font_atlas(&fontAtlas);

    
    pl_cleanup_vulkan();
    pl_destroy_ui_context();

    cleanup(&gtGraphics);

    // platform cleanup
    XAutoRepeatOn(gDisplay);
    xcb_destroy_window(gConnection, gWindow);
    xcb_cursor_context_free(ptCursorContext);
    xcb_key_symbols_free(gKeySyms);
}

//-----------------------------------------------------------------------------
// [SECTION] windows procedure
//-----------------------------------------------------------------------------

void
pl_linux_procedure(xcb_generic_event_t* event)
{

    xcb_client_message_event_t* cm;

    switch (event->response_type & ~0x80) 
    {

        case XCB_CLIENT_MESSAGE: 
        {
            cm = (xcb_client_message_event_t*)event;

            // Window close
            if (cm->data.data32[0] == gWmDeleteWin) 
            {
                gRunning  = false;
            }
            break;
        }

        case XCB_MOTION_NOTIFY: 
        {
            xcb_motion_notify_event_t* motion = (xcb_motion_notify_event_t*)event;
            pl_add_mouse_pos_event((float)motion->event_x, (float)motion->event_y);
            break;
        }

        case XCB_BUTTON_PRESS:
        {
            xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
            switch (press->detail)
            {
                case XCB_BUTTON_INDEX_1: pl_add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, true);   break;
                case XCB_BUTTON_INDEX_2: pl_add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, true); break;
                case XCB_BUTTON_INDEX_3: pl_add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, true);  break;
                case XCB_BUTTON_INDEX_4: pl_add_mouse_wheel_event (0.0f, 1.0f);                   break;
                case XCB_BUTTON_INDEX_5: pl_add_mouse_wheel_event (0.0f, -1.0f);                  break;
                default:                 pl_add_mouse_button_event(press->detail, true);          break;
            }
            break;
        }
        
        case XCB_BUTTON_RELEASE:
        {
            xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
            switch (press->detail)
            {
                case XCB_BUTTON_INDEX_1: pl_add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, false);   break;
                case XCB_BUTTON_INDEX_2: pl_add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, false); break;
                case XCB_BUTTON_INDEX_3: pl_add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, false);  break;
                case XCB_BUTTON_INDEX_4: pl_add_mouse_wheel_event (0.0f, 1.0f);                   break;
                case XCB_BUTTON_INDEX_5: pl_add_mouse_wheel_event (0.0f, -1.0f);                  break;
                default:                 pl_add_mouse_button_event(press->detail, false);          break;
            }
            break;
        }

        case XCB_KEY_PRESS:
        {
            xcb_key_release_event_t *keyEvent = (xcb_key_release_event_t *)event;

            xcb_keycode_t code = keyEvent->detail;
            uint32_t uCol = gtIO->bKeyShift ? 1 : 0;
            KeySym key_sym = XkbKeycodeToKeysym(
                gDisplay, 
                (KeyCode)code,  // event.xkey.keycode,
                0,
                uCol);
            xcb_keysym_t k = xcb_key_press_lookup_keysym(gKeySyms, keyEvent, uCol);
            pl_add_key_event(pl__xcb_key_to_pl_key(key_sym), true);
            if(k < 0xFF)
                pl_add_text_event(k);
            else if (k >= 0x1000100 && k <= 0x110ffff) // utf range
                pl_add_text_event_utf16(k);
            break;
        }
        case XCB_KEY_RELEASE:
        {
            const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
            xcb_keycode_t code = keyEvent->detail;
            KeySym key_sym = XkbKeycodeToKeysym(
                gDisplay, 
                (KeyCode)code,  // event.xkey.keycode,
                0,
                0 /*code & ShiftMask ? 1 : 0*/);
            pl_add_key_event(pl__xcb_key_to_pl_key(key_sym), false);
            break;
        }
        case XCB_CONFIGURE_NOTIFY: 
        {
            // Resizing - note that this is also triggered by moving the window, but should be
            // passed anyway since a change in the x/y could mean an upper-left resize.
            // The application layer can decide what to do with this.
            xcb_configure_notify_event_t* configure_event = (xcb_configure_notify_event_t*)event;

            // Fire the event. The application layer should pick this up, but not handle it
            // as it shouldn be visible to other parts of the application.
            if(configure_event->width != gtIO->afMainViewportSize[0] || configure_event->height != gtIO->afMainViewportSize[1])
            {
                gtIO->afMainViewportSize[0] = configure_event->width;
                gtIO->afMainViewportSize[1] = configure_event->height;
                gtIO->bViewportSizeChanged = true;
            }
            break;
        } 
        default: break;
    }
    free(event);
}

void
pl_update_mouse_cursor_linux(void)
{
    // updating mouse cursor
    if(gtIO->tCurrentCursor != PL_MOUSE_CURSOR_ARROW && gtIO->tNextCursor == PL_MOUSE_CURSOR_ARROW)
        gtIO->bCursorChanged = true;

    if(gtIO->bCursorChanged && gtIO->tNextCursor != gtIO->tCurrentCursor)
    {
        gtIO->tCurrentCursor = gtIO->tNextCursor;
        const char* tX11Cursor = NULL;
        switch (gtIO->tNextCursor)
        {
            case PL_MOUSE_CURSOR_ARROW:       tX11Cursor = "left_ptr"; break;
            case PL_MOUSE_CURSOR_TEXT_INPUT:  tX11Cursor = "xterm"; break;
            case PL_MOUSE_CURSOR_RESIZE_ALL:  tX11Cursor = "fleur"; break;
            case PL_MOUSE_CURSOR_RESIZE_EW:   tX11Cursor = "sb_h_double_arrow"; break;
            case PL_MOUSE_CURSOR_RESIZE_NS:   tX11Cursor = "sb_v_double_arrow"; break;
            case PL_MOUSE_CURSOR_RESIZE_NESW: tX11Cursor = "bottom_left_corner"; break;
            case PL_MOUSE_CURSOR_RESIZE_NWSE: tX11Cursor = "bottom_right_corner"; break;
            case PL_MOUSE_CURSOR_HAND:        tX11Cursor = "hand1"; break;
            case PL_MOUSE_CURSOR_NOT_ALLOWED: tX11Cursor = "circle"; break;
        }  

        xcb_font_t font = xcb_generate_id(gConnection);
        // There is xcb_xfixes_cursor_change_cursor_by_name. However xcb_cursor_load_cursor guarantees
        // finding the cursor for the current X theme.
        xcb_cursor_t cursor = xcb_cursor_load_cursor(ptCursorContext, tX11Cursor);
        // IM_ASSERT(cursor && "X cursor not found!");

        uint32_t value_list = cursor;
        xcb_change_window_attributes(gConnection, gWindow, XCB_CW_CURSOR, &value_list);
        xcb_free_cursor(gConnection, cursor);
        xcb_close_font_checked(gConnection, font);
    }
    gtIO->tNextCursor = PL_MOUSE_CURSOR_ARROW;
    gtIO->bCursorChanged = false;
}

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

void
render_frame(void)
{
    const double dCurrentTime = pl__get_linux_absolute_time();
    gtIO->fDeltaTime = (float)(dCurrentTime - dTime);
    dTime = dCurrentTime;
    if(!gtIO->bViewportMinimized)
    {
        // pl_app_update(gpUserData);
        if(begin_frame(&gtGraphics))
        {

            begin_recording(&gtGraphics);

            pl_new_draw_frame_vulkan();
            pl_new_frame();

            pl_demo(NULL);

            pl_add_line(fgDrawLayer, (plVec2){0}, (plVec2){300.0f, 300.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);

            pl_submit_layer(bgDrawLayer);
            pl_submit_layer(fgDrawLayer);

            plFrameContext* ptCurrentFrame = get_frame_resources(&gtGraphics);

            gptUiCtx->tFrameBufferScale.x = gtIO->afMainFramebufferScale[0];
            gptUiCtx->tFrameBufferScale.y = gtIO->afMainFramebufferScale[1];
            pl_render();
            pl_submit_vulkan_drawlist(&drawlist, gtIO->afMainViewportSize[0], gtIO->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)gtGraphics.szCurrentFrameIndex);
            pl_submit_vulkan_drawlist(pl_get_draw_list(NULL), gtIO->afMainViewportSize[0], gtIO->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)gtGraphics.szCurrentFrameIndex);
            pl_submit_vulkan_drawlist(pl_get_debug_draw_list(NULL), gtIO->afMainViewportSize[0], gtIO->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)gtGraphics.szCurrentFrameIndex);

            end_recording(&gtGraphics);
            end_frame(&gtGraphics);
        }
    }
}

plKey
pl__xcb_key_to_pl_key(uint32_t x_keycode)
{
    switch (x_keycode) 
    {
        case XKB_KEY_BackSpace:   return PL_KEY_BACKSPACE;
        case XKB_KEY_Return:      return PL_KEY_ENTER;
        case XKB_KEY_Tab:         return PL_KEY_TAB;
        case XKB_KEY_Pause:       return PL_KEY_PAUSE;
        case XKB_KEY_Caps_Lock:   return PL_KEY_CAPS_LOCK;
        case XKB_KEY_Escape:      return PL_KEY_ESCAPE;
        case XKB_KEY_space:       return PL_KEY_SPACE;
        case XKB_KEY_Prior:       return PL_KEY_PAGE_UP;
        case XKB_KEY_Next:        return PL_KEY_PAGE_DOWN;
        case XKB_KEY_End:         return PL_KEY_END;
        case XKB_KEY_Home:        return PL_KEY_HOME;
        case XKB_KEY_Left:        return PL_KEY_LEFT_ARROW;
        case XKB_KEY_Up:          return PL_KEY_UP_ARROW;
        case XKB_KEY_Right:       return PL_KEY_RIGHT_ARROW;
        case XKB_KEY_Down:        return PL_KEY_DOWN_ARROW;
        case XKB_KEY_Print:       return PL_KEY_PRINT_SCREEN;
        case XKB_KEY_Insert:      return PL_KEY_INSERT;
        case XKB_KEY_Delete:      return PL_KEY_DELETE;
        case XKB_KEY_Help:        return PL_KEY_MENU;
        case XKB_KEY_Meta_L:      return PL_KEY_LEFT_SUPER;
        case XKB_KEY_Meta_R:      return PL_KEY_RIGHT_SUPER;
        case XKB_KEY_KP_0:        return PL_KEY_KEYPAD_0;
        case XKB_KEY_KP_1:        return PL_KEY_KEYPAD_1;
        case XKB_KEY_KP_2:        return PL_KEY_KEYPAD_2;
        case XKB_KEY_KP_3:        return PL_KEY_KEYPAD_3;
        case XKB_KEY_KP_4:        return PL_KEY_KEYPAD_4;
        case XKB_KEY_KP_5:        return PL_KEY_KEYPAD_5;
        case XKB_KEY_KP_6:        return PL_KEY_KEYPAD_6;
        case XKB_KEY_KP_7:        return PL_KEY_KEYPAD_7;
        case XKB_KEY_KP_8:        return PL_KEY_KEYPAD_8;
        case XKB_KEY_KP_9:        return PL_KEY_KEYPAD_9;
        case XKB_KEY_multiply:    return PL_KEY_KEYPAD_MULTIPLY;
        case XKB_KEY_KP_Add:      return PL_KEY_KEYPAD_ADD;   ;
        case XKB_KEY_KP_Subtract: return PL_KEY_KEYPAD_SUBTRACT;
        case XKB_KEY_KP_Decimal:  return PL_KEY_KEYPAD_DECIMAL;
        case XKB_KEY_KP_Divide:   return PL_KEY_KEYPAD_DIVIDE;
        case XKB_KEY_F1:          return PL_KEY_F1;
        case XKB_KEY_F2:          return PL_KEY_F2;
        case XKB_KEY_F3:          return PL_KEY_F3;
        case XKB_KEY_F4:          return PL_KEY_F4;
        case XKB_KEY_F5:          return PL_KEY_F5;
        case XKB_KEY_F6:          return PL_KEY_F6;
        case XKB_KEY_F7:          return PL_KEY_F7;
        case XKB_KEY_F8:          return PL_KEY_F8;
        case XKB_KEY_F9:          return PL_KEY_F9;
        case XKB_KEY_F10:         return PL_KEY_F10;
        case XKB_KEY_F11:         return PL_KEY_F11;
        case XKB_KEY_F12:         return PL_KEY_F12;
        case XKB_KEY_F13:         return PL_KEY_F13;
        case XKB_KEY_F14:         return PL_KEY_F14;
        case XKB_KEY_F15:         return PL_KEY_F15;
        case XKB_KEY_F16:         return PL_KEY_F16;
        case XKB_KEY_F17:         return PL_KEY_F17;
        case XKB_KEY_F18:         return PL_KEY_F18;
        case XKB_KEY_F19:         return PL_KEY_F19;
        case XKB_KEY_F20:         return PL_KEY_F20;
        case XKB_KEY_F21:         return PL_KEY_F21;
        case XKB_KEY_F22:         return PL_KEY_F22;
        case XKB_KEY_F23:         return PL_KEY_F23;
        case XKB_KEY_F24:         return PL_KEY_F24;
        case XKB_KEY_Num_Lock:    return PL_KEY_NUM_LOCK;
        case XKB_KEY_Scroll_Lock: return PL_KEY_SCROLL_LOCK;
        case XKB_KEY_KP_Equal:    return PL_KEY_KEYPAD_EQUAL;
        case XKB_KEY_Shift_L:     return PL_KEY_LEFT_SHIFT;
        case XKB_KEY_Shift_R:     return PL_KEY_RIGHT_SHIFT;
        case XKB_KEY_Control_L:   return PL_KEY_LEFT_CTRL;
        case XKB_KEY_Control_R:   return PL_KEY_RIGHT_CTRL;
        case XKB_KEY_Alt_L:       return PL_KEY_LEFT_ALT;
        case XKB_KEY_Alt_R:       return PL_KEY_RIGHT_ALT;
        case XKB_KEY_semicolon:   return PL_KEY_SEMICOLON;
        case XKB_KEY_plus:        return PL_KEY_KEYPAD_ADD;
        case XKB_KEY_comma:       return PL_KEY_COMMA;
        case XKB_KEY_minus:       return PL_KEY_MINUS;
        case XKB_KEY_period:      return PL_KEY_PERIOD;
        case XKB_KEY_slash:       return PL_KEY_SLASH;
        case XKB_KEY_grave:       return PL_KEY_GRAVE_ACCENT;
        case XKB_KEY_0:           return PL_KEY_0;
        case XKB_KEY_1:           return PL_KEY_1;
        case XKB_KEY_2:           return PL_KEY_2;
        case XKB_KEY_3:           return PL_KEY_3;
        case XKB_KEY_4:           return PL_KEY_4;
        case XKB_KEY_5:           return PL_KEY_5;
        case XKB_KEY_6:           return PL_KEY_6;
        case XKB_KEY_7:           return PL_KEY_7;
        case XKB_KEY_8:           return PL_KEY_8;
        case XKB_KEY_9:           return PL_KEY_9;
        case XKB_KEY_a:
        case XKB_KEY_A:           return PL_KEY_A;
        case XKB_KEY_b:
        case XKB_KEY_B:           return PL_KEY_B;
        case XKB_KEY_c:
        case XKB_KEY_C:           return PL_KEY_C;
        case XKB_KEY_d:
        case XKB_KEY_D:           return PL_KEY_D;
        case XKB_KEY_e:
        case XKB_KEY_E:           return PL_KEY_E;
        case XKB_KEY_f:
        case XKB_KEY_F:           return PL_KEY_F;
        case XKB_KEY_g:
        case XKB_KEY_G:           return PL_KEY_G;
        case XKB_KEY_h:
        case XKB_KEY_H:           return PL_KEY_H;
        case XKB_KEY_i:
        case XKB_KEY_I:           return PL_KEY_I;
        case XKB_KEY_j:
        case XKB_KEY_J:           return PL_KEY_J;
        case XKB_KEY_k:
        case XKB_KEY_K:           return PL_KEY_K;
        case XKB_KEY_l:
        case XKB_KEY_L:           return PL_KEY_L;
        case XKB_KEY_m:
        case XKB_KEY_M:           return PL_KEY_M;
        case XKB_KEY_n:
        case XKB_KEY_N:           return PL_KEY_N;
        case XKB_KEY_o:
        case XKB_KEY_O:           return PL_KEY_O;
        case XKB_KEY_p:
        case XKB_KEY_P:           return PL_KEY_P;
        case XKB_KEY_q:
        case XKB_KEY_Q:           return PL_KEY_Q;
        case XKB_KEY_r:
        case XKB_KEY_R:           return PL_KEY_R;
        case XKB_KEY_s:
        case XKB_KEY_S:           return PL_KEY_S;
        case XKB_KEY_t:
        case XKB_KEY_T:           return PL_KEY_T;
        case XKB_KEY_u:
        case XKB_KEY_U:           return PL_KEY_U;
        case XKB_KEY_v:
        case XKB_KEY_V:           return PL_KEY_V;
        case XKB_KEY_w:
        case XKB_KEY_W:           return PL_KEY_W;
        case XKB_KEY_x:
        case XKB_KEY_X:           return PL_KEY_X;
        case XKB_KEY_y:
        case XKB_KEY_Y:           return PL_KEY_Y;
        case XKB_KEY_z:
        case XKB_KEY_Z:           return PL_KEY_Z;
        default:
        return PL_KEY_NONE;
    }            
}

// Convert UTF-8 to 32-bit character, process single character input.
// A nearly-branchless UTF-8 decoder, based on work of Christopher Wellons (https://github.com/skeeto/branchless-utf8).
// We handle UTF-8 decoding error by skipping forward.

bool
pl_str_equal(const char* pcStr0, const char* pcStr1)
{
    return strcmp(pcStr0, pcStr1) == 0;
}



//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

plFrameContext*
get_frame_resources(plGraphics* ptGraphics)
{
    return &ptGraphics->sbFrames[ptGraphics->szCurrentFrameIndex];
}

static VkDeviceMemory
allocate_dedicated(plDevice* ptDevice, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    uint32_t uMemoryType = 0u;
    bool bFound = false;
    for (uint32_t i = 0; i < ptDevice->tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (ptDevice->tMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) 
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    assert(bFound);

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = (uint32_t)ulSize,
        .memoryTypeIndex = uMemoryType
    };

    VkDeviceMemory tMemory = VK_NULL_HANDLE;
    VkResult tResult = vkAllocateMemory(ptDevice->tLogicalDevice, &tAllocInfo, NULL, &tMemory);
    PL_VULKAN(tResult);

    return tMemory;
}

static VkSampleCountFlagBits
get_max_sample_count(plDevice* ptDevice)
{

    VkPhysicalDeviceProperties tPhysicalDeviceProperties = {0};
    vkGetPhysicalDeviceProperties(ptDevice->tPhysicalDevice, &tPhysicalDeviceProperties);

    VkSampleCountFlags tCounts = tPhysicalDeviceProperties.limits.framebufferColorSampleCounts & tPhysicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (tCounts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_8_BIT)  { return VK_SAMPLE_COUNT_8_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_4_BIT)  { return VK_SAMPLE_COUNT_4_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_2_BIT)  { return VK_SAMPLE_COUNT_2_BIT; }
    return VK_SAMPLE_COUNT_1_BIT;    
}

static VkFormat
find_supported_format(plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount)
{

    for(uint32_t i = 0u; i < uFormatCount; i++)
    {
        VkFormatProperties tProps = {0};
        vkGetPhysicalDeviceFormatProperties(ptDevice->tPhysicalDevice, ptFormats[i], &tProps);
        if(tProps.optimalTilingFeatures & tFlags)
            return ptFormats[i];
    }

    assert(false && "no supported format found");
    return VK_FORMAT_UNDEFINED;
}

static VkFormat
find_depth_format(plDevice* ptDevice)
{
    const VkFormat atFormats[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    return find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 3);
}

static VkFormat
find_depth_stencil_format(plDevice* ptDevice)
{
     const VkFormat atFormats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    return find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 2);   
}

static bool
format_has_stencil(VkFormat tFormat)
{
    switch(tFormat)
    {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
        case VK_FORMAT_D32_SFLOAT:
        default: return false;
    }
}

static void
transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask)
{
    //VkCommandBuffer commandBuffer = mvBeginSingleTimeCommands();
    VkImageMemoryBarrier tBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = tOldLayout,
        .newLayout           = tNewLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = tImage,
        .subresourceRange    = tSubresourceRange,
    };

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (tOldLayout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            tBarrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            tBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            tBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            tBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch (tNewLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            tBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            tBarrier.dstAccessMask = tBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (tBarrier.srcAccessMask == 0)
                tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            tBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
    }
    vkCmdPipelineBarrier(tCommandBuffer, tSrcStageMask, tDstStageMask, 0, 0, NULL, 0, NULL, 1, &tBarrier);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData) 
{
    if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        printf("error validation layer: %s\n", ptCallbackData->pMessage);
        //pl_log_error_to_f(uLogChannel, "error validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        printf("warn validation layer: %s\n", ptCallbackData->pMessage);
        //pl_log_warn_to_f(uLogChannel, "warn validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        // //pl_log_trace_to_f(uLogChannel, "info validation layer: %s\n", ptCallbackData->pMessage);
    }
    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        // //pl_log_trace_to_f(uLogChannel, "trace validation layer: %s\n", ptCallbackData->pMessage);
    }
    
    return VK_FALSE;
}

static void
create_swapchain(plGraphics* ptGraphics, uint32_t uWidth, uint32_t uHeight, plVulkanSwapchain* ptSwapchainOut)
{
    plDevice* ptDevice = &ptGraphics->tDevice;

    vkDeviceWaitIdle(ptDevice->tLogicalDevice);

    ptSwapchainOut->tMsaaSamples = get_max_sample_count(&ptGraphics->tDevice);

    // query swapchain support

    VkSurfaceCapabilitiesKHR tCapabilities = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptDevice->tPhysicalDevice, ptGraphics->tSurface, &tCapabilities));

    uint32_t uFormatCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptDevice->tPhysicalDevice, ptGraphics->tSurface, &uFormatCount, NULL));
    
    plu_sb_resize(ptSwapchainOut->sbtSurfaceFormats, uFormatCount);

    VkBool32 tPresentSupport = false;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptDevice->tPhysicalDevice, 0, ptGraphics->tSurface, &tPresentSupport));
    assert(uFormatCount > 0);
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptDevice->tPhysicalDevice, ptGraphics->tSurface, &uFormatCount, ptSwapchainOut->sbtSurfaceFormats));

    uint32_t uPresentModeCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptDevice->tPhysicalDevice, ptGraphics->tSurface, &uPresentModeCount, NULL));
    assert(uPresentModeCount > 0 && uPresentModeCount < 16);

    VkPresentModeKHR atPresentModes[16] = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptDevice->tPhysicalDevice, ptGraphics->tSurface, &uPresentModeCount, atPresentModes));

    // choose swap tSurface Format
    static VkFormat atSurfaceFormatPreference[4] = 
    {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB
    };

    bool bPreferenceFound = false;
    VkSurfaceFormatKHR tSurfaceFormat = ptSwapchainOut->sbtSurfaceFormats[0];
    ptSwapchainOut->tFormat = tSurfaceFormat.format;

    for(uint32_t i = 0u; i < 4; i++)
    {
        if(bPreferenceFound) break;
        
        for(uint32_t j = 0u; j < uFormatCount; j++)
        {
            if(ptSwapchainOut->sbtSurfaceFormats[j].format == atSurfaceFormatPreference[i] && ptSwapchainOut->sbtSurfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                tSurfaceFormat = ptSwapchainOut->sbtSurfaceFormats[j];
                ptSwapchainOut->tFormat = tSurfaceFormat.format;
                bPreferenceFound = true;
                break;
            }
        }
    }
    assert(bPreferenceFound && "no preferred surface format found");

    // chose swap present mode
    VkPresentModeKHR tPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!ptSwapchainOut->bVSync)
    {
        for(uint32_t i = 0 ; i < uPresentModeCount; i++)
        {
			if (atPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				tPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if (atPresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				tPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    // chose swap extent 
    VkExtent2D tExtent = {0};
    if(tCapabilities.currentExtent.width != UINT32_MAX)
        tExtent = tCapabilities.currentExtent;
    else
    {
        tExtent.width = plu_max(tCapabilities.minImageExtent.width, plu_min(tCapabilities.maxImageExtent.width, uWidth));
        tExtent.height = plu_max(tCapabilities.minImageExtent.height, plu_min(tCapabilities.maxImageExtent.height, uHeight));
    }
    ptSwapchainOut->tExtent = tExtent;

    // decide image count
    const uint32_t uOldImageCount = ptSwapchainOut->uImageCount;
    uint32_t uDesiredMinImageCount = tCapabilities.minImageCount + 1;
    if(tCapabilities.maxImageCount > 0 && uDesiredMinImageCount > tCapabilities.maxImageCount) 
        uDesiredMinImageCount = tCapabilities.maxImageCount;

	// find the transformation of the surface
	VkSurfaceTransformFlagsKHR tPreTransform;
	if (tCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		// We prefer a non-rotated transform
		tPreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		tPreTransform = tCapabilities.currentTransform;
	}

	// find a supported composite alpha format (not all devices support alpha opaque)
	VkCompositeAlphaFlagBitsKHR tCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	
    // simply select the first composite alpha format available
	static const VkCompositeAlphaFlagBitsKHR atCompositeAlphaFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for (int i = 0; i < 4; i++)
    {
		if (tCapabilities.supportedCompositeAlpha & atCompositeAlphaFlags[i]) 
        {
			tCompositeAlpha = atCompositeAlphaFlags[i];
			break;
		};
	}

    VkSwapchainCreateInfoKHR tCreateSwapchainInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = ptGraphics->tSurface,
        .minImageCount    = uDesiredMinImageCount,
        .imageFormat      = tSurfaceFormat.format,
        .imageColorSpace  = tSurfaceFormat.colorSpace,
        .imageExtent      = tExtent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform     = (VkSurfaceTransformFlagBitsKHR)tPreTransform,
        .compositeAlpha   = tCompositeAlpha,
        .presentMode      = tPresentMode,
        .clipped          = VK_TRUE, // setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
        .oldSwapchain     = ptSwapchainOut->tSwapChain, // setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

	// enable transfer source on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// enable transfer destination on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t auQueueFamilyIndices[] = { (uint32_t)ptDevice->iGraphicsQueueFamily, (uint32_t)ptDevice->iPresentQueueFamily};
    if (ptDevice->iGraphicsQueueFamily != ptDevice->iPresentQueueFamily)
    {
        tCreateSwapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        tCreateSwapchainInfo.queueFamilyIndexCount = 2;
        tCreateSwapchainInfo.pQueueFamilyIndices = auQueueFamilyIndices;
    }

    VkSwapchainKHR tOldSwapChain = ptSwapchainOut->tSwapChain;

    PL_VULKAN(vkCreateSwapchainKHR(ptDevice->tLogicalDevice, &tCreateSwapchainInfo, NULL, &ptSwapchainOut->tSwapChain));

    if(tOldSwapChain)
    {
        if(plu_sb_size(ptDevice->_sbtFrameGarbage) == 0)
        {
            plu_sb_resize(ptDevice->_sbtFrameGarbage, ptSwapchainOut->uImageCount);
        }
        for (uint32_t i = 0u; i < uOldImageCount; i++)
        {
            plu_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame].sbtTextureViews, ptSwapchainOut->sbtImageViews[i]);
            plu_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame].sbtFrameBuffers, ptSwapchainOut->sbtFrameBuffers[i]);
        }
        vkDestroySwapchainKHR(ptDevice->tLogicalDevice, tOldSwapChain, NULL);
    }

    // get swapchain images

    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, NULL));
    plu_sb_resize(ptSwapchainOut->sbtImages, ptSwapchainOut->uImageCount);
    plu_sb_resize(ptSwapchainOut->sbtImageViews, ptSwapchainOut->uImageCount);

    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, ptSwapchainOut->sbtImages));

    for(uint32_t i = 0; i < ptSwapchainOut->uImageCount; i++)
    {

        ptSwapchainOut->sbtImageViews[i] = VK_NULL_HANDLE;

        VkImageViewCreateInfo tViewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = ptSwapchainOut->sbtImages[i],
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = ptSwapchainOut->tFormat,
            .subresourceRange = {
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
            },
            .components = {
                        VK_COMPONENT_SWIZZLE_R,
                        VK_COMPONENT_SWIZZLE_G,
                        VK_COMPONENT_SWIZZLE_B,
                        VK_COMPONENT_SWIZZLE_A
                    }
        };

        PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tViewInfo, NULL, &ptSwapchainOut->sbtImageViews[i]));
    }  //-V1020

    // color & depth
    if(ptSwapchainOut->tColorTextureView)  plu_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame].sbtTextureViews, ptSwapchainOut->tColorTextureView);
    if(ptSwapchainOut->tDepthTextureView)  plu_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame].sbtTextureViews, ptSwapchainOut->tDepthTextureView);
    if(ptSwapchainOut->tColorTexture)      plu_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame].sbtTextures, ptSwapchainOut->tColorTexture);
    if(ptSwapchainOut->tDepthTexture)      plu_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame].sbtTextures, ptSwapchainOut->tDepthTexture);
    if(ptSwapchainOut->tColorTextureMemory)      plu_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame].sbtMemory, ptSwapchainOut->tColorTextureMemory);
    if(ptSwapchainOut->tDepthTextureMemory)      plu_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame].sbtMemory, ptSwapchainOut->tDepthTextureMemory);

    ptSwapchainOut->tColorTextureView = VK_NULL_HANDLE;
    ptSwapchainOut->tColorTexture     = VK_NULL_HANDLE;
    ptSwapchainOut->tDepthTextureView = VK_NULL_HANDLE;
    ptSwapchainOut->tDepthTexture     = VK_NULL_HANDLE;

    VkImageCreateInfo tDepthImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent        = 
        {
            .width  = ptSwapchainOut->tExtent.width,
            .height = ptSwapchainOut->tExtent.height,
            .depth  = 1
        },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .format        = find_depth_stencil_format(&ptGraphics->tDevice),
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptSwapchainOut->tMsaaSamples,
        .flags         = 0
    };

    VkImageCreateInfo tColorImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent        = 
        {
            .width  = ptSwapchainOut->tExtent.width,
            .height = ptSwapchainOut->tExtent.height,
            .depth  = 1
        },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .format        = ptSwapchainOut->tFormat,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptSwapchainOut->tMsaaSamples,
        .flags         = 0
    };

    PL_VULKAN(vkCreateImage(ptDevice->tLogicalDevice, &tDepthImageInfo, NULL, &ptSwapchainOut->tDepthTexture));
    PL_VULKAN(vkCreateImage(ptDevice->tLogicalDevice, &tColorImageInfo, NULL, &ptSwapchainOut->tColorTexture));

    VkMemoryRequirements tDepthMemReqs = {0};
    VkMemoryRequirements tColorMemReqs = {0};
    vkGetImageMemoryRequirements(ptDevice->tLogicalDevice, ptSwapchainOut->tDepthTexture, &tDepthMemReqs);
    vkGetImageMemoryRequirements(ptDevice->tLogicalDevice, ptSwapchainOut->tColorTexture, &tColorMemReqs);


    ptSwapchainOut->tColorTextureMemory = allocate_dedicated(&ptGraphics->tDevice, tColorMemReqs.memoryTypeBits, tColorMemReqs.size, tColorMemReqs.alignment, "swapchain color");
    ptSwapchainOut->tDepthTextureMemory = allocate_dedicated(&ptGraphics->tDevice, tDepthMemReqs.memoryTypeBits, tDepthMemReqs.size, tDepthMemReqs.alignment, "swapchain depth");

    PL_VULKAN(vkBindImageMemory(ptDevice->tLogicalDevice, ptSwapchainOut->tDepthTexture, ptSwapchainOut->tDepthTextureMemory, 0));
    PL_VULKAN(vkBindImageMemory(ptDevice->tLogicalDevice, ptSwapchainOut->tColorTexture, ptSwapchainOut->tColorTextureMemory, 0));

    VkCommandBuffer tCommandBuffer = {0};
    
    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptDevice->tCmdPool,
        .commandBufferCount = 1u,
    };
    vkAllocateCommandBuffers(ptDevice->tLogicalDevice, &tAllocInfo, &tCommandBuffer);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(tCommandBuffer, &tBeginInfo);

    VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
    };

    transition_image_layout(tCommandBuffer, ptSwapchainOut->tColorTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    tRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    transition_image_layout(tCommandBuffer, ptSwapchainOut->tDepthTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptDevice->tLogicalDevice, ptDevice->tCmdPool, 1, &tCommandBuffer);

    VkImageViewCreateInfo tDepthViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptSwapchainOut->tDepthTexture,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tDepthImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
    };

    if(format_has_stencil(tDepthViewInfo.format))
        tDepthViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo tColorViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptSwapchainOut->tColorTexture,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tColorImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tDepthViewInfo, NULL, &ptSwapchainOut->tDepthTextureView));
    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tColorViewInfo, NULL, &ptSwapchainOut->tColorTextureView));

}

static char*
read_file(const char* file, unsigned* size, const char* mode)
{
    FILE* dataFile = fopen(file, mode);

    if (dataFile == NULL)
    {
        assert(false && "File not found.");
        return NULL;
    }

    // obtain file size:
    fseek(dataFile, 0, SEEK_END);
    *size = ftell(dataFile);
    fseek(dataFile, 0, SEEK_SET);

    // allocate memory to contain the whole file:
    char* data = (char*)malloc(sizeof(char)*(*size));

    // copy the file into the buffer:
    size_t result = fread(data, sizeof(char), *size, dataFile);
    if (result != *size)
    {
        if (feof(dataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(dataFile)) {
            perror("Error reading test.bin");
        }
        assert(false && "File not read.");
    }

    fclose(dataFile);

    return data;
}

void
setup_vulkan(plGraphics* ptGraphics)
{

    plDevice* ptDevice = &ptGraphics->tDevice;
    
    ptGraphics->uFramesInFlight = 2;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create instance~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const bool bEnableValidation = true;

    static const char* pcKhronosValidationLayer = "VK_LAYER_KHRONOS_validation";

    const char** sbpcEnabledExtensions = NULL;
    plu_sb_push(sbpcEnabledExtensions, VK_KHR_SURFACE_EXTENSION_NAME);
    plu_sb_push(sbpcEnabledExtensions, VK_KHR_XCB_SURFACE_EXTENSION_NAME);


    if(bEnableValidation)
    {
        plu_sb_push(sbpcEnabledExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        plu_sb_push(sbpcEnabledExtensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    // retrieve supported layers
    uint32_t uInstanceLayersFound = 0u;
    VkLayerProperties* ptAvailableLayers = NULL;
    PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, NULL));
    if(uInstanceLayersFound > 0)
    {
        ptAvailableLayers = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * uInstanceLayersFound);
        PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, ptAvailableLayers));
    }

    // retrieve supported extensions
    uint32_t uInstanceExtensionsFound = 0u;
    VkExtensionProperties* ptAvailableExtensions = NULL;
    PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, NULL));
    if(uInstanceExtensionsFound > 0)
    {
        ptAvailableExtensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * uInstanceExtensionsFound);
        PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, ptAvailableExtensions));
    }

    // ensure extensions are supported
    const char** sbpcMissingExtensions = NULL;
    for(uint32_t i = 0; i < plu_sb_size(sbpcEnabledExtensions); i++)
    {
        const char* requestedExtension = sbpcEnabledExtensions[i];
        bool extensionFound = false;
        for(uint32_t j = 0; j < uInstanceExtensionsFound; j++)
        {
            if(strcmp(requestedExtension, ptAvailableExtensions[j].extensionName) == 0)
            {
                //pl_log_trace_to_f(uLogChannel, "extension %s found", ptAvailableExtensions[j].extensionName);
                extensionFound = true;
                break;
            }
        }

        if(!extensionFound)
        {
            plu_sb_push(sbpcMissingExtensions, requestedExtension);
        }
    }

    // report if all requested extensions aren't found
    if(plu_sb_size(sbpcMissingExtensions) > 0)
    {
        // //pl_log_error_to_f(uLogChannel, "%d %s", plu_sb_size(sbpcMissingExtensions), "Missing Extensions:");
        for(uint32_t i = 0; i < plu_sb_size(sbpcMissingExtensions); i++)
        {
            //pl_log_error_to_f(uLogChannel, "  * %s", sbpcMissingExtensions[i]);
        }

        assert(false && "Can't find all requested extensions");
    }

    // ensure layers are supported
    const char** sbpcMissingLayers = NULL;
    for(uint32_t i = 0; i < 1; i++)
    {
        const char* pcRequestedLayer = (&pcKhronosValidationLayer)[i];
        bool bLayerFound = false;
        for(uint32_t j = 0; j < uInstanceLayersFound; j++)
        {
            if(strcmp(pcRequestedLayer, ptAvailableLayers[j].layerName) == 0)
            {
                //pl_log_trace_to_f(uLogChannel, "layer %s found", ptAvailableLayers[j].layerName);
                bLayerFound = true;
                break;
            }
        }

        if(!bLayerFound)
        {
            plu_sb_push(sbpcMissingLayers, pcRequestedLayer);
        }
    }

    // report if all requested layers aren't found
    if(plu_sb_size(sbpcMissingLayers) > 0)
    {
        //pl_log_error_to_f(uLogChannel, "%d %s", plu_sb_size(sbpcMissingLayers), "Missing Layers:");
        for(uint32_t i = 0; i < plu_sb_size(sbpcMissingLayers); i++)
        {
            //pl_log_error_to_f(uLogChannel, "  * %s", sbpcMissingLayers[i]);
        }
        assert(false && "Can't find all requested layers");
    }

    // Setup debug messenger for vulkan tInstance
    VkDebugUtilsMessengerCreateInfoEXT tDebugCreateInfo = {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = pl__debug_callback,
        .pNext           = VK_NULL_HANDLE
    };

    // create vulkan tInstance
    VkApplicationInfo tAppInfo = {
        .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_2
    };

    VkInstanceCreateInfo tCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &tAppInfo,
        .pNext                   = bEnableValidation ? (VkDebugUtilsMessengerCreateInfoEXT*)&tDebugCreateInfo : VK_NULL_HANDLE,
        .enabledExtensionCount   = plu_sb_size(sbpcEnabledExtensions),
        .ppEnabledExtensionNames = sbpcEnabledExtensions,
        .enabledLayerCount       = 1,
        .ppEnabledLayerNames     = &pcKhronosValidationLayer,

        #ifdef __APPLE__
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
        #endif
    };

    PL_VULKAN(vkCreateInstance(&tCreateInfo, NULL, &ptGraphics->tInstance));
    //pl_log_trace_to_f(uLogChannel, "created vulkan instance");

    // cleanup
    if(ptAvailableLayers)     free(ptAvailableLayers);
    if(ptAvailableExtensions) free(ptAvailableExtensions);
    plu_sb_free(sbpcMissingLayers);
    plu_sb_free(sbpcMissingExtensions);

    if(bEnableValidation)
    {
        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptGraphics->tInstance, "vkCreateDebugUtilsMessengerEXT");
        assert(func != NULL && "failed to set up debug messenger!");
        PL_VULKAN(func(ptGraphics->tInstance, &tDebugCreateInfo, NULL, &ptGraphics->tDbgMessenger));     
        //pl_log_trace_to_f(uLogChannel, "enabled Vulkan validation layers");
    }

    plu_sb_free(sbpcEnabledExtensions);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create surface~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    struct tPlatformData { xcb_connection_t* ptConnection; xcb_window_t tWindow;};
    struct tPlatformData* ptPlatformData = (struct tPlatformData*)pl_get_ui_context()->tIO.pBackendPlatformData;
    const VkXcbSurfaceCreateInfoKHR tSurfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .window = ptPlatformData->tWindow,
        .connection = ptPlatformData->ptConnection
    };
    PL_VULKAN(vkCreateXcbSurfaceKHR(ptGraphics->tInstance, &tSurfaceCreateInfo, NULL, &ptGraphics->tSurface));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create device~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptDevice->iGraphicsQueueFamily = -1;
    ptDevice->iPresentQueueFamily = -1;
    ptDevice->tMemProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    ptDevice->tMemBudgetInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    ptDevice->tMemProps2.pNext = &ptDevice->tMemBudgetInfo;
    // int iDeviceIndex = pl__select_physical_device(ptGraphics->tInstance, ptDevice);

    uint32_t uDeviceCount = 0u;
    int iBestDvcIdx = 0;
    bool bDiscreteGPUFound = false;
    VkDeviceSize tMaxLocalMemorySize = 0u;

    PL_VULKAN(vkEnumeratePhysicalDevices(ptGraphics->tInstance, &uDeviceCount, NULL));
    assert(uDeviceCount > 0 && "failed to find GPUs with Vulkan support!");

    // check if device is suitable
    VkPhysicalDevice atDevices[16] = {0};
    PL_VULKAN(vkEnumeratePhysicalDevices(ptGraphics->tInstance, &uDeviceCount, atDevices));

    // prefer discrete, then memory size
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        vkGetPhysicalDeviceProperties(atDevices[i], &ptDevice->tDeviceProps);
        vkGetPhysicalDeviceMemoryProperties(atDevices[i], &ptDevice->tMemProps);

        for(uint32_t j = 0; j < ptDevice->tMemProps.memoryHeapCount; j++)
        {
            if(ptDevice->tMemProps.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT && ptDevice->tMemProps.memoryHeaps[j].size > tMaxLocalMemorySize && !bDiscreteGPUFound)
            {
                    tMaxLocalMemorySize = ptDevice->tMemProps.memoryHeaps[j].size;
                iBestDvcIdx = i;
            }
        }

        if(ptDevice->tDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && !bDiscreteGPUFound)
        {
            iBestDvcIdx = i;
            bDiscreteGPUFound = true;
        }
    }

    ptDevice->tPhysicalDevice = atDevices[iBestDvcIdx];

    assert(ptDevice->tPhysicalDevice != VK_NULL_HANDLE && "failed to find a suitable GPU!");
    vkGetPhysicalDeviceProperties(atDevices[iBestDvcIdx], &ptDevice->tDeviceProps);
    vkGetPhysicalDeviceMemoryProperties(atDevices[iBestDvcIdx], &ptDevice->tMemProps);
    static const char* pacDeviceTypeName[] = {"Other", "Integrated", "Discrete", "Virtual", "CPU"};

    // print info on chosen device
    //pl_log_info_to_f(uLogChannel, "Device ID: %u", ptDevice->tDeviceProps.deviceID);
    //pl_log_info_to_f(uLogChannel, "Vendor ID: %u", ptDevice->tDeviceProps.vendorID);
    //pl_log_info_to_f(uLogChannel, "API Version: %u", ptDevice->tDeviceProps.apiVersion);
    //pl_log_info_to_f(uLogChannel, "Driver Version: %u", ptDevice->tDeviceProps.driverVersion);
    //pl_log_info_to_f(uLogChannel, "Device Type: %s", pacDeviceTypeName[ptDevice->tDeviceProps.deviceType]);
    //pl_log_info_to_f(uLogChannel, "Device Name: %s", ptDevice->tDeviceProps.deviceName);

    uint32_t uExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(ptDevice->tPhysicalDevice, NULL, &uExtensionCount, NULL);
    VkExtensionProperties* ptExtensions = malloc(uExtensionCount * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(ptDevice->tPhysicalDevice, NULL, &uExtensionCount, ptExtensions);

    for(uint32_t i = 0; i < uExtensionCount; i++)
    {
        if(pl_str_equal(ptExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) ptDevice->bSwapchainExtPresent = true; //-V522
        if(pl_str_equal(ptExtensions[i].extensionName, "VK_KHR_portability_subset"))     ptDevice->bPortabilitySubsetPresent = true; //-V522
    }

    free(ptExtensions);

    ptDevice->tMaxLocalMemSize = ptDevice->tMemProps.memoryHeaps[iBestDvcIdx].size;
    ptDevice->uUniformBufferBlockSize = ptDevice->tDeviceProps.limits.maxUniformBufferRange;

    // find queue families
    uint32_t uQueueFamCnt = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(ptDevice->tPhysicalDevice, &uQueueFamCnt, NULL);

    VkQueueFamilyProperties auQueueFamilies[64] = {0};
    vkGetPhysicalDeviceQueueFamilyProperties(ptDevice->tPhysicalDevice, &uQueueFamCnt, auQueueFamilies);

    for(uint32_t i = 0; i < uQueueFamCnt; i++)
    {
        if (auQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ptDevice->iGraphicsQueueFamily = i;

        VkBool32 tPresentSupport = false;
        PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptDevice->tPhysicalDevice, i, ptGraphics->tSurface, &tPresentSupport));

        if (tPresentSupport) ptDevice->iPresentQueueFamily  = i;

        if (ptDevice->iGraphicsQueueFamily > -1 && ptDevice->iPresentQueueFamily > -1) // complete
            break;
        i++;
    }

    // create logical device

    vkGetPhysicalDeviceFeatures(ptDevice->tPhysicalDevice, &ptDevice->tDeviceFeatures);

    const float fQueuePriority = 1.0f;
    VkDeviceQueueCreateInfo atQueueCreateInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptDevice->iGraphicsQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptDevice->iPresentQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority   
        }
    };
    
    static const char* pcValidationLayers = "VK_LAYER_KHRONOS_validation";

    const char** sbpcDeviceExts = NULL;
    if(ptDevice->bSwapchainExtPresent)      plu_sb_push(sbpcDeviceExts, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if(ptDevice->bPortabilitySubsetPresent) plu_sb_push(sbpcDeviceExts, "VK_KHR_portability_subset");
    plu_sb_push(sbpcDeviceExts, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    VkDeviceCreateInfo tCreateDeviceInfo = {
        .sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount     = atQueueCreateInfos[0].queueFamilyIndex == atQueueCreateInfos[1].queueFamilyIndex ? 1 : 2,
        .pQueueCreateInfos        = atQueueCreateInfos,
        .pEnabledFeatures         = &ptDevice->tDeviceFeatures,
        .ppEnabledExtensionNames  = sbpcDeviceExts,
        .enabledLayerCount        = bEnableValidation ? 1 : 0,
        .ppEnabledLayerNames      = bEnableValidation ? &pcValidationLayers : NULL,
        .enabledExtensionCount    = plu_sb_size(sbpcDeviceExts)
    };
    PL_VULKAN(vkCreateDevice(ptDevice->tPhysicalDevice, &tCreateDeviceInfo, NULL, &ptDevice->tLogicalDevice));

    plu_sb_free(sbpcDeviceExts);

    // get device queues
    vkGetDeviceQueue(ptDevice->tLogicalDevice, ptDevice->iGraphicsQueueFamily, 0, &ptDevice->tGraphicsQueue);
    vkGetDeviceQueue(ptDevice->tLogicalDevice, ptDevice->iPresentQueueFamily, 0, &ptDevice->tPresentQueue);


    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug markers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	ptDevice->vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkDebugMarkerSetObjectTagEXT");
	ptDevice->vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkDebugMarkerSetObjectNameEXT");
	ptDevice->vkCmdDebugMarkerBegin      = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkCmdDebugMarkerBeginEXT");
	ptDevice->vkCmdDebugMarkerEnd        = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkCmdDebugMarkerEndEXT");
	ptDevice->vkCmdDebugMarkerInsert     = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkCmdDebugMarkerInsertEXT");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkCommandPoolCreateInfo tCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    PL_VULKAN(vkCreateCommandPool(ptDevice->tLogicalDevice, &tCommandPoolInfo, NULL, &ptDevice->tCmdPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~swapchain~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptGraphics->tSwapchain.bVSync = true;
    create_swapchain(ptGraphics, (uint32_t)pl_get_ui_context()->tIO.afMainViewportSize[0], (uint32_t)pl_get_ui_context()->tIO.afMainViewportSize[1], &ptGraphics->tSwapchain);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main renderpass~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkAttachmentDescription atAttachments[] = {

        // multisampled color attachment (render to this)
        {
            .format         = ptGraphics->tSwapchain.tFormat,
            .samples        = ptGraphics->tSwapchain.tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },

        // depth attachment
        {
            .format         = find_depth_stencil_format(&ptGraphics->tDevice),
            .samples        = ptGraphics->tSwapchain.tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },

        // color resolve attachment
        {
            .format         = ptGraphics->tSwapchain.tFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
    };

    const VkSubpassDependency tSubpassDependencies[] = {

        // color attachment
        { 
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = 0,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0
        },
        // depth attachment
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0
        }
    };

    const VkAttachmentReference atAttachmentReferences[] = {
        {
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 2,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL     
        }
    };

    const VkSubpassDescription tSubpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &atAttachmentReferences[0],
        .pDepthStencilAttachment = &atAttachmentReferences[1],
        .pResolveAttachments     = &atAttachmentReferences[2]
    };

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 3,
        .pAttachments    = atAttachments,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
    };

    PL_VULKAN(vkCreateRenderPass(ptDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptGraphics->tRenderPass));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    plu_sb_resize(ptGraphics->tSwapchain.sbtFrameBuffers, ptGraphics->tSwapchain.uImageCount);
    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        ptGraphics->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

        VkImageView atViewAttachments[] = {
            ptGraphics->tSwapchain.tColorTextureView,
            ptGraphics->tSwapchain.tDepthTextureView,
            ptGraphics->tSwapchain.sbtImageViews[i]
        };

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptGraphics->tRenderPass,
            .attachmentCount = 3,
            .pAttachments    = atViewAttachments,
            .width           = ptGraphics->tSwapchain.tExtent.width,
            .height          = ptGraphics->tSwapchain.tExtent.height,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptGraphics->tSwapchain.sbtFrameBuffers[i]));
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame resources~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkCommandPoolCreateInfo tFrameCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    
    const VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    const VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    plu_sb_resize(ptGraphics->sbFrames, ptGraphics->uFramesInFlight);
    for(uint32_t i = 0; i < ptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {0};
        PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tImageAvailable));
        PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tRenderFinish));
        PL_VULKAN(vkCreateFence(ptDevice->tLogicalDevice, &tFenceInfo, NULL, &tFrame.tInFlight));
        PL_VULKAN(vkCreateCommandPool(ptDevice->tLogicalDevice, &tFrameCommandPoolInfo, NULL, &tFrame.tCmdPool));

        const VkCommandBufferAllocateInfo tAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = tFrame.tCmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        PL_VULKAN(vkAllocateCommandBuffers(ptDevice->tLogicalDevice, &tAllocInfo, &tFrame.tCmdBuf));  
        ptGraphics->sbFrames[i] = tFrame;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~main descriptor pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkDescriptorPoolSize atPoolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                100000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       100000 }
    };
    VkDescriptorPoolCreateInfo tDescriptorPoolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 100000 * 11,
        .poolSizeCount = 11u,
        .pPoolSizes    = atPoolSizes,
    };
    PL_VULKAN(vkCreateDescriptorPool(ptDevice->tLogicalDevice, &tDescriptorPoolInfo, NULL, &ptGraphics->tDescriptorPool));
}

bool
begin_frame(plGraphics* ptGraphics)
{
    //pl_begin_profile_sample(__FUNCTION__);

    plDevice* ptDevice = &ptGraphics->tDevice;

    plFrameContext* ptCurrentFrame = get_frame_resources(ptGraphics);

    // cleanup queue
    plFrameGarbage* ptGarbage = &ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame];

    if(ptGarbage)
    {
        for(uint32_t i = 0; i < plu_sb_size(ptGarbage->sbtTextures); i++)
            vkDestroyImage(ptDevice->tLogicalDevice, ptGarbage->sbtTextures[i], NULL);

        for(uint32_t i = 0; i < plu_sb_size(ptGarbage->sbtTextureViews); i++)
            vkDestroyImageView(ptDevice->tLogicalDevice, ptGarbage->sbtTextureViews[i], NULL);

        for(uint32_t i = 0; i < plu_sb_size(ptGarbage->sbtFrameBuffers); i++)
            vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptGarbage->sbtFrameBuffers[i], NULL);

        for(uint32_t i = 0; i < plu_sb_size(ptGarbage->sbtMemory); i++)
            vkFreeMemory(ptDevice->tLogicalDevice, ptGarbage->sbtMemory[i], NULL);

        plu_sb_reset(ptGarbage->sbtTextures);
        plu_sb_reset(ptGarbage->sbtTextureViews);
        plu_sb_reset(ptGarbage->sbtFrameBuffers);
        plu_sb_reset(ptGarbage->sbtMemory);
    }

    PL_VULKAN(vkWaitForFences(ptDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));
    VkResult err = vkAcquireNextImageKHR(ptDevice->tLogicalDevice, ptGraphics->tSwapchain.tSwapChain, UINT64_MAX, ptCurrentFrame->tImageAvailable, VK_NULL_HANDLE, &ptGraphics->tSwapchain.uCurrentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            create_swapchain(ptGraphics, (uint32_t)pl_get_ui_context()->tIO.afMainViewportSize[0], (uint32_t)pl_get_ui_context()->tIO.afMainViewportSize[1], &ptGraphics->tSwapchain);

            // recreate frame buffers
            plu_sb_resize(ptGraphics->tSwapchain.sbtFrameBuffers, ptGraphics->tSwapchain.uImageCount);
            for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
            {
                ptGraphics->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

                VkImageView atViewAttachments[] = {
                    ptGraphics->tSwapchain.tColorTextureView,
                    ptGraphics->tSwapchain.tDepthTextureView,
                    ptGraphics->tSwapchain.sbtImageViews[i]
                };

                VkFramebufferCreateInfo tFrameBufferInfo = {
                    .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    .renderPass      = ptGraphics->tRenderPass,
                    .attachmentCount = 3,
                    .pAttachments    = atViewAttachments,
                    .width           = ptGraphics->tSwapchain.tExtent.width,
                    .height          = ptGraphics->tSwapchain.tExtent.height,
                    .layers          = 1u,
                };
                PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptGraphics->tSwapchain.sbtFrameBuffers[i]));
            }

            //pl_end_profile_sample();
            return false;
        }
    }
    else
    {
        PL_VULKAN(err);
    }

    if (ptCurrentFrame->tInFlight != VK_NULL_HANDLE)
        PL_VULKAN(vkWaitForFences(ptDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));

    //pl_end_profile_sample();
    return true; 
}

void
end_frame(plGraphics* ptGraphics)
{
    //pl_begin_profile_sample(__FUNCTION__);

    plDevice* ptDevice = &ptGraphics->tDevice;

    plFrameContext* ptCurrentFrame = get_frame_resources(ptGraphics);

    // submit
    const VkPipelineStageFlags atWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    const VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &ptCurrentFrame->tImageAvailable,
        .pWaitDstStageMask    = atWaitStages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &ptCurrentFrame->tCmdBuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &ptCurrentFrame->tRenderFinish
    };
    PL_VULKAN(vkResetFences(ptDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight));
    PL_VULKAN(vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, ptCurrentFrame->tInFlight));          
    
    // present                        
    const VkPresentInfoKHR tPresentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &ptCurrentFrame->tRenderFinish,
        .swapchainCount     = 1,
        .pSwapchains        = &ptGraphics->tSwapchain.tSwapChain,
        .pImageIndices      = &ptGraphics->tSwapchain.uCurrentImageIndex,
    };
    const VkResult tResult = vkQueuePresentKHR(ptDevice->tPresentQueue, &tPresentInfo);
    if(tResult == VK_SUBOPTIMAL_KHR || tResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        create_swapchain(ptGraphics, (uint32_t)pl_get_ui_context()->tIO.afMainViewportSize[0], (uint32_t)pl_get_ui_context()->tIO.afMainViewportSize[1], &ptGraphics->tSwapchain);

        // recreate frame buffers
        plu_sb_resize(ptGraphics->tSwapchain.sbtFrameBuffers, ptGraphics->tSwapchain.uImageCount);
        for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
        {
            ptGraphics->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

            VkImageView atViewAttachments[] = {
                ptGraphics->tSwapchain.tColorTextureView,
                ptGraphics->tSwapchain.tDepthTextureView,
                ptGraphics->tSwapchain.sbtImageViews[i]
            };

            VkFramebufferCreateInfo tFrameBufferInfo = {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass      = ptGraphics->tRenderPass,
                .attachmentCount = 3,
                .pAttachments    = atViewAttachments,
                .width           = ptGraphics->tSwapchain.tExtent.width,
                .height          = ptGraphics->tSwapchain.tExtent.height,
                .layers          = 1u,
            };
            PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptGraphics->tSwapchain.sbtFrameBuffers[i]));
        }
    }
    else
    {
        PL_VULKAN(tResult);
    }

    ptGraphics->szCurrentFrameIndex = (ptGraphics->szCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;

    //pl_end_profile_sample();
}

void
resize(plGraphics* ptGraphics)
{
    //pl_begin_profile_sample(__FUNCTION__);

    plDevice* ptDevice = &ptGraphics->tDevice;

    plFrameContext* ptCurrentFrame = get_frame_resources(ptGraphics);

    create_swapchain(ptGraphics, (uint32_t)pl_get_ui_context()->tIO.afMainViewportSize[0], (uint32_t)pl_get_ui_context()->tIO.afMainViewportSize[1], &ptGraphics->tSwapchain);

    // recreate frame buffers
    plu_sb_resize(ptGraphics->tSwapchain.sbtFrameBuffers, ptGraphics->tSwapchain.uImageCount);
    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        ptGraphics->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

        VkImageView atViewAttachments[] = {
            ptGraphics->tSwapchain.tColorTextureView,
            ptGraphics->tSwapchain.tDepthTextureView,
            ptGraphics->tSwapchain.sbtImageViews[i]
        };

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptGraphics->tRenderPass,
            .attachmentCount = 3,
            .pAttachments    = atViewAttachments,
            .width           = ptGraphics->tSwapchain.tExtent.width,
            .height          = ptGraphics->tSwapchain.tExtent.height,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptGraphics->tSwapchain.sbtFrameBuffers[i]));
    }

    ptGraphics->szCurrentFrameIndex = 0;

    //pl_end_profile_sample();
}

void
cleanup(plGraphics* ptGraphics)
{
    plDevice* ptDevice = &ptGraphics->tDevice;

    vkDeviceWaitIdle(ptDevice->tLogicalDevice);

    // cleanup per frame resources
    for(uint32_t i = 0; i < plu_sb_size(ptGraphics->sbFrames); i++)
    {
        plFrameContext* ptFrame = &ptGraphics->sbFrames[i];
        vkDestroySemaphore(ptDevice->tLogicalDevice, ptFrame->tImageAvailable, NULL);
        vkDestroySemaphore(ptDevice->tLogicalDevice, ptFrame->tRenderFinish, NULL);
        vkDestroyFence(ptDevice->tLogicalDevice, ptFrame->tInFlight, NULL);
        vkDestroyCommandPool(ptDevice->tLogicalDevice, ptFrame->tCmdPool, NULL);
    }

    // swapchain stuff
    vkDestroyImageView(ptDevice->tLogicalDevice, ptGraphics->tSwapchain.tColorTextureView, NULL);
    vkDestroyImageView(ptDevice->tLogicalDevice, ptGraphics->tSwapchain.tDepthTextureView, NULL);
    vkDestroyImage(ptDevice->tLogicalDevice, ptGraphics->tSwapchain.tColorTexture, NULL);
    vkDestroyImage(ptDevice->tLogicalDevice, ptGraphics->tSwapchain.tDepthTexture, NULL);
    vkFreeMemory(ptDevice->tLogicalDevice, ptGraphics->tSwapchain.tColorTextureMemory, NULL);
    vkFreeMemory(ptDevice->tLogicalDevice, ptGraphics->tSwapchain.tDepthTextureMemory, NULL);

    for(uint32_t i = 0; i < plu_sb_size(ptGraphics->tSwapchain.sbtImageViews); i++)
        vkDestroyImageView(ptDevice->tLogicalDevice, ptGraphics->tSwapchain.sbtImageViews[i], NULL);

    for(uint32_t i = 0; i < plu_sb_size(ptGraphics->tSwapchain.sbtFrameBuffers); i++)
        vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptGraphics->tSwapchain.sbtFrameBuffers[i], NULL);
    
    vkDestroyRenderPass(ptDevice->tLogicalDevice, ptGraphics->tRenderPass, NULL);

    vkDestroyDescriptorPool(ptDevice->tLogicalDevice, ptGraphics->tDescriptorPool, NULL);

    // destroy command pool
    vkDestroyCommandPool(ptDevice->tLogicalDevice, ptDevice->tCmdPool, NULL);

    // destroy device
    vkDestroyDevice(ptDevice->tLogicalDevice, NULL);

    if(ptGraphics->tDbgMessenger)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT tFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptGraphics->tInstance, "vkDestroyDebugUtilsMessengerEXT");
        if (tFunc != NULL)
            tFunc(ptGraphics->tInstance, ptGraphics->tDbgMessenger, NULL);
    }

    // destroy tSurface
    vkDestroySurfaceKHR(ptGraphics->tInstance, ptGraphics->tSurface, NULL);

    // destroy tInstance
    vkDestroyInstance(ptGraphics->tInstance, NULL);

    for(uint32_t i = 0; i < plu_sb_size(ptDevice->_sbtFrameGarbage); i++)
    {
        plu_sb_free(ptDevice->_sbtFrameGarbage[i].sbtFrameBuffers);
        plu_sb_free(ptDevice->_sbtFrameGarbage[i].sbtMemory);
        plu_sb_free(ptDevice->_sbtFrameGarbage[i].sbtTextures);
        plu_sb_free(ptDevice->_sbtFrameGarbage[i].sbtTextureViews);
    }

    plu_sb_free(ptDevice->_sbtFrameGarbage);
    plu_sb_free(ptGraphics->sbFrames);
    plu_sb_free(ptGraphics->tSwapchain.sbtSurfaceFormats);
    plu_sb_free(ptGraphics->tSwapchain.sbtImages);
    plu_sb_free(ptGraphics->tSwapchain.sbtFrameBuffers);
    plu_sb_free(ptGraphics->tSwapchain.sbtImageViews);
}
    

static uint32_t
find_memory_type(VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties)
{
    uint32_t uMemoryType = 0u;
    for (uint32_t i = 0; i < tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties) 
        {
            uMemoryType = i;
            break;
        }
    }
    return uMemoryType;    
}

void
begin_recording(plGraphics* ptGraphics)
{
    plDevice* ptDevice = &ptGraphics->tDevice;

    plFrameContext* ptCurrentFrame = get_frame_resources(ptGraphics);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    PL_VULKAN(vkResetCommandPool(ptDevice->tLogicalDevice, ptCurrentFrame->tCmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    PL_VULKAN(vkBeginCommandBuffer(ptCurrentFrame->tCmdBuf, &tBeginInfo));  

    VkRenderPassBeginInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = ptGraphics->tRenderPass;
    renderPassInfo.framebuffer = ptGraphics->tSwapchain.sbtFrameBuffers[ptGraphics->tSwapchain.uCurrentImageIndex];
    renderPassInfo.renderArea.extent = ptGraphics->tSwapchain.tExtent;

    VkClearValue clearValues[2];
    clearValues[0].color.float32[0] = 0.0f;
    clearValues[0].color.float32[1] = 0.0f;
    clearValues[0].color.float32[2] = 0.0f;
    clearValues[0].color.float32[3] = 1.0f;
    clearValues[1].depthStencil.depth = 0.0f;
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    VkRect2D scissor = {0};
    scissor.extent = ptGraphics->tSwapchain.tExtent;

    VkViewport tViewport = {0};
    tViewport.x = 0.0f;
    tViewport.y = 0.0f;
    tViewport.width = (float)ptGraphics->tSwapchain.tExtent.width;
    tViewport.height = (float)ptGraphics->tSwapchain.tExtent.height;

    vkCmdSetViewport(ptCurrentFrame->tCmdBuf, 0, 1, &tViewport);
    vkCmdSetScissor(ptCurrentFrame->tCmdBuf, 0, 1, &scissor);  

    vkCmdBeginRenderPass(ptCurrentFrame->tCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

}

void
end_recording(plGraphics* ptGraphics)
{
    plDevice* ptDevice = &ptGraphics->tDevice;

    plFrameContext* ptCurrentFrame = get_frame_resources(ptGraphics);

    vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);

    PL_VULKAN(vkEndCommandBuffer(ptCurrentFrame->tCmdBuf));
}