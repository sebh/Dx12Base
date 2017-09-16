
#include "Dx12Base/WindowHelper.h"
#include "Dx12Base/Dx12Device.h"

#include "Game.h"

//#include <imgui.h>
//#include "imgui\imgui_impl_dx11.h"


// the entry point for any Windows program
int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	static bool sVSyncEnable = true;
	static float sTimerGraphWidth = 18.0f;

	// Get a window size that matches the desired client size
	const unsigned int desiredPosX = 20;
	const unsigned int desiredPosY = 20;
	const unsigned int desiredClientWidth  = 1280;
	const unsigned int desiredClientHeight = 720;
	RECT clientRect;
	clientRect.left		= desiredPosX;
	clientRect.right	= desiredPosX + desiredClientWidth;
	clientRect.bottom	= desiredPosY + desiredClientHeight;
	clientRect.top		= desiredPosY;

	// Create the window
	WindowHelper win(hInstance, clientRect, nCmdShow, L"D3D11 Application");
	win.showWindow();

	// Create the d3d device
	Dx12Device::initialise(win.getHwnd());
//	DxGpuPerformance::initialise();

	// Initialise imgui
//	ImGui_ImplDX11_Init(win.getHwnd(), g_dx11Device->getDevice(), g_dx11Device->getDeviceContext());

	// Create the game
	Game game;

	MSG msg = { 0 };
	while (true)
	{
		if (win.processSingleMessage(msg))
		{
			// A message has been processed

			if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
				break;// process escape key

			if (msg.message == WM_QUIT)
				break; // time to quit

			// Update imgui
//			ImGui_ImplDX11_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam);

			// Take into account window resize
			if (msg.message == WM_SIZE && g_dx12Device != NULL && msg.wParam != SIZE_MINIMIZED)
			{
//				ImGui_ImplDX11_InvalidateDeviceObjects();
				// clean up gpu data;		// TODO release swap chain, context and device
				g_dx12Device->getSwapChain()->ResizeBuffers(0, (UINT)LOWORD(msg.lParam), (UINT)HIWORD(msg.lParam), DXGI_FORMAT_UNKNOWN, 0);
				// re create gpu data();	// TODO
//				ImGui_ImplDX11_CreateDeviceObjects();
			}
		}
		else
		{
//			DxGpuPerformance::startFrame();
			const char* frameGpuTimerName = "Frame";
//			DxGpuPerformance::startGpuTimer(frameGpuTimerName, 150, 150, 150);

//			ImGui_ImplDX11_NewFrame();

			// Game update
			game.update(win.getInputData());

			// Game render
			g_dx12Device->beginFrame();

			static bool initDone = false;
			if (!initDone)
			{
				game.initialise();
				initDone = true;
			}

			game.render();

			// Render UI
			{
//				// imgui TODO
			}

			// Swap the back buffer
			g_dx12Device->endFrameAndSwap(sVSyncEnable);
//			DxGpuPerformance::endGpuTimer(frameGpuTimerName);
//			DxGpuPerformance::endFrame();

			// Events have all been processed in this path by the game
			win.clearInputEvents();
		}
	}

//	ImGui_ImplDX11_Shutdown();
//	DxGpuPerformance::shutdown();

	g_dx12Device->closeBufferedFramesBeforeShutdown();	// close all frames
	game.shutdown();									// game release its resources
	Dx12Device::shutdown();								// now we can safely delete the dx12 device we hold

	// End of application
	return 0;
}


