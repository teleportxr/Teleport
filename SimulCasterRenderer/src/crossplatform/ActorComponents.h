#pragma once

namespace scr
{

class VisibilityComponent
{
public:
	void update(float deltaTime);

	void setVisibility(bool visible);
	bool getVisibility() const;

	float getTimeSinceLastVisible() const;

private:
	bool isVisible = true;

	float timeSinceLastVisible = 0;
};

}