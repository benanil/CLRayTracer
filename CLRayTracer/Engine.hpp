#pragma once

enum ProfilerStats
{
	ProfilerStats_Render,
	ProfilerStats_EngineTick,
	Num_ProfilerStats
};

void Engine_AddEndOfFrameEvent(void(*action)());
void Engine_AddOnAppQuitEvent(void(*action)());
void Engine_UpdateProfilerStats(ProfilerStats stats, float ms);
void Engine_EndFrame();
void Engine_Start();
void Engine_Exit();