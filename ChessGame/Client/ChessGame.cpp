#include "stdafx.h"
#include "GameManager.h"

using namespace std;

unique_ptr<CGameManager> GameManager{};

void RenderScene(void)
{
	GameManager->Render();
}

void Idle(void)
{
	GameManager->Idle();
}

void MouseInput(int button, int state, int x, int y)
{
	GameManager->MouseInput(button, state, x, y);
}

void KeyInput(unsigned char key, int x, int y)
{
	GameManager->KeyInput(key, x, y);
}

void SpecialKeyInput(int key, int x, int y)
{
	GameManager->SpecialKeyInput(key, x, y);
}

int main(int argc, char **argv)
{
	// Connect Server
	char ServerIP[256]{};
	cout << "Input Server IP: ";
	cin >> ServerIP;

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

	GameManager = make_unique<CGameManager>(ServerIP);

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutMouseFunc(MouseInput);
	glutSpecialFunc(SpecialKeyInput);

	glutMainLoop();

    return 0;
}

