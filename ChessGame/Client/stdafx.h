#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <tchar.h>
#include <WinSock2.h>
#include <iostream>
#include <string>
#include <memory>
#include "../Server/Protocol.h"
#include "targetver.h"
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

constexpr int WndSizeX{ 400 };
constexpr int WndSizeY{ 400 };