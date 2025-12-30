#include "../../../commons.h"

#ifdef __cplusplus
    #include <cstdint>
    // C++ 端：模擬 GLSL 的 FP16 型別
    using float16_t = uint16_t;
    
    // 定義一個 6 bytes 的結構來對應 GLSL 的 f16vec3
    struct f16vec3 { 
        uint16_t x, y, z; 
    };
#endif

struct PCReSTIRGI {
	vec3 sky_col;
	uint frame_num;
	uint size_x;
	uint size_y;
	int num_lights;
	uint time;
	int max_depth;
	float total_light_area;
	int light_triangle_count;
	uint dir_light_idx;
	uint do_spatiotemporal;
	uint random_num;
	uint total_frame_num;
	float world_radius;
	int enable_accumulation;
};

struct ReservoirSample {
	vec3 x_v;          // 保留 FP32
	float16_t p_q;     // FP16
	f16vec3 n_v;       // FP16
	uint bsdf_props;
	vec3 x_s;          // 保留 FP32
	uint mat_idx;
	f16vec3 n_s;       // FP16
	f16vec3 L_o;       // FP16
	f16vec3 f;         // FP16
};

struct Reservoir {
	float w_sum;       // 保留 FP32 以避免累積誤差
	float16_t W;       // FP16
	uint m;
	// uint pad;       // 移除 padding，或者根據 alignment 需求調整
	ReservoirSample s;
};