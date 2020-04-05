#pragma once

class CPlayer
{
private:
	float PosX{}, PosY{};
	std::shared_ptr<class CRenderer> Renderer{};
public:
	CPlayer(std::shared_ptr<class CRenderer> NewRenderer);
	~CPlayer();

	void Render();

	float GetPosX() const;
	float GetPosY() const;
	void SetPosX(float NewX);
	void SetPosY(float NewY);
};