#pragma once

class CMapTile
{
private:
	std::unique_ptr<class CRenderer> Renderer{};
public:
	CMapTile();
	~CMapTile();

	void Render();
};

