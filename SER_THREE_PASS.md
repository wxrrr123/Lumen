# Three-pass cost-key SER

The guarded implementation supports independent A/B/C/D variants in Retrace,
Validate and Temporal. A skips key computation and reorder, B uses a constant
reorder hint, C computes and uses a cost hint, and D computes the hint without
reorder. Numerical guards in Generate remain enabled by default.

| Pass | Variant environment variable | Optional key sink |
|---|---|---|
| Retrace | `LUMEN_SER_VARIANT` | `LUMEN_RETRACE_COST_SINK=1` |
| Validate | `LUMEN_VALIDATE_SER_VARIANT` | `LUMEN_VALIDATE_COST_SINK=1` |
| Temporal | `LUMEN_TEMPORAL_SER_VARIANT` | `LUMEN_TEMPORAL_COST_SINK=1` |

All variants default to A. Sinks default off. For a key-computation-cost
measurement, enable the selected pass's sink for **both A and D**. A writes
zero, D writes its runtime key. The sink reuses the existing scratch buffer;
the renderer does not consume it to form the image. The comparison is D−A
with the same store on both sides, not D-with-sink against old A-without-sink.
Retrace's invalid-primary-GBuffer invocations still return before its key/store.

The `GL_NV_shader_invocation_reorder` extension and reorder instruction are
compiled only for variants B/C of the selected pass. Hardware without the
Vulkan invocation-reorder feature can use A/D; B/C require hardware support
or a simulator that implements the instruction. A/B/C/D compilation checks
with and without the Retrace sink verify a visible nonconstant D store and
no reorder instruction for A/D. These are SPIR-V checks, not NVIDIA SASS or
register-count measurements.

For timing use a fixed seed, depth and resolution, disable shader profiling,
periodic EXR and the host pipeline-statistics profiler, and keep all other
passes at A. Example for Retrace D at simulator resolution:

```bash
env LUMEN_WIDTH=128 LUMEN_HEIGHT=128 LUMEN_PATH_LENGTH=8 \
    LUMEN_FIXED_SEED=42 LUMEN_WRITE_EXR=0 LUMEN_LOG_GPU_TIMING=1 \
    LUMEN_LOG_FRAME_TIME=1 LUMEN_SER_VARIANT=D LUMEN_RETRACE_COST_SINK=1 \
    LUMEN_VALIDATE_SER_VARIANT=A LUMEN_TEMPORAL_SER_VARIANT=A \
    LUMEN_PROFILE_NEIGHBOR_ACCESS=0 \
    ./build/Lumen scenes/classroom/scene.xml --headless \
    --warmup-seconds 15 --measure-seconds 90 --no-output
```

`LUMEN_LOG_FRAME_TIME=1` additionally logs each completed GPU-query frame's
CPU frame-start time relative to measurement start. The timestamp is saved
with the query pool's frame ID, so three frames in flight do not misassociate
it with the currently submitted frame. This enables exact first/last 30 s
windows. It defaults off and adds no shader instrumentation or GPU wait.

The scene in this branch retains the historical classroom loader hacks.
Imported scenes/loader changes and local experiment logs are separate work.
