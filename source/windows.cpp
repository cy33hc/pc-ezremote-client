#include <SDL2/SDL.h>
#include <stdio.h>
#include <algorithm>
#include <set>
#include "server/http_server.h"
#include "imgui.h"
#include "windows.h"
#include "fs.h"
#include "config.h"
#include "gui.h"
#include "actions.h"
#include "util.h"
#include "lang.h"
#include "ime_dialog.h"
#include "IconsFontAwesome6.h" 
#include "OpenFontIcons.h"

#define MAX_IMAGE_HEIGHT 980
#define MAX_IMAGE_WIDTH 1820

extern "C"
{
#include "inifile.h"
}

bool paused = false;
int view_mode;
static float scroll_direction = 0.0f;
static int selected_local_position = -1;
static int selected_remote_position = -1;

static ime_callback_t ime_callback = nullptr;
static ime_callback_t ime_after_update = nullptr;
static ime_callback_t ime_before_update = nullptr;
static ime_callback_t ime_cancelled = nullptr;
static std::vector<std::string> *ime_multi_field;
static char *ime_single_field;
static int ime_field_size;

static char txt_http_server_port[6];

bool handle_updates = false;
int64_t bytes_transfered;
int64_t bytes_to_download;
int64_t prev_tick;

std::vector<DirEntry> local_files;
std::vector<DirEntry> remote_files;
std::set<DirEntry> multi_selected_local_files;
std::set<DirEntry> multi_selected_remote_files;
std::vector<DirEntry> local_paste_files;
std::vector<DirEntry> remote_paste_files;
DirEntry selected_local_file;
DirEntry selected_remote_file;
ACTIONS selected_action;
ACTIONS paste_action;
char status_message[1024];
char local_file_to_select[256];
char remote_file_to_select[256];
char local_filter[32];
char remote_filter[32];
char dialog_editor_text[1024];
char activity_message[1024];
int selected_browser = 0;
int saved_selected_browser;
bool activity_inprogess = false;
bool stop_activity = false;
bool file_transfering = false;
bool set_focus_to_local = false;
bool set_focus_to_remote = false;
char extract_zip_folder[256];
char zip_file_path[384];
bool show_settings = false;
float scale_factor_x, scale_factor_y;
bool owner_perm[3];
bool group_perm[3];
bool other_perm[3];

// Images varaibles
bool view_image= false;

std::map<std::string, std::string> sfo_params;

// Overwrite dialog variables
bool dont_prompt_overwrite = false;
bool dont_prompt_overwrite_cb = false;
int confirm_transfer_state = -1;
int overwrite_type = OVERWRITE_PROMPT;

int confirm_state = CONFIRM_NONE;
char confirm_message[512];
ACTIONS action_to_take = ACTION_NONE;

namespace Windows
{

    void Init()
    {
        remoteclient = nullptr;

        snprintf(local_file_to_select, 256, "%s", "..");
        snprintf(remote_file_to_select, 256, "%s", "..");
        snprintf(status_message, 1024, "%s", "");
        snprintf(local_filter, 32, "%s", "");
        snprintf(remote_filter, 32, "%s", "");
        snprintf(txt_http_server_port, 6, "%d", http_server_port);
        dont_prompt_overwrite = false;
        confirm_transfer_state = -1;
        dont_prompt_overwrite_cb = false;
        overwrite_type = OVERWRITE_PROMPT;
        local_paste_files.clear();
        remote_paste_files.clear();

        Actions::RefreshLocalFiles(false);
    }

