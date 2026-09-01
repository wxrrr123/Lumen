#pragma once
#include <vector>
#include "Framework/Texture.h"
#include "Integrator.h"
#include "shaders/integrators/restir/gris/gris_commons.h"
using namespace RestirPT;
class ReSTIRPT final : public Integrator {
   public:
	ReSTIRPT(LumenScene* lumen_scene, const vk::BVH& tlas)
		: Integrator(lumen_scene, tlas), config(CAST_CONFIG(lumen_scene->config.get(), ReSTIRPTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;
	virtual bool gui() override;

   private:
	enum class StreamingMethod { INDIVIDUAL_CONTRIBUTIONS, SPLITTING_AT_RECONNECTION };

	enum class MISMethod { TALBOT, PAIRWISE };
	vk::Buffer* gris_gbuffer;
	vk::Buffer* gris_prev_gbuffer;
	vk::Buffer* gris_reservoir_ping_buffer;
	vk::Buffer* gris_reservoir_pong_buffer;
	vk::Buffer* gris_compact_data_ping_buffer;
	vk::Buffer* gris_compact_data_pong_buffer;
	vk::Buffer* gris_importance_flag_ping_buffer;
	vk::Buffer* gris_importance_flag_pong_buffer;
	vk::Buffer* gris_importance_counter_buffer;
	vk::Buffer* prefix_contribution_buffer;
	vk::Buffer* reconnection_buffer;
	vk::Buffer* transformations_buffer;
	vk::Buffer* debug_vis_buffer;
	vk::Buffer* gris_neighbor_access_count_buffer;
	vk::Buffer* gris_neighbor_distance_histogram_buffer;
	// CPU-side copy of last frame's "accessed (count > 0)" mask, for frame-to-frame access
	// pattern stability profiling (see profile_neighbor_access). Size mismatch (e.g. after a
	// resolution change) is treated as "no previous frame" and the comparison is skipped for
	// that frame.
	std::vector<uint8_t> prev_frame_accessed_mask;
	vk::Texture* canonical_contributions_texture;
	vk::Texture* direct_lighting_texture;

	PCReSTIRPT pc_ray{};
	bool enable_accumulation = true;
	bool direct_lighting = true;
	bool enable_rr = false;
	bool enable_spatial_reuse = true;
	bool canonical_only = false;
	bool hide_reconnection_radiance = false;
	bool enable_temporal_reuse = true;
	bool enable_gris = true;
	bool pixel_debug = false;
	bool enable_permutation_sampling = false;
	bool enable_atmosphere = false;
	bool enable_defensive_formulation = true;
	bool enable_occlusion = true;
	bool enable_temporal_jitter = true;
	bool profile_neighbor_access = true;
	bool stable_neighbor_offset = true;
	float spatial_reuse_radius = 32.0f;
	float min_vertex_distance_ratio = 0.00f;
	float gris_separator = 1.0f;
	uint32_t path_length = 0;
	uint32_t num_spatial_samples = 3;
	static constexpr float compact_ratio = 1.0f;
	// Must match NEIGHBOR_DISTANCE_HISTOGRAM_BUCKETS in spatial_reuse.rgen
	static constexpr uint32_t NEIGHBOR_DISTANCE_HISTOGRAM_BUCKETS = 32;
	StreamingMethod streaming_method = StreamingMethod::INDIVIDUAL_CONTRIBUTIONS;
	MISMethod mis_method = MISMethod::PAIRWISE;
	ReSTIRPTConfig* config;
};
