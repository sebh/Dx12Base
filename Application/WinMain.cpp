
#include "Dx12Base/WindowHelper.h"
#include "Dx12Base/Dx12Device.h"

#include "Game.h"

#include "WinImgui.h"


bool ImguiTopLayerProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
	return ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantCaptureMouse;
}


// the entry point for any Windows program
int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	static bool sVSyncEnable = true;
	static bool sStablePowerEnable = false;
	static bool sPreviousStablePowerEnable = !sStablePowerEnable;
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
	Dx12Device::initialise(win.getHwnd(), desiredClientWidth, desiredClientHeight);
	CachedPSOManager::initialise();

	WinImguiInitialise(win.getHwnd());

	// Create the game
	Game game;

	MSG msg = { 0 };
	while (true)
	{
		if (win.processSingleMessage(msg, ImguiTopLayerProcHandler))
		{
			// A message has been processed

			if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
				break;// process escape key

			if (msg.message == WM_QUIT)
				break; // time to quit

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
			// Game update
			game.update(win.getInputData());

			// Game render
			g_dx12Device->beginFrame();

			{
				SCOPED_GPU_TIMER(RenderFrame, 200, 200, 200);

				static bool initDone = false;
				if (!initDone)
				{
					game.initialise();
					initDone = true;
				}

				WinImguiNewFrame();
				game.render();

				ImGui::SetNextWindowSize(ImVec2(400.0f, 400.0f), ImGuiCond_FirstUseEver);
				ImGui::Begin("GPU performance");
				Dx12Device::GPUTimersReport TimerReport = g_dx12Device->GetGPUTimerReport();
				if (TimerReport.mLastValidGPUTimerSlotCount > 0)
				{
					UINT64 StartTime = TimerReport.mLastValidTimeStamps[TimerReport.mLastValidGPUTimers[0].mQueryIndexStart];
					double TickPerSeconds = double(TimerReport.mLastValidTimeStampTickPerSeconds);

					static float sTimerGraphWidthMs = 33.0f;
					ImGui::Checkbox("VSync", &sVSyncEnable); 
	#ifdef _DEBUG
					ImGui::Checkbox("StablePower", &sStablePowerEnable);
	#endif
					ImGui::SliderFloat("TimerGraphWidth (ms)", &sTimerGraphWidthMs, 1.0, 60.0);

					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
					ImGui::BeginChild("Timer graph", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);
					const float WindowPixelWidth = ImGui::GetWindowWidth();
					const float PixelPerMs = WindowPixelWidth / sTimerGraphWidthMs;
	#if 1
					for (int targetLevel = 0; targetLevel < 8; ++targetLevel)
					{
						bool printDone = false;
						for (UINT i=0; i<TimerReport.mLastValidGPUTimerSlotCount; ++i)
						{
							Dx12Device::GPUTimer& Timer = TimerReport.mLastValidGPUTimers[i];
						
							if (Timer.mLevel == targetLevel)
							{
								float TimerStart = float( double(TimerReport.mLastValidTimeStamps[Timer.mQueryIndexStart] - StartTime) / TickPerSeconds );
								float TimerEnd   = float( double(TimerReport.mLastValidTimeStamps[Timer.mQueryIndexEnd]   - StartTime) / TickPerSeconds );
								float TimerStartMs = TimerStart * 1000.0f;
								float TimerEndMs   = TimerEnd * 1000.0f;
								float DurationMs   = TimerEndMs - TimerStartMs;

								ImU32 color = ImColor(int(Timer.mRGBA) & 0xFF, int(Timer.mRGBA>>8) & 0xFF, int(Timer.mRGBA>>16) & 0xFF, int(Timer.mRGBA>>24) & 0xFF);
								ImGui::PushStyleColor(ImGuiCol_Button, color);
								ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
								ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
								{
									// Set cursor to the correct position and size according to when things started this day
									ImGui::SetCursorPosX(TimerStartMs * PixelPerMs);
									ImGui::PushItemWidth(TimerEndMs * PixelPerMs);

									char debugStr[128];
									sprintf_s(debugStr, 128, "%ls %.3f ms\n", Timer.mEventName, DurationMs);
									ImGui::Button(debugStr, ImVec2(DurationMs * PixelPerMs, 0.0f));
									if (ImGui::IsItemHovered())
									{
										ImGui::SetTooltip(debugStr);
									}
									ImGui::SameLine();
									ImGui::PopItemWidth();
								}
								ImGui::PopStyleColor(3);
								printDone = true;
							}
						}
						if (printDone)
							ImGui::NewLine(); // start a new line if anything has been printed
					}
	#endif
					ImGui::EndChild();
					ImGui::PopStyleVar(3);

					{
						char debugStr[256];
						sprintf_s(debugStr, 256, "Cached Raster  PSO count %zu", g_CachedPSOManager->GetCachedRasterPSOCount());
						ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), debugStr);
						sprintf_s(debugStr, 256, "Cached compute PSO count %zu", g_CachedPSOManager->GetCachedComputePSOCount());
						ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), debugStr);
					}

					for (UINT i = 0; i < TimerReport.mLastValidGPUTimerSlotCount; ++i)
					{
						Dx12Device::GPUTimer& Timer = TimerReport.mLastValidGPUTimers[i];
						float TimerStart = float(double(TimerReport.mLastValidTimeStamps[Timer.mQueryIndexStart] - StartTime) / TickPerSeconds);
						float TimerEnd = float(double(TimerReport.mLastValidTimeStamps[Timer.mQueryIndexEnd] - StartTime) / TickPerSeconds);
						float TimerStartMs = TimerStart * 1000.0f;
						float TimerEndMs = TimerEnd * 1000.0f;
						float DurationMs = TimerEndMs - TimerStartMs;

						char* levelOffset = "---------------";	// 16 chars
						unsigned int levelShift = 16 - 2 * Timer.mLevel - 1;
						char* levelOffsetPtr = levelOffset + (levelShift < 0 ? 0 : levelShift); // cheap way to add shifting to a printf

						char debugStr[128];
						sprintf_s(debugStr, 128, "%s%ls %.3f ms\n", levelOffsetPtr, Timer.mEventName, DurationMs);
						ImU32 color = ImColor(int(Timer.mRGBA) & 0xFF, int(Timer.mRGBA >> 8) & 0xFF, int(Timer.mRGBA >> 16) & 0xFF, int(Timer.mRGBA >> 24) & 0xFF);
						ImGui::TextColored(ImVec4(float(int(Timer.mRGBA) & 0xFF) / 255.0f, float(int(Timer.mRGBA >> 8) & 0xFF) / 255.0f, float(int(Timer.mRGBA >> 16) & 0xFF) / 255.0f, 255.0f), debugStr);
					}
				}
				ImGui::End();

				WinImguiRender();
			}

			// Swap the back buffer
			g_dx12Device->endFrameAndSwap(sVSyncEnable);

#if 0		// SetStablePowerState is only available when developer mode is enabled. So it is deisabled by default.
			if (sPreviousStablePowerEnable != sStablePowerEnable)
			{
				
				sPreviousStablePowerEnable = sStablePowerEnable;
				HRESULT hr = g_dx12Device->getDevice()->SetStablePowerState(sStablePowerEnable);
				ATLASSERT(hr == S_OK);
			}
#endif

			// Events have all been processed in this path by the game
			win.clearInputEvents();
		}
	}

	g_dx12Device->closeBufferedFramesBeforeShutdown();	// close all frames

	WinImguiShutdown();
	game.shutdown();									// game release its resources
	CachedPSOManager::shutdown();						// release the cached PSOs
	Dx12Device::shutdown();								// now we can safely delete the dx12 device we hold

	// End of application
	return 0;
}


