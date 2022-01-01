
#pragma once

#define D_ENABLE_IMGUI 1


#include <windows.h>

#if D_ENABLE_IMGUI
#include <imgui.h>
#include <backends\imgui_impl_win32.h>
#include <backends\imgui_impl_dx12.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif // D_ENABLE_IMGUI


void WinImguiInitialise(const HWND hwnd);

void WinImguiNewFrame();

void WinImguiRender();

void WinImguiShutdown();

