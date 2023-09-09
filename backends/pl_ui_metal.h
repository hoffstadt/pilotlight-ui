/*
   pl_vulkan.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] structs
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UI_VULKAN_H
#define PL_UI_VULKAN_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_ui.h"
#import <Metal/Metal.h>

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

NS_ASSUME_NONNULL_BEGIN
void pl_initialize_metal          (id<MTLDevice> device);
void pl_cleanup_metal             (void);
void pl_create_metal_font_texture (plFontAtlas* ptAtlas);
void pl_cleanup_metal_font_texture(plFontAtlas* ptAtlas);
void pl_new_draw_frame_metal      (void);
void pl_submit_metal_drawlist     (plDrawList* ptDrawlist, float fWidth, float fHeight, id<MTLRenderCommandEncoder> renderEncoder, MTLRenderPassDescriptor* renderPassDescriptor);
NS_ASSUME_NONNULL_END

#endif // PL_UI_VULKAN_H