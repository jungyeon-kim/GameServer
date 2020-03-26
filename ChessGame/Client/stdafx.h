#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <memory>
#include "targetver.h"
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#define SERVERPORT 9000

constexpr int WndSizeX{ 400 };
constexpr int WndSizeY{ 400 };

#pragma pack (push, 1)
struct PACKET
{
	char Key{};
	float PosX{}, PosY{};
};
#pragma pack (pop)