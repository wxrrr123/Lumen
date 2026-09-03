#include "Framework/Buffer.h"
#include "Framework/VulkanStructs.h"
#include "LumenPCH.h"
#include "ReSTIRPT.h"
#include <algorithm>
#include <map>
#include <vulkan/vulkan_core.h>
#include "imgui/imgui.h"

void ReSTIRPT::init() {
	Integrator::init();

	std::vector<glm::mat4> transformations;
	transformations.resize(lumen_scene->prim_meshes.size());
	for (auto& pm : lumen_scene->prim_meshes) {
		transformations[pm.prim_idx] = pm.world_matrix;
	}

	gris_gbuffer =
		prm::get_buffer({.name = "GRIS GBuffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(GBuffer)});

	gris_prev_gbuffer =
		prm::get_buffer({.name = "GRIS Previous GBuffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(GBuffer)});

	direct_lighting_texture = prm::get_texture({.name = "Direct Lighting Texture",
												.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
												.dimensions = {Window::width(), Window::height(), 1},
												.format = VK_FORMAT_R32G32B32A32_SFLOAT});
	gris_reservoir_ping_buffer =
		prm::get_buffer({.name = "GRIS Reservoirs Ping",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(Reservoir)});

	gris_reservoir_pong_buffer =
		prm::get_buffer({.name = "GRIS Reservoirs Pong",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(Reservoir)});
						 
	const uint32_t compact_slots =
		std::max(1u, uint32_t(float(Window::width() * Window::height()) * compact_ratio));
						 
	gris_compact_data_ping_buffer =
		prm::get_buffer({.name = "GRIS Compact Data Ping",
						.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
								VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.memory_type = vk::BufferType::GPU,
						.size = (size_t)(compact_slots * sizeof(GrisData))});
						 						
	gris_compact_data_pong_buffer =
		prm::get_buffer({.name = "GRIS Compact Data Pong",
						.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
								VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.memory_type = vk::BufferType::GPU,
						.size = (size_t)(compact_slots * sizeof(GrisData))});
						 
	gris_importance_flag_ping_buffer =
		prm::get_buffer({.name = "GRIS Importance Flag Ping",
						.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
						VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.memory_type = vk::BufferType::GPU,
						.size = Window::width() * Window::height() * sizeof(uint32_t)});
							
	gris_importance_flag_pong_buffer =
		prm::get_buffer({.name = "GRIS Importance Flag Pong",
						.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
						VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.memory_type = vk::BufferType::GPU,
						.size = Window::width() * Window::height() * sizeof(uint32_t)});								

	gris_importance_counter_buffer =
		prm::get_buffer({.name = "GRIS Importance Counter",
						.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
								VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.memory_type = vk::BufferType::GPU,
						.size = sizeof(uint32_t)});					 

	prefix_contribution_buffer =
		prm::get_buffer({.name = "Prefix Contributions",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(glm::vec3)});

	debug_vis_buffer =
		prm::get_buffer({.name = "Debug Vis",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU_TO_CPU,
						 .size = Window::width() * Window::height() * sizeof(uint32_t)});
	reconnection_buffer = prm::get_buffer(
		{.name = "Reservoir Connection",
		 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		 .memory_type = vk::BufferType::GPU,
		 .size = Window::width() * Window::height() * sizeof(ReconnectionData) * (num_spatial_samples + 1)});

	// Spatial reuse neighbor access locality profiling (debug-only, see pc_ray.profile_neighbor_access)
	gris_neighbor_access_count_buffer =
		prm::get_buffer({.name = "GRIS Neighbor Access Count",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU_TO_CPU,
						 .size = Window::width() * Window::height() * sizeof(uint32_t)});

	gris_neighbor_distance_histogram_buffer =
		prm::get_buffer({.name = "GRIS Neighbor Distance Histogram",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU_TO_CPU,
						 .size = NEIGHBOR_DISTANCE_HISTOGRAM_BUCKETS * sizeof(uint32_t)});

	// M4: per-pixel-neighbor-pair replay ray count (retrace_paths.rgen only, see
	// gris_commons.glsl PROFILE_REPLAY_RAY_COUNT). Same (N+1)-slots-per-pixel layout as
	// reconnection_buffer, so it needs the same resize-on-N-change treatment (see gui()).
	gris_replay_ray_count_buffer =
		prm::get_buffer({.name = "GRIS Replay Ray Count",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU_TO_CPU,
						 .size = Window::width() * Window::height() * (num_spatial_samples + 1) * sizeof(uint32_t)});
	gris_replay_shadow_ray_count_buffer =
		prm::get_buffer({.name = "GRIS Replay Shadow Ray Count",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU_TO_CPU,
						 .size = Window::width() * Window::height() * (num_spatial_samples + 1) * sizeof(uint32_t)});

	transformations_buffer = prm::get_buffer({
		.name = "Transformations Buffer",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = transformations.size() * sizeof(glm::mat4),
		.data = transformations.data(),
	});

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->get_device_address();

	desc.material_addr = lumen_scene->materials_buffer->get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->get_device_address();
	// ReSTIR PT (GRIS)
	desc.transformations_addr = transformations_buffer->get_device_address();
	desc.prefix_contributions_addr = prefix_contribution_buffer->get_device_address();
	desc.debug_vis_addr = debug_vis_buffer->get_device_address();
	desc.gris_neighbor_access_count_addr = gris_neighbor_access_count_buffer->get_device_address();
	desc.gris_neighbor_distance_histogram_addr = gris_neighbor_distance_histogram_buffer->get_device_address();
	desc.gris_replay_ray_count_addr = gris_replay_ray_count_buffer->get_device_address();
	desc.gris_replay_shadow_ray_count_addr = gris_replay_shadow_ray_count_buffer->get_device_address();

	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	canonical_contributions_texture = prm::get_texture({
		.name = "Canonical Contributions Texture",
		.usage = VK_IMAGE_USAGE_STORAGE_BIT,
		.dimensions = {Window::width(), Window::height(), 1},
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
	});

	pc_ray.total_light_area = 0;

	frame_num = 0;

	pc_ray.total_frame_num = 0;
	pc_ray.buffer_idx = 0;

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_reservoir_addr, gris_reservoir_ping_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, compact_vertices_addr, lumen_scene->compact_vertices_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, debug_vis_addr, debug_vis_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_neighbor_access_count_addr, gris_neighbor_access_count_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_neighbor_distance_histogram_addr,
								 gris_neighbor_distance_histogram_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_replay_ray_count_addr, gris_replay_ray_count_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_replay_shadow_ray_count_addr,
								 gris_replay_shadow_ray_count_buffer, vk::render_graph());

	path_length = getenv("LUMEN_PATH_LENGTH") ? atoi(getenv("LUMEN_PATH_LENGTH")) : config->path_length;
	if (getenv("LUMEN_MIN_VERTEX_DISTANCE_RATIO")) {
		min_vertex_distance_ratio = (float)atof(getenv("LUMEN_MIN_VERTEX_DISTANCE_RATIO"));
	}

	// SER cost-key reorder experiment variant. A (default): neither flag. D: cost-key computed,
	// reorderThreadNV() never called -- the only variant guaranteed to produce a valid pipeline
	// on hardware without VK_NV_ray_tracing_invocation_reorder, and the only one this GPU can
	// validate correctness for. B: reorderThreadNV() called with no key. C: both (main result).
	const char* ser_variant = getenv("LUMEN_SER_VARIANT");
	if (ser_variant && ser_variant[0] == 'D') {
		ser_enable_cost_reorder = true;
	} else if (ser_variant && ser_variant[0] == 'B') {
		ser_enable_reorder_call = true;
	} else if (ser_variant && ser_variant[0] == 'C') {
		ser_enable_cost_reorder = true;
		ser_enable_reorder_call = true;
	}
	LUMEN_TRACE("[SER] variant: {}", ser_variant ? ser_variant : "A (default)");
}

void ReSTIRPT::render() {
	pc_ray.size_x = Window::width();
	pc_ray.size_y = Window::height();
	pc_ray.enable_temporal_jitter = uint(enable_temporal_jitter);
	pc_ray.num_lights = (int)lumen_scene->gpu_lights.size();
	pc_ray.prev_random_num = pc_ray.general_seed;
	pc_ray.sampling_seed = rand() % UINT_MAX;
	pc_ray.seed2 = rand() % UINT_MAX;
	pc_ray.seed3 = rand() % UINT_MAX;
	pc_ray.max_depth = path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	pc_ray.dir_light_idx = lumen_scene->dir_light_idx;
	pc_ray.enable_accumulation = enable_accumulation;
	pc_ray.num_spatial_samples = num_spatial_samples;
	pc_ray.scene_extent = glm::length(lumen_scene->m_dimensions.max - lumen_scene->m_dimensions.min);
	pc_ray.direct_lighting = direct_lighting;
	pc_ray.enable_rr = enable_rr;

	pc_ray.spatial_radius = spatial_reuse_radius;
	pc_ray.enable_spatial_reuse = enable_spatial_reuse;
	pc_ray.hide_reconnection_radiance = hide_reconnection_radiance;
	pc_ray.min_vertex_distance_ratio = min_vertex_distance_ratio;
	pc_ray.enable_gris = enable_gris;
	pc_ray.frame_num = frame_num;
	pc_ray.pixel_debug = pixel_debug;
	pc_ray.temporal_reuse = uint(enable_temporal_reuse);
	pc_ray.permutation_sampling = uint(enable_permutation_sampling);
	pc_ray.gris_separator = gris_separator;
	pc_ray.canonical_only = canonical_only;
	pc_ray.enable_occlusion = enable_occlusion;
	pc_ray.compact_slot_count =
		std::max(1u, uint32_t(float(Window::width() * Window::height()) * compact_ratio));
	pc_ray.profile_neighbor_access = profile_neighbor_access;
	if (frame_num == 0) {
		LUMEN_TRACE(
			"[RUNTIME CHECK] num_spatial_samples={} enable_spatial_reuse={} max_depth={} "
			"enable_temporal_reuse={} profile_neighbor_access={} enable_gris={}",
			pc_ray.num_spatial_samples, pc_ray.enable_spatial_reuse, pc_ray.max_depth,
			pc_ray.temporal_reuse, pc_ray.profile_neighbor_access, pc_ray.enable_gris);
	}
	pc_ray.stable_neighbor_offset = stable_neighbor_offset;

	const std::initializer_list<lumen::ResourceBinding> common_bindings = {
		output_tex, scene_ubo_buffer, lumen_scene->scene_desc_buffer, lumen_scene->mesh_lights_buffer};

	const std::array<vk::Buffer*, 2> reservoir_buffers = {gris_reservoir_ping_buffer, gris_reservoir_pong_buffer};
	const std::array<vk::Buffer*, 2> gbuffers = {gris_prev_gbuffer, gris_gbuffer};
	const std::array<vk::Buffer*, 2> compact_buffers = {gris_compact_data_ping_buffer, gris_compact_data_pong_buffer};
	const std::array<vk::Buffer*, 2> flag_buffers = {gris_importance_flag_ping_buffer, gris_importance_flag_pong_buffer};

	int ping = pc_ray.total_frame_num % 2;
	int pong = ping ^ 1;

	constexpr int WRITE_OR_CURR_IDX = 1;
	constexpr int READ_OR_PREV_IDX = 0;

	// Trace rays
	vk::render_graph()
		->add_rt("GRIS - Generate Samples",
				 {
					 .shaders = {{"src/shaders/integrators/restir/gris/gris.rgen"},
								 {"src/shaders/integrators/restir/gris/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/integrators/restir/gris/ray.rchit"}},
					 .macros = {{"STREAMING_MODE", int(streaming_method)},
								vk::ShaderMacro("ENABLE_ATMOSPHERE", enable_atmosphere)},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&pc_ray)
		.zero(debug_vis_buffer)
		.bind(common_bindings)
		.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
		.bind(gbuffers[pong])
		.bind(canonical_contributions_texture)
		.bind(direct_lighting_texture)
		.bind(flag_buffers[WRITE_OR_CURR_IDX])
		.bind(compact_buffers[WRITE_OR_CURR_IDX])
		.zero(gris_importance_counter_buffer)
		.bind(gris_importance_counter_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);
	pc_ray.general_seed = rand() % UINT_MAX;
	if (enable_gris) {
		bool should_do_temporal = enable_temporal_reuse && pc_ray.total_frame_num > 0;
		// Temporal Reuse
		vk::render_graph()
			->add_rt("GRIS - Temporal Reuse",
					 {
						 .shaders = {{"src/shaders/integrators/restir/gris/temporal_reuse.rgen"},
									 {"src/shaders/integrators/restir/gris/ray.rmiss"},
									 {"src/shaders/ray_shadow.rmiss"},
									 {"src/shaders/integrators/restir/gris/ray.rchit"}},
						 .dims = {Window::width(), Window::height()},
					 })
			.push_constants(&pc_ray)
			.bind(common_bindings)
			.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
			.bind(reservoir_buffers[READ_OR_PREV_IDX])
			.bind(gbuffers[pong])
			.bind(gbuffers[ping])
			.bind(canonical_contributions_texture)
			.bind(flag_buffers[WRITE_OR_CURR_IDX])
			.bind(flag_buffers[READ_OR_PREV_IDX])
			.bind(compact_buffers[WRITE_OR_CURR_IDX])
			.bind(compact_buffers[READ_OR_PREV_IDX])
			.bind_texture_array(lumen_scene->scene_textures)
			.bind_tlas(tlas)
			.skip_execution(!should_do_temporal);
		pc_ray.seed2 = rand() % UINT_MAX;
		if (!canonical_only) {
			if (mis_method == MISMethod::TALBOT) {
				vk::render_graph()
					->add_rt("GRIS - Spatial Reuse - Talbot",
							 {
								 .shaders = {{"src/shaders/integrators/restir/gris/spatial_reuse_talbot.rgen"},
											 {"src/shaders/integrators/restir/gris/ray.rmiss"},
											 {"src/shaders/ray_shadow.rmiss"},
											 {"src/shaders/integrators/restir/gris/ray.rchit"}},
								 .dims = {Window::width(), Window::height()},
							 })
					.push_constants(&pc_ray)
					.bind(common_bindings)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(reservoir_buffers[READ_OR_PREV_IDX])
					.bind(gbuffers[pong])
					.bind(canonical_contributions_texture)
					.bind(direct_lighting_texture)
					.bind_texture_array(lumen_scene->scene_textures)
					.bind_tlas(tlas);
			} else {
				// Retrace
				vk::render_graph()
					->add_rt("GRIS - Retrace Reservoirs",
							 {
								 .shaders = {{"src/shaders/integrators/restir/gris/retrace_paths.rgen"},
											 {"src/shaders/integrators/restir/gris/ray.rmiss"},
											 {"src/shaders/ray_shadow.rmiss"},
											 {"src/shaders/integrators/restir/gris/ray.rchit"}},
								 .macros = {vk::ShaderMacro("ENABLE_COST_REORDER", ser_enable_cost_reorder),
										   vk::ShaderMacro("ENABLE_REORDER_CALL", ser_enable_reorder_call)},
								 .dims = {Window::width(), Window::height()},
							 })
					.push_constants(&pc_ray)
					.bind(common_bindings)
					.bind(reconnection_buffer)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(gbuffers[pong])
					.bind(flag_buffers[WRITE_OR_CURR_IDX])
					.bind(compact_buffers[WRITE_OR_CURR_IDX])
					.zero(gris_replay_ray_count_buffer)
					.zero(gris_replay_shadow_ray_count_buffer)
					.bind_texture_array(lumen_scene->scene_textures)
					.bind_tlas(tlas);
				// Validate
				vk::render_graph()
					->add_rt("GRIS - Validate Samples",
							 {
								 .shaders = {{"src/shaders/integrators/restir/gris/validate_samples.rgen"},
											 {"src/shaders/integrators/restir/gris/ray.rmiss"},
											 {"src/shaders/ray_shadow.rmiss"},
											 {"src/shaders/integrators/restir/gris/ray.rchit"}},
								 .dims = {Window::width(), Window::height()},
							 })
					.push_constants(&pc_ray)
					.bind(common_bindings)
					.bind(reconnection_buffer)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(gbuffers[pong])
					.bind(flag_buffers[WRITE_OR_CURR_IDX])
					.bind(compact_buffers[WRITE_OR_CURR_IDX])
					.bind_texture_array(lumen_scene->scene_textures)
					.bind_tlas(tlas);

				// Spatial Reuse
				vk::render_graph()
					->add_rt(
						"GRIS - Spatial Reuse",
						{
							.shaders = {{"src/shaders/integrators/restir/gris/spatial_reuse.rgen"},
										{"src/shaders/integrators/restir/gris/ray.rmiss"},
										{"src/shaders/ray_shadow.rmiss"},
										{"src/shaders/integrators/restir/gris/ray.rchit"}},
							.macros = {vk::ShaderMacro("ENABLE_DEFENSIVE_PAIRWISE_MIS", enable_defensive_formulation)},
							.dims = {Window::width(), Window::height()},
						})
					.push_constants(&pc_ray)
					.bind(common_bindings)
					.bind(reconnection_buffer)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(reservoir_buffers[READ_OR_PREV_IDX])
					.bind(gbuffers[pong])
					.bind(canonical_contributions_texture)
					.bind(direct_lighting_texture)
					.bind(flag_buffers[WRITE_OR_CURR_IDX])
					.bind(flag_buffers[READ_OR_PREV_IDX])
					.bind(compact_buffers[WRITE_OR_CURR_IDX])
					.bind(compact_buffers[READ_OR_PREV_IDX])
					.zero(gris_importance_counter_buffer)
					.bind(gris_importance_counter_buffer)
					.zero(gris_neighbor_access_count_buffer)
					.zero(gris_neighbor_distance_histogram_buffer)
					.bind_texture_array(lumen_scene->scene_textures)
					.bind_tlas(tlas);
			}
			if (pixel_debug || (gris_separator < 1.0f && gris_separator > 0.0f)) {
				uint32_t num_wgs = uint32_t((Window::width() * Window::height() + 1023) / 1024);
				vk::render_graph()
					->add_compute(
						"GRIS - Debug Visualization",
						{.shader = vk::Shader("src/shaders/integrators/restir/gris/debug_vis.comp"), .dims = {num_wgs}})
					.push_constants(&pc_ray)
					.bind({output_tex, scene_ubo_buffer, lumen_scene->scene_desc_buffer});
			}
		}
	}
	pc_ray.total_frame_num++;

	if (pc_ray.pixel_debug) {
		vkDeviceWaitIdle(vk::context().device);

		void* mapped = nullptr;
		vmaMapMemory(vk::context().allocator, debug_vis_buffer->allocation, &mapped);
		
		uint32_t* counts = (uint32_t*)mapped;
		uint32_t attempted_compact_count = counts[0];
		uint32_t compact_count = std::min(attempted_compact_count, pc_ray.compact_slot_count);
		uint32_t total = Window::width() * Window::height();
		LUMEN_TRACE("Compact ratio: {:.1f}% ({}/{} stored, {} attempted)",
					compact_count * 100.0f / total,
					compact_count,
					total,
					attempted_compact_count);
		float W = (counts[1]);
		uint M = counts[2];
		float tp = (counts[3]);
		LUMEN_TRACE("pixel 0 - W: {}, M: {}, tp: {}", W, M, tp);
		float spatial_importance = (counts[4]);
		uint compact_slot_count = counts[5];
		LUMEN_TRACE("spatial importance: {}, compact slot count: {}", spatial_importance, compact_slot_count);
		
		vmaUnmapMemory(vk::context().allocator, debug_vis_buffer->allocation);
	}

	if (pc_ray.profile_neighbor_access) {
		vkDeviceWaitIdle(vk::context().device);
		const uint32_t total_pixels = Window::width() * Window::height();

		void* mapped_counts = nullptr;
		vmaMapMemory(vk::context().allocator, gris_neighbor_access_count_buffer->allocation, &mapped_counts);
		const uint32_t* access_counts = (const uint32_t*)mapped_counts;

		std::map<uint32_t, uint32_t> access_histogram;	// access count -> number of pixels
		uint64_t total_accepted_accesses = 0;
		std::vector<uint8_t> curr_frame_accessed_mask(total_pixels);
		for (uint32_t i = 0; i < total_pixels; i++) {
			access_histogram[access_counts[i]]++;
			total_accepted_accesses += access_counts[i];
			curr_frame_accessed_mask[i] = access_counts[i] > 0 ? 1 : 0;
		}
		vmaUnmapMemory(vk::context().allocator, gris_neighbor_access_count_buffer->allocation);

		LUMEN_TRACE("=== Spatial reuse neighbor access count distribution ({} total accepted accesses) ===",
					total_accepted_accesses);
		for (const auto& [access_count, pixel_count] : access_histogram) {
			LUMEN_TRACE("  accessed {} time(s): {} pixels ({:.2f}%)", access_count, pixel_count,
						pixel_count * 100.0f / total_pixels);
		}

		// Frame-to-frame access pattern stability: compare this frame's accessed/unaccessed mask
		// against the mask captured last time profiling ran (skipped on the first profiled frame,
		// or right after a resolution change, since there's no comparable previous mask yet).
		if (prev_frame_accessed_mask.size() == total_pixels) {
			uint64_t curr_accessed = 0, curr_unaccessed = 0;
			uint64_t accessed_overlap = 0, unaccessed_overlap = 0;
			for (uint32_t i = 0; i < total_pixels; i++) {
				if (curr_frame_accessed_mask[i]) {
					curr_accessed++;
					accessed_overlap += prev_frame_accessed_mask[i];
				} else {
					curr_unaccessed++;
					unaccessed_overlap += prev_frame_accessed_mask[i] == 0 ? 1 : 0;
				}
			}
			LUMEN_TRACE("=== Spatial reuse neighbor access frame-to-frame stability ===");
			LUMEN_TRACE("  of {} accessed pixels this frame, {} ({:.2f}%) were also accessed last frame",
						curr_accessed, accessed_overlap,
						curr_accessed > 0 ? accessed_overlap * 100.0f / curr_accessed : 0.0f);
			LUMEN_TRACE("  of {} unaccessed pixels this frame, {} ({:.2f}%) were also unaccessed last frame",
						curr_unaccessed, unaccessed_overlap,
						curr_unaccessed > 0 ? unaccessed_overlap * 100.0f / curr_unaccessed : 0.0f);
		}
		prev_frame_accessed_mask = std::move(curr_frame_accessed_mask);

		void* mapped_hist = nullptr;
		vmaMapMemory(vk::context().allocator, gris_neighbor_distance_histogram_buffer->allocation, &mapped_hist);
		const uint32_t* dist_buckets = (const uint32_t*)mapped_hist;

		uint64_t total_dist_events = 0;
		for (uint32_t b = 0; b < NEIGHBOR_DISTANCE_HISTOGRAM_BUCKETS; b++) {
			total_dist_events += dist_buckets[b];
		}
		LUMEN_TRACE("=== Spatial reuse neighbor buffer-index distance distribution ({} events) ===",
					total_dist_events);
		for (uint32_t b = 0; b < NEIGHBOR_DISTANCE_HISTOGRAM_BUCKETS; b++) {
			if (dist_buckets[b] == 0) continue;
			uint32_t lo = b == 0 ? 0u : (1u << (b - 1));
			uint32_t hi = (1u << b) - 1u;
			LUMEN_TRACE("  |index distance| in [{}, {}]: {} events ({:.2f}%)", lo, hi, dist_buckets[b],
						total_dist_events > 0 ? dist_buckets[b] * 100.0f / total_dist_events : 0.0f);
		}
		vmaUnmapMemory(vk::context().allocator, gris_neighbor_distance_histogram_buffer->allocation);
	}

	// M4: actual replay traceRayEXT count per pixel-neighbor pair (retrace_paths.rgen only).
	// Reuses the profile_neighbor_access gate since it's the same "is profiling on" switch.
	if (pc_ray.profile_neighbor_access && num_spatial_samples > 0) {
		vkDeviceWaitIdle(vk::context().device);
		const uint32_t total_pixels = Window::width() * Window::height();
		const uint32_t slots_per_pixel = num_spatial_samples + 1;	 // slot 0 unused, see gris_commons.glsl

		void* mapped_replay = nullptr;
		vmaMapMemory(vk::context().allocator, gris_replay_ray_count_buffer->allocation, &mapped_replay);
		const uint32_t* replay_counts = (const uint32_t*)mapped_replay;

		std::map<uint32_t, uint32_t> pair_histogram;	// ray count for one pixel-neighbor pair -> num pairs
		std::map<uint32_t, uint32_t> thread_histogram; // summed ray count for one pixel's N pairs -> num pixels
		uint64_t total_pairs = 0, zero_pairs = 0, total_replay_rays = 0;
		uint64_t zero_threads = 0;
		uint64_t gbuffer_valid_pixels = 0;	// slot 0 sanity check, see retrace_paths.rgen
		for (uint32_t px = 0; px < total_pixels; px++) {
			if (replay_counts[slots_per_pixel * px] != 0) gbuffer_valid_pixels++;
			uint32_t thread_sum = 0;
			for (uint32_t i = 0; i < num_spatial_samples; i++) {
				uint32_t c = replay_counts[slots_per_pixel * px + i + 1];
				pair_histogram[c]++;
				total_pairs++;
				if (c == 0) zero_pairs++;
				total_replay_rays += c;
				thread_sum += c;
			}
			thread_histogram[thread_sum]++;
			if (thread_sum == 0) zero_threads++;
		}
		vmaUnmapMemory(vk::context().allocator, gris_replay_ray_count_buffer->allocation);
		LUMEN_TRACE("=== M4 GBuffer sanity check: {} / {} pixels ({:.2f}%) had a valid primary gbuffer ===",
					gbuffer_valid_pixels, total_pixels, gbuffer_valid_pixels * 100.0f / total_pixels);

		auto report_histogram = [&](const char* label, const std::map<uint32_t, uint32_t>& hist, uint64_t total_items,
									uint64_t zero_items, const std::string& csv_path) {
			LUMEN_TRACE("=== M4 replay ray count [{}] ({} items, {} zero, {:.2f}% idle) ===", label, total_items,
						zero_items, total_items > 0 ? zero_items * 100.0f / total_items : 0.0f);
			uint64_t sum_active = 0, n_active = 0;
			uint32_t max_val = 0;
			for (const auto& [val, count] : hist) {
				if (val > 0) {
					sum_active += (uint64_t)val * count;
					n_active += count;
					max_val = std::max(max_val, val);
				}
				LUMEN_TRACE("  ray_count={}: {} items ({:.2f}%)", val, count,
							total_items > 0 ? count * 100.0f / total_items : 0.0f);
			}
			LUMEN_TRACE("  active-only mean={:.3f} max={}", n_active > 0 ? (double)sum_active / n_active : 0.0,
						max_val);
			std::ofstream csv(csv_path);
			csv << "ray_count,item_count\n";
			for (const auto& [val, count] : hist) csv << val << "," << count << "\n";
		};
		report_histogram("per-pair", pair_histogram, total_pairs, zero_pairs,
						 "m4_replay_ray_count_per_pair.csv");
		report_histogram("per-thread-sum", thread_histogram, total_pixels, zero_threads,
						 "m4_replay_ray_count_per_thread.csv");
		LUMEN_TRACE("=== M4 replay ray count total across all pairs this frame: {} ===", total_replay_rays);
	}
}

bool ReSTIRPT::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

void ReSTIRPT::destroy() {
	Integrator::destroy();
	auto buffer_list = {gris_gbuffer,
						gris_reservoir_ping_buffer,
						gris_reservoir_pong_buffer,
						gris_compact_data_ping_buffer,
						gris_compact_data_pong_buffer,
						gris_importance_flag_ping_buffer,
						gris_importance_flag_pong_buffer,
						gris_importance_counter_buffer,
						transformations_buffer,
						prefix_contribution_buffer,
						reconnection_buffer,
						gris_prev_gbuffer,
						debug_vis_buffer,
						gris_neighbor_access_count_buffer,
						gris_neighbor_distance_histogram_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
	prm::remove(canonical_contributions_texture);
	prm::remove(direct_lighting_texture);
}

bool ReSTIRPT::gui() {
	bool result = Integrator::gui();
	result |= ImGui::Checkbox("Direct lighting", &direct_lighting);
	result |= ImGui::Checkbox("Enable atmosphere", &enable_atmosphere);
	result |= ImGui::Checkbox("Enable Russian roulette", &enable_rr);
	result |= ImGui::Checkbox("Enable accumulation", &enable_accumulation);
	bool enable_gris_changed = ImGui::Checkbox("Enable GRIS", &enable_gris);
	result |= enable_gris_changed;
	if (enable_gris_changed) {
		pc_ray.total_frame_num = 0;
	}
	result |= ImGui::SliderInt("Path length", (int*)&path_length, 0, 12);
	result |= ImGui::Checkbox("Enable canonical-only mode", &canonical_only);
	if (!enable_gris) {
		return result;
	}
	int curr_streaming_method = static_cast<int>(streaming_method);
	std::array<const char*, 2> streaming_methods = {
		"Individual contributions",
		"Split at reconnection",
	};
	if (ImGui::Combo("Streaming method", &curr_streaming_method, streaming_methods.data(),
					 int(streaming_methods.size()))) {
		result = true;
		streaming_method = static_cast<StreamingMethod>(curr_streaming_method);
	}
	if (canonical_only) {
		return result;
	}
	result |= ImGui::SliderFloat("GRIS / Default", &gris_separator, 0.0f, 1.0f);
	result |= ImGui::Checkbox("Enable occlusion", &enable_occlusion);
	result |= ImGui::Checkbox("Temporal jitter", &enable_temporal_jitter);
	result |= ImGui::Checkbox("Debug pixels", &pixel_debug);
	ImGui::Checkbox("Profile spatial reuse neighbor access", &profile_neighbor_access);
	result |= ImGui::Checkbox("Stable neighbor offset (cross-frame fixed)", &stable_neighbor_offset);
	result |= ImGui::Checkbox("Enable defensive formulation", &enable_defensive_formulation);
	result |= ImGui::Checkbox("Enable permutation sampling", &enable_permutation_sampling);
	result |= ImGui::Checkbox("Enable spatial reuse", &enable_spatial_reuse);
	result |= ImGui::Checkbox("Hide reconnection radiance", &hide_reconnection_radiance);
	std::array<const char*, 2> mis_methods = {
		"Talbot (Reconnection only)",
		"Pairwise",
	};
	int curr_mis_method = static_cast<int>(mis_method);
	if (ImGui::Combo("MIS method", &curr_mis_method, mis_methods.data(), int(mis_methods.size()))) {
		result = true;
		mis_method = static_cast<MISMethod>(curr_mis_method);
	}
	result |= ImGui::Checkbox("Enable temporal reuse", &enable_temporal_reuse);
	bool spatial_samples_changed = ImGui::SliderInt("Num spatial samples", (int*)&num_spatial_samples, 0, 12);
	result |= spatial_samples_changed;
	result |= ImGui::SliderFloat("Spatial radius", &spatial_reuse_radius, 0.0f, 128.0f);
	result |= ImGui::SliderFloat("Min reconnection distance ratio", &min_vertex_distance_ratio, 0.0f, 1.0f);

	if (spatial_samples_changed && num_spatial_samples > 0) {
		vkDeviceWaitIdle(vk::context().device);
		prm::remove(reconnection_buffer);
		reconnection_buffer = prm::get_buffer(
			{.name = "Reservoir Connection",
			 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
					  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			 .memory_type = vk::BufferType::GPU,
			 .size = Window::width() * Window::height() * sizeof(ReconnectionData) * (num_spatial_samples + 1)});
		prm::remove(gris_replay_ray_count_buffer);
		gris_replay_ray_count_buffer =
			prm::get_buffer({.name = "GRIS Replay Ray Count",
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							 .memory_type = vk::BufferType::GPU_TO_CPU,
							 .size = Window::width() * Window::height() * (num_spatial_samples + 1) * sizeof(uint32_t)});
		prm::remove(gris_replay_shadow_ray_count_buffer);
		gris_replay_shadow_ray_count_buffer =
			prm::get_buffer({.name = "GRIS Replay Shadow Ray Count",
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							 .memory_type = vk::BufferType::GPU_TO_CPU,
							 .size = Window::width() * Window::height() * (num_spatial_samples + 1) * sizeof(uint32_t)});
	}
	return result;
}
