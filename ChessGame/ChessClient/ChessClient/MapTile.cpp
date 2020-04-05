#include "stdafx.h"
#include "MapTile.h"
#include "Renderer.h"

using namespace std;

CMapTile::CMapTile(std::shared_ptr<class CRenderer> NewRenderer)
{
	Renderer = NewRenderer;
}

CMapTile::~CMapTile()
{
}

void CMapTile::Render()
{
	for (float X = -WndSizeX / 2; X < WndSizeX / 2; X += WndSizeX / 4.0f)
		for (float Y = -WndSizeY / 2; Y < WndSizeY / 2; Y += WndSizeY / 4.0f)
		{
			Renderer->DrawSolidRect(X + WndSizeX / 16.0f, Y + WndSizeX / 16.0f, 0.0f,
				WndSizeX / 8.0f, 0.8f, 0.0f, 0.5f, 1.0f);
			Renderer->DrawSolidRect(X + WndSizeX / 16.0f * 3.0f, Y + WndSizeX / 16.0f * 3.0f, 0.0f,
				WndSizeX / 8.0f, 0.8f, 0.0f, 0.5f, 1.0f);
		}
}
