#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_16bit_storage : require

layout(push_constant) uniform _PushConstantRay { PCReSTIRGI pc; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer RestirSamples { ReservoirSample d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer Reservoirs { Reservoir d[]; };

uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uvec4 seed =
    init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy, pc.total_frame_num);

const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;

void init_s(out ReservoirSample s) {
    s.x_v = vec3(0);
    s.n_v = f16vec3(0);     // [FIX] 顯式建構
    s.x_s = vec3(0);
    s.n_s = f16vec3(0);     // [FIX] 顯式建構
    s.L_o = f16vec3(0);     // [FIX] 顯式建構
    s.f = f16vec3(0);       // [FIX] 顯式建構
    s.p_q = float16_t(0.0); // [FIX] 顯式建構, 注意是 0.0 不是 0
}

void init_reservoir(out Reservoir r) {
    r.w_sum = 0;
    r.W = float16_t(0.0);   // [FIX] 顯式建構
    r.m = 0;
    init_s(r.s);
}

void update_reservoir(inout Reservoir r, const ReservoirSample s, float w_i) {
    r.w_sum += w_i;
    r.m++;
    if (rand(seed) < w_i / r.w_sum) {

        r.s = s;
    }
}

float p_hat(const vec3 f) { return length(f); }


uint offset(const uint pingpong) {
    return pingpong * pc.size_x * pc.size_y;
}


bool similar(ReservoirSample q, ReservoirSample q_n) {
    const float depth_threshold = 0.5;
    const float angle_threshold = 25 * PI / 180;
    if (q.mat_idx != q_n.mat_idx ||
        dot(q_n.n_v, q.n_v) < cos(angle_threshold)) {
        return false;
    }
    return true;
}