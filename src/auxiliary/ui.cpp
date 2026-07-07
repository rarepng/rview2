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
template <typename T> void DrawAutoUI(const T& obj) {
	ImGui::SeparatorText("reflection test dump");

	template for (constexpr auto member : std::meta::nonstatic_data_members_of(^T)) {
		constexpr std::string_view name = std::meta::identifier_of(member);
		ImGui::BulletText("%s", name.data());
	}
}
#endif

bool ui::init(rvkbucket& renderData) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
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
														 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 24}}};

	VkDescriptorPoolCreateInfo imguiPoolInfo{};
	imguiPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	imguiPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	imguiPoolInfo.maxSets = 24;
	imguiPoolInfo.poolSizeCount = imguiPoolSizes.size();
	imguiPoolInfo.pPoolSizes = imguiPoolSizes.data();

	if (vkCreateDescriptorPool(renderData.vkdevice.device, &imguiPoolInfo, nullptr, &renderData.dpools[rview::core::idximguipool])) {
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

void ui::createdbgframe(rvkbucket& renderData) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	ImGui_ImplSDL3_ProcessEvent(&renderData.e);

	ImGuiWindowFlags imguiWindowFlags = 0;
	ImGui::Begin(" ", nullptr, imguiWindowFlags);
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
	ImGui::Text("Active Count: %u", active_entities);

	ImGui::SeparatorText("Camera Controls");

	ImGui::DragFloat3("Cam Position", glm::value_ptr(rview::core::camwpos), 0.1f);

	ImGui::DragFloat("Azimuth (Yaw)", &rview::core::azimuth, 0.5f);
	ImGui::SliderFloat("Elevation (Pitch)", &rview::core::elevation, -89.0f, 89.0f);

	float fov_degrees = glm::degrees(rview::core::fov);

	if (ImGui::SliderFloat("Field of View", &fov_degrees, 1.0f, 359.0f, "%.1f deg")) {
		rview::core::fov = glm::radians(fov_degrees);
		vkrenderer::setsize(renderData, renderData.width, renderData.height);
	}

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

		ImGui::SeparatorText("Animation Pool Selection");

		uint32_t currentPoolHandle = g_scene.poolHandles[g_selected_entity];
		std::string currentPoolName = "Bind Pose / None";

		if (currentPoolHandle != model_manager::FALLBACK_POOL_HANDLE) {
			const auto* pool = model_manager::g_animPoolRegistry.GetPool(currentPoolHandle);

			if (pool)
				currentPoolName = pool->name + " (ID: " + std::to_string(currentPoolHandle) + ")";
		}

		if (ImGui::BeginCombo("Active Pool", currentPoolName.c_str())) {
			bool isBindPoseSelected = (currentPoolHandle == model_manager::FALLBACK_POOL_HANDLE);

			if (ImGui::Selectable("Bind Pose / None", isBindPoseSelected)) {
				if (!isBindPoseSelected) {
					model_manager::StateChangeReq req{};
					req.dense_idx = g_selected_entity;
					req.targetPool = model_manager::FALLBACK_POOL_HANDLE;
					req.targetFlags = model_manager::ANIM_FLAG_NONE;
					req.targetExecutionFrame = rview::core::global_frame_counter.load(std::memory_order_relaxed) + 1;
					model_manager::g_stateChangeQueue.push(req);
				}
			}

			if (isBindPoseSelected)
				ImGui::SetItemDefaultFocus();

			for (uint32_t i = 1; i < model_manager::AnimPoolRegistry::MAX_POOLS; ++i) {
				if (!model_manager::g_animPoolRegistry.is_active[i])
					continue;

				uint32_t handle = (static_cast<uint32_t>(model_manager::g_animPoolRegistry.generations[i]) << 16) | i;
				bool isSelected = (currentPoolHandle == handle);

				std::string poolLabel = model_manager::g_animPoolRegistry.pools[i].name + " (ID: " + std::to_string(handle) + ")";

				if (ImGui::Selectable(poolLabel.c_str(), isSelected)) {
					if (!isSelected) {
						model_manager::StateChangeReq req{};
						req.dense_idx = g_selected_entity;
						req.targetPool = handle;
						req.targetFlags = model_manager::g_animPoolRegistry.pools[i].pipelineFlags;
						req.targetExecutionFrame = rview::core::global_frame_counter.load(std::memory_order_relaxed) + 1;
						model_manager::g_stateChangeQueue.push(req);
					}
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		if (currentPoolHandle != model_manager::FALLBACK_POOL_HANDLE) {
			auto* pool = model_manager::g_animPoolRegistry.GetPoolMutable(currentPoolHandle);

			if (pool) {
				ImGui::Text("Pipeline Flags: %u", static_cast<uint32_t>(pool->pipelineFlags));
				ImGui::SliderFloat("Playback Speed", &pool->playbackSpeed, 0.0f, 5.0f, "%.2fx");
				ImGui::Checkbox("Loop", &pool->loop);
				ImGui::ProgressBar(pool->duration > 0.0f ? pool->currentTime / pool->duration : 0.0f);
			}
		} else {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "[BIND POSE / FROZEN]");
		}

		ImGui::Checkbox("Visible", &model_manager::g_registry.is_visible[g_selected_entity]);

		ImGui::SeparatorText("Shape Keys / Morph Targets");

		uint32_t modelID = g_scene.modelIDs[g_selected_entity];

		if (modelID < g_assets.models.size()) {
			const ModelMetadata& modelMeta = g_assets.models[modelID];

			if (modelMeta.primitiveCount > 0) {
				const PrimitiveMetadata& primMeta = g_assets.primitives[modelMeta.firstPrimitiveIndex];
				uint32_t targetCount = primMeta.targetCount;

				if (targetCount > 0) {
					ImGui::PushID("MorphTargets");

					uint32_t offset = model_manager::g_registry.morph_weight_offsets[g_selected_entity];
					uint32_t cnt = model_manager::g_registry.morph_weight_counts[g_selected_entity];
					uint32_t safeTargetCount = std::min(targetCount, cnt);

					for (uint32_t i = 0; i < safeTargetCount; ++i) {
						std::string label = "Shape Key " + std::to_string(i);

						ImGui::SliderFloat(label.c_str(), &model_manager::g_registry.flat_morph_weights[offset + i], 0.0f, 1.0f, "%.3f");
					}

					ImGui::PopID();
				} else {
					ImGui::TextDisabled("No shape keys on this model.");
				}
			}
		}

		if (ImGui::Button("Duplicate/Add Instance", ImVec2(-1, 30))) {
			uint32_t srcModelID = g_scene.modelIDs[g_selected_entity];

			model_manager::Entity new_ent = model_manager::g_registry.create_entity(srcModelID);

			if (model_manager::g_registry.is_valid(new_ent)) {
				uint32_t new_dense_idx = model_manager::g_registry.get_dense_index(new_ent);

				g_scene.worldPositions[new_dense_idx] = g_scene.worldPositions[g_selected_entity] + glm::vec3(1.0f, 0.0f, 1.0f);
				g_scene.rotations[new_dense_idx] = g_scene.rotations[g_selected_entity];
				g_scene.scales[new_dense_idx] = g_scene.scales[g_selected_entity];
				g_scene.poolHandles[new_dense_idx] = g_scene.poolHandles[g_selected_entity];

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
			model_manager::Entity ent = {(gen << 20) | sparse_idx};

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
					vkrenderer::g_kill_queue.push_back({{(gen << 20) | sparse_idx}, targetModelID});
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

	ImGui::Begin("Animation Pool Manager");
	static uint32_t selected_manager_pool = model_manager::FALLBACK_POOL_HANDLE;

	ImGui::Text("Active Pools: %u / %u", model_manager::g_animPoolRegistry.get_active_count(), model_manager::AnimPoolRegistry::MAX_POOLS - 1);

	if (ImGui::BeginListBox("##PoolList", ImVec2(-FLT_MIN, 150))) {
		for (uint32_t i = 1; i < model_manager::AnimPoolRegistry::MAX_POOLS; ++i) {
			if (!model_manager::g_animPoolRegistry.is_active[i])
				continue;

			uint32_t handle = (static_cast<uint32_t>(model_manager::g_animPoolRegistry.generations[i]) << 16) | i;
			auto& pool = model_manager::g_animPoolRegistry.pools[i];

			bool is_selected = (selected_manager_pool == handle);
			std::string label = pool.name + "##" + std::to_string(handle);

			if (ImGui::Selectable(label.c_str(), is_selected)) {
				selected_manager_pool = handle;
			}

			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndListBox();
	}

	if (selected_manager_pool != model_manager::FALLBACK_POOL_HANDLE) {
		auto* pool = model_manager::g_animPoolRegistry.GetPoolMutable(selected_manager_pool);

		if (pool) {
			ImGui::SeparatorText("Pool Information");
			ImGui::Text("Name: %s", pool->name.c_str());
			ImGui::Text("Duration: %.2fs", pool->duration);
			ImGui::Text("Frames: %u @ %.1f Hz", pool->frameCount, pool->sampleRate);
			ImGui::Text("Skeleton Signature: %llu", (unsigned long long)pool->skeletonSignature);

			ImGui::SeparatorText("Execution Domain");
			int currentDomain = static_cast<int>(pool->domain);
			const char* domainNames[] = {"CPU_BAKED", "GPU_BAKED", "~CPU_EVAL~", "~GPU_EVAL~"};

			if (ImGui::Combo("Domain", &currentDomain, domainNames, IM_ARRAYSIZE(domainNames))) {
				if (currentDomain != static_cast<int>(pool->domain)) {
					model_manager::PoolMigrationReq migReq{};
					migReq.poolHandle = selected_manager_pool;
					migReq.targetDomain = static_cast<model_manager::ExecutionDomain>(currentDomain);
					model_manager::g_poolMigrationQueue.push(migReq);
				}
			}

			if (pool->migrationState != model_manager::PoolMigrationState::STABLE) {
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Migration in progress...");
			}

			bool uiRetain = pool->retainCpuData;

			if (ImGui::Checkbox("Retain CPU Data (IK/Blending)", &uiRetain)) {
				if (!uiRetain && pool->retainCpuData) {
					model_manager::g_poolPurgeQueue.push({selected_manager_pool});
				}
			}

			if (!uiRetain) {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[Purged]");
			}

			ImGui::SeparatorText("Actions");

			if (ImGui::Button("Duplicate Pool", ImVec2(-1, 30))) {
				model_manager::AnimPoolData clonedPool;
				clonedPool.skeletonSignature = pool->skeletonSignature;
				clonedPool.nodeCount = pool->nodeCount;
				clonedPool.jointCount = pool->jointCount;
				clonedPool.frameCount = pool->frameCount;
				clonedPool.name = pool->name + " (Copy)";
				clonedPool.duration = pool->duration;
				clonedPool.sampleRate = pool->sampleRate;

				clonedPool.pipelineFlags = model_manager::ANIM_FLAG_GPU_BAKED;
				clonedPool.domain = model_manager::ExecutionDomain::GPU_BAKED;
				clonedPool.migrationState = model_manager::PoolMigrationState::STABLE;

				clonedPool.baseSkeleton = pool->baseSkeleton;
				clonedPool.localTracks = pool->localTracks;
				clonedPool.globalTransforms = pool->globalTransforms;
				clonedPool.finalJointDQs = pool->finalJointDQs;

				uint32_t newHandle = model_manager::g_animPoolRegistry.RegisterPool(std::move(clonedPool));

				if (newHandle != model_manager::FALLBACK_POOL_HANDLE) {
					selected_manager_pool = newHandle;
				}
			}

			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));

			if (ImGui::Button("Delete Pool", ImVec2(-1, 30))) {
				model_manager::g_animPoolRegistry.DestroyPool(selected_manager_pool);
				selected_manager_pool = model_manager::FALLBACK_POOL_HANDLE;
			}

			ImGui::PopStyleColor(2);

		} else {
			selected_manager_pool = model_manager::FALLBACK_POOL_HANDLE;
		}
	}


	ImGui::End();

	if (g_selected_entity < active_entities && g_scene.modelIDs[g_selected_entity] != 0xFFFFFFFF) {
		ImGuiIO& io = ImGui::GetIO();
		ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), g_scene.worldPositions[g_selected_entity]) *
							  glm::mat4_cast(g_scene.rotations[g_selected_entity]) * glm::scale(glm::mat4(1.0f), g_scene.scales[g_selected_entity]);

		ImGuizmo::Manipulate(glm::value_ptr(vkrenderer::persviewproj[0]),
							 glm::value_ptr(vkrenderer::persviewproj[1]),
							 ImGuizmo::TRANSLATE | ImGuizmo::ROTATE | ImGuizmo::SCALE,
							 ImGuizmo::LOCAL,
							 glm::value_ptr(transform));

		if (ImGuizmo::IsUsing()) {
			glm::vec3 scale, translation, skew;
			glm::quat rotation;
			glm::vec4 perspective;
			glm::decompose(transform, scale, rotation, translation, skew, perspective);

			g_scene.worldPositions[g_selected_entity] = translation;
			g_scene.rotations[g_selected_entity] = rotation;
			g_scene.scales[g_selected_entity] = scale;
		}
	}
}
bool ui::createloadingscreen(rvkbucket& mvkobjs) {

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

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

bool ui::createpausebuttons(rvkbucket& mvkobjs) {

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

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
	if (vkrenderer::g_activeDrops.empty())
		return;

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

			bool is_disabled = !drop.parseFinished;

			if (is_disabled) {
				const char* status = "Working...";

				if (drop.currentStep == model_manager::ParseStep::parsing)
					status = "Parsing...";

				if (drop.currentStep == model_manager::ParseStep::baking)
					status = "Baking...";

				ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status);
				ImGui::BeginDisabled();
			}

			bool spawn_clicked = ImGui::Button("Spawn", ImVec2(120, 0));

			if (is_disabled)
				ImGui::EndDisabled();

			ImGui::SameLine();
			bool cancel_clicked = ImGui::Button("Cancel", ImVec2(120, 0));
			ImGui::Separator();
			ImGui::PopID();

			if (spawn_clicked) {
				drop.stagingData.requested_instances = drop.instanceCount;
				drop.stagingData.spawn_position = drop.spawnPos;
				vkrenderer::g_commit_queue.push_back(std::move(drop.stagingData));
				vkrenderer::g_activeDrops.erase(vkrenderer::g_activeDrops.begin() + i);
			} else if (cancel_clicked) {
				vkrenderer::cancelspawn(i);
			}
		}

		if (!allParsed)
			ImGui::BeginDisabled();

		if (ImGui::Button("Spawn All", ImVec2(120, 0))) {
			for (auto& drop : vkrenderer::g_activeDrops) {
				drop.stagingData.requested_instances = drop.instanceCount;
				drop.stagingData.spawn_position = drop.spawnPos;
				vkrenderer::g_commit_queue.push_back(std::move(drop.stagingData));
			}

			vkrenderer::g_activeDrops.clear();
		}

		if (!allParsed)
			ImGui::EndDisabled();

		ImGui::SameLine();

		if (ImGui::Button("Cancel All", ImVec2(120, 0))) {
			vkrenderer::cancelall();
		}
	}

	ImGui::End();
}
void ui::render(rvkbucket& renderData, VkCommandBuffer cbuffer) {
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cbuffer);
}

void ui::cleanup(rvkbucket& mvkobjs) {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void ui::backspace() {
	if (!inputxt.empty())
		inputxt.pop_back();
}
