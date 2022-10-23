// Log code from here: https://github.com/ambrosiogabe/CppUtils
// License: https://github.com/ambrosiogabe/CppUtils/blob/master/LICENSE
// Thanks Gabe

#include "Logger.hpp"
#include <mutex>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <stdarg.h>
#include <fstream>

static std::mutex logMutex;

#ifdef _WIN32
#include <Windows.h>
#include <crtdbg.h>

namespace Logger
{
	static bool showFileName = true;
	
	void ShowFileName(bool value) { showFileName = value; }
}

static constexpr const char* GetFileName(const char* str)
{
	for (int len = __builtin_strlen(str) - 1; len > 0; --len)
	{
		if (str[len] == '/' || str[len] == '\\') return str + len+1;
	}
	return str;
}


void Logger::FileLog(const char* filename, const char* outputDir, int line, const char* message, Logger::Severity severity)
{
	static const char* const SeverityToString[] = { "Info         ", "Warning      ", "Error!       ", "Fatal Error! "};
	
	std::ofstream ofs(outputDir, std::ios_base::app);
	
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S", std::localtime(&now));

	ofs << "[" << buf << "]"
		<< SeverityToString[(unsigned char)severity]
		<< GetFileName(filename) 
		<< ":" << line << ": " << message << "\n";

	ofs.close();
}

void Logger::Log(const char* filename, int line, const char* format, ...)
{
	//	std::lock_guard<std::mutex> lock(logMutex);
	
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 2);
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%I:%M:%S", std::localtime(&now));
	printf("[%s] ", buf);
	
	printf("Log: %s:%d ", GetFileName(filename), line);

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0F);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	
	printf("\n");
}

void Logger::Warning(const char* filename, int line, const char* format, ...) 
{
	std::lock_guard<std::mutex> lock(logMutex);
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_RED);
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%I:%M:%S", std::localtime(&now));
	printf("[%s]: ", buf);
	
	if (showFileName)	printf("Warning: %s:%d ", GetFileName(filename), line);
	else				printf("Warning: ");

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0F);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\n");
}

void Logger::Error(const char* filename, int line, const char* format, ...)
{
	std::lock_guard<std::mutex> lock(logMutex);

	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED);
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%I:%M:%S", std::localtime(&now));
	printf("[%s]: ", buf);

	printf("Error: %s%d \n", GetFileName(filename), line);

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0F);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\n");
}

#elif defined(__linux__) // end LOGGING_IMPL_WIN32
// begin LOGGING_IMPL_LINUX

#include <csignal>

namespace ColorCode
{
	const char* KNRM = "\x1B[0m";
	const char* KRED = "\x1B[31m";
	const char* KGRN = "\x1B[32m";
	const char* KYEL = "\x1B[33m";
	const char* KBLU = "\x1B[34m";
	const char* KMAG = "\x1B[35m";
	const char* KCYN = "\x1B[36m";
	const char* KWHT = "\x1B[37m";
}

void Log(const char* filename, int line, const char* format, ...)
{
	std::lock_guard<std::mutex> lock(logMutex);
	printf("%s%s (line %d) Log: \n", ColorCode::KBLU, filename, line);

	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S", std::localtime(&now));
	printf("%s[%s]: ", ColorCode::KNRM, buf);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\n");
}

void Warning(const char* filename, int line, const char* format, ...)
{
	std::lock_guard<std::mutex> lock(logMutex);
	printf("%s%s (line %d) Warning: \n", ColorCode::KYEL, filename, line);

	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S", std::localtime(&now));
	printf("%s[%s]: ", ColorCode::KNRM, buf);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\n");
}

void Error(const char* filename, int line, const char* format, ...)
{
	std::lock_guard<std::mutex> lock(logMutex);
	printf("%s%s (line %d) Error: \n", ColorCode::KRED, filename, line);

	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S", std::localtime(&now));
	printf("%s[%s]: ", ColorCode::KNRM, buf);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\n");
}
#else

void Logger::Log(const char* filename, int line, const char* format, ...)
{
	std::lock_guard<std::mutex> lock(logMutex);
	printf("%s (line %d) Log: \n", filename, line);

	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S", std::localtime(&now));
	printf("[%s]: ", buf);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\n");
}

void Logger::Warning(const char* filename, int line, const char* format, ...)
{
	std::lock_guard<std::mutex> lock(logMutex);
	printf("%s (line %d) Warning: \n", filename, line);

	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S", std::localtime(&now));
	printf("[%s]: ", buf);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\n");
}

void Logger::Error(const char* filename, int line, const char* format, ...)
{
	std::lock_guard<std::mutex> lock(logMutex);
	printf("%s (line %d) Error: \n", filename, line);

	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char buf[20] = { 0 };
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S", std::localtime(&now));
	printf("[%s]: ", buf);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\n");
}
#endif // defined win32