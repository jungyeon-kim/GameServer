#include "stdafx.h"
#include "Player.h"
#include "Renderer.h"

using namespace std;

CPlayer::CPlayer(shared_ptr<class CRenderer> NewRenderer)
{
	Renderer = NewRenderer;

	PosX = WndSizeX / 16.0f;
	PosY = WndSizeY / 16.0f;
}

CPlayer::~CPlayer()
{
}

void CPlayer::Render()
{
	Renderer->DrawSolidRect(PosX, PosY, 0.0f, WndSizeX / 10.0f, 1.0f, 1.0f, 1.0f, 1.0f);
}

float CPlayer::GetPosX() const
{
	return PosX;
}

float CPlayer::GetPosY() const
{
	return PosY;
}

void CPlayer::SetPosX(float NewX)
{
	if (-WndSizeX / 2.0f < NewX && NewX < WndSizeX / 2.0f) PosX = NewX;
}

void CPlayer::SetPosY(float NewY)
{
	if (-WndSizeY / 2.0f < NewY && NewY < WndSizeY / 2.0f) PosY = NewY;
}
