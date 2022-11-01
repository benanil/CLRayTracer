#pragma once

struct GLFWwindow;

namespace Editor
{
	void Destroy();

	void Create(GLFWwindow* window);

	// static Event OnEditor;
	// void Editor::AddOnEditor(const Action& action) |

	void Begin();

	void Render(unsigned screenRenderImageGl);

	void DarkTheme();
	void DeepDark();

	void Light();
}
