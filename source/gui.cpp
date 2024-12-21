#include "imgui.h"
#include <stdio.h>
#include "windows.h"
#include "gui.h"
#include "SDL2/SDL.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

bool done = false;
int gui_mode = GUI_MODE_BROWSER;

namespace GUI
{
	int RenderLoop(SDL_Renderer *renderer)
	{
		Windows::Init();
		while (!done)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL2_ProcessEvent(&event);
				if (event.type == SDL_QUIT)
					done = true;
				if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
					done = true;
			}

			ImGui_ImplSDLRenderer2_NewFrame();
			ImGui_ImplSDL2_NewFrame();

			ImGui::NewFrame();

			if (gui_mode == GUI_MODE_BROWSER)
			{
				Windows::HandleWindowInput();
			}
			Windows::MainWindow();
			Windows::ExecuteActions();
			if (gui_mode == GUI_MODE_IME)
			{
				Windows::HandleImeInput();
			}

			ImGui::Render();
			ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
			SDL_RenderPresent(renderer);
		}
		return 0;
	}
}
