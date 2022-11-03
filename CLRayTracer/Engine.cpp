#include "Engine.hpp"
#include "Editor/Editor.hpp"
#include "Renderer.hpp"

// things that I want to edit in editor
// edit materials
// edit sun angle
// edit mesh instance positions
//

static void(*EndFrameEvents[20])(void);
static int NumEndFrameEvents = 0;

static void(*ApplicationExitEvents[20])(void);
static int NumApplicationExitEvents = 0;

void Engine_AddEndOfFrameEvent(void(*action)())
{
	EndFrameEvents[NumEndFrameEvents++] = action;
}

void Engine_AddOnAppQuitEvent(void(*action)())
{
	ApplicationExitEvents[NumApplicationExitEvents++] = action;
}

static const char* ProfilerNames[] = {
	"Renderer", "EngineTick"
};

static float ProfilerSpeeds[20] = {0.0f};

void Engine_UpdateProfilerStats(ProfilerStats stats, float ms)
{
	ProfilerSpeeds[stats] = ms;
}
#ifndef IMGUI_DISABLE
static void DisplayProfilerStats()
{
	for (int i = 0; i < Num_ProfilerStats; ++i) {
		ImGui::LabelText(ProfilerNames[i], "%f ms", ProfilerSpeeds[i]);
	}
}
#endif

void Engine_Start()
{
	Editor::AddOnEditor(DisplayProfilerStats);
	ResourceManager::PrepareMeshes();
	// skybox texture no need to store handle
	ResourceManager::ImportTexture("Assets/cape_hill_4k.jpg");
	
	char jupiterTexture = ResourceManager::ImportTexture("Assets/2k_jupiter.jpg");
	MeshHandle bmwMesh = ResourceManager::ImportMesh("Assets/bmw.obj");

	ResourceManager::PushMeshesToGPU();
	ResourceManager::PushTexturesToGPU();
	// todo create instances
	Renderer::BeginInstanceRegister();

	Renderer::RegisterMeshInstance(bmwMesh, ResourceManager::DefaultMaterial, float3(0.0f, 0.2f, 0.0f));

	Renderer::EndInstanceRegister();
	// create random sphere positions&colors
	Sphere sphere = Renderer::CreateSphere(2.0f, float3(5.0f, 0.0f, 0.0f));
	Renderer::PushSphere(sphere);
	Renderer::UpdateSpheres();
}

void Engine_EndFrame()
{
	while (NumEndFrameEvents)
		EndFrameEvents[--NumEndFrameEvents]();
}

void Engine_Exit()
{
	while(NumApplicationExitEvents--)
		ApplicationExitEvents[NumApplicationExitEvents]();
}