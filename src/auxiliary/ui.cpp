#include "core/scene.hpp"
#include "model_manager.hpp"
#include "vkrenderer.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <string>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <misc/cpp/imgui_stdlib.h>
#include <vk/commandbuffer.hpp>
#include <ui.hpp>

// BIG BIG BIG BIG BIG BIG [[WIP]]
// A LOT OF HARDCODED AND MAGIC NUMBERS TEMPORARILY AND PANELS JUST FOR THE SAKE OF TESTING

#if defined(__cpp_reflection)
template <typename T>
void DrawAutoUI(const T& obj) {
	ImGui::SeparatorText("reflection test dump");

	template for (constexpr auto member : std::meta::nonstatic_data_members_of(^T)) {
		constexpr std::string_view name = std::meta::identifier_of(member);
		ImGui::BulletText("%s", name.data());
	}
}
#endif

bool ui::init(rvkbucket &renderData) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();
	//hardcoded but ig it's fine here
	io.Fonts->AddFontFromFileTTF("resources/comicbd.ttf", 29.0f);
	io.Fonts->AddFontFromFileTTF("resources/bruce.ttf", 52.0f);

	std::array<VkDescriptorPoolSize, 11> imguiPoolSizes{{{VK_DESCRIPTOR_TYPE_SAMPLER, 24},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 24},
			{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 24},
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 24},
			{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 24},
			{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 24},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 24},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 24},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 24},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 24},
			{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 24}
		}};

	VkDescriptorPoolCreateInfo imguiPoolInfo{};
	imguiPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	imguiPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	imguiPoolInfo.maxSets = 24;
	imguiPoolInfo.poolSizeCount = imguiPoolSizes.size();
	imguiPoolInfo.pPoolSizes = imguiPoolSizes.data();

	if (vkCreateDescriptorPool(renderData.vkdevice.device, &imguiPoolInfo, nullptr,
	                           &renderData.dpools[rview::core::idximguipool])) {
		return false;
	}

	ImGui_ImplSDL3_InitForVulkan(renderData.wind);


	VkPipelineRenderingCreateInfo plinerenderinginfo{};
	plinerenderinginfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	plinerenderinginfo.colorAttachmentCount = 1;
	plinerenderinginfo.pColorAttachmentFormats = &renderData.schain.image_format;
	plinerenderinginfo.depthAttachmentFormat = renderData.rddepthformat;
	plinerenderinginfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;



	ImGui_ImplVulkan_InitInfo imguiIinitInfo{};
	imguiIinitInfo.Instance = renderData.inst.instance;
	imguiIinitInfo.PhysicalDevice = renderData.vkdevice.physical_device;
	imguiIinitInfo.Device = renderData.vkdevice.device;
	imguiIinitInfo.Queue = renderData.graphicsQ;
	imguiIinitInfo.DescriptorPool = renderData.dpools[rview::core::idximguipool];
	imguiIinitInfo.MinImageCount = 2;
	imguiIinitInfo.ImageCount = renderData.schainimgs.size();
	imguiIinitInfo.UseDynamicRendering = true;
	imguiIinitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	imguiIinitInfo.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
	imguiIinitInfo.PipelineInfoMain.PipelineRenderingCreateInfo = plinerenderinginfo;

	ImGui_ImplVulkan_Init(&imguiIinitInfo);


	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(0.25f);
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Once, {0.8f, 0.2f});

	ImGui_ImplSDL3_ProcessEvent(&renderData.e);

	return true;
}

static uint32_t g_selected_entity = 0xFFFFFFFF;
static int active_ik_gizmo_chain = 0;

