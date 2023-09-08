/*
   apple_pl.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] internal api
// [SECTION] globals
// [SECTION] entry point
// [SECTION] plNSView
// [SECTION] plNSViewController
// [SECTION] internal implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_ui.h"
#include "pl_ui_internal.h"
#include "pl_ui_metal.h"

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#import <time.h>
#include <stdlib.h>   // malloc
#include <string.h>   // strncpy
#include <sys/stat.h> // timespec
#include <stdio.h>    // file api
#include <copyfile.h> // copyfile
#include <dlfcn.h>    // dlopen, dlsym, dlclose
#include <sys/socket.h>   // sockets
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

@protocol plNSViewDelegate <NSObject>
- (void)drawableResize:(CGSize)size;
- (void)renderToMetalLayer:(nonnull CAMetalLayer *)metalLayer;
- (void)shutdown;
@end

@interface plNSViewController : NSViewController <plNSViewDelegate>
@end

@interface plNSView : NSView <CALayerDelegate>
@property (nonatomic, nonnull, readonly) CAMetalLayer *metalLayer;
@property (nonatomic, getter=isPaused) BOOL paused;
@property (nonatomic, nullable,retain) plNSViewController* delegate;
- (void)initCommon;
- (void)resizeDrawable:(CGFloat)scaleFactor;
- (void)stopRenderLoop;
- (void)render;
@end

@interface plNSAppDelegate : NSObject <NSApplicationDelegate>
@end
@implementation plNSAppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app { return YES;}
@end

@interface plKeyEventResponder: NSView<NSTextInputClient>
@end

typedef struct _plGraphics
{
    id<MTLDevice> tDevice;
    id<MTLCommandQueue> tCmdQueue;
    uint32_t            uCurrentFrame;
    CAMetalLayer*       pMetalLayer;

    // temp
    id<MTLTexture>             depthTarget;
    MTLRenderPassDescriptor*   drawableRenderDescriptor;
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tRenderPipelineState;

    // per frame
    id<CAMetalDrawable>         tCurrentDrawable;
    id<MTLCommandBuffer>        tCurrentCommandBuffer;
    id<MTLRenderCommandEncoder> tCurrentRenderEncoder;
} plGraphics;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plKey pl__osx_key_to_pl_key(int iKey);

// clip board
static const char* pl__get_clipboard_text(void* user_data_ctx);
static void        pl__set_clipboard_text(void* pUnused, const char* text);

static inline CFTimeInterval pl__get_absolute_time(void) { return (CFTimeInterval)((double)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9); }

// metal
void setup_metal(void);
void resize     (void);
void cleanup    (void);

    // per frame
bool begin_frame    (void);
void end_frame      (void);
void begin_recording(void);
void end_recording  (void);

// Undocumented methods for creating cursors. (from Dear ImGui)
@interface NSCursor()
+ (id)_windowResizeNorthWestSouthEastCursor;
+ (id)_windowResizeNorthEastSouthWestCursor;
+ (id)_windowResizeNorthSouthCursor;
+ (id)_windowResizeEastWestCursor;
@end

static plKey pl__osx_key_to_pl_key(int iKey);
static void  pl__add_osx_tracking_area(NSView* _Nonnull view);
static bool  pl__handle_osx_event(NSEvent* event, NSView* view);

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

static NSWindow*            gWindow = NULL;
static NSViewController*    gViewController = NULL;
static bool                 gRunning = true;
static plKeyEventResponder* gKeyEventResponder = NULL;
static NSTextInputContext*  gInputContext = NULL;
static id                   gMonitor;
static CFTimeInterval tTime;
static NSCursor*      aptMouseCursors[PL_MOUSE_CURSOR_COUNT];

plIO*           gtIO             = NULL;
plUiContext*    gptUiCtx         = NULL;

plDrawList   drawlist;
plDrawLayer* fgDrawLayer;
plDrawLayer* bgDrawLayer;
plFontAtlas  fontAtlas;
plGraphics   tGraphics;

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main()
{

    // os provided apis
    gptUiCtx = pl_create_ui_context();

    // setup & retrieve io context 
    gtIO = pl_get_io();

    // create view controller
    gViewController = [[plNSViewController alloc] init];
    gKeyEventResponder = [[plKeyEventResponder alloc] initWithFrame:NSZeroRect];

    // set clipboard functions (may need to move this to OS api)
    gtIO->set_clipboard_text_fn = pl__set_clipboard_text;
    gtIO->get_clipboard_text_fn = pl__get_clipboard_text;

    // create window
    gWindow = [NSWindow windowWithContentViewController:gViewController];
    [gWindow orderFront:nil];
    [gWindow center];
    [gWindow becomeKeyWindow];

    gInputContext = [[NSTextInputContext alloc] initWithClient:gKeyEventResponder];

    // create app delegate
    id appDelegate = [[plNSAppDelegate alloc] init];
    gWindow.delegate = appDelegate;
    NSApplication.sharedApplication.delegate = appDelegate;

    // Load cursors. Some of them are undocumented.
    aptMouseCursors[PL_MOUSE_CURSOR_ARROW] = [NSCursor arrowCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_TEXT_INPUT] = [NSCursor IBeamCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_ALL] = [NSCursor closedHandCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_HAND] = [NSCursor pointingHandCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_NOT_ALLOWED] = [NSCursor operationNotAllowedCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_NS] = [NSCursor respondsToSelector:@selector(_windowResizeNorthSouthCursor)] ? [NSCursor _windowResizeNorthSouthCursor] : [NSCursor resizeUpDownCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_EW] = [NSCursor respondsToSelector:@selector(_windowResizeEastWestCursor)] ? [NSCursor _windowResizeEastWestCursor] : [NSCursor resizeLeftRightCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_NESW] = [NSCursor respondsToSelector:@selector(_windowResizeNorthEastSouthWestCursor)] ? [NSCursor _windowResizeNorthEastSouthWestCursor] : [NSCursor closedHandCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_NWSE] = [NSCursor respondsToSelector:@selector(_windowResizeNorthWestSouthEastCursor)] ? [NSCursor _windowResizeNorthWestSouthEastCursor] : [NSCursor closedHandCursor];

    // run app
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
}

//-----------------------------------------------------------------------------
// [SECTION] plNSView
//-----------------------------------------------------------------------------

@implementation plNSView
{
    CVDisplayLinkRef _displayLink;
    dispatch_source_t _displaySource;
}

- (instancetype) initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if(self)
    {
        [self initCommon];
    }
    [self addSubview:gKeyEventResponder];
    pl__add_osx_tracking_area(self);
    return self;
}

- (void)dealloc
{
    [self stopRenderLoop];
    [gWindow.delegate release];
    [gWindow release];
    [_delegate shutdown];
    [super dealloc];
}

- (void)resizeDrawable:(CGFloat)scaleFactor
{
    CGSize newSize = self.bounds.size;

    gtIO->afMainFramebufferScale[0] = scaleFactor;
    gtIO->afMainFramebufferScale[1] = scaleFactor;

    if(newSize.width <= 0 || newSize.width <= 0)
    {
        return;
    }

    if(newSize.width == _metalLayer.drawableSize.width && newSize.height == _metalLayer.drawableSize.height)
    {
        return;
    }

    _metalLayer.drawableSize = newSize;

    [(id)_delegate drawableResize:newSize];
    
}

- (void)render
{
    [(id)_delegate renderToMetalLayer:_metalLayer];
}

- (void)initCommon
{
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
    _metalLayer = (CAMetalLayer*) self.layer;
    self.layer.delegate = self;
}

- (CALayer *)makeBackingLayer
{
    return [CAMetalLayer layer];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self setupCVDisplayLinkForScreen:self.window.screen];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

- (BOOL)setupCVDisplayLinkForScreen:(NSScreen*)screen
{
    // The CVDisplayLink callback, DispatchRenderLoop, never executes
    // on the main thread. To execute rendering on the main thread, create
    // a dispatch source using the main queue (the main thread).
    // DispatchRenderLoop merges this dispatch source in each call
    // to execute rendering on the main thread.
    _displaySource = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, dispatch_get_main_queue());
    plNSView* weakSelf = self;
    dispatch_source_set_event_handler(_displaySource, ^(){
        @autoreleasepool
        {
            [weakSelf render];
        }
    });
    dispatch_resume(_displaySource);

    CVReturn cvReturn;

    // Create a display link capable of being used with all active displays
    cvReturn = CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);

    if(cvReturn != kCVReturnSuccess)
    {
        return NO;
    }

    // Set DispatchRenderLoop as the callback function and
    // supply _displaySource as the argument to the callback.
    cvReturn = CVDisplayLinkSetOutputCallback(_displayLink, &DispatchRenderLoop, (__bridge void*)_displaySource);

    if(cvReturn != kCVReturnSuccess)
    {
        return NO;
    }

    // Associate the display link with the display on which the
    // view resides
    CGDirectDisplayID viewDisplayID =
        (CGDirectDisplayID) [self.window.screen.deviceDescription[@"NSScreenNumber"] unsignedIntegerValue];

    cvReturn = CVDisplayLinkSetCurrentCGDisplay(_displayLink, viewDisplayID);

    if(cvReturn != kCVReturnSuccess)
    {
        return NO;
    }

    CVDisplayLinkStart(_displayLink);

    NSNotificationCenter* notificationCenter = [NSNotificationCenter defaultCenter];

    // Register to be notified when the window closes so that you
    // can stop the display link
    [notificationCenter addObserver:self
                           selector:@selector(windowWillClose:)
                               name:NSWindowWillCloseNotification
                             object:self.window];

    return YES;
}

- (void)windowWillClose:(NSNotification*)notification
{
    // Stop the display link when the window is closing since there
    // is no point in drawing something that can't be seen
    if(notification.object == self.window)
    {
        CVDisplayLinkStop(_displayLink);
        dispatch_source_cancel(_displaySource);
    }

    // pl_app_shutdown(gUserData);
}

// This is the renderer output callback function
static CVReturn
DispatchRenderLoop(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext)
{

    // 'DispatchRenderLoop' is always called on a secondary thread.  Merge the dispatch source
    // setup for the main queue so that rendering occurs on the main thread
    dispatch_source_t source = (__bridge dispatch_source_t)displayLinkContext;
    dispatch_source_merge_data(source, 1);
    return kCVReturnSuccess;
}

- (void)stopRenderLoop
{
    if(_displayLink)
    {
        // Stop the display link BEFORE releasing anything in the view otherwise the display link
        // thread may call into the view and crash when it encounters something that no longer
        // exists
        CVDisplayLinkStop(_displayLink);
        CVDisplayLinkRelease(_displayLink);
        dispatch_source_cancel(_displaySource);
    }
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

- (void)setBoundsSize:(NSSize)size
{
    [super setBoundsSize:size];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

@end

//-----------------------------------------------------------------------------
// [SECTION] plNSViewController
//-----------------------------------------------------------------------------

@implementation plNSViewController

- (void)loadView
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    NSRect frame = NSMakeRect(0, 0, 500, 500);
    self.view = [[plNSView alloc] initWithFrame:frame];

    plNSView *view = (plNSView *)self.view;
    view.metalLayer.device = device;    
    view.delegate = self;
    view.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;

    // #ifdef PL_VULKAN_BACKEND
    //     plIOContext* gtIOContext = pl_get_io_context();
    //     gtIOContext->pBackendPlatformData = view.metalLayer;
    // #endif

    gtIO->afMainViewportSize[0] = 500;
    gtIO->afMainViewportSize[1] = 500;

    tGraphics.tDevice = device;

    setup_metal();

    // setup drawing api 
    pl_initialize_metal(device);

    // create draw list & layers
    pl_register_drawlist(&drawlist);
    bgDrawLayer = pl_request_layer(&drawlist, "Background Layer");
    fgDrawLayer = pl_request_layer(&drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&fontAtlas);
    pl_build_font_atlas(&fontAtlas);
    pl_build_metal_font_atlas(&fontAtlas);
    pl_set_default_font(&fontAtlas.sbFonts[0]);  
}

- (void)drawableResize:(CGSize)size
{
    gtIO->afMainViewportSize[0] = size.width;
    gtIO->afMainViewportSize[1] = size.height;
    resize();
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)layer
{
    tGraphics.pMetalLayer = layer;
    gtIO->pBackendPlatformData = layer;

    gtIO->afMainFramebufferScale[0] = self.view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
    gtIO->afMainFramebufferScale[1] = gtIO->afMainFramebufferScale[0];

    // not osx
    // CGFloat framebufferScale = view.window.screen.scale ?: UIScreen.mainScreen.scale;

    // updating mouse cursor
    if(gtIO->tCurrentCursor != PL_MOUSE_CURSOR_ARROW && gtIO->tNextCursor == PL_MOUSE_CURSOR_ARROW)
        gtIO->bCursorChanged = true;

    if(gtIO->bCursorChanged && gtIO->tNextCursor != gtIO->tCurrentCursor)
    {
        gtIO->tCurrentCursor = gtIO->tNextCursor;
        NSCursor* ptMacCursor = aptMouseCursors[gtIO->tCurrentCursor] ?: aptMouseCursors[PL_MOUSE_CURSOR_ARROW];
        [ptMacCursor set];
    }
    gtIO->tNextCursor = PL_MOUSE_CURSOR_ARROW;
    gtIO->bCursorChanged = false;

    if(self.view)
    {
        const float fDpi = (float)[self.view.window backingScaleFactor];
        gtIO->afMainViewportSize[0] = (float)self.view.bounds.size.width;
        gtIO->afMainViewportSize[1] = (float)self.view.bounds.size.height;
        gtIO->afMainFramebufferScale[0] = fDpi;
        gtIO->afMainFramebufferScale[1] = fDpi;
    }

    if(tTime == 0.0)
        tTime = pl__get_absolute_time();

    double dCurrentTime = pl__get_absolute_time();
    gtIO->fDeltaTime = (float)(dCurrentTime - tTime);
    tTime = dCurrentTime;

    if(!gtIO->bViewportMinimized)
    {
        if(begin_frame())
        {

            begin_recording();

            pl_new_draw_frame_metal();
            pl_new_frame();

            pl_demo(NULL);

            pl_add_line(fgDrawLayer, (plVec2){0}, (plVec2){300.0f, 300.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);

            pl_submit_layer(bgDrawLayer);
            pl_submit_layer(fgDrawLayer);

            gptUiCtx->tFrameBufferScale.x = gtIO->afMainFramebufferScale[0];
            gptUiCtx->tFrameBufferScale.y = gtIO->afMainFramebufferScale[1];
            pl_render();
            pl_submit_metal_drawlist(&drawlist, gtIO->afMainViewportSize[0], gtIO->afMainViewportSize[1], tGraphics.tCurrentRenderEncoder, tGraphics.drawableRenderDescriptor);
            pl_submit_metal_drawlist(pl_get_draw_list(NULL), gtIO->afMainViewportSize[0], gtIO->afMainViewportSize[1], tGraphics.tCurrentRenderEncoder, tGraphics.drawableRenderDescriptor);
            pl_submit_metal_drawlist(pl_get_debug_draw_list(NULL), gtIO->afMainViewportSize[0], gtIO->afMainViewportSize[1], tGraphics.tCurrentRenderEncoder, tGraphics.drawableRenderDescriptor);

            end_recording();
            end_frame();
        }
    }
}

- (void)shutdown
{
    if(gMonitor != NULL)
    {
        [NSEvent removeMonitor:gMonitor];
        gMonitor = NULL;
    }
}

@end

@implementation plKeyEventResponder
{
    float _posX;
    float _posY;
    NSRect _imeRect;
}

#pragma mark - Public

- (void)setImePosX:(float)posX imePosY:(float)posY
{
    _posX = posX;
    _posY = posY;
}

- (void)updateImePosWithView:(NSView *)view
{
    NSWindow *window = view.window;
    if (!window)
        return;
    NSRect contentRect = [window contentRectForFrameRect:window.frame];
    NSRect rect = NSMakeRect(_posX, contentRect.size.height - _posY, 0, 0);
    _imeRect = [window convertRectToScreen:rect];
}

- (void)viewDidMoveToWindow
{
    // Eensure self is a first responder to receive the input events.
    [self.window makeFirstResponder:self];
}

- (void)keyDown:(NSEvent*)event
{
    if (!pl__handle_osx_event(event, self))
        [super keyDown:event];

    // call to the macOS input manager system.
    [self interpretKeyEvents:@[event]];
}

- (void)keyUp:(NSEvent*)event
{
    if (!pl__handle_osx_event(event, self))
        [super keyUp:event];
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
    NSString* characters;
    if ([aString isKindOfClass:[NSAttributedString class]])
        characters = [aString string];
    else
        characters = (NSString*)aString;

    pl_add_text_events_utf8(characters.UTF8String);
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)doCommandBySelector:(SEL)myselector
{
}

- (nullable NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    return _imeRect;
}

- (BOOL)hasMarkedText
{
    return NO;
}

- (NSRange)markedRange
{
    return NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange
{
    return NSMakeRange(NSNotFound, 0);
}

- (void)setMarkedText:(nonnull id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
}

- (void)unmarkText
{
}

- (nonnull NSArray<NSAttributedStringKey>*)validAttributesForMarkedText
{
    return @[];
}

@end

static bool
pl__handle_osx_event(NSEvent* event, NSView* view)
{
    if (event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown || event.type == NSEventTypeOtherMouseDown)
    {
        int button = (int)[event buttonNumber];
        if (button >= 0 && button < PL_MOUSE_BUTTON_COUNT)
            pl_add_mouse_button_event(button, true);
        return true;
    }

    if (event.type == NSEventTypeLeftMouseUp || event.type == NSEventTypeRightMouseUp || event.type == NSEventTypeOtherMouseUp)
    {
        int button = (int)[event buttonNumber];
        if (button >= 0 && button < PL_MOUSE_BUTTON_COUNT)
            pl_add_mouse_button_event(button, false);
        return true;
    }

    if (event.type == NSEventTypeMouseMoved || event.type == NSEventTypeLeftMouseDragged || event.type == NSEventTypeRightMouseDragged || event.type == NSEventTypeOtherMouseDragged)
    {
        NSPoint mousePoint = event.locationInWindow;
        mousePoint = [view convertPoint:mousePoint fromView:nil];
        if ([view isFlipped])
            mousePoint = NSMakePoint(mousePoint.x, mousePoint.y);
        else
            mousePoint = NSMakePoint(mousePoint.x, view.bounds.size.height - mousePoint.y);
        pl_add_mouse_pos_event((float)mousePoint.x, (float)mousePoint.y);
        return true;
    }

    if (event.type == NSEventTypeScrollWheel)
    {
        // Ignore canceled events.
        //
        // From macOS 12.1, scrolling with two fingers and then decelerating
        // by tapping two fingers results in two events appearing:
        //
        // 1. A scroll wheel NSEvent, with a phase == NSEventPhaseMayBegin, when the user taps
        // two fingers to decelerate or stop the scroll events.
        //
        // 2. A scroll wheel NSEvent, with a phase == NSEventPhaseCancelled, when the user releases the
        // two-finger tap. It is this event that sometimes contains large values for scrollingDeltaX and
        // scrollingDeltaY. When these are added to the current x and y positions of the scrolling view,
        // it appears to jump up or down. It can be observed in Preview, various JetBrains IDEs and here.
        if (event.phase == NSEventPhaseCancelled)
            return false;

        double wheel_dx = 0.0;
        double wheel_dy = 0.0;

        #if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
        if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6)
        {
            wheel_dx = [event scrollingDeltaX];
            wheel_dy = [event scrollingDeltaY];
            if ([event hasPreciseScrollingDeltas])
            {
                wheel_dx *= 0.1;
                wheel_dy *= 0.1;
            }
        }
        else
        #endif // MAC_OS_X_VERSION_MAX_ALLOWED
        {
            wheel_dx = [event deltaX];
            wheel_dy = [event deltaY];
        }
        if (wheel_dx != 0.0 || wheel_dy != 0.0)
            pl_add_mouse_wheel_event((float)wheel_dx * 0.1f, (float)wheel_dy * 0.1f);

        return true;
    }

    if (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp)
    {
        if ([event isARepeat])
            return true;

        int key_code = (int)[event keyCode];
        pl_add_key_event(pl__osx_key_to_pl_key(key_code), event.type == NSEventTypeKeyDown);
        return true;
    }

    if (event.type == NSEventTypeFlagsChanged)
    {
        unsigned short key_code = [event keyCode];
        NSEventModifierFlags modifier_flags = [event modifierFlags];

        pl_add_key_event(PL_KEY_MOD_SHIFT, (modifier_flags & NSEventModifierFlagShift)   != 0);
        pl_add_key_event(PL_KEY_MOD_CTRL,  (modifier_flags & NSEventModifierFlagControl) != 0);
        pl_add_key_event(PL_KEY_MOD_ALT,   (modifier_flags & NSEventModifierFlagOption)  != 0);
        pl_add_key_event(PL_KEY_MOD_SUPER, (modifier_flags & NSEventModifierFlagCommand) != 0);

        plKey key = pl__osx_key_to_pl_key(key_code);
        if (key != PL_KEY_NONE)
        {
            // macOS does not generate down/up event for modifiers. We're trying
            // to use hardware dependent masks to extract that information.
            NSEventModifierFlags mask = 0;
            switch (key)
            {
                case PL_KEY_LEFT_CTRL:   mask = 0x0001; break;
                case PL_KEY_RIGHT_CTRL:  mask = 0x2000; break;
                case PL_KEY_LEFT_SHIFT:  mask = 0x0002; break;
                case PL_KEY_RIGHT_SHIFT: mask = 0x0004; break;
                case PL_KEY_LEFT_SUPER:  mask = 0x0008; break;
                case PL_KEY_RIGHT_SUPER: mask = 0x0010; break;
                case PL_KEY_LEFT_ALT:    mask = 0x0020; break;
                case PL_KEY_RIGHT_ALT:   mask = 0x0040; break;
                default: return true;
            }

            NSEventModifierFlags modifier_flags = [event modifierFlags];
            pl_add_key_event(key, (modifier_flags & mask) != 0);
            // io.SetKeyEventNativeData(key, key_code, -1); // To support legacy indexing (<1.87 user code)
        }

        return true;
    }

    return false;
}

static void 
pl__add_osx_tracking_area(NSView* _Nonnull view)
{
    if(gMonitor) return;
    NSEventMask eventMask = 0;
    eventMask |= NSEventMaskMouseMoved | NSEventMaskScrollWheel;
    eventMask |= NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged;
    eventMask |= NSEventMaskRightMouseDown | NSEventMaskRightMouseUp | NSEventMaskRightMouseDragged;
    eventMask |= NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp | NSEventMaskOtherMouseDragged;
    eventMask |= NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged;
    gMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:eventMask
                                                        handler:^NSEvent* _Nullable(NSEvent* event)
    {
        pl__handle_osx_event(event, view);
        return event;
    }]; 
}

const char*
pl__get_clipboard_text(void* user_data_ctx)
{
    plu_sb_reset(gtIO->sbcClipboardData);

    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSString* available = [pasteboard availableTypeFromArray: [NSArray arrayWithObject:NSPasteboardTypeString]];
    if (![available isEqualToString:NSPasteboardTypeString])
        return NULL;

    NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
    if (string == nil)
        return NULL;

    const char* string_c = (const char*)[string UTF8String];
    size_t string_len = strlen(string_c);
    plu_sb_resize(gtIO->sbcClipboardData, (int)string_len + 1);
    strcpy(gtIO->sbcClipboardData, string_c);
    return gtIO->sbcClipboardData;
}

void
pl__set_clipboard_text(void* pUnused, const char* text)
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
    [pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

plKey
pl__osx_key_to_pl_key(int iKey)
{
    switch (iKey)
    {
        case kVK_ANSI_A:              return PL_KEY_A;
        case kVK_ANSI_S:              return PL_KEY_S;
        case kVK_ANSI_D:              return PL_KEY_D;
        case kVK_ANSI_F:              return PL_KEY_F;
        case kVK_ANSI_H:              return PL_KEY_H;
        case kVK_ANSI_G:              return PL_KEY_G;
        case kVK_ANSI_Z:              return PL_KEY_Z;
        case kVK_ANSI_X:              return PL_KEY_X;
        case kVK_ANSI_C:              return PL_KEY_C;
        case kVK_ANSI_V:              return PL_KEY_V;
        case kVK_ANSI_B:              return PL_KEY_B;
        case kVK_ANSI_Q:              return PL_KEY_Q;
        case kVK_ANSI_W:              return PL_KEY_W;
        case kVK_ANSI_E:              return PL_KEY_E;
        case kVK_ANSI_R:              return PL_KEY_R;
        case kVK_ANSI_Y:              return PL_KEY_Y;
        case kVK_ANSI_T:              return PL_KEY_T;
        case kVK_ANSI_1:              return PL_KEY_1;
        case kVK_ANSI_2:              return PL_KEY_2;
        case kVK_ANSI_3:              return PL_KEY_3;
        case kVK_ANSI_4:              return PL_KEY_4;
        case kVK_ANSI_6:              return PL_KEY_6;
        case kVK_ANSI_5:              return PL_KEY_5;
        case kVK_ANSI_Equal:          return PL_KEY_EQUAL;
        case kVK_ANSI_9:              return PL_KEY_9;
        case kVK_ANSI_7:              return PL_KEY_7;
        case kVK_ANSI_Minus:          return PL_KEY_MINUS;
        case kVK_ANSI_8:              return PL_KEY_8;
        case kVK_ANSI_0:              return PL_KEY_0;
        case kVK_ANSI_RightBracket:   return PL_KEY_RIGHT_BRACKET;
        case kVK_ANSI_O:              return PL_KEY_O;
        case kVK_ANSI_U:              return PL_KEY_U;
        case kVK_ANSI_LeftBracket:    return PL_KEY_LEFT_BRACKET;
        case kVK_ANSI_I:              return PL_KEY_I;
        case kVK_ANSI_P:              return PL_KEY_P;
        case kVK_ANSI_L:              return PL_KEY_L;
        case kVK_ANSI_J:              return PL_KEY_J;
        case kVK_ANSI_Quote:          return PL_KEY_APOSTROPHE;
        case kVK_ANSI_K:              return PL_KEY_K;
        case kVK_ANSI_Semicolon:      return PL_KEY_SEMICOLON;
        case kVK_ANSI_Backslash:      return PL_KEY_BACKSLASH;
        case kVK_ANSI_Comma:          return PL_KEY_COMMA;
        case kVK_ANSI_Slash:          return PL_KEY_SLASH;
        case kVK_ANSI_N:              return PL_KEY_N;
        case kVK_ANSI_M:              return PL_KEY_M;
        case kVK_ANSI_Period:         return PL_KEY_PERIOD;
        case kVK_ANSI_Grave:          return PL_KEY_GRAVE_ACCENT;
        case kVK_ANSI_KeypadDecimal:  return PL_KEY_KEYPAD_DECIMAL;
        case kVK_ANSI_KeypadMultiply: return PL_KEY_KEYPAD_MULTIPLY;
        case kVK_ANSI_KeypadPlus:     return PL_KEY_KEYPAD_ADD;
        case kVK_ANSI_KeypadClear:    return PL_KEY_NUM_LOCK;
        case kVK_ANSI_KeypadDivide:   return PL_KEY_KEYPAD_DIVIDE;
        case kVK_ANSI_KeypadEnter:    return PL_KEY_KEYPAD_ENTER;
        case kVK_ANSI_KeypadMinus:    return PL_KEY_KEYPAD_SUBTRACT;
        case kVK_ANSI_KeypadEquals:   return PL_KEY_KEYPAD_EQUAL;
        case kVK_ANSI_Keypad0:        return PL_KEY_KEYPAD_0;
        case kVK_ANSI_Keypad1:        return PL_KEY_KEYPAD_1;
        case kVK_ANSI_Keypad2:        return PL_KEY_KEYPAD_2;
        case kVK_ANSI_Keypad3:        return PL_KEY_KEYPAD_3;
        case kVK_ANSI_Keypad4:        return PL_KEY_KEYPAD_4;
        case kVK_ANSI_Keypad5:        return PL_KEY_KEYPAD_5;
        case kVK_ANSI_Keypad6:        return PL_KEY_KEYPAD_6;
        case kVK_ANSI_Keypad7:        return PL_KEY_KEYPAD_7;
        case kVK_ANSI_Keypad8:        return PL_KEY_KEYPAD_8;
        case kVK_ANSI_Keypad9:        return PL_KEY_KEYPAD_9;
        case kVK_Return:              return PL_KEY_ENTER;
        case kVK_Tab:                 return PL_KEY_TAB;
        case kVK_Space:               return PL_KEY_SPACE;
        case kVK_Delete:              return PL_KEY_BACKSPACE;
        case kVK_Escape:              return PL_KEY_ESCAPE;
        case kVK_CapsLock:            return PL_KEY_CAPS_LOCK;
        case kVK_Control:             return PL_KEY_LEFT_CTRL;
        case kVK_Shift:               return PL_KEY_LEFT_SHIFT;
        case kVK_Option:              return PL_KEY_LEFT_ALT;
        case kVK_Command:             return PL_KEY_LEFT_SUPER;
        case kVK_RightControl:        return PL_KEY_RIGHT_CTRL;
        case kVK_RightShift:          return PL_KEY_RIGHT_SHIFT;
        case kVK_RightOption:         return PL_KEY_RIGHT_ALT;
        case kVK_RightCommand:        return PL_KEY_RIGHT_SUPER;
        case kVK_F5:                  return PL_KEY_F5;
        case kVK_F6:                  return PL_KEY_F6;
        case kVK_F7:                  return PL_KEY_F7;
        case kVK_F3:                  return PL_KEY_F3;
        case kVK_F8:                  return PL_KEY_F8;
        case kVK_F9:                  return PL_KEY_F9;
        case kVK_F11:                 return PL_KEY_F11;
        case kVK_F13:                 return PL_KEY_PRINT_SCREEN;
        case kVK_F10:                 return PL_KEY_F10;
        case 0x6E:                    return PL_KEY_MENU;
        case kVK_F12:                 return PL_KEY_F12;
        case kVK_Help:                return PL_KEY_INSERT;
        case kVK_Home:                return PL_KEY_HOME;
        case kVK_PageUp:              return PL_KEY_PAGE_UP;
        case kVK_ForwardDelete:       return PL_KEY_DELETE;
        case kVK_F4:                  return PL_KEY_F4;
        case kVK_End:                 return PL_KEY_END;
        case kVK_F2:                  return PL_KEY_F2;
        case kVK_PageDown:            return PL_KEY_PAGE_DOWN;
        case kVK_F1:                  return PL_KEY_F1;
        case kVK_LeftArrow:           return PL_KEY_LEFT_ARROW;
        case kVK_RightArrow:          return PL_KEY_RIGHT_ARROW;
        case kVK_DownArrow:           return PL_KEY_DOWN_ARROW;
        case kVK_UpArrow:             return PL_KEY_UP_ARROW;
        default:                      return PL_KEY_NONE;
    }
}


void
setup_metal(void)
{
    plGraphics* ptMetalGraphics = &tGraphics;

    // create command queue
    ptMetalGraphics->tCmdQueue = [tGraphics.tDevice newCommandQueue];

    // setup
    // render pass descriptor
    ptMetalGraphics->drawableRenderDescriptor = [MTLRenderPassDescriptor new];

    // color attachment
    ptMetalGraphics->drawableRenderDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    ptMetalGraphics->drawableRenderDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    ptMetalGraphics->drawableRenderDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

    // depth attachment
    ptMetalGraphics->drawableRenderDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
    ptMetalGraphics->drawableRenderDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
    ptMetalGraphics->drawableRenderDescriptor.depthAttachment.clearDepth = 1.0;    
}

void
resize(void)
{
    plGraphics* ptMetalGraphics = &tGraphics;

    plIO* ptIo = pl_get_io();

    // recreate depth texture
    MTLTextureDescriptor *depthTargetDescriptor = [MTLTextureDescriptor new];
    depthTargetDescriptor.width       = (uint32_t)ptIo->afMainViewportSize[0];
    depthTargetDescriptor.height      = (uint32_t)ptIo->afMainViewportSize[1];
    depthTargetDescriptor.pixelFormat = MTLPixelFormatDepth32Float;
    depthTargetDescriptor.storageMode = MTLStorageModePrivate;
    depthTargetDescriptor.usage       = MTLTextureUsageRenderTarget;
    ptMetalGraphics->depthTarget = [tGraphics.tDevice newTextureWithDescriptor:depthTargetDescriptor];
    ptMetalGraphics->drawableRenderDescriptor.depthAttachment.texture = ptMetalGraphics->depthTarget;
}

bool
begin_frame(void)
{
    plGraphics* ptMetalGraphics = &tGraphics;
    
    ptMetalGraphics->uCurrentFrame++;

        // get next drawable
    ptMetalGraphics->tCurrentDrawable = [ptMetalGraphics->pMetalLayer nextDrawable];

    if(!ptMetalGraphics->tCurrentDrawable)
        return false;

    // set color attachment to next drawable
    ptMetalGraphics->drawableRenderDescriptor.colorAttachments[0].texture = ptMetalGraphics->tCurrentDrawable.texture;

    ptMetalGraphics->tCurrentCommandBuffer = [ptMetalGraphics->tCmdQueue commandBuffer];

    return true;
}

void
end_frame(void)
{
    plGraphics* ptMetalGraphics = &tGraphics;
    [ptMetalGraphics->tCurrentCommandBuffer presentDrawable:ptMetalGraphics->tCurrentDrawable];
    [ptMetalGraphics->tCurrentCommandBuffer commit];
}

void
begin_recording(void)
{
    plGraphics* ptMetalGraphics = &tGraphics;
    ptMetalGraphics->tCurrentRenderEncoder = [ptMetalGraphics->tCurrentCommandBuffer renderCommandEncoderWithDescriptor:ptMetalGraphics->drawableRenderDescriptor];

}

void
end_recording(void)
{
    plGraphics* ptMetalGraphics = &tGraphics;
    [ptMetalGraphics->tCurrentRenderEncoder endEncoding];
}