
#include "WinImgui.h"
#include "Dx12Base/Dx12Device.h"


static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;


void WinImguiInitialise(const HWND hwnd)
{
#if D_ENABLE_IMGUI

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
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(g_dx12Device->getDevice(), frameBufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
		g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
		g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

#endif
}


void WinImguiNewFrame()
{
#if D_ENABLE_IMGUI

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

#endif
}

void WinImguiRender()
{
#if D_ENABLE_IMGUI
	SCOPED_GPU_EVENT(IMGUI);

	// Render UI

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

#endif
}

void WinImguiShutdown()
{
#if D_ENABLE_IMGUI

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	resetComPtr(&g_pd3dSrvDescHeap);

#endif
}




