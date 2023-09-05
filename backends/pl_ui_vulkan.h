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
#include "vulkan/vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVulkanInit
{
   VkPhysicalDevice      tPhysicalDevice;
   VkDevice              tLogicalDevice;
   uint32_t              uImageCount;
   uint32_t              uFramesInFlight;
   VkRenderPass          tRenderPass; // default render pass
   VkSampleCountFlagBits tMSAASampleCount;
} plVulkanInit;


//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void            pl_initialize_vulkan        (const plVulkanInit* ptInit);
void            pl_cleanup_vulkan           (void);
void            pl_build_vulkan_font_atlas  (plFontAtlas* ptAtlas);
void            pl_cleanup_vulkan_font_atlas(plFontAtlas* ptAtlas);
void            pl_new_draw_frame_vulkan    (void);
void            pl_submit_vulkan_drawlist   (plDrawList* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex);
void            pl_submit_vulkan_drawlist_ex(plDrawList* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount);
VkDescriptorSet pl_add_texture              (VkImageView tImageView, VkImageLayout tImageLayout);


#endif // PL_UI_VULKAN_H