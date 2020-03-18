#include "stdafx.h"
#include "Player.h"
#include "Renderer.h"

using namespace std;

CPlayer::CPlayer()
{
	Renderer = make_unique<CRenderer>(WndSizeX, WndSizeY);

	X = WndSizeX / 16.0f;
	Y = WndSizeY / 16.0f;
}

CPlayer::~CPlayer()
{
}

void CPlayer::Render()
{
	Renderer->DrawSolidRect(X, Y, 0.0f, WndSizeX / 10.0f, 1.0f, 1.0f, 1.0f, 1.0f);
}

float CPlayer::GetX() const
{
	return X;
}

float CPlayer::GetY() const
{
	return Y;
}

void CPlayer::SetX(float NewX)
{
	if (-WndSizeX / 2.0f < NewX && NewX < WndSizeX / 2.0f) X = NewX;
}

void CPlayer::SetY(float NewY)
{
	if (-WndSizeY / 2.0f < NewY && NewY < WndSizeY / 2.0f) Y = NewY;
}
