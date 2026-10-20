#pragma once
#include "../LumenPCH.h"
#include "Shader.h"
#include "Texture.h"
#include "SBTWrapper.h"
#include "RenderGraphTypes.h"
#include "../shaders/descriptor_bindings.h"

namespace vk {
struct Pipeline;

struct Pipeline {
   public:
	enum class PipelineType { GFX = 0, RT = 1, COMPUTE = 2 };
	Pipeline(const std::string& name);
	void reload();
	void cleanup();
	void create_gfx_pipeline(const GraphicsPassSettings& settings, const std::vector<uint32_t>& descriptor_counts,
							 std::vector<vk::Texture*> color_outputs, vk::Texture* depth_output);
	void create_rt_pipeline(const RTPassSettings& settings, const std::vector<uint32_t>& descriptor_counts);
	void create_compute_pipeline(const ComputePassSettings& settings, const std::vector<uint32_t>& descriptor_counts);
	const std::array<VkStridedDeviceAddressRegionKHR, 4> get_rt_regions();
	void refresh();

	std::unordered_map<std::string, std::filesystem::file_time_type> paths;
	VkPipeline handle = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
	// TLAS is pushed separately into set 0 because update templates cannot carry it.
	VkWriteDescriptorSetAccelerationStructureKHR tlas_info = {};
	SBTWrapper sbt_wrapper;
	PipelineType type;
	bool running = true;
	VkDescriptorUpdateTemplate update_template = nullptr;
	VkShaderStageFlags pc_stages = 0;
	std::string name;
	uint32_t push_constant_size = 0;
	VkDescriptorType descriptor_types[32] = {};
	std::vector<uint32_t> descriptor_counts;

	// In the future we may have multiple AS descriptors


   private:
	void create_pipeline_layout(const std::vector<Shader>& shaders, const std::vector<uint32_t> push_const_sizes);
	void create_update_template(const std::vector<Shader>& shaders, const std::vector<uint32_t>& descriptor_counts);
	void create_set_layout(const std::vector<Shader>& shaders, const std::vector<uint32_t>& descriptor_counts);
	bool tracking_stopped = true;
	std::mutex mut;
	std::condition_variable cv;
	uint32_t binding_mask;
};

}  // namespace vk
