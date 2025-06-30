#define GLM_ENABLE_EXPERIMENTAL

#include <string>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "vk/commandbuffer.hpp"
#include "ui.hpp"

bool ui::init(rvk &renderData) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// std::shared_ptr<ImGuiContext> x=std::make_shared<ImGuiContext>(ImGui::CreateContext());

	ImGuiIO &io = ImGui::GetIO();

	io.Fonts->AddFontFromFileTTF("resources/comicbd.ttf", 29.0f);
	io.Fonts->AddFontFromFileTTF("resources/bruce.ttf", 52.0f);

	std::array<VkDescriptorPoolSize,11> imguiPoolSizes{{{VK_DESCRIPTOR_TYPE_SAMPLER, 24},
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
	                           &renderData.dpools[rvk::idximguipool])) {
		return false;
	}
	ImGui_ImplSDL3_InitForVulkan(renderData.wind);

	ImGui_ImplVulkan_InitInfo imguiIinitInfo{};
	imguiIinitInfo.Instance = renderData.inst.instance;
	imguiIinitInfo.PhysicalDevice = renderData.physdev.physical_device;
	imguiIinitInfo.Device = renderData.vkdevice.device;
	imguiIinitInfo.Queue = renderData.graphicsQ;
	imguiIinitInfo.DescriptorPool = renderData.dpools[rvk::idximguipool];
	imguiIinitInfo.MinImageCount = 2;
	imguiIinitInfo.ImageCount = renderData.schainimgs.size();
	imguiIinitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	imguiIinitInfo.RenderPass = renderData.rdrenderpass;

	// imguiIinitInfo.UseDynamicRendering = true;

	ImGui_ImplVulkan_Init(&imguiIinitInfo);


	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_ProcessEvent(renderData.e);

	/* init plot vectors */
	mfpsvalues.reserve(mnumfpsvalues);
	mframetimevalues.reserve(mnumframetimevalues);
	mmodeluploadvalues.reserve(mnummodeluploadvalues);
	mmatrixgenvalues.reserve(mnummatrixgenvalues);
	mikvalues.reserve(mnumikvalues);
	mmatrixuploadvalues.reserve(mnummatrixuploadvalues);
	muigenvalues.reserve(mnumuigenvalues);
	mmuidrawvalues.reserve(mnummuidrawvalues);
	mfpsvalues.resize(mnumfpsvalues);
	mframetimevalues.resize(mnumframetimevalues);
	mmodeluploadvalues.resize(mnummodeluploadvalues);
	mmatrixgenvalues.resize(mnummatrixgenvalues);
	mikvalues.resize(mnumikvalues);
	mmatrixuploadvalues.resize(mnummatrixuploadvalues);
	muigenvalues.resize(mnumuigenvalues);
	mmuidrawvalues.resize(mnummuidrawvalues);

	return true;
}

