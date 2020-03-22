
#include "Dx12Base/WindowHelper.h"
#include "Dx12Base/Dx12Device.h"

#include "Game.h"

#include <imgui.h>
#include <examples\imgui_impl_win32.h>
#include <examples\imgui_impl_dx12.h>
//#include "imgui\imgui_impl_dx11.h"

static bool show_demo_window = true;
static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HRESULT hr = g_dx12Device->getDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap));
	ATLASSERT(hr == S_OK);

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(win.getHwnd());
	ImGui_ImplDX12_Init(g_dx12Device->getDevice(), frameBufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
		g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
		g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());


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

			if (ImGui_ImplWin32_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
				continue;

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


			// Start the Dear ImGui frame
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			ImGui::ShowDemoWindow(&show_demo_window);

			game.render();

			// Render UI
			{
				ID3D12GraphicsCommandList* commandList = g_dx12Device->getFrameCommandList();
				ID3D12Resource* backBuffer = g_dx12Device->getBackBuffer();

				D3D12_RESOURCE_BARRIER bbPresentToRt = {};
				bbPresentToRt.Transition.pResource = backBuffer;
				bbPresentToRt.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
				bbPresentToRt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
				bbPresentToRt.Transition.Subresource = 0;
				commandList->ResourceBarrier(1, &bbPresentToRt);

				D3D12_CPU_DESCRIPTOR_HANDLE descriptor = g_dx12Device->getBackBufferDescriptor();
				commandList->OMSetRenderTargets(1, &descriptor, FALSE, NULL);
				commandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
				ImGui::Render();
				ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

				D3D12_RESOURCE_BARRIER bbRtToPresent = {};
				bbRtToPresent.Transition.pResource = backBuffer;
				bbRtToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				bbRtToPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
				bbRtToPresent.Transition.Subresource = 0;
				commandList->ResourceBarrier(1, &bbRtToPresent);
			}

			// Swap the back buffer
			g_dx12Device->endFrameAndSwap(sVSyncEnable);
//			DxGpuPerformance::endGpuTimer(frameGpuTimerName);
//			DxGpuPerformance::endFrame();

			// Events have all been processed in this path by the game
			win.clearInputEvents();
		}
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	if (g_pd3dSrvDescHeap) { g_pd3dSrvDescHeap->Release(); g_pd3dSrvDescHeap = NULL; }

//	DxGpuPerformance::shutdown();

	g_dx12Device->closeBufferedFramesBeforeShutdown();	// close all frames
	game.shutdown();									// game release its resources
	Dx12Device::shutdown();								// now we can safely delete the dx12 device we hold

	// End of application
	return 0;
}