void ui::createdbgframe(rvkbucket &renderData) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	ImGui_ImplSDL3_ProcessEvent(&renderData.e);

	ImGuiWindowFlags imguiWindowFlags = 0;
	ImGui::Begin("Engine Inspector", nullptr, imguiWindowFlags);
	static std::array<float, 60> fps_history{0};
	static int fps_idx = 0;
	static float fps_sum = 0.0f;

	if (rview::core::frametime > 0.0f) {
		float current_fps = 1000.0f / 1000.0f / rview::core::frametime;
		fps_sum -= fps_history[fps_idx];
		fps_history[fps_idx] = current_fps;
		fps_sum += current_fps;
		fps_idx = (fps_idx + 1) % 60;
		mfps = fps_sum / 60.0f;
	}

	ImGui::Text("FPS: %.1f (%.2f ms)", mfps, rview::core::frametime);
	ImGui::Separator();

	uint32_t active_entities = g_scene.entity_count.load(std::memory_order_relaxed);
	ImGui::Text("Active Entities: %u", active_entities);

	ImGui::SeparatorText("Camera Controls");

	ImGui::DragFloat3("Cam Position", glm::value_ptr(rview::core::camwpos), 0.1f);

	ImGui::DragFloat("Azimuth (Yaw)", &rview::core::azimuth, 0.5f);
	ImGui::SliderFloat("Elevation (Pitch)", &rview::core::elevation, -89.0f, 89.0f);

	float fov_degrees = glm::degrees(rview::core::fov);

	if (ImGui::SliderFloat("Field of View", &fov_degrees, 1.0f, 359.0f, "%.1f deg")) {
		rview::core::fov = glm::radians(fov_degrees);
		vkrenderer::setsize(renderData, renderData.width, renderData.height);
	}

	ImGui::SeparatorText("VTuber / Procedural");

	static float breath_speed = 2.0f;
	static float breath_amp = 0.02f;
	ImGui::SliderFloat("Breathing Speed", &breath_speed, 0.1f, 5.0f);
	ImGui::SliderFloat("Breathing Depth", &breath_amp, 0.0f, 0.1f);

	static bool debug_mouse_gaze = false;
	static float gaze_depth = 5.0f;
	ImGui::Checkbox("Shift + Mouse drives IK Target", &debug_mouse_gaze);

	if (debug_mouse_gaze) {
		ImGui::SliderFloat("Gaze Depth", &gaze_depth, 1.0f, 10.0f);
	}

	//TEMP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// DOESNT WORK YET [[ PLEASE HOLD ]]
	if (debug_mouse_gaze && g_selected_entity != 0xFFFFFFFF && g_scene.modelIDs[g_selected_entity] != 0xFFFFFFFF) {
		const bool isShiftHeld = (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LSHIFT] ||
		                          SDL_GetKeyboardState(NULL)[SDL_SCANCODE_RSHIFT]);

		if (isShiftHeld) {
			float mouse_x, mouse_y;
			SDL_GetMouseState(&mouse_x, &mouse_y);

			float ndc_x = (2.0f * mouse_x) / renderData.width - 1.0f;
			float ndc_y = (2.0f * mouse_y) / renderData.height - 1.0f;

			glm::mat4 invVP = glm::inverse(vkrenderer::persviewproj[1] * vkrenderer::persviewproj[0]);

			glm::vec4 screenPos = glm::vec4(ndc_x, ndc_y, 0.1f, 1.0f);
			glm::vec4 worldPos = invVP * screenPos;
			worldPos /= worldPos.w;

			glm::vec3 rayDir = glm::normalize(glm::vec3(worldPos) - rview::core::camwpos);

			glm::vec3 target_pos = rview::core::camwpos + (rayDir * gaze_depth);

			model_manager::g_registry.ik_target_pos[g_selected_entity][0] = target_pos;

			if (model_manager::g_registry.ik_active_chains[g_selected_entity] == 0) {
				model_manager::g_registry.ik_active_chains[g_selected_entity] = 1;
			}
		}
	}

	//TEMP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	int sel_ent = (g_selected_entity == 0xFFFFFFFF) ? -1 : static_cast<int>(g_selected_entity);
	int max_ent = (active_entities > 0) ? active_entities - 1 : -1;

	if (ImGui::SliderInt("Selected Entity", &sel_ent, -1, max_ent)) {
		g_selected_entity = (sel_ent == -1) ? 0xFFFFFFFF : static_cast<uint32_t>(sel_ent);
	}

	if (g_selected_entity < active_entities && g_scene.modelIDs[g_selected_entity] != 0xFFFFFFFF) {
		ImGui::SeparatorText("Transform");

		ImGui::DragFloat3("Position", glm::value_ptr(g_scene.worldPositions[g_selected_entity]), 0.1f);

		glm::vec3 euler = glm::degrees(glm::eulerAngles(g_scene.rotations[g_selected_entity]));

		if (ImGui::DragFloat3("Rotation", glm::value_ptr(euler), 1.0f)) {
			g_scene.rotations[g_selected_entity] = glm::quat(glm::radians(euler));
		}

		ImGui::DragFloat3("Scale", glm::value_ptr(g_scene.scales[g_selected_entity]), 0.1f);

		ImGui::SeparatorText("Animation & IK");

		int anim_mode = static_cast<int>(model_manager::g_registry.anim_mode[g_selected_entity]);
		const char* modes[] = { "Baked", "Hybrid", "Dynamic", "Hero" };

		if (ImGui::Combo("Anim Mode", &anim_mode, modes, IM_ARRAYSIZE(modes))) {
			model_manager::g_registry.anim_mode[g_selected_entity] = static_cast<AnimMode>(anim_mode);
		}

		if (ImGui::SliderInt("Clip ID", &model_manager::g_registry.anim_clip[g_selected_entity], -1, 10)) {
			g_scene.animTimePositions[g_selected_entity] = 0.0f;
		}

		ImGui::SameLine();

		if (model_manager::g_registry.anim_clip[g_selected_entity] == -1) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "[BIND POSE / FROZEN]");
		} else {
			ImGui::TextDisabled("(Playing Clip)");
		}

		ImGui::Checkbox("Visible", &model_manager::g_registry.is_visible[g_selected_entity]);

		ImGui::SeparatorText("IK Controls");
		ImGui::DragFloat3("IK Target", glm::value_ptr(model_manager::g_registry.ik_target_pos[g_selected_entity][0]), 0.05f);

		ImGui::SeparatorText("Shape Keys / Morph Targets");

		uint32_t modelID = g_scene.modelIDs[g_selected_entity];

		if (modelID < g_assets.models.size()) {
			const ModelMetadata& modelMeta = g_assets.models[modelID];

			if (modelMeta.primitiveCount > 0) {
				const PrimitiveMetadata& primMeta = g_assets.primitives[modelMeta.firstPrimitiveIndex];
				uint32_t targetCount = primMeta.targetCount;

				if (targetCount > 0) {
					ImGui::PushID("MorphTargets");

					uint32_t active_weights = static_cast<uint32_t>(model_manager::g_registry.morph_weights[g_selected_entity].size());
					uint32_t safeTargetCount = std::min(targetCount, active_weights);

					for (uint32_t i = 0; i < safeTargetCount; ++i) {
						std::string label = "Shape Key " + std::to_string(i);

						ImGui::SliderFloat(
						    label.c_str(),
						    &model_manager::g_registry.morph_weights[g_selected_entity][i],
						    0.0f, 1.0f, "%.3f"
						);
					}

					ImGui::PopID();
				} else {
					ImGui::TextDisabled("No shape keys on this model.");
				}
			}
		}

		ImGui::SeparatorText("Animation Blending & Playback");

		ImGui::SliderFloat("Playback Speed", &model_manager::g_registry.anim_speed[g_selected_entity], 0.0f, 5.0f, "%.2fx");

		auto it = model_manager::g_cpuModels.find(modelID);

		if (it != model_manager::g_cpuModels.end() && !it->second.bakedClips.empty()) {
			int numClips = static_cast<int>(it->second.bakedClips.size());

			ImGui::TextDisabled("Crossfade to a new clip:");
			ImGui::SliderInt("Target Clip", &model_manager::g_registry.target_anim_clip[g_selected_entity], -1, numClips - 1);
			ImGui::SliderFloat("Blend Duration", &model_manager::g_registry.blend_duration[g_selected_entity], 0.1f, 3.0f, "%.2f sec");

			if (ImGui::Button("Trigger Crossfade", ImVec2(-1, 0))) {
				model_manager::g_registry.current_blend_time[g_selected_entity] = 0.0f;
			}

			ImGui::Spacing();

			ImGui::SliderFloat("Anim Magnitude", &model_manager::g_registry.anim_magnitude[g_selected_entity], 0.0f, 2.0f, "%.2f");

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("0.0 = Rest Pose | 1.0 = Normal | >1.0 = Exaggerated");
			}

			ImGui::SeparatorText("N-Way Blending (Simultaneous)");

			for (uint32_t layer = 0; layer < model_manager::Registry::MAX_BLEND_LAYERS; ++layer) {
				ImGui::PushID(layer);

				std::string layerLabel = "Layer " + std::to_string(layer);
				ImGui::Text("%s", layerLabel.c_str());

				ImGui::SetNextItemWidth(120.0f);
				ImGui::SliderInt("Clip", &model_manager::g_registry.blend_clips[g_selected_entity][layer], -1, numClips - 1);

				ImGui::SameLine();

				ImGui::SetNextItemWidth(150.0f);
				ImGui::SliderFloat("Weight", &model_manager::g_registry.blend_weights[g_selected_entity][layer], 0.0f, 1.0f);

				ImGui::PopID();
			}

			ImGui::SeparatorText("State Crossfading (Temporal)");
			ImGui::TextDisabled("Transition entire state over time");
			ImGui::SliderInt("Target Master Clip", &model_manager::g_registry.target_anim_clip[g_selected_entity], -1, numClips - 1);
			ImGui::SliderFloat("Fade Duration", &model_manager::g_registry.blend_duration[g_selected_entity], 0.1f, 3.0f, "%.2f sec");

			if (ImGui::Button("Trigger Fade", ImVec2(-1, 0))) {
				model_manager::g_registry.current_blend_time[g_selected_entity] = 0.0f;
			}
		}

		ImGui::SeparatorText("IK Chains (Hybrid / Hero)");

		AnimMode mode = model_manager::g_registry.anim_mode[g_selected_entity];

		if (mode == AnimMode::Hybrid || mode == AnimMode::Hero) {

			int active_chains = static_cast<int>(model_manager::g_registry.ik_active_chains[g_selected_entity]);

			// hardcoded
			if (ImGui::SliderInt("Active IK Chains", &active_chains, 0, 4)) {
				model_manager::g_registry.ik_active_chains[g_selected_entity] = static_cast<uint32_t>(active_chains);
			}

			for (int i = 0; i < active_chains; ++i) {
				ImGui::PushID(i);

				if (ImGui::TreeNode("IK Chain")) {

					ImGui::DragInt("Effector Bone ID", &model_manager::g_registry.ik_effector_node[g_selected_entity][i], 1, -1, 511);
					ImGui::DragInt("Root Bone ID", &model_manager::g_registry.ik_root_node[g_selected_entity][i], 1, -1, 511);

					ImGui::DragFloat3("Target Pos", glm::value_ptr(model_manager::g_registry.ik_target_pos[g_selected_entity][i]), 0.05f);

					if (ImGui::RadioButton("Control Target with 3D Gizmo", active_ik_gizmo_chain == i)) {
						active_ik_gizmo_chain = i;
					}

					ImGui::TreePop();
				}

				ImGui::PopID();
			}
		} else {
			ImGui::TextDisabled("Switch to Hybrid or Hero mode to enable IK.");
		}

		if (ImGui::Button("Duplicate/Add Instance", ImVec2(-1, 30))) {
			uint32_t srcModelID = g_scene.modelIDs[g_selected_entity];
			bool isSkinned = g_scene.isSkinned[g_selected_entity];

			uint32_t b_count = model_manager::g_registry.bone_count[g_selected_entity];
			uint32_t j_count = model_manager::g_registry.joint_count[g_selected_entity];

			uint32_t src_b_start = model_manager::g_registry.bone_start_index[g_selected_entity];
			uint32_t src_j_start = model_manager::g_registry.joint_start_index[g_selected_entity];

			uint32_t new_b_start = (b_count > 0) ? static_cast<uint32_t>(model_manager::g_bone_locals.size()) : 0;
			uint32_t new_j_start = (j_count > 0) ? static_cast<uint32_t>(model_manager::g_joint_to_node.size()) : 0;

			model_manager::Entity new_ent = model_manager::g_registry.create_entity(
			                                    srcModelID, isSkinned, new_b_start, b_count, new_j_start, j_count
			                                );

			if (model_manager::g_registry.is_valid(new_ent)) {
				uint32_t new_dense_idx = model_manager::g_registry.get_dense_index(new_ent);

				model_manager::g_registry.position(new_ent) = g_scene.worldPositions[g_selected_entity] + glm::vec3(1.0f, 0.0f, 1.0f);
				g_scene.rotations[new_dense_idx] = g_scene.rotations[g_selected_entity];
				g_scene.scales[new_dense_idx] = g_scene.scales[g_selected_entity];

				if (b_count > 0) {
					for (uint32_t b = 0; b < b_count; ++b) {
						int32_t old_parent = model_manager::g_bone_parents[src_b_start + b];

						int32_t new_parent = (old_parent >= 0) ? (old_parent - src_b_start + new_b_start) : -1;

						model_manager::g_bone_parents.push_back(new_parent);
						model_manager::g_bone_entity_owner.push_back(new_dense_idx);
						model_manager::g_bone_locals.push_back(model_manager::g_bone_locals[src_b_start + b]);
						model_manager::g_bone_globals.push_back(model_manager::g_bone_globals[src_b_start + b]);
					}

					if (j_count > 0) {
						for (uint32_t j = 0; j < j_count; ++j) {
							uint32_t old_target_node = model_manager::g_joint_to_node[src_j_start + j];

							uint32_t new_target_node = old_target_node - src_b_start + new_b_start;
							model_manager::g_joint_to_node.push_back(new_target_node);

							model_manager::g_joint_inverse_binds.push_back(model_manager::g_joint_inverse_binds[src_j_start + j]);
							model_manager::g_joint_final_matrices.push_back(model_manager::g_joint_final_matrices[src_j_start + j]);
						}
					}
				}

				auto it = model_manager::g_cpuModels.find(srcModelID);

				if (it != model_manager::g_cpuModels.end()) {
					it->second.refCount++;
				}

				g_selected_entity = new_dense_idx;
			}
		}


		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));

		if (ImGui::Button("Delete Instance", ImVec2(-1, 30))) {
			uint32_t sparse_idx = model_manager::g_registry.dense_to_sparse[g_selected_entity];
			uint32_t gen = model_manager::g_registry.generations[sparse_idx];
			model_manager::Entity ent = { (gen << 20) | sparse_idx };

			uint32_t targetModelID = g_scene.modelIDs[g_selected_entity];
			vkrenderer::g_kill_queue.push_back({ent, targetModelID});

			g_selected_entity = 0xFFFFFFFF;
		}

		if (ImGui::Button("Delete ALL Instances of this Model", ImVec2(-1, 30))) {
			uint32_t targetModelID = g_scene.modelIDs[g_selected_entity];
			uint32_t active_count = g_scene.entity_count.load(std::memory_order_relaxed);

			for (uint32_t i = 0; i < active_count; ++i) {
				if (g_scene.modelIDs[i] == targetModelID) {
					uint32_t sparse_idx = model_manager::g_registry.dense_to_sparse[i];
					uint32_t gen = model_manager::g_registry.generations[sparse_idx];
					vkrenderer::g_kill_queue.push_back({ { (gen << 20) | sparse_idx }, targetModelID });
				}
			}

			g_selected_entity = 0xFFFFFFFF;
		}

		ImGui::PopStyleColor(2);


	}


	ImGui::Separator();

	if (ImGui::Button("Save State to JSON", ImVec2(-1, 30))) {
		rview::io::save_state_to_json();
	}

	ImGui::End();

	if (g_selected_entity < active_entities && g_scene.modelIDs[g_selected_entity] != 0xFFFFFFFF) {
		ImGuiIO& io = ImGui::GetIO();
		ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), g_scene.worldPositions[g_selected_entity]) *
		                      glm::mat4_cast(g_scene.rotations[g_selected_entity]) *
		                      glm::scale(glm::mat4(1.0f), g_scene.scales[g_selected_entity]);

		ImGuizmo::Manipulate(
		    glm::value_ptr(vkrenderer::persviewproj[0]),
		    glm::value_ptr(vkrenderer::persviewproj[1]),
		    ImGuizmo::TRANSLATE | ImGuizmo::ROTATE | ImGuizmo::SCALE,
		    ImGuizmo::LOCAL,
		    glm::value_ptr(transform)
		);

		if (ImGuizmo::IsUsing()) {
			glm::vec3 scale, translation, skew;
			glm::quat rotation;
			glm::vec4 perspective;
			glm::decompose(transform, scale, rotation, translation, skew, perspective);

			g_scene.worldPositions[g_selected_entity] = translation;
			g_scene.rotations[g_selected_entity] = rotation;
			g_scene.scales[g_selected_entity] = scale;
		}

		if ((model_manager::g_registry.anim_mode[g_selected_entity] == AnimMode::Hero ||
		        model_manager::g_registry.anim_mode[g_selected_entity] == AnimMode::Hybrid) &&
		        model_manager::g_registry.ik_active_chains[g_selected_entity] > 0) {

			int chain_idx = active_ik_gizmo_chain;

			if (chain_idx >= static_cast<int>(model_manager::g_registry.ik_active_chains[g_selected_entity])) {
				chain_idx = 0;
				active_ik_gizmo_chain = 0;
			}

			glm::mat4 ik_matrix = glm::translate(glm::mat4(1.0f), model_manager::g_registry.ik_target_pos[g_selected_entity][chain_idx]);

			ImGuizmo::Manipulate(
			    glm::value_ptr(vkrenderer::persviewproj[0]),
			    glm::value_ptr(vkrenderer::persviewproj[1]),
			    ImGuizmo::TRANSLATE, ImGuizmo::WORLD, glm::value_ptr(ik_matrix)
			);

			if (ImGuizmo::IsUsing()) {
				model_manager::g_registry.ik_target_pos[g_selected_entity][chain_idx] = glm::vec3(ik_matrix[3]);
			}
		}
	}
}