void ui::createdbgframe(rvk &renderData, selection &settings) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGui_ImplSDL3_ProcessEvent(renderData.e);

	{
		ImGuiWindowFlags imguiWindowFlags = 0;

		ImGui::SetNextWindowBgAlpha(0.8f);

		ImGui::Begin("debug", nullptr, imguiWindowFlags);

		static float newFps = 0.0f;
		/* avoid inf values (division by zero) */
		if (renderData.frametime > 0.0) {
			newFps = 1.0f / renderData.frametime * 1000.f;
		}
		/* make an averge value to avoid jumps */
		// mfps = (mavgalpha * mfps) + (1.0f - mavgalpha) * newFps;
		mfps = newFps;

		/* clamp manual input on all sliders to min/max */
		// ImGuiSliderFlags flags = ImGuiSliderFlags_ClampOnInput;
		ImGuiSliderFlags flags = ImGuiSliderFlags_None;

		static double updateTime = 0.0;

		/* avoid double compares */
		if (updateTime < 0.000001) {
			updateTime = ImGui::GetTime();
		}

		static int fpsOffset = 0;
		static int frameTimeOffset = 0;
		static int modelUploadOffset = 0;
		static int matrixGenOffset = 0;
		static int ikOffset = 0;
		static int matrixUploadOffset = 0;
		static int uiGenOffset = 0;
		static int uiDrawOffset = 0;

		if (updateTime < ImGui::GetTime()) {
			mfpsvalues.at(fpsOffset) = mfps;
			fpsOffset = ++fpsOffset % mnumfpsvalues;

			mframetimevalues.at(frameTimeOffset) = renderData.frametime;
			frameTimeOffset = ++frameTimeOffset % mnumframetimevalues;

			mmodeluploadvalues.at(modelUploadOffset) = renderData.uploadubossbotime;
			modelUploadOffset = ++modelUploadOffset % mnummodeluploadvalues;

			mmatrixgenvalues.at(matrixGenOffset) = renderData.updateanimtime;
			matrixGenOffset = ++matrixGenOffset % mnummatrixgenvalues;

			mikvalues.at(ikOffset) = renderData.updatemattime;
			ikOffset = ++ikOffset % mnumikvalues;

			mmatrixuploadvalues.at(matrixUploadOffset) = renderData.iksolvetime;
			matrixUploadOffset = ++matrixUploadOffset % mnummatrixuploadvalues;

			muigenvalues.at(uiGenOffset) = renderData.rduigeneratetime;
			uiGenOffset = ++uiGenOffset % mnumuigenvalues;

			mmuidrawvalues.at(uiDrawOffset) = renderData.rduidrawtime;
			uiDrawOffset = ++uiDrawOffset % mnummuidrawvalues;

			updateTime += 1.0 / 30.0;
		}

		ImGui::BeginGroup();
		ImGui::Text("FPS:");
		ImGui::SameLine();
		ImGui::Text("%s", std::to_string(1.0f / renderData.frametime).c_str());
		ImGui::EndGroup();

		if (ImGui::CollapsingHeader("General")) {
			ImGui::Text("Triangles:");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.tricount + renderData.gltftricount).c_str());

			std::string windowDims = std::to_string(renderData.width) + "x" + std::to_string(renderData.height);
			ImGui::Text("Window Dimensions:");
			ImGui::SameLine();
			ImGui::Text("%s", windowDims.c_str());

			std::string imgWindowPos = std::to_string(static_cast<int>(ImGui::GetWindowPos().x)) + "/" +
			                           std::to_string(static_cast<int>(ImGui::GetWindowPos().y));
			ImGui::Text("ImGui Window Position:");
			ImGui::SameLine();
			ImGui::Text("%s", imgWindowPos.c_str());
		}

		if (ImGui::CollapsingHeader("Timers")) {
			ImGui::BeginGroup();
			ImGui::Text("Frame Time:");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.frametime).c_str());
			ImGui::SameLine();
			ImGui::Text("ms");
			ImGui::EndGroup();

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				float averageFrameTime = 0.0f;
				for (const auto value : mframetimevalues) {
					averageFrameTime += value;
				}
				averageFrameTime /= static_cast<float>(mnummatrixgenvalues);
				std::string frameTimeOverlay = "now:     " + std::to_string(renderData.frametime) +
				                               " ms\n30s avg: " + std::to_string(averageFrameTime) + " ms";
				ImGui::Text("Frame Time       ");
				ImGui::SameLine();
				ImGui::PlotLines("##FrameTime", mframetimevalues.data(), mframetimevalues.size(), frameTimeOffset,
				                 frameTimeOverlay.c_str(), 0.0f, FLT_MAX, ImVec2(0, 80));
				ImGui::EndTooltip();
			}

			ImGui::BeginGroup();
			ImGui::Text("Upload Time:");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.uploadubossbotime).c_str());
			ImGui::SameLine();
			ImGui::Text("ms");
			ImGui::EndGroup();

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				float averageModelUpload = 0.0f;
				for (const auto value : mmodeluploadvalues) {
					averageModelUpload += value;
				}
				averageModelUpload /= static_cast<float>(mnummodeluploadvalues);
				std::string modelUploadOverlay = "now:     " + std::to_string(renderData.uploadubossbotime) +
				                                 " ms\n30s avg: " + std::to_string(averageModelUpload) + " ms";
				ImGui::Text("VBO Upload");
				ImGui::SameLine();
				ImGui::PlotLines("##ModelUploadTimes", mmodeluploadvalues.data(), mmodeluploadvalues.size(), modelUploadOffset,
				                 modelUploadOverlay.c_str(), 0.0f, FLT_MAX, ImVec2(0, 80));
				ImGui::EndTooltip();
			}

			ImGui::BeginGroup();
			ImGui::Text("Update Animations Time:");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.updateanimtime).c_str());
			ImGui::SameLine();
			ImGui::Text("ms");
			ImGui::EndGroup();

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				float averageMatGen = 0.0f;
				for (const auto value : mmatrixgenvalues) {
					averageMatGen += value;
				}
				averageMatGen /= static_cast<float>(mnummatrixgenvalues);
				std::string matrixGenOverlay = "now:     " + std::to_string(renderData.updateanimtime) +
				                               " ms\n30s avg: " + std::to_string(averageMatGen) + " ms";
				ImGui::Text("Matrix Generation");
				ImGui::SameLine();
				ImGui::PlotLines("##MatrixGenTimes", mmatrixgenvalues.data(), mmatrixgenvalues.size(), matrixGenOffset,
				                 matrixGenOverlay.c_str(), 0.0f, FLT_MAX, ImVec2(0, 80));
				ImGui::EndTooltip();
			}

			ImGui::BeginGroup();
			ImGui::Text("Update TRS Mats :");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.updatemattime).c_str());
			ImGui::SameLine();
			ImGui::Text("ms");
			ImGui::EndGroup();

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				float averageIKTime = 0.0f;
				for (const auto value : mikvalues) {
					averageIKTime += value;
				}
				averageIKTime /= static_cast<float>(mnumikvalues);
				std::string ikOverlay = "now:     " + std::to_string(renderData.updatemattime) +
				                        " ms\n30s avg: " + std::to_string(averageIKTime) + " ms";
				ImGui::Text("(IK Generation)");
				ImGui::SameLine();
				ImGui::PlotLines("##IKTimes", mikvalues.data(), mikvalues.size(), ikOffset, ikOverlay.c_str(), 0.0f, FLT_MAX,
				                 ImVec2(0, 80));
				ImGui::EndTooltip();
			}

			ImGui::BeginGroup();
			ImGui::Text("Matrix Upload Time:");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.iksolvetime).c_str());
			ImGui::SameLine();
			ImGui::Text("ms");
			ImGui::EndGroup();

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				float averageMatrixUpload = 0.0f;
				for (const auto value : mmatrixuploadvalues) {
					averageMatrixUpload += value;
				}
				averageMatrixUpload /= static_cast<float>(mnummatrixuploadvalues);
				std::string matrixUploadOverlay = "now:     " + std::to_string(renderData.uploadubossbotime) +
				                                  " ms\n30s avg: " + std::to_string(averageMatrixUpload) + " ms";
				ImGui::Text("UBO Upload");
				ImGui::SameLine();
				ImGui::PlotLines("##MatrixUploadTimes", mmatrixuploadvalues.data(), mmatrixuploadvalues.size(),
				                 matrixUploadOffset, matrixUploadOverlay.c_str(), 0.0f, FLT_MAX, ImVec2(0, 80));
				ImGui::EndTooltip();
			}

			ImGui::BeginGroup();
			ImGui::Text("UI Generation Time:");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.rduigeneratetime).c_str());
			ImGui::SameLine();
			ImGui::Text("ms");
			ImGui::EndGroup();

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				float averageUiGen = 0.0f;
				for (const auto value : muigenvalues) {
					averageUiGen += value;
				}
				averageUiGen /= static_cast<float>(mnumuigenvalues);
				std::string uiGenOverlay = "now:     " + std::to_string(renderData.rduigeneratetime) +
				                           " ms\n30s avg: " + std::to_string(averageUiGen) + " ms";
				ImGui::Text("UI Generation");
				ImGui::SameLine();
				ImGui::PlotLines("##ModelUpload", muigenvalues.data(), muigenvalues.size(), uiGenOffset, uiGenOverlay.c_str(),
				                 0.0f, FLT_MAX, ImVec2(0, 80));
				ImGui::EndTooltip();
			}

			ImGui::BeginGroup();
			ImGui::Text("UI Draw Time:");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.rduidrawtime).c_str());
			ImGui::SameLine();
			ImGui::Text("ms");
			ImGui::EndGroup();

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				float averageUiDraw = 0.0f;
				for (const auto value : mmuidrawvalues) {
					averageUiDraw += value;
				}
				averageUiDraw /= static_cast<float>(mnummuidrawvalues);
				std::string uiDrawOverlay = "now:     " + std::to_string(renderData.rduidrawtime) +
				                            " ms\n30s avg: " + std::to_string(averageUiDraw) + " ms";
				ImGui::Text("UI Draw");
				ImGui::SameLine();
				ImGui::PlotLines("##UIDrawTimes", mmuidrawvalues.data(), mmuidrawvalues.size(), uiGenOffset,
				                 uiDrawOverlay.c_str(), 0.0f, FLT_MAX, ImVec2(0, 80));
				ImGui::EndTooltip();
			}
		}

		if (ImGui::CollapsingHeader("Camera")) {
			ImGui::Text("Camera Position:");
			ImGui::SameLine();
			ImGui::Text("%s", glm::to_string(renderData.camwpos).c_str());

			ImGui::Text("View Azimuth:");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.azimuth).c_str());

			ImGui::Text("View Elevation:");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.elevation).c_str());

			ImGui::Text("Field of View");
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(renderData.fov).c_str());
			// ImGui::SliderFloat("##FOV", &renderData.rdfov, 40.0f, 150.0f, "%f", flags);
		}

		if (ImGui::CollapsingHeader("models")) {
			ImGui::Text("model count  : %d", settings.instancesettings.size());

			ImGui::Text("selected model :");
			ImGui::SameLine();
			ImGui::PushButtonRepeat(true);
			if (ImGui::ArrowButton("##LEFTMOD", ImGuiDir_Left) && settings.midx > 0) {
				settings.midx--;
			}
			ImGui::SameLine();
			ImGui::PushItemWidth(30);
			ImGui::DragScalar("##SELMOD", ImGuiDataType_U64, &settings.midx, flags);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::ArrowButton("##RIGHTMOD", ImGuiDir_Right) && settings.midx < settings.instancesettings.size() - 1) {
				settings.midx++;
			}
			ImGui::PopButtonRepeat();

			ImGui::Text("instance count  : %d", settings.instancesettings.at(settings.midx).size());

			ImGui::Text("selected instance :");
			ImGui::SameLine();
			ImGui::PushButtonRepeat(true);
			if (ImGui::ArrowButton("##LEFTINST", ImGuiDir_Left) && settings.iidx > 0) {
				settings.iidx--;
			}
			ImGui::SameLine();
			ImGui::PushItemWidth(30);
			ImGui::DragScalar("##SELINST", ImGuiDataType_U64, &settings.iidx, flags);
			// ImGui::DragScalar("##SELINST", &settings.iidx, 1, 0,
			//     renderData.rdnumberofinstances - 1, "%3d", flags);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::ArrowButton("##RIGHTINST", ImGuiDir_Right) &&
			        settings.iidx < settings.instancesettings.at(settings.midx).size() - 1) {
				settings.iidx++;
			}
			ImGui::PopButtonRepeat();

			ImGui::Text("scale :");
			ImGui::SameLine();
			ImGui::SliderFloat3("##WORLDSCALE",
			                    glm::value_ptr(settings.instancesettings.at(settings.midx).at(settings.iidx)->msworldscale),
			                    0.0f, 1000.0f, "%.1f", flags);

			ImGui::Text("position (x,z)  :");
			ImGui::SameLine();
			ImGui::SliderFloat3("##WORLDPOS",
			                    glm::value_ptr(settings.instancesettings.at(settings.midx).at(settings.iidx)->msworldpos),
			                    -75.0f, 75.0f, "%.1f", flags);

			ImGui::Text("rotation - yaw   :");
			ImGui::SameLine();
			ImGui::SliderFloat("##WORLDROT", &settings.instancesettings.at(settings.midx).at(settings.iidx)->msworldrot.y,
			                   -180.0f, 180.0f, "%.0f", flags);
		}

		if (ImGui::CollapsingHeader("model")) {
			ImGui::Checkbox("draw", &settings.instancesettings.at(settings.midx).at(settings.iidx)->msdrawmodel);
			// ImGui::Checkbox("Draw Skeleton", &settings.msdrawskeleton);

			ImGui::Text("skinning:");
			ImGui::SameLine();
			if (ImGui::RadioButton("linear",
			                       settings.instancesettings.at(settings.midx).at(settings.iidx)->mvertexskinningmode ==
			                       skinningmode::linear)) {
				settings.instancesettings.at(settings.midx).at(settings.iidx)->mvertexskinningmode = skinningmode::linear;
			}
			// ImGui::SameLine();
			// if (ImGui::RadioButton("dual quats",
			//     settings.mvertexskinningmode == skinningmode::dualquat)) {
			//     settings.mvertexskinningmode = skinningmode::dualquat;
			// }
		}

		if (ImGui::CollapsingHeader("animation")) {
			ImGui::Checkbox("play", &settings.instancesettings.at(settings.midx).at(settings.iidx)->msplayanimation);

			if (!settings.instancesettings.at(settings.midx).at(settings.iidx)->msplayanimation) {
				ImGui::BeginDisabled();
			}

			ImGui::Text("playback direction:");
			ImGui::SameLine();
			if (ImGui::RadioButton("forward",
			                       settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimationplaydirection ==
			                       replaydirection::forward)) {
				settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimationplaydirection =
				                             replaydirection::forward;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("backward",
			                       settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimationplaydirection ==
			                       replaydirection::backward)) {
				settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimationplaydirection =
				                             replaydirection::backward;
			}

			if (!settings.instancesettings.at(settings.midx).at(settings.iidx)->msplayanimation) {
				ImGui::EndDisabled();
			}

			ImGui::Text("clip   ");
			ImGui::SameLine();
			if (ImGui::BeginCombo(
			            "##ClipCombo",
			            settings.instancesettings.at(settings.midx)
			            .at(settings.iidx)
			            ->msclipnames.at(settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimclip)
			            .c_str())) {
				for (size_t i {0}; i < settings.instancesettings.at(settings.midx).at(settings.iidx)->msclipnames.size(); ++i) {
					const bool isSelected = (settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimclip == i);
					if (ImGui::Selectable(
					            settings.instancesettings.at(settings.midx).at(settings.iidx)->msclipnames.at(i).c_str(),
					            isSelected)) {
						settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimclip = i;
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (settings.instancesettings.at(settings.midx).at(settings.iidx)->msplayanimation) {
				ImGui::Text("speed  ");
				ImGui::SameLine();
				ImGui::SliderFloat("##ClipSpeed", &settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimspeed,
				                   0.0f, 2.0f, "%.3f", flags);
			} else {
				ImGui::Text("timepos");
				ImGui::SameLine();
				ImGui::SliderFloat("##ClipPos", &settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimtimepos,
				                   0.0f, settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimendtime, "%.3f",
				                   flags);
			}
		}

		if (ImGui::CollapsingHeader("blending")) {
			ImGui::Text("blend type:");
			ImGui::SameLine();
			if (ImGui::RadioButton("Fade In/Out",
			                       settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode ==
			                       blendmode::fadeinout)) {
				settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode = blendmode::fadeinout;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("crossfading",
			                       settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode ==
			                       blendmode::crossfade)) {
				settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode = blendmode::crossfade;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("additive",
			                       settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode ==
			                       blendmode::additive)) {
				settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode = blendmode::additive;
			}

			if (settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode == blendmode::fadeinout) {
				ImGui::Text("factor");
				ImGui::SameLine();
				ImGui::SliderFloat("##BlendFactor",
				                   &settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimblendfactor, 0.0f,
				                   1.0f, "%.3f", flags);
			}

			if (settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode == blendmode::crossfade ||
			        settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode == blendmode::additive) {
				ImGui::Text("Dest Clip   ");
				ImGui::SameLine();
				if (ImGui::BeginCombo(
				            "##DestClipCombo",
				            settings.instancesettings.at(settings.midx)
				            .at(settings.iidx)
				            ->msclipnames
				            .at(settings.instancesettings.at(settings.midx).at(settings.iidx)->mscrossblenddestanimclip)
				            .c_str())) {
					for (size_t i {0}; i < settings.instancesettings.at(settings.midx).at(settings.iidx)->msclipnames.size(); ++i) {
						const bool isSelected =
						    (settings.instancesettings.at(settings.midx).at(settings.iidx)->mscrossblenddestanimclip == i);
						if (ImGui::Selectable(
						            settings.instancesettings.at(settings.midx).at(settings.iidx)->msclipnames.at(i).c_str(),
						            isSelected)) {
							settings.instancesettings.at(settings.midx).at(settings.iidx)->mscrossblenddestanimclip = i;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				ImGui::Text("Cross Blend ");
				ImGui::SameLine();
				ImGui::SliderFloat("##CrossBlendFactor",
				                   &settings.instancesettings.at(settings.midx).at(settings.iidx)->msanimcrossblendfactor, 0.0f,
				                   1.0f, "%.3f", flags);
			}

			if (settings.instancesettings.at(settings.midx).at(settings.iidx)->msblendingmode == blendmode::additive) {
				ImGui::Text("Split Node  ");
				ImGui::SameLine();
				if (ImGui::BeginCombo(
				            "##SplitNodeCombo",
				            settings.instancesettings.at(settings.midx)
				            .at(settings.iidx)
				            ->msskelnodenames.at(settings.instancesettings.at(settings.midx).at(settings.iidx)->msskelsplitnode)
				            .c_str())) {
					for (size_t i {0}; i < settings.instancesettings.at(settings.midx).at(settings.iidx)->msskelnodenames.size();
					        ++i) {
						if (settings.instancesettings.at(settings.midx)
						        .at(settings.iidx)
						        ->msskelnodenames.at(i)
						        .compare("(invalid)") != 0) {
							const bool isSelected =
							    (settings.instancesettings.at(settings.midx).at(settings.iidx)->msskelsplitnode == i);
							if (ImGui::Selectable(
							            settings.instancesettings.at(settings.midx).at(settings.iidx)->msskelnodenames.at(i).c_str(),
							            isSelected)) {
								settings.instancesettings.at(settings.midx).at(settings.iidx)->msskelsplitnode = i;
							}
							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
					}
					ImGui::EndCombo();
				}
			}
		}

		if (ImGui::CollapsingHeader("inverse kinematics")) {
			if (ImGui::RadioButton("Off",
			                       settings.instancesettings.at(settings.midx).at(settings.iidx)->msikmode == ikmode::off)) {
				settings.instancesettings.at(settings.midx).at(settings.iidx)->msikmode = ikmode::off;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("CCD",
			                       settings.instancesettings.at(settings.midx).at(settings.iidx)->msikmode == ikmode::ccd)) {
				settings.instancesettings.at(settings.midx).at(settings.iidx)->msikmode = ikmode::ccd;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("FABRIK", settings.instancesettings.at(settings.midx).at(settings.iidx)->msikmode ==
			                       ikmode::fabrik)) {
				settings.instancesettings.at(settings.midx).at(settings.iidx)->msikmode = ikmode::fabrik;
			}

			if (settings.instancesettings.at(settings.midx).at(settings.iidx)->msikmode == ikmode::ccd ||
			        settings.instancesettings.at(settings.midx).at(settings.iidx)->msikmode == ikmode::fabrik) {
				ImGui::Text("IK Iterations  :");
				ImGui::SameLine();
				ImGui::SliderInt("##IKITER", &settings.instancesettings.at(settings.midx).at(settings.iidx)->msikiterations, 0,
				                 15, "%d", flags);

				ImGui::Text("Target Position:");
				ImGui::SameLine();
				ImGui::SliderFloat3(
				    "##IKTargetPOS",
				    glm::value_ptr(settings.instancesettings.at(settings.midx).at(settings.iidx)->msiktargetpos), -10.0f, 10.0f,
				    "%.3f", flags);
				ImGui::Text("Effector Node  :");
				ImGui::SameLine();
				if (ImGui::BeginCombo("##EffectorNodeCombo",
				                      settings.instancesettings.at(settings.midx)
				                      .at(settings.iidx)
				                      ->msskelnodenames
				                      .at(settings.instancesettings.at(settings.midx).at(settings.iidx)->msikeffectornode)
				                      .c_str())) {
					for (size_t i {0}; i < settings.instancesettings.at(settings.midx).at(settings.iidx)->msskelnodenames.size();
					        ++i) {
						if (settings.instancesettings.at(settings.midx)
						        .at(settings.iidx)
						        ->msskelnodenames.at(i)
						        .compare("(invalid)") != 0) {
							const bool isSelected =
							    (settings.instancesettings.at(settings.midx).at(settings.iidx)->msikeffectornode == i);
							if (ImGui::Selectable(
							            settings.instancesettings.at(settings.midx).at(settings.iidx)->msskelnodenames.at(i).c_str(),
							            isSelected)) {
								settings.instancesettings.at(settings.midx).at(settings.iidx)->msikeffectornode = i;
							}

							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
					}
					ImGui::EndCombo();
				}
				ImGui::Text("IK Root Node   :");
				ImGui::SameLine();
				if (ImGui::BeginCombo(
				            "##RootNodeCombo",
				            settings.instancesettings.at(settings.midx)
				            .at(settings.iidx)
				            ->msskelnodenames.at(settings.instancesettings.at(settings.midx).at(settings.iidx)->msikrootnode)
				            .c_str())) {
					for (size_t i{0}; i < settings.instancesettings.at(settings.midx).at(settings.iidx)->msskelnodenames.size();
					        ++i) {
						if (settings.instancesettings.at(settings.midx)
						        .at(settings.iidx)
						        ->msskelnodenames.at(i)
						        .compare("(invalid)") != 0) {
							const bool isSelected =
							    (settings.instancesettings.at(settings.midx).at(settings.iidx)->msikrootnode == i);
							if (ImGui::Selectable(
							            settings.instancesettings.at(settings.midx).at(settings.iidx)->msskelnodenames.at(i).c_str(),
							            isSelected)) {
								settings.instancesettings.at(settings.midx).at(settings.iidx)->msikrootnode = i;
							}

							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
					}
					ImGui::EndCombo();
				}
			}
		}

		ImGui::End();
	}
}

bool ui::createloadingscreen(rvk &mvkobjs) {

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGuiIO &io = ImGui::GetIO();

	ImGui_ImplSDL3_ProcessEvent(mvkobjs.e);

	ImGuiWindowFlags imguiWindowFlags = 0;
	imguiWindowFlags |= ImGuiWindowFlags_NoBackground;
	imguiWindowFlags |= ImGuiWindowFlags_NoResize;
	imguiWindowFlags |= ImGuiWindowFlags_NoMove;
	imguiWindowFlags |= ImGuiWindowFlags_NoSavedSettings;
	imguiWindowFlags |= ImGuiWindowFlags_NoCollapse;
	imguiWindowFlags |= ImGuiWindowFlags_NoTitleBar;

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), 1, {0.5f, 0.5f});

	ImGui::Begin("loading", nullptr, imguiWindowFlags);

	// ImGui::ProgressBar(static_cast<float>(std::rand()%100)/100.0f, {600,100});
	ImGui::ProgressBar(mvkobjs.loadingprog, {600, 100});

	ImGui::End();

	return true;
}

bool ui::createpausebuttons(rvk &mvkobjs) {

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGuiIO &io = ImGui::GetIO();

	ImGui_ImplSDL3_ProcessEvent(mvkobjs.e);

	ImGuiWindowFlags imguiWindowFlags = 0;
	imguiWindowFlags |= ImGuiWindowFlags_MenuBar;
	imguiWindowFlags |= ImGuiWindowFlags_NoBackground;
	imguiWindowFlags |= ImGuiWindowFlags_NoResize;
	imguiWindowFlags |= ImGuiWindowFlags_NoMove;
	imguiWindowFlags |= ImGuiWindowFlags_NoSavedSettings;
	imguiWindowFlags |= ImGuiWindowFlags_NoCollapse;
	imguiWindowFlags |= ImGuiWindowFlags_NoTitleBar;
	imguiWindowFlags |= ImGuiWindowFlags_AlwaysAutoResize;

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), 1, {0.5f, 0.5f});

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
		*mvkobjs.mshutdown = true;
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

void ui::render(rvk &renderData, VkCommandBuffer &cbuffer) {
	ImGui::Render();

	// renderData.mtx2.lock();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cbuffer);
	// renderData.mtx2.unlock();
}

void ui::cleanup(rvk &mvkobjs) {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void ui::backspace() {
	if (!inputxt.empty())
		inputxt.pop_back();
}
