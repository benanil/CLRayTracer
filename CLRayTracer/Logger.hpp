#pragma once

#if !(defined(__GNUC__) || defined(__GNUG__))
#define AXLOG(format, ...)		Logger::Log    (__FILE__, __LINE__, format, __VA_ARGS__)
#define AXWARNING(format, ...) Logger::Warning(__FILE__, __LINE__, format, __VA_ARGS__)
#define AXERROR(format, ...)	Logger::Error  (__FILE__, __LINE__, format, __VA_ARGS__)
#define AXFILELOG(outputDir, message, severity) Logger::FileLog(__FILE__, outputDir, __LINE__, message, severity)
#else
#define AX_LOG(format, ...)		Logger::Log    (__FILE__, __LINE__, format,##__VA_ARGS__)
#define AX_WARNING(format, ...) Logger::Warning(__FILE__, __LINE__, format,##__VA_ARGS__)
#define AX_ERROR(format, ...)	Logger::Error  (__FILE__, __LINE__, format,##__VA_ARGS__)
#endif

namespace Logger
{
	enum class Severity : unsigned char
	{
		Info = 0,
		Warning,
		Error,
		FatalError
	};

	void FileLog(const char* filename, const char* outputDir, int line, const char* message, Severity severity);
	void Log    (const char* filename, int line, const char* format, ...);
	void Warning(const char* filename, int line, const char* format, ...);
	void Error  (const char* filename, int line, const char* format, ...);
	void ShowFileName(bool value);
}

