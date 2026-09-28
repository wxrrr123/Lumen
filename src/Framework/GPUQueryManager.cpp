#include "../LumenPCH.h"
#include "GPUQueryManager.h"

namespace GPUQueryManager {

TimestampData _data;
TimestampData _pool_data[vk::MAX_FRAMES_IN_FLIGHT];
uint32_t _curr_query_idx = 0;
uint32_t _curr_pool_idx = 0;

void begin(VkCommandBuffer cmd, const char* name) {
	LUMEN_ASSERT(_curr_query_idx < 4096, "Query pool exhausted");
	_pool_data[_curr_pool_idx].names[_curr_query_idx >> 1] = std::string(name);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::context().query_pool_timestamps[_curr_pool_idx],
						_curr_query_idx++);
}
void end(VkCommandBuffer cmd) {
	LUMEN_ASSERT(_curr_query_idx < 4096, "Query pool exhausted");
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk::context().query_pool_timestamps[_curr_pool_idx],
						_curr_query_idx++);
	_pool_data[_curr_pool_idx].size = _curr_query_idx;
}

void set_frame_id(uint64_t frame_id, uint64_t cpu_frame_start_ns) {
	_pool_data[_curr_pool_idx].frame_id = frame_id;
	_pool_data[_curr_pool_idx].cpu_frame_start_ns = cpu_frame_start_ns;
}

void collect(uint32_t curr_frame_idx) {
	auto& pool_data = _pool_data[curr_frame_idx];
	_data.size = pool_data.size;
	_data.frame_id = pool_data.frame_id;
	_data.cpu_frame_start_ns = pool_data.cpu_frame_start_ns;
	// Note: curr_frame_idx is the index of the command buffer that has finished its execution
	if (pool_data.size > 0) {
		vkGetQueryPoolResults(vk::context().device, vk::context().query_pool_timestamps[curr_frame_idx], 0,
							  pool_data.size, sizeof(uint64_t) * pool_data.size, _data.timestamps, sizeof(uint64_t),
							  VK_QUERY_RESULT_64_BIT);
		for (size_t i = 0; i < pool_data.size / 2; ++i) {
			_data.names[i] = pool_data.names[i];
		}
	}
	_curr_pool_idx = curr_frame_idx;
	_curr_query_idx = 0;
	pool_data.size = 0;
	pool_data.frame_id = UINT64_MAX;
	pool_data.cpu_frame_start_ns = 0;
	vkResetQueryPool(vk::context().device, vk::context().query_pool_timestamps[curr_frame_idx], 0, 4096);
}

void collect() { collect(_curr_pool_idx); }

const TimestampData& get() { return _data; }
}  // namespace GPUQueryManager
