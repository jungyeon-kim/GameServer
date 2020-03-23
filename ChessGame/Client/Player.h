#pragma once

class CPlayer
{
private:
	float X{}, Y{};
	std::unique_ptr<class CRenderer> Renderer{};
public:
	CPlayer();
	~CPlayer();

	void Render();

	float GetX() const;
	float GetY() const;
	void SetX(float NewX);
	void SetY(float NewY);
};