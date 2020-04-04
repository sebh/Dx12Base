#pragma once

#include "Dx12Base/WindowInput.h"

class Game
{
public:
	Game();
	~Game();

	void initialise();
	void shutdown();

	void update(const WindowInputData& inputData);
	void render();

private:

	/// Load/reload all shaders if compilation is succesful.
	/// @ReloadMode: true if we are simply trying to reload shaders.
	void loadShaders(bool ReloadMode);
	/// release all shaders
	void releaseShaders();
};


