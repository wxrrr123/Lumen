#pragma once
#include "LumenPCH.h"
#include <volk/volk.h>
#include "RenderGraph.h"

namespace vk {

void init_imgui();
void init(bool validation_layers);
void destroy_imgui();
void add_device_extension(const char* name);
// Requested but not required: added to the enabled device extension list only if
// context().physical_device actually reports it (checked between pick_physical_device() and
// create_logical_device()). Must be called before init(). Query the outcome afterwards with
// is_device_extension_enabled().
void add_optional_device_extension(const char* name);
bool is_device_extension_enabled(const char* name);
std::vector<Texture*>& swapchain_images();
void recreate_swapchain();
uint32_t prepare_frame();
VkResult submit_frame(uint32_t image_idx);
lumen::RenderGraph* render_graph();
void cleanup_app_data();
void cleanup();
};	// namespace vk
