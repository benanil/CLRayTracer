#include "Engine.hpp"
#include "Editor/Editor.hpp"
#include "Renderer.hpp"
#include "Math/Transform.hpp"
#include "Window.hpp"

// things that I want to edit in editor
// edit materials DONE
// edit sun angle
// edit mesh instance positions
// spawn new mesh
// recompile kernel

static void(*EndFrameEvents[20])(void);
static int NumEndFrameEvents = 0;

static void(*ApplicationExitEvents[20])(void);
static int NumApplicationExitEvents = 0;
static float SunAngle = -1.96f;

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
	ImGui::Begin("Lighting");
	ImGui::DragFloat("SunAngle", &SunAngle, 0.025f, -3.14f, 0.0f);
	ImGui::Separator();
	for (int i = 0; i < Num_ProfilerStats; ++i) {
		ImGui::LabelText(ProfilerNames[i], "%f ms", ProfilerSpeeds[i]);
	}
	ImGui::End();
}
#endif

static MeshHandle bmwMesh;
static Transform bmwTransform = {};

void Engine_Start()
{
	Editor::AddOnEditor(DisplayProfilerStats);
	ResourceManager::PrepareMeshes();
	// skybox texture no need to store handle
	ResourceManager::ImportTexture("Assets/cape_hill_4k.jpg");
	
	char jupiterTexture = ResourceManager::ImportTexture("Assets/2k_jupiter.jpg");
	MeshHandle bmwMesh = ResourceManager::ImportMesh("Assets/bmw.obj");
	bmwTransform.SetPosition(0.0f, 5.50f, 0.0f);
	bmwTransform.SetRotationEulerDegree(Vector3f(00.0f, 45.0f, 45.0f));
	bmwTransform.SetScale(Vector3f(1.2f,1.2f, 1.2f));

	Renderer::SetMeshMatrix(bmwMesh, bmwTransform.GetMatrix());

	ResourceManager::PushMeshesToGPU();
	ResourceManager::PushTexturesToGPU();
	// todo create instances
	Renderer::BeginInstanceRegister();

	Renderer::RegisterMeshInstance(bmwMesh, ResourceManager::DefaultMaterial, bmwTransform.GetMatrix());

	Renderer::EndInstanceRegister();
}

float Engine_Tick()
{
	float dt = (float)Window::DeltaTime();
	static float rotation = 0.0f;
	bool positionChanged = false;

	bool pressing = Window::GetMouseButton(MouseButton_Right);

	float speed = (dt * 5.0f) + (Window::GetKey(KeyCode_LEFT_SHIFT) * 2.0f);
	Vector3f dir{};

	if (!pressing && Window::GetKey(KeyCode_W)) dir -= Vector3f::Right(), positionChanged |= 1;
	if (!pressing && Window::GetKey(KeyCode_S)) dir += Vector3f::Right(), positionChanged |= 1;
	if (!pressing && Window::GetKey(KeyCode_A)) dir += Vector3f::Forward(), positionChanged |= 1;
	if (!pressing && Window::GetKey(KeyCode_D)) dir -= Vector3f::Forward(), positionChanged |= 1;
	
	//bmwTransform.SetRotationEuler(Vector3f(0, rotation, 0));
	rotation += dt * 0.5f;
	
	if (positionChanged)
	{
		dir = dir.Normalized();
		bmwTransform.position += dir * speed;
		bmwTransform.UpdatePosition();
	}
	Renderer::SetMeshMatrix(bmwMesh, bmwTransform.GetMatrix());
	return SunAngle;
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