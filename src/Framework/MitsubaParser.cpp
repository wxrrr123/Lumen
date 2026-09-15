#include "../LumenPCH.h"
#include "MitsubaParser.h"
#include <mitsuba_parser/tinyparser-mitsuba.h>

// tinyparser stores transforms row major, glm is column major
static glm::mat4 to_glm(const Transform& src) {
	glm::mat4 dst;
	float* p_dst = (float*)glm::value_ptr(dst);
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			p_dst[4 * i + j] = src.matrix[4 * j + i];
		}
	}
	return dst;
}

static glm::vec3 to_glm(const Color& c) { return glm::vec3(c.r, c.g, c.b); }
static glm::vec3 to_glm(const Vector& v) { return glm::vec3(v.x, v.y, v.z); }

// Wrapper BSDFs that carry the actual BSDF as a child
static bool is_bsdf_wrapper(const std::string& type) {
	return type == "twosided" || type == "mask" || type == "bumpmap" || type == "normalmap" || type == "coating" ||
		   type == "roughcoating";
}

int MitsubaParser::find_bsdf(const std::string& id) const {
	if (id.empty()) {
		return -1;
	}
	for (int i = 0; i < (int)bsdfs.size(); i++) {
		if (bsdfs[i].name == id) {
			return i;
		}
	}
	return -1;
}

MitsubaParser::MitsubaBSDF MitsubaParser::parse_bsdf(const Object* obj) {
	MitsubaBSDF bsdf;
	bsdf.name = obj->id();
	// Unwrap twosided/mask/bumpmap/... until we reach the actual BSDF
	while (is_bsdf_wrapper(obj->pluginType())) {
		const Object* inner = nullptr;
		for (const auto& child : obj->anonymousChildren()) {
			if (child->type() == OT_BSDF) {
				inner = child.get();
				break;
			}
		}
		if (!inner) {
			for (const auto& child : obj->namedChildren()) {
				if (child.second->type() == OT_BSDF) {
					inner = child.second.get();
					break;
				}
			}
		}
		if (!inner) {
			break;
		}
		obj = inner;
	}
	bsdf.type = obj->pluginType();
	for (const auto& prop : obj->properties()) {
		// Get reflectance
		if (prop.second.type() == PT_COLOR) {
			if (prop.first.find("reflectance") == std::string::npos &&
				prop.first.find("specularReflectance") == std::string::npos) {
				continue;
			}
			// Assume RGB for the moment
			bsdf.albedo = to_glm(prop.second.getColor());
		}
		if (prop.first == "alpha") {
			bsdf.roughness = std::sqrt(prop.second.getNumber());
		}
		if (prop.first == "int_ior") {
			bsdf.ior = prop.second.getNumber();
		}
	}
	for (const auto& named_child : obj->namedChildren()) {
		const Object* tex = named_child.second.get();
		if (tex->type() != OT_TEXTURE) {
			continue;
		}
		if (tex->pluginType() == "checkerboard") {
			// Procedural textures are not supported; use the average colour instead
			glm::vec3 c0(0.4f), c1(0.2f);
			for (const auto& texture_prop : tex->properties()) {
				if (texture_prop.first == "color0" && texture_prop.second.type() == PT_COLOR) {
					c0 = to_glm(texture_prop.second.getColor());
				} else if (texture_prop.first == "color1" && texture_prop.second.type() == PT_COLOR) {
					c1 = to_glm(texture_prop.second.getColor());
				}
			}
			bsdf.albedo = 0.5f * (c0 + c1);
			continue;
		}
		for (const auto& texture_prop : tex->properties()) {
			if (texture_prop.first == "filename") {
				bsdf.texture = texture_prop.second.getString();
			}
		}
	}
	return bsdf;
}

