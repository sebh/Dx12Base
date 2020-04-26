#pragma once

#include "Dx12Base/WindowInput.h"
#include "Dx12Base/Dx12Device.h"
#include "Dx12Base/Dx12RayTracing.h"

struct ViewCamera
{
	ViewCamera();

	// Source state
	float4 mPos;
	float mYaw;
	float mPitch;

	// Derived data from source state
	float4 mLeft;
	float4 mForward;
	float4 mUp;

	// Movement status
	int mMoveForward;
	int mMoveLeft;
	int mMouseDx;
	int mMouseDy;

	void Update();

	float4x4 GetViewMatrix() const;
};

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
	
	ViewCamera View;
	bool mInitialisedMouse = false;
	int mLastMouseX = 0;
	int mLastMouseY = 0;
	int mMouseX = 0;
	int mMouseY = 0;

	InputLayout* layout;

	RenderBufferGeneric* vertexBuffer;
	RenderBufferGeneric* indexBuffer;

	uint SphereIndexCount;
	uint SphereVertexCount;
	InputLayout* SphereVertexLayout;
	RenderBufferGeneric* SphereVertexBuffer;
	RenderBufferGeneric* SphereIndexBuffer;

	VertexShader* MeshVertexShader;
	PixelShader*  MeshPixelShader;

	TypedBuffer* UavBuffer;

	VertexShader* vertexShader;
	PixelShader*  pixelShader;
	PixelShader*  ToneMapShaderPS;
	ComputeShader*  computeShader;

	RenderTexture* texture;
	RenderTexture* texture2;
	RenderTexture* HdrTexture;
	RenderTexture* HdrTexture2;
	RenderTexture* DepthTexture;

	bool ShowRtResult = true;
};


