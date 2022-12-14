#include "Editor.hpp"

#ifndef NEDITOR

// this code mostly copied from ZargoEngine's GUI class

namespace Editor
{
	constexpr ImVec4 HEADER_COLOR = { .81f, .6f, 0, 1 };

	void GUI::Header(const char* title)
	{
		static const ImVec4 Orange{ 1.0f, 0.68f ,0.0f ,1.0f };
		ImGui::GetFont()->FontSize += 3;
		ImGui::TextColored(Orange, title);
		ImGui::GetFont()->FontSize -= 3;
	}

	// void GUI::TextureField(const char* name, Texture*& texture)
	// {
	// 	ImGui::Text(name);
	// 	ImGui::SameLine();
	// 	ImGui::Image((void*)texture->resourceView, { filesize, filesize }, { 0, 1 }, { 1, 0 });
	// 
	// 	if (ImGui::BeginDragDropTarget())
	// 	{
	// 		if (const ImGuiPayload* payloadPtr = ImGui::AcceptDragDropPayload("TEXTURE"))
	// 		{
	// 			if (payloadPtr->Data)
	// 			{
	// 				AssetManager::TryGetTexture(texture, (const char*)payloadPtr->Data);
	// 			}
	// 		}
	// 	}
	// }

	// Usage:
	// static int value = 0;
	// const char* names[3]{ "enum0", "enum1", "enum2" };
	// GUI::EnumField(value, names, 3, "TestEnum", NULL, 0);
	/// <summary> note this is not usefull for bitfields | flags </summary> 
	/// <param name="count"> number of names </param>
	bool GUI::EnumField(int& value, const char** names, const int& count, const char* label,
		const Action& onSellect, const ImGuiComboFlags& flags)
	{
		if (ImGui::BeginCombo(label, names[value], flags))
		{
			for (char i = 0; i < count; i++)
			{
				if (ImGui::Selectable(names[i], i == value))
				{
					value = i;
					if (onSellect) onSellect();
					else { ImGui::EndCombo(); return true; }
				}
			}
			ImGui::EndCombo();
		}
	}


	void GUI::RightClickPopUp(const char* name, const TitleAndAction* menuItems, const int& count)
	{
		if (ImGui::BeginPopupContextWindow(name))
		{
			for (char i = 0; i < count; i++)
			{
				if (ImGui::MenuItem(menuItems[i].title))
				{
					menuItems[i].action();
				}
			}
			ImGui::EndPopup();
		}
	}

	bool GUI::DragUIElementString(const char* file, const char* type, void* texture)
	{
		if (ImGui::IsItemFocused() && ImGui::IsAnyMouseDown() && ImGui::BeginDragDropSource())
		{
			// std::cout << "drag type: " << type << std::endl;
			ImGui::SetDragDropPayload(type, file, strlen(file) + 1);
			if (texture != nullptr)
			{
				ImGui::Image((void*)texture, { filesize, filesize }, { 0, 1 }, { 1, 0 });
			}
			ImGui::EndDragDropSource();
			return true;
		}
		return false;
	}

	void GUI::DropUIElementString(const char* type, const FileCallback& callback)
	{
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payloadPtr = ImGui::AcceptDragDropPayload(type))
			{
				if (payloadPtr->Data)
				{
					callback((char*)payloadPtr->Data);
				}
			}
		}
	}

	template<typename T>
	inline bool GUI::DragUIElement(const T* file, const char* type, void* texture)
	{
		if (ImGui::IsItemFocused() && ImGui::IsAnyMouseDown() && ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(type, file, sizeof(T));
			// if (texture != nullptr) {
			// 	ImGui::Image((void*)texture, { filesize, filesize }, { 0, 1 }, { 1, 0 });
			// }
			ImGui::EndDragDropSource();
			return true;
		}
		return false;
	}

	template<typename T>
	inline void GUI::DropUIElement(const char* type, const void(*callback)(T))
	{
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payloadPtr = ImGui::AcceptDragDropPayload(type))
			{
				if (payloadPtr->Data)
				{
					callback((char*)payloadPtr->Data);
					AXLOG("dropped data type of: %s", typeid(T).name);
				}
			}
		}
	}
}

#endif