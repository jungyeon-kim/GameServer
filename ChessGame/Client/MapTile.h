#pragma once

class CMapTile
{
private:
	std::shared_ptr<class CRenderer> Renderer{};
public:
	CMapTile(std::shared_ptr<class CRenderer> NewRenderer);
	~CMapTile();

	void Render();
};

