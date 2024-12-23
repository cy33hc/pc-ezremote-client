#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>
#include "ime_dialog.h"
#include "windows.h"

static int ime_dialog_running = 0;
static char inputTextBuffer[1024 + 1];
static char initial_ime_text[1024];
static int max_text_length;
static char title[100];

static void utf16_to_utf8(const u_int16_t *src, u_int8_t *dst)
{
  int i;
  for (i = 0; src[i]; i++)
  {
    if ((src[i] & 0xFF80) == 0)
    {
      *(dst++) = src[i] & 0xFF;
    }
    else if ((src[i] & 0xF800) == 0)
    {
      *(dst++) = ((src[i] >> 6) & 0xFF) | 0xC0;
      *(dst++) = (src[i] & 0x3F) | 0x80;
    }
    else if ((src[i] & 0xFC00) == 0xD800 && (src[i + 1] & 0xFC00) == 0xDC00)
    {
      *(dst++) = (((src[i] + 64) >> 8) & 0x3) | 0xF0;
      *(dst++) = (((src[i] >> 2) + 16) & 0x3F) | 0x80;
      *(dst++) = ((src[i] >> 4) & 0x30) | 0x80 | ((src[i + 1] << 2) & 0xF);
      *(dst++) = (src[i + 1] & 0x3F) | 0x80;
      i += 1;
    }
    else
    {
      *(dst++) = ((src[i] >> 12) & 0xF) | 0xE0;
      *(dst++) = ((src[i] >> 6) & 0x3F) | 0x80;
      *(dst++) = (src[i] & 0x3F) | 0x80;
    }
  }

  *dst = '\0';
}

static void utf8_to_utf16(const u_int8_t *src, u_int16_t *dst)
{
  int i;
  for (i = 0; src[i];)
  {
    if ((src[i] & 0xE0) == 0xE0)
    {
      *(dst++) = ((src[i] & 0x0F) << 12) | ((src[i + 1] & 0x3F) << 6) | (src[i + 2] & 0x3F);
      i += 3;
    }
    else if ((src[i] & 0xC0) == 0xC0)
    {
      *(dst++) = ((src[i] & 0x1F) << 6) | (src[i + 1] & 0x3F);
      i += 2;
    }
    else
    {
      *(dst++) = src[i];
      i += 1;
    }
  }

  *dst = '\0';
}

namespace Dialog
{
  int initImeDialog(const char *Title, const char *initialTextBuffer, int max_text_length, int type, float posx, float posy)
  {
    if (ime_dialog_running)
      return IME_DIALOG_ALREADY_RUNNING;

    if ((initialTextBuffer && strlen(initialTextBuffer) > 1023) || (Title && strlen(Title) > 99))
    {
      ime_dialog_running = 0;
      return -1;
    }

    memset(&inputTextBuffer[0], 0, sizeof(inputTextBuffer));
    memset(&initial_ime_text[0], 0, sizeof(initial_ime_text));

    if (initialTextBuffer)
    {
      snprintf(initial_ime_text, 1023, "%s", initialTextBuffer);
    }

    // converts the multibyte string src to a wide-character string starting at dest.
    snprintf(inputTextBuffer, sizeof(inputTextBuffer), "%s", initialTextBuffer);
    snprintf(title, sizeof(title), "%s", Title);

    ime_dialog_running = 1;
    SDL_StartTextInput();
    
    return 1;
  }

  int isImeDialogRunning()
  {
    return ime_dialog_running;
  }

  char *getImeDialogInputText()
  {
    return inputTextBuffer;
  }

  const char *getImeDialogInitialText()
  {
    return initial_ime_text;
  }

  int updateImeDialog()
  {
    if (!ime_dialog_running)
      return IME_DIALOG_RESULT_NONE;

    int status = IME_DIALOG_RESULT_RUNNING;
    ImGuiKey keyReturned = ImGuiKey_COUNT;
    ImGuiStyle *style = &ImGui::GetStyle();
    ImVec4 *colors = style->Colors;
    char str_id[512];

    snprintf(str_id, 512, "%s##ImePopup", title);
    ImGui::SetWindowFontScale(Windows::scaleX(1.f));
    ImGui::OpenPopup(str_id);
    ImGui::SetNextWindowPos(ImVec2(Windows::scaleX(220), Windows::scaleX(200)));
    ImGui::SetNextWindowSizeConstraints(ImVec2(Windows::scaleX(1000), Windows::scaleX(50)), ImVec2(Windows::scaleX(1000), Windows::scaleX(200)), NULL, NULL);
    if (ImGui::BeginPopupModal(str_id, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
    {
      ImGui::SetNextItemWidth(Windows::scaleX(980));
      if (ImGui::IsWindowAppearing())
      {
        ImGui::SetKeyboardFocusHere();
        ImGuiIO &io = ImGui::GetIO();
      }
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + Windows::scaleX(980));
      ImGui::InputText("##inputTextBuffer", (char *)inputTextBuffer, sizeof(inputTextBuffer));

      if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight))
      {
        ime_dialog_running = 0;
        status = IME_DIALOG_RESULT_CANCELED;
        SDL_StopTextInput();
      }
      else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_GamepadStart))
      {
        ime_dialog_running = 0;
        status = IME_DIALOG_RESULT_FINISHED;
        SDL_StopTextInput();
      }

      ImGui::EndPopup();
    }
    ImGui::SetWindowFontScale(1.0f);

    return status;
  }

} // namespace Dialog