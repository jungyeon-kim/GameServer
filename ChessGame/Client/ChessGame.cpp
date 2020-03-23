/*
Copyright 2017 Lee Taek Hee (Korea Polytech University)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include "Player.h"
#include "MapTile.h"

using namespace std;

unique_ptr<CPlayer> Player{};
unique_ptr<CMapTile> MapTile{};

void RenderScene(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0f, 0.3f, 0.3f, 1.0f);

	MapTile->Render();
	Player->Render();

	glutSwapBuffers();
}

void Idle(void)
{
	RenderScene();
}

void MouseInput(int button, int state, int x, int y)
{
	RenderScene();
}

void KeyInput(unsigned char key, int x, int y)
{
	RenderScene();
}

void SpecialKeyInput(int key, int x, int y)
{
	switch (key)
	{
	case GLUT_KEY_UP:
		Player->SetY(Player->GetY() + WndSizeY / 8.0f);
		break;
	case GLUT_KEY_DOWN:
		Player->SetY(Player->GetY() - WndSizeY / 8.0f);
		break;
	case GLUT_KEY_LEFT:
		Player->SetX(Player->GetX() - WndSizeX / 8.0f);
		break;
	case GLUT_KEY_RIGHT:
		Player->SetX(Player->GetX() + WndSizeX / 8.0f);
		break;
	}

	RenderScene();
}

int main(int argc, char **argv)
{
	// Initialize GL things
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(WndSizeX, WndSizeY);
	glutCreateWindow("Chess Client");

	glewInit();
	if (glewIsSupported("GL_VERSION_3_0"))
	{
		cout << " GLEW Version is 3.0\n ";
	}
	else
	{
		cout << "GLEW 3.0 not supported\n ";
	}

	Player = make_unique<CPlayer>();
	MapTile = make_unique<CMapTile>();

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutMouseFunc(MouseInput);
	glutSpecialFunc(SpecialKeyInput);

	glutMainLoop();

    return 0;
}

