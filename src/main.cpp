#include "LumenPCH.h"
#include "Framework/Window.h"
#include "RayTracer/RayTracer.h"

void window_size_callback(GLFWwindow* window, int width, int height) {}

int main(int argc, char* argv[]) {
#ifdef _DEBUG
	bool enable_debug = true;
#else
	bool enable_debug = false;
#endif
	bool fullscreen = false;
	// Small resolution so a full frame completes under GPGPU-Sim's cycle-accurate simulation
	// (the 1850x1016 default is ~1.88M pixels, far too many to simulate). Overridable via env
	// vars for native-GPU runs that don't have this constraint (e.g. LUMEN_WIDTH=1850
	// LUMEN_HEIGHT=1016 ./Lumen ...).
	int width = getenv("LUMEN_WIDTH") ? atoi(getenv("LUMEN_WIDTH")) : 128;
	int height = getenv("LUMEN_HEIGHT") ? atoi(getenv("LUMEN_HEIGHT")) : 128;
	bool headless = false;
	int headless_frames = 500;
	double headless_warmup_seconds = 0.0;
	double headless_measure_seconds = 0.0;
	std::string headless_output = "output/headless.exr";
	bool save_headless_output = true;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--headless") {
			headless = true;
		} else if (arg == "--frames" && i + 1 < argc) {
			headless_frames = std::atoi(argv[++i]);
		} else if (arg == "--warmup-seconds" && i + 1 < argc) {
			headless_warmup_seconds = std::atof(argv[++i]);
		} else if (arg == "--measure-seconds" && i + 1 < argc) {
			headless_measure_seconds = std::atof(argv[++i]);
		} else if (arg == "--output" && i + 1 < argc) {
			headless_output = argv[++i];
		} else if (arg == "--no-output") {
			save_headless_output = false;
		}
	}
	Logger::init();
	lumen::ThreadPool::init();
	Window::init(width, height, fullscreen, headless);
	{
		RayTracer app(enable_debug, argc, argv);
		app.init();
		if (headless) {
			if (headless_measure_seconds > 0.0) {
				using Clock = std::chrono::steady_clock;
				auto run_for = [&](double seconds) {
					auto deadline = Clock::now() + std::chrono::duration<double>(seconds);
					while (Clock::now() < deadline) {
						app.update();
					}
				};
				app.set_gpu_timing_enabled(false);
				run_for(headless_warmup_seconds);
				app.set_gpu_timing_enabled(true);
				run_for(headless_measure_seconds);
				app.set_gpu_timing_enabled(false);
				if (save_headless_output) {
					app.capture_next_output();
					app.update();
				}
			} else {
				for (int i = 0; i < headless_frames; ++i) {
					if (save_headless_output && i == headless_frames - 1) {
						app.capture_next_output();
					}
					app.update();
				}
			}
			if (save_headless_output && (headless_measure_seconds > 0.0 || headless_frames > 0)) {
				app.save_output(headless_output);
			}
		} else {
			while (!Window::should_close()) {
				Window::poll();
				app.update();
			}
		}
		app.cleanup();
	}
	Window::destroy();
	lumen::ThreadPool::destroy();
	return 0;
}