    void HandleWindowInput()
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Insert) && !paused)
        {
            if (selected_browser & LOCAL_BROWSER && strcmp(selected_local_file.name, "..") != 0)
            {
                auto search_item = multi_selected_local_files.find(selected_local_file);
                if (search_item != multi_selected_local_files.end())
                {
                    multi_selected_local_files.erase(search_item);
                }
                else
                {
                    multi_selected_local_files.insert(selected_local_file);
                }
            }
            if (selected_browser & REMOTE_BROWSER && strcmp(selected_remote_file.name, "..") != 0)
            {
                auto search_item = multi_selected_remote_files.find(selected_remote_file);
                if (search_item != multi_selected_remote_files.end())
                {
                    multi_selected_remote_files.erase(search_item);
                }
                else
                {
                    multi_selected_remote_files.insert(selected_remote_file);
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1) && !paused)
        {
            set_focus_to_remote = true;
        }

        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiKey_2) && !paused)
        {
            set_focus_to_remote = true;
        }

        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiKey_1) && !paused)
        {
            set_focus_to_local = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1) && !paused)
        {
            set_focus_to_local = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_GamepadStart) && !paused)
        {
            selected_action = ACTION_DISCONNECT_AND_EXIT;
        }

        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiKey_Q) && !paused)
        {
            selected_action = ACTION_DISCONNECT_AND_EXIT;
        }
    }

    void SetModalMode(bool modal)
    {
        paused = modal;
    }

    std::string getUniqueZipFilename()
    {
        std::string zipfolder;
        std::string zipname;
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        zipfolder = files.begin()->directory;

        if (files.size() == 1)
        {
            zipname = files.begin()->name;
        }
        else if (strcmp(files.begin()->directory, "/") == 0)
        {
            zipname = "new_zip";
        }
        else
        {
            zipname = std::string(files.begin()->directory);
            zipname = zipname.substr(zipname.find_last_of("/") + 1);
        }

        std::string zip_path;
        zip_path = zipfolder + "/" + zipname;
        int i = 0;
        while (true)
        {
            std::string temp_path;
            i > 0 ? temp_path = zip_path + "(" + std::to_string(i) + ").zip" : temp_path = zip_path + ".zip";
            if (!FS::FileExists(temp_path))
                return temp_path;
            i++;
        }
    }

    std::string getExtractFolder()
    {
        std::string zipfolder;
        std::vector<DirEntry> files;
        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;

        if (local_browser_selected)
        {
            if (multi_selected_local_files.size() > 0)
                std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_local_file);
        }
        else
        {
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_remote_file);
        }

        if (files.size() > 1)
        {
            zipfolder = local_directory;
        }
        else
        {
            std::string filename = std::string(files.begin()->name);
            size_t dot_pos = filename.find_last_of(".");
            zipfolder = std::string(local_directory) + "/" + filename.substr(0, dot_pos);
        }
        return zipfolder;
    }

    void ConnectionPanel()
    {
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        static char title[128];
        snprintf(title, 127, "ezRemote %s", lang_strings[STR_CONNECTION_SETTINGS]);
        BeginGroupPanel(title, ImVec2(scaleX(1425), scaleY(100)));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + scaleY(10));
        char id[256];
        std::string hidden_password = (strlen(remote_settings->password) > 0) ? std::string("*******") : "";
        ImVec2 pos;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(4));
        bool is_connected = remoteclient != nullptr && remoteclient->IsConnected();

        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetItemDefaultFocus();
        }
        snprintf(id, 255, "%s###connectbutton", is_connected ? lang_strings[STR_DISCONNECT] : lang_strings[STR_CONNECT]);
        if (ImGui::Button(id, ImVec2(scaleX(150), 0)))
        {
            selected_action = is_connected ? ACTION_DISCONNECT : ACTION_CONNECT;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", is_connected ? lang_strings[STR_DISCONNECT] : lang_strings[STR_CONNECT]);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(150);
        if (is_connected)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.3f);
        }
        if (ImGui::BeginCombo("##Site", display_site, ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest | ImGuiComboFlags_NoArrowButton))
        {
            static char site_id[64];
            static char site_display[512];
            for (int n = 0; n < sites.size(); n++)
            {
                const bool is_selected = strcmp(sites[n].c_str(), last_site) == 0;
                snprintf(site_id, 63, "%s %d", lang_strings[STR_SITE], n + 1);
                snprintf(site_display, 511, "%s %d    %s", lang_strings[STR_SITE], n + 1, site_settings[sites[n]].server);
                if (ImGui::Selectable(site_display, is_selected))
                {
                    snprintf(last_site, 31, "%s", sites[n].c_str());
                    snprintf(display_site, 31, "%s", site_id);
                    remote_settings = &site_settings[sites[n]];
                    snprintf(remote_directory, 255, "%s", remote_settings->default_directory);
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (is_connected)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        snprintf(id, 255, "%s##server", remote_settings->server);
        int width = 400;
        if (remote_settings->type == CLIENT_TYPE_HTTP_SERVER)
            width = 400;
        else if (remote_settings->type == CLIENT_TYPE_NFS)
            width = 800;
        pos = ImGui::GetCursorPos();

        if (ImGui::Button(id, ImVec2(scaleX(width), 0)))
        {
            ime_single_field = remote_settings->server;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_callback = SingleValueImeCallback;
            ime_after_update = AferServerChangeCallback;
            Dialog::initImeDialog(lang_strings[STR_SERVER], remote_settings->server, 255, 1, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", remote_settings->server);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();

        if (remote_settings->type == CLIENT_TYPE_HTTP_SERVER)
        {
            ImGui::SetNextItemWidth(100);
            if (ImGui::BeginCombo("##HttpServer", remote_settings->http_server_type, ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest | ImGuiComboFlags_NoArrowButton))
            {
                for (int n = 0; n < http_servers.size(); n++)
                {
                    const bool is_selected = strcmp(http_servers[n].c_str(), remote_settings->http_server_type) == 0;
                    if (ImGui::Selectable(http_servers[n].c_str(), is_selected))
                    {
                        snprintf(remote_settings->http_server_type, 24, "%s", http_servers[n].c_str());
                    }
                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
        }

        if (remote_settings->type != CLIENT_TYPE_NFS)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(5));
            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_USERNAME]);
            ImGui::SameLine();

            width = 120;
            snprintf(id, 256, "%s##username", remote_settings->username);
            pos = ImGui::GetCursorPos();
            if (ImGui::Button(id, ImVec2(scaleX(width), 0)))
            {
                ime_single_field = remote_settings->username;
                ResetImeCallbacks();
                ime_field_size = 32;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_USERNAME], remote_settings->username, 32, 1, pos.x, pos.y);
                gui_mode = GUI_MODE_IME;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("%s", remote_settings->username);
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
        }
        
        if (remote_settings->type != CLIENT_TYPE_NFS)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(5));
            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_PASSWORD]);
            ImGui::SameLine();

            snprintf(id, 256, "%s##password", hidden_password.c_str());
            pos = ImGui::GetCursorPos();
            if (ImGui::Button(id, ImVec2(scaleX(100), 0)))
            {
                ime_single_field = remote_settings->password;
                ResetImeCallbacks();
                ime_field_size = 127;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_PASSWORD], remote_settings->password, 127, 1, pos.x, pos.y);
                gui_mode = GUI_MODE_IME;
            }
        }

        ImGui::PopStyleVar();

        ImGui::SameLine();
        ImGui::SetCursorPosX(scaleX(1380));
        if (ImGui::Button(ICON_FA_GEAR, ImVec2(scaleX(35), 0)))
        {
            show_settings = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_SETTINGS]);
            ImGui::EndTooltip();
        }

        ImGui::Dummy(ImVec2(0, scaleY(10)));
        EndGroupPanel();
    }

    void BrowserPanel()
    {
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        selected_browser = 0;
        ImVec2 pos;

        ImGui::Dummy(ImVec2(0, scaleY(5)));
        BeginGroupPanel(lang_strings[STR_LOCAL], ImVec2(scaleX(708), scaleY(708)));
        ImGui::Dummy(ImVec2(0, scaleY(5)));

        float posX = ImGui::GetCursorPosX();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_DIRECTORY]);
        ImGui::SameLine();
        ImVec2 size = ImGui::CalcTextSize(local_directory);
        ImGui::SetCursorPosX(posX + scaleX(150));
        ImGui::PushID("local_directory##local");
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(local_directory, ImVec2(scaleX(370), 0)))
        {
            ime_single_field = local_directory;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_after_update = AfterLocalFileChangesCallback;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_DIRECTORY], local_directory, 256, 1, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::PopID();
        ImGui::PopStyleVar();
        if (size.x > 560 && ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", local_directory);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();

        ImGui::PushID("refresh##local");
        if (ImGui::Button(lang_strings[STR_REFRESH], ImVec2(scaleX(150), 0)))
        {
            selected_action = ACTION_REFRESH_LOCAL_FILES;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_REFRESH]);
            ImGui::EndTooltip();
        }
        ImGui::PopID();

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_FILTER]);
        ImGui::SameLine();
        ImGui::SetCursorPosX(posX + scaleX(150));
        ImGui::PushID("local_filter##local");
        if (ImGui::Button(local_filter, ImVec2(scaleX(370), 0)))
        {
            ime_single_field = local_filter;
            ResetImeCallbacks();
            ime_field_size = 31;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_FILTER], local_filter, 31, 1, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::PopID();
        ImGui::PopStyleVar();
        ImGui::SameLine();

        ImGui::PushID("search##local");
        if (ImGui::Button(lang_strings[STR_SEARCH], ImVec2(scaleX(150), 0)))
        {
            selected_action = ACTION_APPLY_LOCAL_FILTER;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_SEARCH]);
            ImGui::EndTooltip();
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + scaleY(10));
        ImGui::BeginChild("Local##ChildWindow", ImVec2(scaleX(679), scaleY(555)));
        ImGui::Separator();
        ImGui::Columns(2, "Local##Columns", true);
        int i = 0;
        if (set_focus_to_local)
        {
            set_focus_to_local = false;
            ImGui::SetWindowFocus();
        }
        for (int j = 0; j < local_files.size(); j++)
        {
            DirEntry item = local_files[j];
            ImGui::SetColumnWidth(-1, scaleX(530));
            ImGui::PushID(i);
            auto search_item = multi_selected_local_files.find(item);
            if (search_item != multi_selected_local_files.end())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            }
            if (ImGui::Selectable(item.name, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(scaleX(679), 0)))
            {
                selected_local_file = item;
                if (item.isDir)
                {
                    selected_action = ACTION_CHANGE_LOCAL_DIRECTORY;
                }
            }
            ImGui::PopID();
            if (ImGui::IsItemFocused())
            {
                selected_local_file = item;
            }
            if (ImGui::IsItemHovered())
            {
                if (ImGui::CalcTextSize(item.name).x > 530)
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", item.name);
                    ImGui::EndTooltip();
                }
                if ((ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) && !paused)
                {
                    if (j == 0)
                    {
                        selected_local_position = local_files.size()-1;
                        scroll_direction = 0.0f;
                    }
                }
                else if ((ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) && !paused)
                {
                    if (j == local_files.size()-1)
                    {
                        selected_local_position = 0;
                        scroll_direction = 1.0f;
                    }
                }
            }
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            {
                if (strcmp(local_file_to_select, item.name) == 0)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(0.5f);
                    snprintf(local_file_to_select, 256, "%s", "");
                }
                if (selected_local_position == j && !paused)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(scroll_direction);
                    selected_local_position = -1;
                }
                selected_browser |= LOCAL_BROWSER;
            }
            ImGui::NextColumn();
            ImGui::SetColumnWidth(-1, scaleX(150));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(item.display_size).x - ImGui::GetScrollX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::Text("%s", item.display_size);
            if (search_item != multi_selected_local_files.end())
            {
                ImGui::PopStyleColor();
            }
            ImGui::NextColumn();
            ImGui::Separator();
            i++;
        }
        ImGui::Columns(1);
        ImGui::EndChild();
        EndGroupPanel();
        ImGui::SameLine();

        BeginGroupPanel(lang_strings[STR_REMOTE], ImVec2(scaleX(708), scaleY(708)));
        ImGui::Dummy(ImVec2(0, scaleY(5)));
        posX = ImGui::GetCursorPosX();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_DIRECTORY]);
        ImGui::SameLine();
        size = ImGui::CalcTextSize(remote_directory);
        ImGui::SetCursorPosX(posX + scaleX(150));
        ImGui::PushID("remote_directory##remote");
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(remote_directory, ImVec2(scaleX(370), 0)))
        {
            ime_single_field = remote_directory;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_after_update = AfterRemoteFileChangesCallback;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_DIRECTORY], remote_directory, 256, 1, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::PopID();
        ImGui::PopStyleVar();
        if (size.x > 560 && ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", remote_directory);
            ImGui::EndTooltip();
        }

        ImGui::SameLine();
        ImGui::PushID("refresh##remote");
        if (ImGui::Button(lang_strings[STR_REFRESH], ImVec2(scaleX(150), 0)))
        {
            selected_action = ACTION_REFRESH_REMOTE_FILES;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_REFRESH]);
            ImGui::EndTooltip();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_FILTER]);
        ImGui::SameLine();
        ImGui::SetCursorPosX(posX + scaleX(150));
        ImGui::PushID("remote_filter##remote");
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(remote_filter, ImVec2(scaleX(370), 0)))
        {
            ime_single_field = remote_filter;
            ResetImeCallbacks();
            ime_field_size = 31;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_FILTER], remote_filter, 31, 1, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        };
        ImGui::PopID();
        ImGui::PopStyleVar();
        ImGui::SameLine();

        ImGui::PushID("search##remote");
        if (ImGui::Button(lang_strings[STR_SEARCH], ImVec2(scaleX(150), 0)))
        {
            selected_action = ACTION_APPLY_REMOTE_FILTER;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_SEARCH]);
            ImGui::EndTooltip();
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + scaleY(10));
        ImGui::BeginChild(ImGui::GetID("Remote##ChildWindow"), ImVec2(scaleX(679), scaleY(555)));
        if (set_focus_to_remote)
        {
            set_focus_to_remote = false;
            ImGui::SetWindowFocus();
        }
        ImGui::Separator();
        ImGui::Columns(2, "Remote##Columns", true);
        i = 99999;
        for (int j = 0; j < remote_files.size(); j++)
        {
            DirEntry item = remote_files[j];

            ImGui::SetColumnWidth(-1, scaleX(530));
            auto search_item = multi_selected_remote_files.find(item);
            if (search_item != multi_selected_remote_files.end())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            }
            ImGui::PushID(i);
            if (ImGui::Selectable(item.name, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(scaleX(919), 0)))
            {
                selected_remote_file = item;
                if (item.isDir)
                {
                    selected_action = ACTION_CHANGE_REMOTE_DIRECTORY;
                }
            }
            if (ImGui::IsItemFocused())
            {
                selected_remote_file = item;
            }
            if (ImGui::IsItemHovered())
            {
                if (ImGui::CalcTextSize(item.name).x > 530)
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", item.name);
                    ImGui::EndTooltip();
                }
                if ((ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) && !paused)
                {
                    if (j == 0)
                    {
                        selected_remote_position = remote_files.size()-1;
                        scroll_direction = 0.0f;
                    }
                }
                else if ((ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) && !paused)
                {
                    if (j == remote_files.size()-1)
                    {
                        selected_remote_position = 0;
                        scroll_direction = 1.0f;
                    }
                }
            }
            ImGui::PopID();
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            {
                if (strcmp(remote_file_to_select, item.name) == 0)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(0.5f);
                    snprintf(remote_file_to_select, 256, "%s", "");
                }
                if (selected_remote_position == j && !paused)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(scroll_direction);
                    selected_remote_position = -1;
                }
                selected_browser |= REMOTE_BROWSER;
            }
            ImGui::NextColumn();
            ImGui::SetColumnWidth(-1, scaleX(150));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(item.display_size).x - ImGui::GetScrollX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::Text("%s", item.display_size);
            if (search_item != multi_selected_remote_files.end())
            {
                ImGui::PopStyleColor();
            }
            ImGui::NextColumn();
            ImGui::Separator();
            i++;
        }
        ImGui::Columns(1);
        ImGui::EndChild();
        EndGroupPanel();

        if ((ImGui::IsKeyPressed(ImGuiKey_F8) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) && !paused)
        {
            if (selected_browser & LOCAL_BROWSER)
            {
                selected_local_file = local_files[0];
                selected_action = ACTION_CHANGE_LOCAL_DIRECTORY;
            }
            else if (selected_browser & REMOTE_BROWSER)
            {
                if (remoteclient != nullptr && remote_files.size() > 0)
                {
                    selected_remote_file = remote_files[0];
                    selected_action = ACTION_CHANGE_REMOTE_DIRECTORY;
                }
            }
        }
    }

    void StatusPanel()
    {
        ImGui::Dummy(ImVec2(0, scaleY(5)));
        BeginGroupPanel(lang_strings[STR_MESSAGES], ImVec2(scaleX(1425), scaleY(100)));
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::Dummy(ImVec2(scaleX(1400), scaleY(30)));
        ImGui::SetCursorPos(pos);
        ImGui::SetCursorPosX(pos.x + scaleX(10));
        ImGui::PushTextWrapPos(1370);
        if (strncmp(status_message, "4", 1) == 0 || strncmp(status_message, "3", 1) == 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", status_message);
        }
        else
        {
            ImGui::Text("%s", status_message);
        }
        ImGui::PopTextWrapPos();
        ImGui::SameLine();
        EndGroupPanel();
    }

    int getSelectableFlag(uint32_t remote_action)
    {
        int flag = ImGuiSelectableFlags_Disabled;
        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;

        if ((local_browser_selected && selected_local_file.selectable) ||
            (remote_browser_selected && selected_remote_file.selectable &&
             remoteclient != nullptr && (remoteclient->SupportedActions() & remote_action)))
        {
            flag = ImGuiSelectableFlags_None;
        }
        return flag;
    }

    void ShowActionsDialog()
    {
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        int flags;

        if (ImGui::IsKeyPressed(ImGuiMod_Alt) && !paused)
        {
            if (!paused)
                saved_selected_browser = selected_browser;

            if (saved_selected_browser > 0)
            {
                SetModalMode(true);
                ImGui::OpenPopup(lang_strings[STR_ACTIONS]);
            }
        }

        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;
        if (local_browser_selected)
        {
            ImGui::SetNextWindowPos(ImVec2(scaleX(245), scaleX(210)));
        }
        else if (remote_browser_selected)
        {
            ImGui::SetNextWindowPos(ImVec2(scaleX(965), scaleX(210)));
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(scaleX(230), scaleX(150)), ImVec2(scaleX(230), scaleX(660)), NULL, NULL);
        if (ImGui::BeginPopupModal(lang_strings[STR_ACTIONS], NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::PushID("Select All##settings");
            if (ImGui::Selectable(lang_strings[STR_SELECT_ALL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                SetModalMode(false);
                if (local_browser_selected)
                    selected_action = ACTION_LOCAL_SELECT_ALL;
                else if (remote_browser_selected)
                    selected_action = ACTION_REMOTE_SELECT_ALL;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Clear All##settings");
            if (ImGui::Selectable(lang_strings[STR_CLEAR_ALL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                SetModalMode(false);
                if (local_browser_selected)
                    selected_action = ACTION_LOCAL_CLEAR_ALL;
                else if (remote_browser_selected)
                    selected_action = ACTION_REMOTE_CLEAR_ALL;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Cut##settings");
            if (ImGui::Selectable(lang_strings[STR_CUT], false, getSelectableFlag(REMOTE_ACTION_CUT) | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                selected_action = local_browser_selected ? ACTION_LOCAL_CUT : ACTION_REMOTE_CUT;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Copy##settings");
            if (ImGui::Selectable(lang_strings[STR_COPY], false, getSelectableFlag(REMOTE_ACTION_COPY) | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                selected_action = local_browser_selected ? ACTION_LOCAL_COPY : ACTION_REMOTE_COPY;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Paste##settings");
            flags = ImGuiSelectableFlags_Disabled;
            if ((local_browser_selected && local_paste_files.size() > 0) ||
                (remote_browser_selected && remote_paste_files.size() > 0 &&
                 remoteclient != nullptr && (remoteclient->SupportedActions() | REMOTE_ACTION_PASTE)))
                flags = ImGuiSelectableFlags_None;
            if (ImGui::Selectable(lang_strings[STR_PASTE], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                SetModalMode(false);
                selected_action = local_browser_selected ? ACTION_LOCAL_PASTE : ACTION_REMOTE_PASTE;
                file_transfering = true;
                confirm_transfer_state = 0;
                dont_prompt_overwrite_cb = dont_prompt_overwrite;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
            {
                int height = local_browser_selected ? (local_paste_files.size() * 30) + 42 : (remote_paste_files.size() * 30) + 42;
                ImGui::SetNextWindowSize(ImVec2(scaleX(500), scaleX(height)));
                ImGui::BeginTooltip();
                int text_width = ImGui::CalcTextSize(lang_strings[STR_FILES]).x;
                int file_pos = ImGui::GetCursorPosX() + text_width + scaleX(15);
                ImGui::Text("%s: %s", lang_strings[STR_TYPE], (paste_action == ACTION_LOCAL_CUT | paste_action == ACTION_REMOTE_CUT) ? lang_strings[STR_CUT] : lang_strings[STR_COPY]);
                ImGui::Text("%s:", lang_strings[STR_FILES]);
                ImGui::SameLine();
                std::vector<DirEntry> files = (local_browser_selected) ? local_paste_files : remote_paste_files;
                for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
                {
                    ImGui::SetCursorPosX(file_pos);
                    ImGui::Text("%s", it->path);
                }
                ImGui::EndTooltip();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Delete##settings");
            if (ImGui::Selectable(lang_strings[STR_DELETE], false, getSelectableFlag(REMOTE_ACTION_DELETE) | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                confirm_state = CONFIRM_WAIT;
                snprintf(confirm_message, 512, "%s", lang_strings[STR_DEL_CONFIRM_MSG]);
                if (local_browser_selected)
                    action_to_take = ACTION_DELETE_LOCAL;
                else if (remote_browser_selected)
                    action_to_take = ACTION_DELETE_REMOTE;
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Rename##settings");
            flags = getSelectableFlag(REMOTE_ACTION_RENAME);
            if ((local_browser_selected && multi_selected_local_files.size() > 1) ||
                (remote_browser_selected && multi_selected_remote_files.size() > 1))
                flags = ImGuiSelectableFlags_Disabled;
            if (ImGui::Selectable(lang_strings[STR_RENAME], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_RENAME_LOCAL;
                else if (remote_browser_selected)
                    selected_action = ACTION_RENAME_REMOTE;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("setdefaultfolder##settings");
            if (ImGui::Selectable(lang_strings[STR_SET_DEFAULT_DIRECTORY], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_SET_DEFAULT_LOCAL_FOLDER;
                else if (remote_browser_selected)
                    selected_action = ACTION_SET_DEFAULT_REMOTE_FOLDER;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("New Folder##settings");
            flags = ImGuiSelectableFlags_None;
            if (remote_browser_selected && remoteclient != nullptr && !(remoteclient->SupportedActions() & REMOTE_ACTION_NEW_FOLDER))
            {
                flags = ImGuiSelectableFlags_Disabled;
            }
            if (ImGui::Selectable(lang_strings[STR_NEW_FOLDER], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_NEW_LOCAL_FOLDER;
                else if (remote_browser_selected)
                    selected_action = ACTION_NEW_REMOTE_FOLDER;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("New File##settings");
            flags = ImGuiSelectableFlags_None;
            if (remote_browser_selected && remoteclient != nullptr && !(remoteclient->SupportedActions() & REMOTE_ACTION_NEW_FILE))
            {
                flags = ImGuiSelectableFlags_Disabled;
            }
            if (ImGui::Selectable(lang_strings[STR_NEW_FILE], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_NEW_LOCAL_FILE;
                else if (remote_browser_selected)
                    selected_action = ACTION_NEW_REMOTE_FILE;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Extract##settings");
            if (ImGui::Selectable(lang_strings[STR_EXTRACT], false, getSelectableFlag(REMOTE_ACTION_EXTRACT) | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                ResetImeCallbacks();
                snprintf(extract_zip_folder, 256, "%s", getExtractFolder().c_str());
                ime_single_field = extract_zip_folder;
                ime_field_size = 255;
                ime_callback = SingleValueImeCallback;
                if (local_browser_selected)
                    ime_after_update = AfterExtractFolderCallback;
                else
                    ime_after_update = AfterExtractRemoteFolderCallback;
                Dialog::initImeDialog(lang_strings[STR_EXTRACT_LOCATION], extract_zip_folder, 255, 1, 600, 350);
                gui_mode = GUI_MODE_IME;
                file_transfering = false;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            if (local_browser_selected)
            {
                ImGui::PushID("Compress##settings");
                if (ImGui::Selectable(lang_strings[STR_COMPRESS], false, getSelectableFlag(REMOTE_ACTION_NONE) | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
                {
                    std::string zipname;
                    std::string zipfolder;

                    ResetImeCallbacks();
                    snprintf(zip_file_path, 384, "%s", getUniqueZipFilename().c_str());
                    ime_single_field = zip_file_path;
                    ime_field_size = 383;
                    ime_callback = SingleValueImeCallback;
                    ime_after_update = AfterZipFileCallback;
                    Dialog::initImeDialog(lang_strings[STR_ZIP_FILE_PATH], zip_file_path, 383, 1, 600, 350);
                    gui_mode = GUI_MODE_IME;
                    file_transfering = true;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();

                flags = getSelectableFlag(REMOTE_ACTION_UPLOAD);
                if (local_browser_selected && remoteclient != nullptr && !(remoteclient->SupportedActions() & REMOTE_ACTION_UPLOAD))
                {
                    flags = ImGuiSelectableFlags_Disabled;
                }
                ImGui::PushID("Upload##settings");
                if (ImGui::Selectable(lang_strings[STR_UPLOAD], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_UPLOAD;
                    file_transfering = true;
                    confirm_transfer_state = 0;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();
            }

            if (remote_browser_selected)
            {
                ImGui::PushID("Download##settings");
                if (ImGui::Selectable(lang_strings[STR_DOWNLOAD], false, getSelectableFlag(REMOTE_ACTION_DOWNLOAD) | ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_DOWNLOAD;
                    file_transfering = true;
                    confirm_transfer_state = 0;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();

            }

            ImGui::PushID("Properties##settings");
            if (ImGui::Selectable(lang_strings[STR_PROPERTIES], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                if (local_browser_selected)
                {
                    owner_perm[0] = selected_local_file.st_mode & S_IRUSR;
                    owner_perm[1] = selected_local_file.st_mode & S_IWUSR;
                    owner_perm[2] = selected_local_file.st_mode & S_IXUSR;
                    group_perm[0] = selected_local_file.st_mode & S_IRGRP;
                    group_perm[1] = selected_local_file.st_mode & S_IWGRP;
                    group_perm[2] = selected_local_file.st_mode & S_IXGRP;
                    other_perm[0] = selected_local_file.st_mode & S_IROTH;
                    other_perm[1] = selected_local_file.st_mode & S_IWOTH;
                    other_perm[2] = selected_local_file.st_mode & S_IXOTH;

                    selected_action = ACTION_SHOW_LOCAL_PROPERTIES;
                }
                else if (remote_browser_selected)
                    selected_action = ACTION_SHOW_REMOTE_PROPERTIES;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Cancel##settings");
            if (ImGui::Selectable(lang_strings[STR_CANCEL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(scaleX(220), 0)))
            {
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetItemDefaultFocus();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (confirm_state == CONFIRM_WAIT)
        {
            ImGui::OpenPopup(lang_strings[STR_CONFIRM]);
            ImGui::SetNextWindowPos(ImVec2(scaleX(400), scaleX(320)));
            ImGui::SetNextWindowSizeConstraints(ImVec2(scaleX(640), scaleX(100)), ImVec2(scaleX(640), scaleX(200)), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_CONFIRM], NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + scaleX(620));
                ImGui::Text("%s", confirm_message);
                ImGui::PopTextWrapPos();
                ImGui::NewLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(220));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + scaleY(5));
                if (ImGui::Button(lang_strings[STR_NO], ImVec2(scaleX(100), 0)))
                {
                    confirm_state = CONFIRM_NO;
                    selected_action = ACTION_NONE;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                };
                ImGui::SameLine();
                if (ImGui::Button(lang_strings[STR_YES], ImVec2(scaleX(100), 0)))
                {
                    confirm_state = CONFIRM_YES;
                    selected_action = action_to_take;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        if (confirm_transfer_state == 0)
        {
            ImGui::OpenPopup(lang_strings[STR_OVERWRITE_OPTIONS]);
            ImGui::SetNextWindowPos(ImVec2(scaleX(400), scaleX(300)));
            ImGui::SetNextWindowSizeConstraints(ImVec2(scaleX(640), scaleX(100)), ImVec2(scaleX(640), scaleX(400)), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_OVERWRITE_OPTIONS], NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
            {
                ImGui::RadioButton(lang_strings[STR_DONT_OVERWRITE], &overwrite_type, 0);
                ImGui::RadioButton(lang_strings[STR_ASK_FOR_CONFIRM], &overwrite_type, 1);
                ImGui::RadioButton(lang_strings[STR_DONT_ASK_CONFIRM], &overwrite_type, 2);
                ImGui::Separator();
                ImGui::Checkbox("##AlwaysUseOption", &dont_prompt_overwrite_cb);
                ImGui::SameLine();
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + scaleX(625));
                ImGui::Text("%s", lang_strings[STR_ALLWAYS_USE_OPTION]);
                ImGui::PopTextWrapPos();
                ImGui::Separator();

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(170));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + scaleY(5));
                if (ImGui::Button(lang_strings[STR_CANCEL], ImVec2(scaleX(150), 0)))
                {
                    confirm_transfer_state = 2;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    selected_action = ACTION_NONE;
                    ImGui::CloseCurrentPopup();
                };
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::SameLine();
                if (ImGui::Button(lang_strings[STR_CONTINUE], ImVec2(scaleX(150), 0)))
                {
                    confirm_transfer_state = 1;
                    dont_prompt_overwrite = dont_prompt_overwrite_cb;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void ShowPropertiesDialog(DirEntry item)
    {
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        SetModalMode(true);
        ImGui::OpenPopup(lang_strings[STR_PROPERTIES]);

        ImGui::SetNextWindowPos(ImVec2(scaleX(370), scaleX(250)));
        ImGui::SetNextWindowSizeConstraints(ImVec2(scaleX(700), scaleX(80)), ImVec2(scaleX(700), scaleX(400)), NULL, NULL);
        if (ImGui::BeginPopupModal(lang_strings[STR_PROPERTIES], NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_TYPE]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(scaleX(150));
            ImGui::Text("%s", item.isDir ? lang_strings[STR_FOLDER] : lang_strings[STR_FILE]);
            ImGui::Separator();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_NAME]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(scaleX(150));
            ImGui::TextWrapped("%s", item.name);
            ImGui::Separator();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_SIZE]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(scaleX(150));
            ImGui::Text("%ld   (%s)", item.file_size, item.display_size);
            ImGui::Separator();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_DATE]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(scaleX(150));
            ImGui::Text("%02d/%02d/%d %02d:%02d:%02d", item.modified.day, item.modified.month, item.modified.year,
                        item.modified.hours, item.modified.minutes, item.modified.seconds);
            ImGui::Separator();

            if (saved_selected_browser == LOCAL_BROWSER && strcmp(selected_local_file.name, "..") != 0)
            {
                float pos_x = ImGui::GetCursorPosX();
                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_PERMISSIONS]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(300));
                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_READ]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(400));
                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_WRITE]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(500));
                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_EXECUTE]);

                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_OWNER]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(310));
                ImGui::Checkbox("##owner_read", &owner_perm[0]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(410));
                ImGui::Checkbox("##owner_write", &owner_perm[1]);
                ImGui::SameLine();ImGui:: SetCursorPosX(pos_x + scaleX(510));
                ImGui::Checkbox("##owner_execute", &owner_perm[2]);

                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_GROUP]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(310));
                ImGui::Checkbox("##group_read", &group_perm[0]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(410));
                ImGui::Checkbox("##group_write", &group_perm[1]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(510));
                ImGui::Checkbox("##group_execute", &group_perm[2]);

                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_OTHER]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(310));
                ImGui::Checkbox("##other_read", &other_perm[0]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(410));
                ImGui::Checkbox("##other_write", &other_perm[1]);
                ImGui::SameLine(); ImGui::SetCursorPosX(pos_x + scaleX(510));
                ImGui::Checkbox("##other_execute", &other_perm[2]);

                ImGui::Separator();
            }

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(300));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + scaleY(5));
            if (ImGui::Button(lang_strings[STR_CLOSE], ImVec2(scaleX(100), 0)))
            {
                SetModalMode(false);
                selected_action = ACTION_NONE;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetItemDefaultFocus();
            }

            if (saved_selected_browser == LOCAL_BROWSER && strcmp(selected_local_file.name, "..") != 0)
            {
                ImGui::SameLine();
                if (ImGui::Button(lang_strings[STR_UPDATE], ImVec2(scaleX(100), 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_UPDATE_PERMISSION;
                    ImGui::CloseCurrentPopup();
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                SetModalMode(false);
                selected_action = ACTION_NONE;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void ShowProgressDialog()
    {
        if (activity_inprogess)
        {
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGuiStyle *style = &ImGui::GetStyle();
            ImVec4 *colors = style->Colors;

            SetModalMode(true);
            ImGui::OpenPopup(lang_strings[STR_PROGRESS]);

            ImGui::SetNextWindowPos(ImVec2(scaleX(400), scaleX(300)));
            ImGui::SetNextWindowSizeConstraints(ImVec2(scaleX(640), scaleX(80)), ImVec2(scaleX(640), scaleX(400)), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_PROGRESS], NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
            {
                ImVec2 cur_pos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(cur_pos);
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + scaleX(620));
                ImGui::Text("%s", activity_message);
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPosY(cur_pos.y + scaleX(95));

                if (file_transfering)
                {
                    static float progress = 0.0f;
                    static double transfer_speed = 0.0f;
                    static char progress_text[32];
                    static uint64_t cur_tick;
                    static double tick_delta;
                   
                    cur_tick = GetMyTick();

                    tick_delta = (cur_tick - prev_tick) * 1.0f / 1000000000.0f;

                    progress = bytes_transfered * 1.0f / (float)bytes_to_download;
                    transfer_speed = (bytes_transfered * 1.0f / tick_delta) / 1048576.0f;

                    snprintf(progress_text, 32, "%.2f MB/s", transfer_speed);
                    ImGui::ProgressBar(progress, ImVec2(scaleX(625), 0), progress_text);
                }

                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(245));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + scaleY(5));
                if (ImGui::Button(lang_strings[STR_CANCEL], ImVec2(scaleX(150), 0)))
                {
                    stop_activity = true;
                    SetModalMode(false);
                }
                if (stop_activity)
                {
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + scaleX(620));
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", lang_strings[STR_CANCEL_ACTION_MSG]);
                    ImGui::PopTextWrapPos();
                }
                ImGui::EndPopup();
            }
        }
    }

    void ShowSettingsDialog()
    {
        if (show_settings && gui_mode != GUI_MODE_IME)
        {
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGuiStyle *style = &ImGui::GetStyle();
            ImVec4 *colors = style->Colors;

            SetModalMode(true);
            ImGui::OpenPopup(lang_strings[STR_SETTINGS]);

            ImGui::SetNextWindowPos(ImVec2(scaleX(570), scaleX(80)));
            ImGui::SetNextWindowSizeConstraints(ImVec2(scaleX(850), scaleX(80)), ImVec2(scaleX(850), scaleX(900)), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_SETTINGS], NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
            {
                char id[256];
                ImVec2 field_size;
                float width;

                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s", lang_strings[STR_GLOBAL]);
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::Text("%s", lang_strings[STR_LANGUAGE]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::SetNextItemWidth(scaleX(690));
                if (ImGui::BeginCombo("##Language", language, ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest))
                {
                    for (int n = 0; n < langs.size(); n++)
                    {
                        const bool is_selected = strcmp(langs[n].c_str(), language) == 0;
                        if (ImGui::Selectable(langs[n].c_str(), is_selected))
                        {
                            snprintf(language, 128, "%s", langs[n].c_str());
                        }

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Separator();

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::Text("%s", lang_strings[STR_SHOW_HIDDEN_FILES]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(scaleX(805));
                ImGui::Checkbox("##show_hidden_files", &show_hidden_files);
                ImGui::Separator();

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::Text("%s", lang_strings[STR_TEMP_DIRECTORY]);
                ImGui::SameLine();
                field_size = ImGui::CalcTextSize(lang_strings[STR_TEMP_DIRECTORY]);
                width = field_size.x + scaleX(45);
                snprintf(id, 256, "%s##temp_direcotry", temp_folder);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
                if (ImGui::Button(id, ImVec2(scaleX(835)-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = temp_folder;
                    ime_field_size = 512;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_COMPRESSED_FILE_PATH], temp_folder, 255, 1, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::PopStyleVar();
                ImGui::Separator();

                // Web Server settings
                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s", lang_strings[STR_WEB_SERVER]);
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::Text("%s", lang_strings[STR_ENABLE]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(scaleX(805));
                ImGui::Checkbox("##web_server_enabled", &web_server_enabled);
                ImGui::Separator();

                field_size = ImGui::CalcTextSize(lang_strings[STR_PORT]);
                width = field_size.x + scaleX(45);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::Text("%s", lang_strings[STR_PORT]);
                ImGui::SameLine();
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));

                snprintf(id, 256, "%s##http_server_port", txt_http_server_port);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                if (ImGui::Button(id, ImVec2(scaleX(835)-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = txt_http_server_port;
                    ime_field_size = 5;
                    ime_callback = SingleValueImeCallback;
                    ime_after_update = AfterHttpPortChangeCallback;
                    Dialog::initImeDialog(lang_strings[STR_PORT], txt_http_server_port, 5, 2, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::Text("%s", lang_strings[STR_COMPRESSED_FILE_PATH]);
                ImGui::SameLine();
                field_size = ImGui::CalcTextSize(lang_strings[STR_COMPRESSED_FILE_PATH]);
                width = field_size.x + scaleX(45);
                snprintf(id, 256, "%s##compressed_file_path", compressed_file_path);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                if (ImGui::Button(id, ImVec2(scaleX(835)-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = compressed_file_path;
                    ime_field_size = 512;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_COMPRESSED_FILE_PATH], compressed_file_path, 512, 1, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::PopStyleVar();
                ImGui::Separator();

                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s", lang_strings[STR_ALLDEBRID]);
                ImGui::Separator();

                field_size = ImGui::CalcTextSize(lang_strings[STR_API_KEY]);
                width = field_size.x + scaleX(45);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::Text("%s", lang_strings[STR_API_KEY]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(width);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));

                if (strlen(alldebrid_api_key) > 0)
                    snprintf(id, 256, "%s", "*********************************************##alldebrid_api_key");
                else
                    snprintf(id, 256, "%s", "##alldebrid_api_key");
                if (ImGui::Button(id, ImVec2(scaleX(855-width), 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = alldebrid_api_key;
                    ime_field_size = 63;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_API_KEY], alldebrid_api_key, 63, 1, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::PopStyleVar();
                ImGui::Separator();

                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s", lang_strings[STR_REALDEBRID]);
                ImGui::Separator();

                field_size = ImGui::CalcTextSize(lang_strings[STR_API_KEY]);
                width = field_size.x + scaleX(45);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + scaleX(15));
                ImGui::Text("%s", lang_strings[STR_API_KEY]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(width);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));

                if (strlen(realdebrid_api_key) > 0)
                    snprintf(id, 256, "%s", "*********************************************##realdebrid_api_key");
                else
                    snprintf(id, 256, "%s", "##realdebrid_api_key");
                if (ImGui::Button(id, ImVec2(scaleX(855-width), 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = realdebrid_api_key;
                    ime_field_size = 63;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_API_KEY], realdebrid_api_key, 63, 1, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::PopStyleVar();
                ImGui::Separator();

                snprintf(id, 256,"%s##settings", lang_strings[STR_CLOSE]);
                if (ImGui::Button(id, ImVec2(scaleX(835), 0)))
                {
                    show_settings = false;
                    CONFIG::SaveGlobalConfig();
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::SameLine();

                if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || ImGui::IsKeyPressed(ImGuiKey_Escape))
                {
                    show_settings = false;
                    CONFIG::SaveGlobalConfig();
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
    }

    void MainWindow()
    {
        Windows::SetupWindow();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);

        if (ImGui::Begin("Remote Client", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar))
        {
            ConnectionPanel();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + scaleY(3));
            BrowserPanel();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + scaleY(3));
            StatusPanel();
            ShowProgressDialog();
            ShowActionsDialog();
            ShowSettingsDialog();
        }
        ImGui::End();
    }

    void ExecuteActions()
    {
        std::vector<char> sfo;
        uint32_t mode = 0;
        int ret;

        switch (selected_action)
        {
        case ACTION_CHANGE_LOCAL_DIRECTORY:
            Actions::HandleChangeLocalDirectory(selected_local_file);
            break;
        case ACTION_CHANGE_REMOTE_DIRECTORY:
            Actions::HandleChangeRemoteDirectory(selected_remote_file);
            break;
        case ACTION_REFRESH_LOCAL_FILES:
            Actions::HandleRefreshLocalFiles();
            break;
        case ACTION_REFRESH_REMOTE_FILES:
            Actions::HandleRefreshRemoteFiles();
            break;
        case ACTION_APPLY_LOCAL_FILTER:
            Actions::RefreshLocalFiles(true);
            selected_action = ACTION_NONE;
            break;
        case ACTION_APPLY_REMOTE_FILTER:
            Actions::RefreshRemoteFiles(true);
            selected_action = ACTION_NONE;
            break;
        case ACTION_NEW_LOCAL_FOLDER:
        case ACTION_NEW_REMOTE_FOLDER:
        case ACTION_NEW_LOCAL_FILE:
        case ACTION_NEW_REMOTE_FILE:
            if (gui_mode != GUI_MODE_IME)
            {
                snprintf(dialog_editor_text, 1024, "%s", "");
                ime_single_field = dialog_editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                ImVec2 pos = (selected_action == ACTION_NEW_LOCAL_FOLDER || selected_action == ACTION_NEW_LOCAL_FILE) ? ImVec2(scaleX(410), scaleX(350)) : ImVec2(scaleX(1330), scaleX(350));
                Dialog::initImeDialog((selected_action == ACTION_NEW_LOCAL_FILE || selected_action == ACTION_NEW_REMOTE_FILE)? lang_strings[STR_NEW_FILE]: lang_strings[STR_NEW_FOLDER], dialog_editor_text, 128, 1, pos.x, pos.y);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_DELETE_LOCAL:
            activity_inprogess = true;
            snprintf(activity_message, 1024, "%s", "");
            stop_activity = false;
            selected_action = ACTION_NONE;
            Actions::DeleteSelectedLocalFiles();
            break;
        case ACTION_DELETE_REMOTE:
            activity_inprogess = true;
            snprintf(activity_message, 1024, "%s", "");
            stop_activity = false;
            selected_action = ACTION_NONE;
            Actions::DeleteSelectedRemotesFiles();
            break;
        case ACTION_UPLOAD:
            snprintf(status_message, 1024, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                snprintf(activity_message, 1024, "%s", "");
                stop_activity = false;
                Actions::UploadFiles();
                confirm_transfer_state = -1;
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_DOWNLOAD:
            snprintf(status_message, 1024, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                snprintf(activity_message, 1024, "%s", "");
                stop_activity = false;
                Actions::DownloadFiles();
                confirm_transfer_state = -1;
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_EXTRACT_LOCAL_ZIP:
            snprintf(status_message, 1024, "%s", "");
            activity_inprogess = true;
            snprintf(activity_message, 1024, "%s", "");
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::ExtractLocalZips();
            break;
        case ACTION_EXTRACT_REMOTE_ZIP:
            snprintf(status_message, 1024, "%s", "");
            activity_inprogess = true;
            snprintf(activity_message, 1024, "%s", "");
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::ExtractRemoteZips();
            break;
        case ACTION_CREATE_LOCAL_ZIP:
            snprintf(status_message, 1024, "%s", "");
            activity_inprogess = true;
            snprintf(activity_message, 1024, "%s", "");
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::MakeLocalZip();
            break;
        case ACTION_RENAME_LOCAL:
            if (gui_mode != GUI_MODE_IME)
            {
                if (multi_selected_local_files.size() > 0)
                    snprintf(dialog_editor_text, 1024, "%s", multi_selected_local_files.begin()->name);
                else
                    snprintf(dialog_editor_text, 1024, "%s", selected_local_file.name);
                ime_single_field = dialog_editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_RENAME], dialog_editor_text, 128, 1, 410, 350);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_RENAME_REMOTE:
            if (gui_mode != GUI_MODE_IME)
            {
                if (multi_selected_remote_files.size() > 0)
                    snprintf(dialog_editor_text, 1024, "%s", multi_selected_remote_files.begin()->name);
                else
                    snprintf(dialog_editor_text, 1024, "%s", selected_remote_file.name);
                ime_single_field = dialog_editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_RENAME], dialog_editor_text, 128, 1, 1330, 350);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_SHOW_LOCAL_PROPERTIES:
            ShowPropertiesDialog(selected_local_file);
            break;
        case ACTION_SHOW_REMOTE_PROPERTIES:
            ShowPropertiesDialog(selected_remote_file);
            break;
        case ACTION_LOCAL_SELECT_ALL:
            Actions::SelectAllLocalFiles();
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_SELECT_ALL:
            Actions::SelectAllRemoteFiles();
            selected_action = ACTION_NONE;
            break;
        case ACTION_LOCAL_CLEAR_ALL:
            multi_selected_local_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_CLEAR_ALL:
            multi_selected_remote_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_CONNECT:
            snprintf(status_message, 1024, "%s", "");
            Actions::Connect();
            break;
        case ACTION_DISCONNECT:
            snprintf(status_message, 1024, "%s", "");
            Actions::Disconnect();
            break;
        case ACTION_DISCONNECT_AND_EXIT:
            Actions::Disconnect();
            HttpServer::Stop();
            done = true;
            break;
        case ACTION_LOCAL_CUT:
        case ACTION_LOCAL_COPY:
            paste_action = selected_action;
            local_paste_files.clear();
            if (multi_selected_local_files.size() > 0)
                std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(local_paste_files));
            else
                local_paste_files.push_back(selected_local_file);
            multi_selected_local_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_CUT:
        case ACTION_REMOTE_COPY:
            paste_action = selected_action;
            remote_paste_files.clear();
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(remote_paste_files));
            else
                remote_paste_files.push_back(selected_remote_file);
            multi_selected_remote_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_LOCAL_PASTE:
            snprintf(status_message, 1024, "%s", "");
            snprintf(activity_message, 1024, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                snprintf(activity_message, 1024, "%s", "");
                stop_activity = false;
                confirm_transfer_state = -1;
                if (paste_action == ACTION_LOCAL_CUT)
                    Actions::MoveLocalFiles();
                else if (paste_action == ACTION_LOCAL_COPY)
                    Actions::CopyLocalFiles();
                else
                {
                    activity_inprogess = false;
                }
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_REMOTE_PASTE:
            snprintf(status_message, 1024, "%s", "");
            snprintf(activity_message, 1024, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                snprintf(activity_message, 1024, "%s", "");
                stop_activity = false;
                confirm_transfer_state = -1;
                if (paste_action == ACTION_REMOTE_CUT)
                    Actions::MoveRemoteFiles();
                else if (paste_action == ACTION_REMOTE_COPY)
                    Actions::CopyRemoteFiles();
                else
                {
                    activity_inprogess = false;
                }
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_SET_DEFAULT_LOCAL_FOLDER:
            CONFIG::SaveLocalDirecotry(local_directory);
            snprintf(status_message, 1024, "\"%s\" %s", local_directory, lang_strings[STR_SET_DEFAULT_DIRECTORY_MSG]);
            selected_action = ACTION_NONE;
            break;
        case ACTION_SET_DEFAULT_REMOTE_FOLDER:
            snprintf(remote_settings->default_directory, 256, "%s", remote_directory);
            CONFIG::SaveConfig();
            snprintf(status_message, 1024, "\"%s\" %s", remote_directory, lang_strings[STR_SET_DEFAULT_DIRECTORY_MSG]);
            selected_action = ACTION_NONE;
            break;
        case ACTION_UPDATE_PERMISSION:
            mode += owner_perm[0] ? 0400 : 0;
            mode += owner_perm[1] ? 0200 : 0;
            mode += owner_perm[2] ? 0100 : 0;
            mode += group_perm[0] ? 0040 : 0;
            mode += group_perm[1] ? 0020 : 0;
            mode += group_perm[2] ? 0010 : 0;
            mode += other_perm[0] ? 0004 : 0;
            mode += other_perm[1] ? 0002 : 0;
            mode += other_perm[2] ? 0001 : 0;
            snprintf(status_message, 1024, "%s", "");
            
            ret = chmod(selected_local_file.path, mode);
            if (ret == 0)
            {
                selected_action = ACTION_REFRESH_LOCAL_FILES;
            }
            else
            {
                snprintf(status_message, 1024, "%s", lang_strings[STR_FAIL_UPDATE_PERMISSION_MSG]);
                selected_action = ACTION_NONE;
            }
            break;
        default:
            break;
        }
    }

    void ResetImeCallbacks()
    {
        ime_callback = nullptr;
        ime_after_update = nullptr;
        ime_before_update = nullptr;
        ime_cancelled = nullptr;
        ime_field_size = 1;
    }

    void HandleImeInput()
    {
        if (Dialog::isImeDialogRunning())
        {
            int ime_result = Dialog::updateImeDialog();
            if (ime_result == IME_DIALOG_RESULT_FINISHED || ime_result == IME_DIALOG_RESULT_CANCELED)
            {
                if (ime_result == IME_DIALOG_RESULT_FINISHED)
                {
                    if (ime_before_update != nullptr)
                    {
                        ime_before_update(ime_result);
                    }

                    if (ime_callback != nullptr)
                    {
                        ime_callback(ime_result);
                    }
                    if (ime_after_update != nullptr)
                    {
                        ime_after_update(ime_result);
                    }
                }
                else if (ime_cancelled != nullptr)
                {
                    ime_cancelled(ime_result);
                }

                ResetImeCallbacks();
                gui_mode = GUI_MODE_BROWSER;
            }
        }
    }

    void SingleValueImeCallback(int ime_result)
    {
        SDL_StopTextInput();
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            char *new_value = (char *)Dialog::getImeDialogInputText();
            snprintf(ime_single_field, ime_field_size, "%s", new_value);
        }
    }

    void MultiValueImeCallback(int ime_result)
    {
        SDL_StopTextInput();
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            char *new_value = (char *)Dialog::getImeDialogInputText();
            char *initial_value = (char *)Dialog::getImeDialogInitialText();
            if (strlen(initial_value) == 0)
            {
                ime_multi_field->push_back(std::string(new_value));
            }
            else
            {
                for (int i = 0; i < ime_multi_field->size(); i++)
                {
                    if (strcmp((*ime_multi_field)[i].c_str(), initial_value) == 0)
                    {
                        (*ime_multi_field)[i] = std::string(new_value);
                    }
                }
            }
        }
    }

    void NullAfterValueChangeCallback(int ime_result) {}

    void AfterLocalFileChangesCallback(int ime_result)
    {
        selected_action = ACTION_REFRESH_LOCAL_FILES;
    }

    void AfterRemoteFileChangesCallback(int ime_result)
    {
        selected_action = ACTION_REFRESH_REMOTE_FILES;
    }

    void AfterFolderNameCallback(int ime_result)
    {
        switch (selected_action)
        {
        case ACTION_NEW_LOCAL_FOLDER:
            Actions::CreateNewLocalFolder(dialog_editor_text);
            break;
        case ACTION_NEW_REMOTE_FOLDER:
            Actions::CreateNewRemoteFolder(dialog_editor_text);
            break;
        case ACTION_RENAME_LOCAL:
            if (multi_selected_local_files.size() > 0)
                Actions::RenameLocalFolder(multi_selected_local_files.begin()->path, dialog_editor_text);
            else
                Actions::RenameLocalFolder(selected_local_file.path, dialog_editor_text);
            break;
        case ACTION_RENAME_REMOTE:
            if (multi_selected_remote_files.size() > 0)
                Actions::RenameRemoteFolder(multi_selected_remote_files.begin()->path, dialog_editor_text);
            else
                Actions::RenameRemoteFolder(selected_remote_file.path, dialog_editor_text);
            break;
        case ACTION_NEW_LOCAL_FILE:
            Actions::CreateLocalFile(dialog_editor_text);
            break;
        case ACTION_NEW_REMOTE_FILE:
            Actions::CreateRemoteFile(dialog_editor_text);
            break;
        default:
            break;
        }
        selected_action = ACTION_NONE;
    }

    void CancelActionCallBack(int ime_result)
    {
        selected_action = ACTION_NONE;
    }

    void AfterExtractFolderCallback(int ime_result)
    {
        selected_action = ACTION_EXTRACT_LOCAL_ZIP;
    }

    void AfterExtractRemoteFolderCallback(int ime_result)
    {
        selected_action = ACTION_EXTRACT_REMOTE_ZIP;
    }

    void AfterZipFileCallback(int ime_result)
    {
        selected_action = ACTION_CREATE_LOCAL_ZIP;
    }

    void AferServerChangeCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            CONFIG::SetClientType(remote_settings);
            if (strncasecmp(remote_settings->server, "https://archive.org/", 20) == 0)
            {
                snprintf(remote_settings->http_server_type, 24, "%s", HTTP_SERVER_ARCHIVEORG);
            }
            else if (strncasecmp(remote_settings->server, "https://myrient.erista.me/", 26) == 0)
            {
                snprintf(remote_settings->http_server_type, 24, "%s", HTTP_SERVER_MYRIENT);
            }
            else if (strncasecmp(remote_settings->server, "https://github.com/", 19) == 0)
            {
                snprintf(remote_settings->http_server_type, 24, "%s", HTTP_SERVER_GITHUB);
            }
        }
    }

    void AfterHttpPortChangeCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            http_server_port = atoi(txt_http_server_port);
        }
    }

}
