#pragma once
#include "Math/Vector.hpp"

namespace Window
{
	int Create();
	void Destroy();
	bool ShouldClose();
	void EndFrame();
	unsigned GetWidth();
	unsigned GetHeight();
	void ChangeName(float ms);
	
	// TIME
	double GetTime();
	double DeltaTime();

	// INPUT
	void InfiniteMouse();
	Vector2i GetWindowScale();
	Vector2i GetMonitorScale();
	Vector2i GetMouseScreenPos();
	Vector2f GetMouseWindowPos();

	void SetMouseScreenPos(Vector2i pos);
	void SetMouseWindowPos (Vector2f pos);

	bool GetKey(int keyCode);
	bool GetKeyUp(int keyCode);
	bool GetKeyDown(int keyCode);

	bool GetMouseButton(int keyCode);
	bool GetMouseButtonUp(int keyCode);
	bool GetMouseButtonDown(int keyCode);
}

enum KeyCode
{
	KeyCode_SPACE              = 32,
	KeyCode_APOSTROPHE         = 39, /* ' */
	KeyCode_COMMA              = 44, /* , */
	KeyCode_MINUS              = 45, /* - */
	KeyCode_PERIOD             = 46, /* . */
	KeyCode_SLASH              = 47, /* / */
	KeyCode_0                  = 48,
	KeyCode_1                  = 49,
	KeyCode_2                  = 50,
	KeyCode_3                  = 51,
	KeyCode_4                  = 52,
	KeyCode_5                  = 53,
	KeyCode_6                  = 54,
	KeyCode_7                  = 55,
	KeyCode_8                  = 56,
	KeyCode_9                  = 57,
	KeyCode_SEMICOLON          = 59, /* ; */
	KeyCode_EQUAL              = 61, /* = */
	KeyCode_A                  = 65,
	KeyCode_B                  = 66,
	KeyCode_C                  = 67,
	KeyCode_D                  = 68,
	KeyCode_E                  = 69,
	KeyCode_F                  = 70,
	KeyCode_G                  = 71,
	KeyCode_H                  = 72,
	KeyCode_I                  = 73,
	KeyCode_J                  = 74,
	KeyCode_K                  = 75,
	KeyCode_L                  = 76,
	KeyCode_M                  = 77,
	KeyCode_N                  = 78,
	KeyCode_O                  = 79,
	KeyCode_P                  = 80,
	KeyCode_Q                  = 81,
	KeyCode_R                  = 82,
	KeyCode_S                  = 83,
	KeyCode_T                  = 84,
	KeyCode_U                  = 85,
	KeyCode_V                  = 86,
	KeyCode_W                  = 87,
	KeyCode_X                  = 88,
	KeyCode_Y                  = 89,
	KeyCode_Z                  = 90,
	KeyCode_LEFT_BRACKET       = 91, /* [ */
	KeyCode_BACKSLASH          = 92, /* \ */
	KeyCode_RIGHT_BRACKET      = 93, /* ] */
	KeyCode_GRAVE_ACCENT       = 96, /* ` */
	KeyCode_WORLD_1            = 161, 
	KeyCode_WORLD_2            = 162, 
	
	KeyCode_ESCAPE           = 256,
	KeyCode_ENTER            = 257,
	KeyCode_TAB              = 258,
	KeyCode_BACKSPACE        = 259,
	KeyCode_INSERT           = 260,
	KeyCode_DELETE           = 261,
	KeyCode_RIGHT            = 262,
	KeyCode_LEFT             = 263,
	KeyCode_DOWN             = 264,
	KeyCode_UP               = 265,
	KeyCode_PAGE_UP          = 266,
	KeyCode_PAGE_DOWN        = 267,
	KeyCode_HOME             = 268,
	KeyCode_END              = 269,
	KeyCode_CAPS_LOCK        = 280,
	KeyCode_SCROLL_LOCK      = 281,
	KeyCode_NUM_LOCK         = 282,
	KeyCode_PRINT_SCREEN     = 283,
	KeyCode_PAUSE            = 284,
	KeyCode_F1               = 290,
	KeyCode_F2               = 291,
	KeyCode_F3               = 292,
	KeyCode_F4               = 293,
	KeyCode_F5               = 294,
	KeyCode_F6               = 295,
	KeyCode_F7               = 296,
	KeyCode_F8               = 297,
	KeyCode_F9               = 298,
	KeyCode_F10              = 299,
	KeyCode_F11              = 300,
	KeyCode_F12              = 301,
	KeyCode_KP_DECIMAL       = 330,
	KeyCode_KP_DIVIDE        = 331,
	KeyCode_KP_MULTIPLY      = 332,
	KeyCode_KP_SUBTRACT      = 333,
	KeyCode_KP_ADD           = 334,
	KeyCode_KP_ENTER         = 335,
	KeyCode_KP_EQUAL         = 336,
	KeyCode_LEFT_SHIFT       = 340,
	KeyCode_LEFT_CONTROL     = 341,
	KeyCode_LEFT_ALT         = 342,
	KeyCode_LEFT_SUPER       = 343,
	KeyCode_RIGHT_SHIFT      = 344,
	KeyCode_RIGHT_CONTROL    = 345,
	KeyCode_RIGHT_ALT        = 346,
	KeyCode_RIGHT_SUPER      = 347,
	KeyCode_MENU             = 348,
};

enum ButtonAction
{
	ButtonAction_Release = 0,
	ButtonAction_Press   = 1,
	ButtonAction_Repeat  = 2
};

enum MouseButton
{
	MouseButton_Left,
	MouseButton_Right
};