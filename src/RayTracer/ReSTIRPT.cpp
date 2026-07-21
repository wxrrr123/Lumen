#include "Framework/Buffer.h"
#include "Framework/VulkanStructs.h"
#include "LumenPCH.h"
#include "ReSTIRPT.h"
#include <algorithm>
#include <bit>
#include <map>
#include <vector>
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

	// Page-level access locality profiling (see spatial_reuse.rgen). Sized for the worst-case GUI
	// settings (largest modeled element, smallest page and tile size) so the runtime sliders never
	// require a reallocation.
	{
		const uint64_t total_pixels = uint64_t(Window::width()) * Window::height();
		const uint64_t max_pages =
			(total_pixels * PAGE_PROFILE_MAX_ELEM_BYTES + PAGE_PROFILE_MIN_PAGE_SIZE - 1) / PAGE_PROFILE_MIN_PAGE_SIZE;
		gris_page_touch_bitmap_buffer =
			prm::get_buffer({.name = "GRIS Page Touch Bitmap",
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							 .memory_type = vk::BufferType::GPU_TO_CPU,
							 .size = ((max_pages + 31) / 32) * sizeof(uint32_t)});

		const uint64_t max_tiles = ((Window::width() + PAGE_PROFILE_MIN_TILE_SIZE - 1) / PAGE_PROFILE_MIN_TILE_SIZE) *
								   ((Window::height() + PAGE_PROFILE_MIN_TILE_SIZE - 1) / PAGE_PROFILE_MIN_TILE_SIZE);
		gris_tile_page_bitmap_buffer =
			prm::get_buffer({.name = "GRIS Tile Page Bitmap",
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							 .memory_type = vk::BufferType::GPU_TO_CPU,
							 .size = max_tiles * PAGE_PROFILE_WINDOW_WORDS * sizeof(uint32_t)});

		gris_page_stats_buffer =
			prm::get_buffer({.name = "GRIS Page Stats",
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							 .memory_type = vk::BufferType::GPU_TO_CPU,
							 .size = PAGE_STATS_UINT_COUNT * sizeof(uint32_t)});
	}

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
	desc.gris_page_touch_bitmap_addr = gris_page_touch_bitmap_buffer->get_device_address();
	desc.gris_tile_page_bitmap_addr = gris_tile_page_bitmap_buffer->get_device_address();
	desc.gris_page_stats_addr = gris_page_stats_buffer->get_device_address();

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
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_page_touch_bitmap_addr, gris_page_touch_bitmap_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_tile_page_bitmap_addr, gris_tile_page_bitmap_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_page_stats_addr, gris_page_stats_buffer, vk::render_graph());

	path_length = config->path_length;
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
	pc_ray.stable_neighbor_offset = stable_neighbor_offset;
	pc_ray.page_size_bytes = 1u << uint32_t(page_profile_page_size_log2);
	pc_ray.profile_elem_bytes = uint32_t(page_profile_elem_bytes);
	pc_ray.profile_tile_size = 1u << uint32_t(page_profile_tile_log2);

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
								 {"src/shaders/integrators/restir/gris/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
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
									 {"src/shaders/integrators/restir/gris/ray.rchit"},
									 {"src/shaders/ray.rahit"}},
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
											 {"src/shaders/integrators/restir/gris/ray.rchit"},
											 {"src/shaders/ray.rahit"}},
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
											 {"src/shaders/integrators/restir/gris/ray.rchit"},
											 {"src/shaders/ray.rahit"}},
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
				// Validate
				vk::render_graph()
					->add_rt("GRIS - Validate Samples",
							 {
								 .shaders = {{"src/shaders/integrators/restir/gris/validate_samples.rgen"},
											 {"src/shaders/integrators/restir/gris/ray.rmiss"},
											 {"src/shaders/ray_shadow.rmiss"},
											 {"src/shaders/integrators/restir/gris/ray.rchit"},
											 {"src/shaders/ray.rahit"}},
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
										{"src/shaders/integrators/restir/gris/ray.rchit"},
										{"src/shaders/ray.rahit"}},
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
					.zero(gris_page_touch_bitmap_buffer)
					.zero(gris_tile_page_bitmap_buffer)
					.zero(gris_page_stats_buffer)
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

		// --- Page-level access locality (see spatial_reuse.rgen page profiling comment) ---
		const uint32_t page_size_bytes = pc_ray.page_size_bytes;
		const uint32_t elem_bytes = pc_ray.profile_elem_bytes;
		const uint32_t tile_size = pc_ray.profile_tile_size;
		const uint32_t num_pages =
			uint32_t((uint64_t(total_pixels) * elem_bytes + page_size_bytes - 1) / page_size_bytes);

		void* mapped_stats = nullptr;
		vmaMapMemory(vk::context().allocator, gris_page_stats_buffer->allocation, &mapped_stats);
		const uint32_t* page_stats = (const uint32_t*)mapped_stats;
		const uint64_t total_page_accesses = page_stats[PAGE_STATS_TOTAL_ACCESSES_IDX];
		const uint32_t window_overflow = page_stats[PAGE_STATS_WINDOW_OVERFLOW_IDX];
		const uint32_t max_footprint = page_stats[PAGE_STATS_MAX_FOOTPRINT_IDX];

		const uint32_t radius = uint32_t(std::ceil(spatial_reuse_radius));
		const uint64_t footprint_bound = uint64_t(2) * radius * Window::height() + 2 * radius;
		LUMEN_TRACE("=== Page-level locality (elem {} B, page {} B -> {} elems/page, buffer {} pages, tile {}x{}) ===",
					elem_bytes, page_size_bytes, page_size_bytes / elem_bytes, num_pages, tile_size, tile_size);
		LUMEN_TRACE("  {} neighbor accesses; footprint bound 2*R*size_y+2*R = {} elems ({:.1f} pages), observed max {} elems ({:.1f} pages)",
					total_page_accesses, footprint_bound,
					double(footprint_bound) * elem_bytes / page_size_bytes, max_footprint,
					double(max_footprint) * elem_bytes / page_size_bytes);

		uint64_t footprint_events = 0;
		for (uint32_t b = 0; b < PAGE_FOOTPRINT_HISTOGRAM_BUCKETS; b++) {
			footprint_events += page_stats[b];
		}
		LUMEN_TRACE("  per-pixel buffer-index footprint (max - min accessed index, {} pixels with accesses):",
					footprint_events);
		for (uint32_t b = 0; b < PAGE_FOOTPRINT_HISTOGRAM_BUCKETS; b++) {
			if (page_stats[b] == 0) continue;
			uint32_t lo = b == 0 ? 0u : (1u << (b - 1));
			uint32_t hi = (1u << b) - 1u;
			LUMEN_TRACE("    footprint in [{}, {}] elems (<= {:.1f} pages): {} pixels ({:.2f}%)", lo, hi,
						double(hi) * elem_bytes / page_size_bytes, page_stats[b],
						footprint_events > 0 ? page_stats[b] * 100.0 / footprint_events : 0.0);
		}
		vmaUnmapMemory(vk::context().allocator, gris_page_stats_buffer->allocation);

		// Frame-wide unique page coverage
		void* mapped_page_bitmap = nullptr;
		vmaMapMemory(vk::context().allocator, gris_page_touch_bitmap_buffer->allocation, &mapped_page_bitmap);
		const uint32_t* page_bitmap = (const uint32_t*)mapped_page_bitmap;
		uint64_t frame_unique_pages = 0;
		for (uint32_t w = 0; w < (num_pages + 31) / 32; w++) {
			frame_unique_pages += std::popcount(page_bitmap[w]);
		}
		vmaUnmapMemory(vk::context().allocator, gris_page_touch_bitmap_buffer->allocation);
		LUMEN_TRACE("  frame coverage: {} / {} pages touched by neighbor accesses ({:.2f}%)", frame_unique_pages,
					num_pages, num_pages > 0 ? frame_unique_pages * 100.0 / num_pages : 0.0);

		// Per-tile unique page count distribution
		const uint32_t tiles_x = (Window::width() + tile_size - 1) / tile_size;
		const uint32_t tiles_y = (Window::height() + tile_size - 1) / tile_size;
		const uint32_t num_tiles = tiles_x * tiles_y;
		void* mapped_tile_bitmap = nullptr;
		vmaMapMemory(vk::context().allocator, gris_tile_page_bitmap_buffer->allocation, &mapped_tile_bitmap);
		const uint32_t* tile_bitmap = (const uint32_t*)mapped_tile_bitmap;
		std::vector<uint32_t> tile_unique_pages;
		tile_unique_pages.reserve(num_tiles);
		uint32_t tiles_without_accesses = 0;
		uint64_t tile_pages_sum = 0;
		for (uint32_t t = 0; t < num_tiles; t++) {
			uint32_t unique = 0;
			for (uint32_t w = 0; w < PAGE_PROFILE_WINDOW_WORDS; w++) {
				unique += std::popcount(tile_bitmap[t * PAGE_PROFILE_WINDOW_WORDS + w]);
			}
			if (unique == 0) {
				tiles_without_accesses++;
			} else {
				tile_unique_pages.push_back(unique);
				tile_pages_sum += unique;
			}
		}
		vmaUnmapMemory(vk::context().allocator, gris_tile_page_bitmap_buffer->allocation);

		if (!tile_unique_pages.empty()) {
			std::sort(tile_unique_pages.begin(), tile_unique_pages.end());
			auto percentile = [&](uint32_t p) {
				return tile_unique_pages[size_t(tile_unique_pages.size() - 1) * p / 100];
			};
			LUMEN_TRACE(
				"  unique pages per {}x{} tile: min {}, p10 {}, median {}, p90 {}, max {}, mean {:.1f} "
				"({} active tiles, {} without accesses, buffer total {} pages)",
				tile_size, tile_size, tile_unique_pages.front(), percentile(10), percentile(50), percentile(90),
				tile_unique_pages.back(), double(tile_pages_sum) / tile_unique_pages.size(),
				uint32_t(tile_unique_pages.size()), tiles_without_accesses, num_pages);
		}
		if (window_overflow > 0) {
			LUMEN_TRACE(
				"  WARNING: {} accesses fell outside the {}-page per-tile window -- per-tile stats undercount; "
				"lower elem bytes / raise page size, or grow PAGE_PROFILE_WINDOW_PAGES",
				window_overflow, PAGE_PROFILE_WINDOW_PAGES);
		}
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
						gris_neighbor_distance_histogram_buffer,
						gris_page_touch_bitmap_buffer,
						gris_tile_page_bitmap_buffer,
						gris_page_stats_buffer};
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
	if (profile_neighbor_access) {
		ImGui::SliderInt("Page size (log2 bytes)", &page_profile_page_size_log2, 10, 16);
		ImGui::SliderInt("Modeled reservoir elem (bytes)", &page_profile_elem_bytes, 16,
						 int(PAGE_PROFILE_MAX_ELEM_BYTES));
		ImGui::SliderInt("Profiling tile size (log2 px)", &page_profile_tile_log2, 3, 6);
	}
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
	}
	return result;
}