bool ui::createloadingscreen(rvkbucket &mvkobjs) {

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGuiIO &io = ImGui::GetIO();

	ImGui_ImplSDL3_ProcessEvent(&mvkobjs.e);

	ImGuiWindowFlags imguiWindowFlags = 0;
	imguiWindowFlags |= ImGuiWindowFlags_NoBackground;
	imguiWindowFlags |= ImGuiWindowFlags_NoResize;
	imguiWindowFlags |= ImGuiWindowFlags_NoMove;
	imguiWindowFlags |= ImGuiWindowFlags_NoSavedSettings;
	imguiWindowFlags |= ImGuiWindowFlags_NoCollapse;
	imguiWindowFlags |= ImGuiWindowFlags_NoTitleBar;

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Once, {0.8f, 0.2f});

	ImGui::Begin("loading", nullptr, imguiWindowFlags);

	// ImGui::ProgressBar(static_cast<float>(std::rand()%100)/100.0f, {600,100});
	// ImGui::ProgressBar(mvkobjs.loadingprog, {600, 100});

	ImGui::End();

	return true;
}

bool ui::createpausebuttons(rvkbucket &mvkobjs) {

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGuiIO &io = ImGui::GetIO();

	ImGui_ImplSDL3_ProcessEvent(&mvkobjs.e);

	ImGuiWindowFlags imguiWindowFlags = 0;
	imguiWindowFlags |= ImGuiWindowFlags_MenuBar;
	imguiWindowFlags |= ImGuiWindowFlags_NoBackground;
	imguiWindowFlags |= ImGuiWindowFlags_NoResize;
	imguiWindowFlags |= ImGuiWindowFlags_NoMove;
	imguiWindowFlags |= ImGuiWindowFlags_NoSavedSettings;
	imguiWindowFlags |= ImGuiWindowFlags_NoCollapse;
	imguiWindowFlags |= ImGuiWindowFlags_NoTitleBar;
	imguiWindowFlags |= ImGuiWindowFlags_AlwaysAutoResize;

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Once, {0.8f, 0.2f});

	ImGui::Begin("pause", nullptr, imguiWindowFlags);

	ImGui::PushStyleColor(ImGuiCol_Button, {0.0f, 0.0f, 0.0f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.4f, 0.4f, 0.4f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.9f, 0.9f, 0.9f, 1.0f});

	ImGui::PushFont(io.Fonts->Fonts[0]);

	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, {0.0f, 0.0f, 0.0f, 0.2f});
	ImGui::PushStyleColor(ImGuiCol_PopupBg, {0.0f, 0.0f, 0.0f, 0.2f});
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {1.0f, 1.0f, 1.0f, 0.4f});
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, {1.0f, 1.0f, 1.0f, 1.0f});

	// ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, { 400,600 });

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("settings")) {
			if (ImGui::MenuItem("windowed", "F4")) {
				mvkobjs.fullscreen = !mvkobjs.fullscreen;
				SDL_SetWindowFullscreen(mvkobjs.wind, mvkobjs.fullscreen);
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::PopFont();

	ImGui::PushFont(io.Fonts->Fonts[1]);

	ImGui::BeginGroup();

	bool p = ImGui::Button("continue", {400, 200});
	ImGui::PushFont(io.Fonts->Fonts[0]);
	ImGui::PopFont();

	if (ImGui::Button("EXIT", {400, 120}))
		mvkobjs.mshutdown = true;
	ImGui::PopStyleColor(7);
	ImGui::PopStyleVar(0);
	ImGui::EndGroup();

	ImGui::PopFont();

	ImGui::End();

	ImGui::EndFrame();

	return p;
}

void ui::addchat(std::string s) {
	// ImGui::SetNextWindowScroll({10000.0f,10000.0f});
	chattxts.push_back(s);
}

void ui::createdropwidget(rvkbucket& mvkobjs, VkCommandBuffer c) {
	if (vkrenderer::g_activeDrops.empty()) return;

	ImGui::SetNextWindowBgAlpha(0.9f);

	if (ImGui::Begin("Pending Spawns", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {

		bool allParsed = true;

		for (int i = (int)vkrenderer::g_activeDrops.size() - 1; i >= 0; --i) {
			auto& drop = vkrenderer::g_activeDrops[i];
			allParsed &= drop.parseFinished;

			ImGui::PushID(drop.dropID);
			ImGui::Text("File: %s", drop.filename.c_str());

			ImGui::SliderInt("Instances", &drop.instanceCount, 1, 100);
			ImGui::DragFloat3("Spawn Location", glm::value_ptr(drop.spawnPos), 0.1f);

			if (!drop.parseFinished) {
				const char* status = "Working...";

				if (drop.currentStep == model_manager::ParseStep::parsing) status = "Parsing...";

				if (drop.currentStep == model_manager::ParseStep::baking) status = "Baking...";

				ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status);
				ImGui::BeginDisabled();
			}

			if (ImGui::Button("Spawn", ImVec2(120, 0))) {
				drop.stagingData.requested_instances = drop.instanceCount;
				drop.stagingData.spawn_position = drop.spawnPos;

				vkrenderer::g_commit_queue.push_back(std::move(drop.stagingData));

				vkrenderer::g_activeDrops.erase(vkrenderer::g_activeDrops.begin() + i);
			}

			if (!drop.parseFinished) ImGui::EndDisabled();

			ImGui::SameLine();

			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				vkrenderer::cancelspawn(i);
			}

			ImGui::Separator();
			ImGui::PopID();
		}

		if (!allParsed) ImGui::BeginDisabled();

		if (ImGui::Button("Spawn All", ImVec2(120, 0))) {
			for (auto& drop : vkrenderer::g_activeDrops) {
				drop.stagingData.requested_instances = drop.instanceCount;
				drop.stagingData.spawn_position = drop.spawnPos;
				vkrenderer::g_commit_queue.push_back(std::move(drop.stagingData));
			}

			vkrenderer::g_activeDrops.clear();
		}

		if (!allParsed) ImGui::EndDisabled();

		ImGui::SameLine();

		if (ImGui::Button("Cancel All", ImVec2(120, 0))) {
			vkrenderer::cancelall();
		}
	}

	ImGui::End();
}
void ui::render(rvkbucket &renderData, VkCommandBuffer cbuffer) {
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cbuffer);
}

void ui::cleanup(rvkbucket &mvkobjs) {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void ui::backspace() {
	if (!inputxt.empty())
		inputxt.pop_back();
}
