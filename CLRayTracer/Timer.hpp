#pragma once
#include <chrono>
#include <stdio.h>
#include "Logger.hpp"

#ifndef NDEBUG
#	define CSTIMER(message) Timer timer = Timer(message);
#else
#   define CSTIMER(message) Timer timer = Timer(message);
#endif

struct Timer
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start_point;
	bool printMilisecond;

	const char* message;

	Timer(const char* _message) : message(_message), printMilisecond(true)
	{
		start_point = std::chrono::high_resolution_clock::now();
	}

	const double GetTime()
	{
		using namespace std::chrono;
		auto end_point = high_resolution_clock::now();
		auto start = time_point_cast<microseconds>(start_point).time_since_epoch().count();
		auto end = time_point_cast<microseconds>(end_point).time_since_epoch().count();
		printMilisecond = false;
		return (end - start) * 0.001;
	}

	~Timer()
	{
		using namespace std::chrono;
		if (!printMilisecond) return;
		auto end_point = high_resolution_clock::now();
		auto start = time_point_cast<microseconds>(start_point).time_since_epoch().count();
		auto end = time_point_cast<microseconds>(end_point).time_since_epoch().count();
		auto _duration = end - start;
		printf("speed ms: %f", (_duration * 0.001));
	}
};