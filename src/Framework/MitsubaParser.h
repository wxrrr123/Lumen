#pragma once
#include "../LumenPCH.h"
#include <mitsuba_parser/tinyparser-mitsuba.h>
using namespace TPM_NAMESPACE;

struct MitsubaParser {
	struct MitsubaBSDF {
		std::string name = "";
		std::string type = "";
		std::string texture = "";
		glm::vec3 albedo = glm::vec3(1);
		glm::vec3 emissive_factor = glm::vec3(0);
		float roughness = 0;
		float ior = 1.0f;
	};

	struct MitsubaIntegrator {
		std::string type = "path";
		int depth = 10;
		bool enable_vm = false;
		glm::vec3 sky_col = glm::vec3(0);
	};

	enum class MitsubaShape { Obj, Rectangle, Disk, Cube, Sphere, Unsupported };

	struct MitsubaLight {
		std::string type;
		glm::vec3 from = glm::vec3(0, 1, 0);
		glm::vec3 to = glm::vec3(0);
		glm::vec3 L = glm::vec3(1);
	};

	struct MitsubaMesh {
		std::string file = "";
		// In case
		std::string bsdf_ref = "";
		int bsdf_idx = -1;
		// Black material for the backing face added behind rectangle/disk area emitters (-1: none)
		int backing_bsdf_idx = -1;
		// sphere: centre/radius (local units, applied before transform)
		glm::vec3 sphere_center = glm::vec3(0);
		float sphere_radius = 1.0f;
		MitsubaShape shape = MitsubaShape::Obj;
		std::string shape_type = "";
		glm::mat4 transform = glm::mat4(1);
	};

	struct MitsubaCamera {
		// Full field of view in degrees along fov_axis (Mitsuba semantics)
		float fov = 45.0f;
		std::string fov_axis = "x";
		int film_width = 1280;
		int film_height = 720;
		// Camera-to-world matrix in Mitsuba's convention (+x left, +y up, camera looks along +z)
		glm::mat4 cam_matrix = glm::mat4(1);
	};
	struct MitsubaEnvmap {
		std::string file = "";
		float scale = 1.0f;
	};
	void parse(const std::string& path);

	std::vector<MitsubaBSDF> bsdfs;
	std::vector<MitsubaMesh> meshes;
	std::vector<MitsubaLight> lights;
	MitsubaIntegrator integrator;
	MitsubaCamera camera;
	MitsubaEnvmap envmap;

   private:
	MitsubaBSDF parse_bsdf(const Object* obj);
	int find_bsdf(const std::string& id) const;
};
