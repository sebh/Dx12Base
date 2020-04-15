#pragma once

#include "Dx12Base/WindowInput.h"
#include "Dx12Base/Dx12Device.h"

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
	



	InputLayout* layout;

	RenderBuffer* vertexBuffer;
	RenderBuffer* indexBuffer;

	uint SphereIndexCount;
	InputLayout* SphereVertexLayout;
	RenderBuffer* SphereVertexBuffer;
	RenderBuffer* SphereIndexBuffer;

	VertexShader* MeshVertexShader;
	PixelShader*  MeshPixelShader;

	RenderBuffer* UavBuffer;

	VertexShader* vertexShader;
	PixelShader*  pixelShader;
	PixelShader*  ToneMapShaderPS;
	ComputeShader*  computeShader;

	RenderTexture* texture;
	RenderTexture* HdrTexture;
	RenderTexture* DepthTexture;
};


