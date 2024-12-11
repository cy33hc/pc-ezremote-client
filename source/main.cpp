#undef main

#include <sstream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "imgui.h"
#include "SDL2/SDL.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "server/http_server.h"
#include "config.h"
#include "lang.h"
#include "gui.h"
#include "textures.h"
#include "util.h"
#include "windows.h"

#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080

// SDL window and software renderer
SDL_Window *window;
SDL_Renderer *renderer;

ImVec4 ColorFromBytes(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
	return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);
};

void InitImgui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	char font_path[PATH_MAX + 1];

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	io.Fonts->Clear();
	io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines;

	std::string lang = std::string(language);
	lang = Util::Trim(lang, " ");
	float font_size = MIN(Windows::scaleX(26.0f), 36.0f);
	ImFontConfig config;
	config.OversampleH = 1;
	config.OversampleV = 1;

	snprintf(font_path, PATH_MAX, "%s/share/ezremote-client/assets/fonts/Roboto_ext.ttf", g_root_path);
	io.Fonts->AddFontFromFileTTF(font_path, font_size, &config, latin);

	config.MergeMode = true;
	if (lang.compare("Korean") == 0)
	{
		io.Fonts->AddFontFromFileTTF(font_path, font_size, &config, io.Fonts->GetGlyphRangesKorean());
	}
	else if (lang.compare("Simplified Chinese") == 0)
	{
		io.Fonts->AddFontFromFileTTF(font_path, font_size, &config, io.Fonts->GetGlyphRangesChineseFull());
	}
	else if (lang.compare("Traditional Chinese") == 0)
	{
		io.Fonts->AddFontFromFileTTF(font_path, font_size, &config, io.Fonts->GetGlyphRangesChineseFull());
	}
	else if (lang.compare("Japanese") == 0 || lang.compare("Ryukyuan") == 0)
	{
		io.Fonts->AddFontFromFileTTF(font_path, font_size, &config, io.Fonts->GetGlyphRangesJapanese());
	}
	else if (lang.compare("Thai") == 0)
	{
		io.Fonts->AddFontFromFileTTF(font_path, font_size, &config, io.Fonts->GetGlyphRangesThai());
	}
	else if (lang.compare("Vietnamese") == 0)
	{
		io.Fonts->AddFontFromFileTTF(font_path, font_size, &config, io.Fonts->GetGlyphRangesVietnamese());
	}
	else if (lang.compare("Greek") == 0)
	{
		io.Fonts->AddFontFromFileTTF(font_path, font_size, &config, io.Fonts->GetGlyphRangesGreek());
	}
	else if (lang.compare("Arabic") == 0)
	{
		io.Fonts->AddFontFromFileTTF(font_path, font_size, &config, arabic);
	}

	config.GlyphMinAdvanceX = 13.0f; // Use if you want to make the icon monospaced
	snprintf(font_path, PATH_MAX, "%s/share/ezremote-client/assets/fonts/fa-solid-900.ttf", g_root_path);
	io.Fonts->AddFontFromFileTTF(font_path, Windows::scaleX(20.0f), &config, fa_icons);
	snprintf(font_path, PATH_MAX, "%s/share/ezremote-client/assets/fonts/OpenFontIcons.ttf", g_root_path);
	io.Fonts->AddFontFromFileTTF(font_path, Windows::scaleX(20.0f), &config, of_icons);
	io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
	io.Fonts->Build();

	Lang::SetTranslation();

	auto &style = ImGui::GetStyle();
	style.AntiAliasedLinesUseTex = false;
	style.AntiAliasedLines = true;
	style.AntiAliasedFill = true;
	style.WindowRounding = 1.0f;
	style.FrameRounding = 2.0f;
	style.GrabRounding = 2.0f;

	ImVec4* colors = style.Colors;
	const ImVec4 bgColor           = ColorFromBytes(37, 37, 38);
	const ImVec4 bgColorBlur       = ColorFromBytes(37, 37, 38, 170);
	const ImVec4 lightBgColor      = ColorFromBytes(82, 82, 85);
	const ImVec4 veryLightBgColor  = ColorFromBytes(90, 90, 95);

	const ImVec4 titleColor        = ColorFromBytes(10, 100, 142);
	const ImVec4 panelColor        = ColorFromBytes(51, 51, 55);
	const ImVec4 panelHoverColor   = ColorFromBytes(29, 151, 236);
	const ImVec4 panelActiveColor  = ColorFromBytes(0, 119, 200);

	const ImVec4 textColor         = ColorFromBytes(255, 255, 255);
	const ImVec4 textDisabledColor = ColorFromBytes(151, 151, 151);
	const ImVec4 borderColor       = ColorFromBytes(78, 78, 78);

	colors[ImGuiCol_Text]                 = textColor;
	colors[ImGuiCol_TextDisabled]         = textDisabledColor;
	colors[ImGuiCol_TextSelectedBg]       = panelActiveColor;
	colors[ImGuiCol_WindowBg]             = bgColor;
	colors[ImGuiCol_ChildBg]              = panelColor;
	colors[ImGuiCol_PopupBg]              = bgColor;
	colors[ImGuiCol_Border]               = borderColor;
	colors[ImGuiCol_BorderShadow]         = borderColor;
	colors[ImGuiCol_FrameBg]              = panelColor;
	colors[ImGuiCol_FrameBgHovered]       = panelHoverColor;
	colors[ImGuiCol_FrameBgActive]        = panelActiveColor;
	colors[ImGuiCol_TitleBg]              = titleColor;
	colors[ImGuiCol_TitleBgActive]        = titleColor;
	colors[ImGuiCol_TitleBgCollapsed]     = titleColor;
	colors[ImGuiCol_MenuBarBg]            = panelColor;
	colors[ImGuiCol_ScrollbarBg]          = panelColor;
	colors[ImGuiCol_ScrollbarGrab]        = lightBgColor;
	colors[ImGuiCol_ScrollbarGrabHovered] = veryLightBgColor;
	colors[ImGuiCol_ScrollbarGrabActive]  = veryLightBgColor;
	colors[ImGuiCol_CheckMark]            = panelActiveColor;
	colors[ImGuiCol_SliderGrab]           = panelHoverColor;
	colors[ImGuiCol_SliderGrabActive]     = panelActiveColor;
	colors[ImGuiCol_Button]               = panelColor;
	colors[ImGuiCol_ButtonHovered]        = panelHoverColor;
	colors[ImGuiCol_ButtonActive]         = panelHoverColor;
	colors[ImGuiCol_Header]               = panelColor;
	colors[ImGuiCol_HeaderHovered]        = panelHoverColor;
	colors[ImGuiCol_HeaderActive]         = panelActiveColor;
	colors[ImGuiCol_Separator]            = borderColor;
	colors[ImGuiCol_SeparatorHovered]     = borderColor;
	colors[ImGuiCol_SeparatorActive]      = borderColor;
	colors[ImGuiCol_ResizeGrip]           = bgColor;
	colors[ImGuiCol_ResizeGripHovered]    = panelColor;
	colors[ImGuiCol_ResizeGripActive]     = lightBgColor;
	colors[ImGuiCol_PlotLines]            = panelActiveColor;
	colors[ImGuiCol_PlotLinesHovered]     = panelHoverColor;
	colors[ImGuiCol_PlotHistogram]        = panelActiveColor;
	colors[ImGuiCol_PlotHistogramHovered] = panelHoverColor;
	colors[ImGuiCol_ModalWindowDimBg]     = bgColorBlur;
	colors[ImGuiCol_DragDropTarget]       = bgColor;
	colors[ImGuiCol_NavHighlight]         = titleColor;
	colors[ImGuiCol_Tab]                  = bgColor;
	colors[ImGuiCol_TabActive]            = panelActiveColor;
	colors[ImGuiCol_TabUnfocused]         = bgColor;
	colors[ImGuiCol_TabUnfocusedActive]   = panelActiveColor;
	colors[ImGuiCol_TabHovered]           = panelHoverColor;

}

static void terminate()
{
}

int main()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		return 0;
	}

	CONFIG::LoadConfig();
	HttpServer::Start();

	// Create a window context
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
	SDL_SetHint(SDL_HINT_RETURN_KEY_HIDES_IME, "1");
	
	window = SDL_CreateWindow("main", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_ALLOW_HIGHDPI);
	if (window == NULL)
		return 0;

	renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL)
		return 0;

	SDL_Rect rect;
	#if defined(STEAM)
	rect.w = 1920; rect.h = 1080;
	#else
	SDL_GetDisplayUsableBounds(0, &rect);
	#endif

	scale_factor_x = (rect.w * 1.0f) / 1440.0f;
	scale_factor_y = (rect.h * 1.0f) / 900.0f;

	InitImgui();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer_Init(renderer);
	ImGui_ImplSDLRenderer_CreateFontsTexture();

	atexit(terminate);

	GUI::RenderLoop(renderer);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	ImGui::DestroyContext();

	return 0;
}
