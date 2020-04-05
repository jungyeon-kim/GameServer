#include "stdafx.h"
#include "Player.h"
#include "Renderer.h"

using namespace std;

CPlayer::CPlayer(shared_ptr<class CRenderer> NewRenderer)
{
	Renderer = NewRenderer;
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
	PosX = NewX;
}

void CPlayer::SetPosY(float NewY)
{
	PosY = NewY;
}
