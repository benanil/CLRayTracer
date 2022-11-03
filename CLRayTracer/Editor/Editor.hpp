#pragma once
#include <imgui.h>

struct GLFWwindow;
typedef void(*Action)();

namespace Editor
{
	void Destroy();

	void Create(GLFWwindow* window);

	void Begin();
	void AddOnEditor(void(*action)());

	void Render(unsigned screenRenderImageGl);

	void DarkTheme();
	void DeepDark();

	void Light();

	constexpr float filesize = 35;
	constexpr float miniSize = 12.5f;

	typedef void(*FileCallback)(const char* ptr);

	struct TitleAndAction
	{
		const char* title;
		Action action;
		TitleAndAction() : title("empty title"), action(nullptr) {}
		TitleAndAction(const char* _title, Action _action) : title(_title), action(_action) {}
	};

	namespace GUI
	{
		void Header(const char* title);
		// void TextureField(const char* name, Texture*& texture);

		// inline bool ImageButton(DXShaderResourceView* texture, const float& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1))
		// {
		// 	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.30f, 0.30f, 0.65f));
		// 	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
		// 	bool clicked = ImGui::ImageButton((void*)texture, { size , size }, uv0, uv1);
		// 	ImGui::PopStyleColor(1);
		// 	ImGui::PopStyleVar(1);
		// 	return clicked;
		// }

		bool EnumField(int& value, const char** names, const int& count, const char* label,
			const Action& onSellect = NULL, const ImGuiComboFlags& flags = 0);

		void RightClickPopUp(const char* name, const TitleAndAction* menuItems, const int& count);

		bool DragUIElementString(const char* file, const char* type, void* texture);
		void DropUIElementString(const char* type, const FileCallback& callback);

		template<typename T>
		bool DragUIElement(const T* file, const char* type, void* texture);
		
		template<typename T>
		void DropUIElement(const char* type, const void(*callback)(T));

		inline bool IconButton(const char* name, const ImVec2& size = ImVec2(0, 0))
		{
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 1));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.16f, 0.18f));

			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.3f);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
			bool clicked = ImGui::Button(name, size);
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(2);

			return clicked;
		}
	}

	namespace ResourcesWindow
	{
		const char* GetCurrentPath();
		void Initialize();
		void DrawWindow();
	}
}
