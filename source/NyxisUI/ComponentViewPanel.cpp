#include "NyxisUI/ComponentViewPanel.hpp"
#include "EditorLayer.hpp"
#include "MaterialEditorPanel.hpp"
#include "Core/Application.hpp"
#include "Graphics/GLTFModel.hpp"
#include "Scene/Components.hpp"
#include "NyxisUI/Widgets.hpp"
#include "Scene/NyxisProject.hpp"

namespace Nyxis
{
	template<typename T>
	void AddComponentComboItem(const char* name, Ref<Scene> scene, Entity entity)
	{
		if (!scene->m_Registry.all_of<T>(entity))
		{
			if (ImGui::MenuItem(name))
			{
				scene->AddComponent<T>(entity);
				ImGui::CloseCurrentPopup();
			}
		}
	}

	void ComponentViewPanel::OnUpdate()
	{
		auto scene = Application::GetScene();
		auto selectedEntity = EditorLayer::GetSelectedEntity();

		ImGui::PushStyleColor(ImGuiCol_Separator, { 124, 124, 124, 1 });
		ImGui::Begin("Component", nullptr, ImGuiWindowFlags_NoScrollbar);
		// Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		if (scene->m_Registry.valid(selectedEntity))
		{
			if (scene->m_Registry.all_of<TagComponent>(selectedEntity))
			{
				auto& tag = scene->GetComponent<TagComponent>(selectedEntity);
				char buffer[256] = {};
				strcpy(buffer, tag.Tag.c_str());
				if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
				{
					tag.Tag = std::string(buffer);
				}
			}

			const ImVec4 clear_color = { 0,0,0,0 };
			ImGui::PushStyleColor(ImGuiCol_Button, clear_color);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, clear_color);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, clear_color);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, clear_color);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, clear_color);

			DrawComponentNode<TransformComponent>("Transform", selectedEntity, [&](TransformComponent& transform)
				{
					NyxisWidgets::TableVec3({ "Translation", "Rotation", "Scale", "Velocity", "Acceleration" },
						{ &transform.translation, &transform.rotation, &transform.scale, &transform.velocity, &transform.acceleration });
				});

			DrawComponentNode<RigidBody>("RigidBody", selectedEntity, [&](RigidBody& rigidBody)
				{
					NyxisWidgets::TableFloat({ "Mass", "Restitution", "Friction" },
						{ &rigidBody.mass, &rigidBody.restitution, &rigidBody.friction });
			ImGui::Checkbox("Kinematic", &rigidBody.isKinematic);
			ImGui::SameLine();
			ImGui::Checkbox("Static", &rigidBody.isStatic);
				});

			DrawComponentNode<Model>("Model", selectedEntity, [&](Model& model)
				{
					// list of all models in project models directory
					static const std::string ext = ".gltf";
					const std::string assetspath = Application::GetProject()->GetAssetPath();
					const std::string path = "/models/";

					if (ImGui::BeginCombo("Models", model.path.c_str()))
					{
						std::vector<std::string> models{};
						for (const auto& entry : std::filesystem::recursive_directory_iterator(assetspath + path))
						{
							if (entry.path().extension() == ext)
								models.push_back(entry.path().string().substr(assetspath.size() + path.size()));
						}
						static int current_item = -1;
						for (int n = 0; n < models.size(); n++)
						{
							const bool is_selected = (current_item == n);
							if (ImGui::Selectable(models[n].data(), is_selected))
								current_item = n;
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
						if (current_item != -1)
						{
							EditorLayer::DeselectMaterial();
							EditorLayer::DeselectEntity();
							scene->LoadModel(selectedEntity, path + models[current_item]);
							current_item = -1;
						}
					}

					if (model.materials.size() == 0)
						return;

					// combo selection of materials

					static int current_item = 0;
					Material* selected_material = &model.materials[current_item];
					const char* preview = selected_material->name.c_str();
					if(ImGui::BeginCombo("Materials", preview))
					{
						for (int n = 0; n < model.materials.size(); n++)
						{
							const bool is_selected = (current_item == n);
							if (ImGui::Selectable(model.materials[n].name.c_str(), is_selected))
							{
								current_item = n;
								selected_material = &model.materials[current_item];
								EditorLayer::SetSelectedMaterial(selected_material);
							}
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					constexpr static auto table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
					constexpr static auto column_flags = ImGuiTableColumnFlags_WidthFixed;

					ImGui::BeginTable("Materials", 2, table_flags);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

					ImGui::TableNextRow();
					auto padding = ImGui::GetStyle().FramePadding.x;
					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-FLT_MIN - 100 - padding); // Right-aligned

					auto lambda = [&](const char* name, ModelTexture* texture)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text(name);
						ImGui::TableSetColumnIndex(1);
						EditorLayer::ImageButton(MaterialEditorPanel::materialDescriptorSets, texture);
					};

					lambda("Base Color", selected_material->baseColorTexture);
					lambda("Normal Texture", selected_material->normalTexture);
					lambda("Metallic Texture", selected_material->metallicRoughnessTexture);
					lambda("Occlusion Texture", selected_material->occlusionTexture);
					lambda("Emissive Texture", selected_material->emissiveTexture);

					ImGui::EndTable();

				});

			DrawComponentNode<Collider>("Collider", selectedEntity, [&](Collider& collider)
				{
					auto preview = collider_name[collider.type];

					if (ImGui::BeginCombo("Collider Type", preview.c_str(), ImGuiComboFlags_NoArrowButton))
					{
						if (ImGui::Selectable("Box"))
							collider.type = ColliderType::Box;

						if (ImGui::Selectable("Sphere"))
							collider.type = ColliderType::Sphere;

						ImGui::EndCombo();
					}

					if (collider.type == ColliderType::Box)
						ImGui::DragFloat3("Collider Size", &collider.size.x, 0.05f);

					else if (collider.type == ColliderType::Sphere)
						ImGui::DragFloat("Collider Radius", &collider.radius, 0.05f);
				});
			ImGui::PopStyleColor(5);
			const float width = ImGui::GetContentRegionAvail().x;

			ImGui::SetNextItemWidth(width);
			if (ImGui::BeginCombo("##", "Add Component", ImGuiComboFlags_NoArrowButton))
			{
				AddComponentComboItem<TagComponent>("Tag Component", scene, selectedEntity);
				AddComponentComboItem<TransformComponent>("Transform Component", scene, selectedEntity);
				AddComponentComboItem<RigidBody>("Rigid Body", scene, selectedEntity);
				AddComponentComboItem<Model>("Model", scene, selectedEntity);
				ImGui::EndCombo();
			}
		}
		ImGui::End();
		ImGui::PopStyleColor(1);
	}

	template <typename T>
	void ComponentViewPanel::DrawComponentNode(const char* name, Entity entity, std::function<void(T& component)> func)
	{
		constexpr ImGuiBackendFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
		auto scene = Application::GetScene();

		if (scene->m_Registry.all_of<T>(entity))
		{
			const auto style = ImGui::GetStyle();
			const auto padding = style.FramePadding.x;
			const auto header_color = style.Colors[ImGuiCol_Header];
			auto& component = scene->GetComponent<T>(entity);
			auto lineHeight = NyxisWidgets::LineHeight();

			const auto availableRegion = ImGui::GetContentRegionAvail();
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), flags, name);

			const auto size = ImGui::GetItemRectSize();
			const auto pos = ImGui::GetItemRectMin();

			ImGui::SameLine(availableRegion.x - lineHeight / 2 - padding);
			bool removeComponent = false;
			if (ImGui::Button("...", { lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("RemoveComponent");
			}

			if (ImGui::BeginPopup("RemoveComponent"))
			{
				if (ImGui::MenuItem("Remove"))
				{
					removeComponent = true;
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				func(component);
				const ImVec2 fill = ImGui::GetCursorPos();
				ImGui::TreePop();
				ImGui::GetWindowDrawList()->AddRectFilled({ pos.x, pos.y }, { pos.x + size.x, fill.y + lineHeight }, ImGui::ColorConvertFloat4ToU32(header_color), 5);
				ImGui::Dummy({ size.x, 2 });
			}

			else
			{
				ImGui::GetWindowDrawList()->AddRectFilled(pos, { pos.x + availableRegion.x, pos.y + lineHeight }, ImGui::ColorConvertFloat4ToU32(header_color), 5);
			}
			if (removeComponent)
			{
				scene->RemoveComponent<T>(entity);
				ImGui::CloseCurrentPopup();
			}
		}
	}

}