void MitsubaParser::parse(const std::string& path) {
	SceneLoader loader;
	auto scene = loader.loadFromFile(path);

	for (const auto& child : scene.anonymousChildren()) {
		Object* obj = child.get();
		switch (obj->type()) {
			case OT_INTEGRATOR: {
				integrator.type = obj->pluginType();
				for (const auto& prop : obj->properties()) {
					if (prop.first == "max_depth") {
						integrator.depth = (int)prop.second.getInteger();
					}
					if (prop.first == "enable_vm") {
						integrator.enable_vm = prop.second.getBool();
					}
				}

			} break;
			case OT_SENSOR: {
				for (const auto& prop : obj->properties()) {
					if (prop.first == "fov") {
						camera.fov = prop.second.getNumber();
					} else if (prop.first == "fov_axis") {
						camera.fov_axis = prop.second.getString();
					} else if (prop.first == "to_world") {
						camera.cam_matrix = to_glm(prop.second.getTransform());
					}
				}
				for (const auto& sensor_child : obj->anonymousChildren()) {
					if (sensor_child->type() != OT_FILM) {
						continue;
					}
					for (const auto& prop : sensor_child->properties()) {
						if (prop.first == "width") {
							camera.film_width = (int)prop.second.getInteger();
						} else if (prop.first == "height") {
							camera.film_height = (int)prop.second.getInteger();
						}
					}
				}

			} break;
			case OT_BSDF: {
				bsdfs.push_back(parse_bsdf(obj));
			} break;
			case OT_SHAPE: {
				MitsubaMesh mesh;
				mesh.shape_type = obj->pluginType();
				if (mesh.shape_type == "obj") {
					mesh.shape = MitsubaShape::Obj;
				} else if (mesh.shape_type == "rectangle") {
					mesh.shape = MitsubaShape::Rectangle;
				} else if (mesh.shape_type == "disk") {
					mesh.shape = MitsubaShape::Disk;
				} else if (mesh.shape_type == "cube") {
					mesh.shape = MitsubaShape::Cube;
				} else if (mesh.shape_type == "sphere") {
					mesh.shape = MitsubaShape::Sphere;
				} else {
					mesh.shape = MitsubaShape::Unsupported;
				}
				for (const auto& prop : obj->properties()) {
					if (prop.first == "filename") {
						mesh.file = prop.second.getString();
					} else if (prop.first == "to_world") {
						mesh.transform = to_glm(prop.second.getTransform());
					} else if (prop.first == "center") {
						mesh.sphere_center = to_glm(prop.second.getVector());
					} else if (prop.first == "radius") {
						mesh.sphere_radius = prop.second.getNumber();
					}
				}
				if (mesh.shape == MitsubaShape::Obj && mesh.file.empty()) {
					mesh.shape = MitsubaShape::Unsupported;
				}

				// BSDF: either a <ref> to a top-level BSDF or an inline <bsdf>
				glm::vec3 radiance(0);
				bool is_emitter = false;
				for (const auto& mesh_child : obj->anonymousChildren()) {
					if (mesh_child->type() == OT_BSDF) {
						const auto ref = mesh_child->id();
						mesh.bsdf_idx = find_bsdf(ref);
						mesh.bsdf_ref = ref;
						if (mesh.bsdf_idx == -1) {
							MitsubaBSDF inline_bsdf = parse_bsdf(mesh_child.get());
							inline_bsdf.name = "__inline_bsdf_" + std::to_string(bsdfs.size());
							bsdfs.push_back(inline_bsdf);
							mesh.bsdf_idx = (int)bsdfs.size() - 1;
						}
					} else if (mesh_child->type() == OT_EMITTER && mesh_child->pluginType() == "area") {
						is_emitter = true;
						for (const auto& prop : mesh_child->properties()) {
							if (prop.first == "radiance" && prop.second.type() == PT_COLOR) {
								radiance = to_glm(prop.second.getColor());
							}
						}
					}
				}
				if (mesh.bsdf_idx == -1) {
					MitsubaBSDF fallback;
					fallback.name = "__default_bsdf_" + std::to_string(bsdfs.size());
					fallback.type = "diffuse";
					fallback.albedo = glm::vec3(0.5f);
					bsdfs.push_back(fallback);
					mesh.bsdf_idx = (int)bsdfs.size() - 1;
				}
				if (is_emitter) {
					// Emission is a material property in Lumen, so give the emitter its own material
					MitsubaBSDF emissive = bsdfs[mesh.bsdf_idx];
					emissive.name = "__area_emitter_" + std::to_string(bsdfs.size());
					emissive.emissive_factor = radiance;
					bsdfs.push_back(emissive);
					mesh.bsdf_idx = (int)bsdfs.size() - 1;
					if (mesh.shape == MitsubaShape::Rectangle || mesh.shape == MitsubaShape::Disk) {
						// Mitsuba area emitters are one-sided but Lumen's are two-sided; the loader
						// puts a black backing face behind the emitter to block the back side.
						MitsubaBSDF backing;
						backing.name = "__emitter_backing_" + std::to_string(bsdfs.size());
						backing.type = "diffuse";
						backing.albedo = glm::vec3(0);
						bsdfs.push_back(backing);
						mesh.backing_bsdf_idx = (int)bsdfs.size() - 1;
					}
				}
				if (mesh.shape == MitsubaShape::Unsupported) {
					LUMEN_WARN("Mitsuba: skipping unsupported shape type '{}'", mesh.shape_type);
				}
				meshes.push_back(mesh);
			} break;
			case OT_EMITTER: {
				const std::string& type = obj->pluginType();
				if (type == "sunsky" || type == "sun" || type == "directional") {
					// Two flavours: Lumen-style files carry explicit sun_color/sky_color (see the
					// hand-edited classroom scene), original Mitsuba files only have
					// turbidity/sun_scale/sky_scale, for which we use fixed default colours.
					MitsubaLight light;
					light.type = "directional";
					bool has_sun_color = false;
					glm::vec3 sun_color(1.0f, 0.95f, 0.85f);
					float sun_scale = 1.0f;
					float sky_scale = 0.0f;
					bool has_sky_color = false;
					for (const auto& prop : obj->properties()) {
						if (prop.first == "sun_direction") {
							light.from = to_glm(prop.second.getVector());
						} else if (prop.first == "direction") {
							// Mitsuba directional lights point *along* direction
							light.from = -to_glm(prop.second.getVector());
						} else if (prop.first == "sun_color") {
							sun_color = to_glm(prop.second.getVector());
							has_sun_color = true;
						} else if (prop.first == "irradiance" && prop.second.type() == PT_COLOR) {
							sun_color = to_glm(prop.second.getColor());
							has_sun_color = true;
						} else if (prop.first == "sun_scale" || prop.first == "scale") {
							sun_scale = prop.second.getNumber();
						} else if (prop.first == "sky_scale") {
							sky_scale = prop.second.getNumber();
						} else if (prop.first == "sky_color") {
							integrator.sky_col = to_glm(prop.second.getVector());
							has_sky_color = true;
						}
					}
					if (has_sun_color) {
						light.L = 100.0f * sun_color * sun_scale;
					} else {
						light.L = sun_color * sun_scale;
					}
					if (!has_sky_color && sky_scale > 0.0f) {
						integrator.sky_col = glm::vec3(0.53f, 0.8f, 0.92f) * sky_scale * 0.25f;
					}
					lights.push_back(light);
				} else if (type == "constant") {
					for (const auto& prop : obj->properties()) {
						if (prop.first == "radiance" && prop.second.type() == PT_COLOR) {
							integrator.sky_col = to_glm(prop.second.getColor());
						}
					}
				} else if (type == "envmap") {
					for (const auto& prop : obj->properties()) {
						if (prop.first == "filename") {
							envmap.file = prop.second.getString();
						} else if (prop.first == "scale") {
							envmap.scale = prop.second.getNumber();
						}
					}
				} else {
					LUMEN_WARN("Mitsuba: skipping unsupported emitter type '{}'", type);
				}
			} break;
			default:
				break;
		}
	}
}
