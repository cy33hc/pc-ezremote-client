#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <json-c/json.h>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <minizip/unzip.h>
#include "clients/ftpclient.h"
#include "clients/smbclient.h"
#include "clients/webdav.h"
#include "clients/apache.h"
#include "clients/archiveorg.h"
#include "clients/github.h"
#include "clients/myrient.h"
#include "clients/nginx.h"
#include "clients/npxserve.h"
#include "clients/nfsclient.h"
#include "clients/iis.h"
#include "clients/rclone.h"
#include "clients/sftpclient.h"
#include "filehost/filehost.h"
#include "server/http_server.h"
#include "common.h"
#include "fs.h"
#include "config.h"
#include "windows.h"
#include "util.h"
#include "lang.h"
#include "actions.h"
#include "zip_util.h"

namespace Actions
{
    static int FtpCallback(int64_t xfered, void *arg)
    {
        bytes_transfered = xfered;
        return 1;
    }

    void RefreshLocalFiles(bool apply_filter)
    {
        multi_selected_local_files.clear();
        local_files.clear();
        int err;
        if (strlen(local_filter) > 0 && apply_filter)
        {
            std::vector<DirEntry> temp_files = FS::ListDir(local_directory, &err);
            std::string lower_filter = Util::ToLower(local_filter);
            for (std::vector<DirEntry>::iterator it = temp_files.begin(); it != temp_files.end();)
            {
                std::string lower_name = Util::ToLower(it->name);
                if (lower_name.find(lower_filter) != std::string::npos || strcmp(it->name, "..") == 0)
                {
                    local_files.push_back(*it);
                }
                ++it;
            }
            temp_files.clear();
        }
        else
        {
            local_files = FS::ListDir(local_directory, &err);
        }
        DirEntry::Sort(local_files);
        if (err != 0)
            snprintf(status_message, 1024, "%s", lang_strings[STR_FAIL_READ_LOCAL_DIR_MSG]);
    }

    void RefreshRemoteFiles(bool apply_filter)
    {
        if (remoteclient == nullptr)
            return;

        if (remoteclient != nullptr && !remoteclient->Ping())
        {
            remoteclient->Quit();
            remoteclient = nullptr;
            snprintf(status_message, 1024, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return;
        }

        std::string strList;
        multi_selected_remote_files.clear();
        remote_files.clear();
        if (strlen(remote_filter) > 0 && apply_filter)
        {
            std::vector<DirEntry> temp_files = remoteclient->ListDir(remote_directory);
            std::string lower_filter = Util::ToLower(remote_filter);
            for (std::vector<DirEntry>::iterator it = temp_files.begin(); it != temp_files.end();)
            {
                std::string lower_name = Util::ToLower(it->name);
                if (lower_name.find(lower_filter) != std::string::npos || strcmp(it->name, "..") == 0)
                {
                    remote_files.push_back(*it);
                }
                ++it;
            }
            temp_files.clear();
        }
        else
        {
            remote_files = remoteclient->ListDir(remote_directory);
        }
        DirEntry::Sort(remote_files);
    }

    void HandleChangeLocalDirectory(const DirEntry entry)
    {
        if (!entry.isDir)
            return;

        if (strcmp(entry.name, "..") == 0)
        {
            std::string temp_path = std::string(entry.directory);
            if (temp_path.size() > 1)
            {
                if (temp_path.find_last_of("/") == 0)
                {
                    snprintf(local_directory, 255, "%s", "/");
                }
                else
                {
                    snprintf(local_directory, 255, "%s", temp_path.substr(0, temp_path.find_last_of("/")).c_str());
                }
            }
            snprintf(local_file_to_select, 256, "%s", temp_path.substr(temp_path.find_last_of("/") + 1).c_str());
        }
        else
        {
            snprintf(local_directory, 255, "%s", entry.path);
        }
        RefreshLocalFiles(false);
        if (strcmp(entry.name, "..") != 0)
        {
            snprintf(local_file_to_select, 256, "%s", local_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleChangeRemoteDirectory(const DirEntry entry)
    {
        if (!entry.isDir)
            return;

        if (!remoteclient->Ping())
        {
            remoteclient->Quit();
            snprintf(status_message, 1024, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return;
        }

        if (strcmp(entry.name, "..") == 0)
        {
            std::string temp_path = std::string(entry.directory);
            if (temp_path.size() > 1)
            {
                if (temp_path.find_last_of("/") == 0)
                {
                    snprintf(remote_directory, 255, "%s", "/");
                }
                else
                {
                    snprintf(remote_directory, 255, "%s", temp_path.substr(0, temp_path.find_last_of("/")).c_str());
                }
            }
            snprintf(remote_file_to_select, 256, "%s", temp_path.substr(temp_path.find_last_of("/") + 1).c_str());
        }
        else
        {
            snprintf(remote_directory, 255, "%s", entry.path);
        }
        RefreshRemoteFiles(false);
        if (strcmp(entry.name, "..") != 0)
        {
            snprintf(remote_file_to_select, 256, "%s", remote_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleRefreshLocalFiles()
    {
        int prev_count = local_files.size();
        RefreshLocalFiles(false);
        int new_count = local_files.size();
        if (prev_count != new_count)
        {
            snprintf(local_file_to_select, 256, "%s", local_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleRefreshRemoteFiles()
    {
        if (remoteclient != nullptr)
        {
            int prev_count = remote_files.size();
            RefreshRemoteFiles(false);
            int new_count = remote_files.size();
            if (prev_count != new_count)
            {
                snprintf(remote_file_to_select, 256, "%s", remote_files[0].name);
            }
        }
        selected_action = ACTION_NONE;
    }

    void CreateNewLocalFolder(char *new_folder)
    {
        snprintf(status_message, 1024, "%s", "");
        std::string folder = std::string(new_folder);
        folder = Util::Rtrim(Util::Trim(folder, " "), "/");
        std::string path = FS::GetPath(local_directory, folder);
        if (!FS::MkDirs(path))
        {
            snprintf(status_message, 1024, "%s", lang_strings[STR_FAIL_CREATE_LOCAL_FOLDER_MSG]);
        }
        else
        {
            RefreshLocalFiles(false);
            snprintf(local_file_to_select, 256, "%s", folder.c_str());
        }
    }

    void CreateNewRemoteFolder(char *new_folder)
    {
        snprintf(status_message, 1024, "%s", "");
        std::string folder = std::string(new_folder);
        folder = Util::Rtrim(Util::Trim(folder, " "), "/");
        std::string path = remoteclient->GetPath(remote_directory, folder.c_str());
        if (remoteclient->Mkdir(path.c_str()))
        {
            RefreshRemoteFiles(false);
            snprintf(remote_file_to_select, 256, "%s", folder.c_str());
        }
        else
        {
            snprintf(status_message, 1024, "%s - %s", lang_strings[STR_FAILED], remoteclient->LastResponse());
        }
    }

    void RenameLocalFolder(const char *old_path, const char *new_path)
    {
        snprintf(status_message, 1024, "%s", "");
        std::string new_name = std::string(new_path);
        new_name = Util::Rtrim(Util::Trim(new_name, " "), "/");
        std::string path = FS::GetPath(local_directory, new_name);
        if (FS::Rename(old_path, path) != 0)
        {
            snprintf(status_message, 1024, "Failed to rename %s to %s", old_path, path);
        }
        RefreshLocalFiles(false);
        snprintf(local_file_to_select, 256, "%s", new_name.c_str());
    }

    void RenameRemoteFolder(const char *old_path, const char *new_path)
    {
        snprintf(status_message, 1024, "%s", "");
        std::string new_name = std::string(new_path);
        new_name = Util::Rtrim(Util::Trim(new_name, " "), "/");
        std::string path = FS::GetPath(remote_directory, new_name);
        if (remoteclient->Rename(old_path, path.c_str()))
        {
            RefreshRemoteFiles(false);
            snprintf(remote_file_to_select, 256, "%s", new_name.c_str());
        }
        else
        {
            snprintf(status_message, 1024, "%s - %s", lang_strings[STR_FAILED], remoteclient->LastResponse());
        }
    }

    void *DeleteSelectedLocalFilesThread(void *argp)
    {
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            FS::RmRecursive(it->path);
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void DeleteSelectedLocalFiles()
    {
        int ret = pthread_create(&bk_activity_thid, NULL, DeleteSelectedLocalFilesThread, NULL);
        if (ret != 0)
        {
            activity_inprogess = false;
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_LOCAL_FILES;
        }
    }

    void *DeleteSelectedRemotesFilesThread(void *argp)
    {
        if (remoteclient->Ping())
        {
            std::vector<DirEntry> files;
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_remote_file);

            for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
            {
                if (it->isDir)
                    remoteclient->Rmdir(it->path, true);
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETING], it->path);
                    if (!remoteclient->Delete(it->path))
                    {
                        snprintf(status_message, 1024, "%s - %s", lang_strings[STR_FAILED], remoteclient->LastResponse());
                    }
                }
            }
            selected_action = ACTION_REFRESH_REMOTE_FILES;
        }
        else
        {
            snprintf(status_message, 1024, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            Disconnect();
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        return NULL;
    }

    void DeleteSelectedRemotesFiles()
    {
        int res = pthread_create(&bk_activity_thid, NULL, DeleteSelectedRemotesFilesThread, NULL);
        if (res != 0)
        {
            activity_inprogess = false;
            Windows::SetModalMode(false);
        }
    }

    int UploadFile(const char *src, const char *dest)
    {
        int ret;
        if (!remoteclient->Ping())
        {
            remoteclient->Quit();
            snprintf(status_message, 1024, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return 0;
        }

        if (overwrite_type == OVERWRITE_PROMPT && remoteclient->FileExists(dest))
        {
            snprintf(confirm_message, 512, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                usleep(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && remoteclient->FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            struct timespec tp;
            prev_tick = GetMyTick();
            return remoteclient->Put(src, dest);
        }

        return 1;
    }

    int Upload(const DirEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
            remoteclient->Mkdir(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    remoteclient->Mkdir(new_path);
                    ret = Upload(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_UPLOADING], entries[i].path);
                    bytes_to_download = entries[i].file_size;
                    bytes_transfered = 0;
                    prev_tick = GetMyTick();
                    
                    ret = UploadFile(entries[i].path, new_path);
                    if (ret <= 0)
                    {
                        snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_UPLOAD_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_UPLOADING], src.name);
            bytes_to_download = src.file_size;
            ret = UploadFile(src.path, new_path);
            if (ret <= 0)
            {
                free(new_path);
                snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_UPLOAD_MSG], src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *UploadFilesThread(void *argp)
    {
        file_transfering = true;
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (it->isDir)
            {
                char new_dir[512];
                snprintf(new_dir, 512, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory) ? "" : "/", it->name);
                Upload(*it, new_dir);
            }
            else
            {
                Upload(*it, remote_directory);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void UploadFiles()
    {
        snprintf(status_message, 1024, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, UploadFilesThread, NULL);
        if (res != 0)
        {
            activity_inprogess = false;
            file_transfering = false;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_REMOTE_FILES;
        }
    }

    int DownloadFile(const char *src, const char *dest)
    {
        bytes_transfered = 0;
        prev_tick = GetMyTick();

        if (!remoteclient->Size(src, &bytes_to_download))
        {
            remoteclient->Quit();
            snprintf(status_message, 1024, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return 0;
        }

        if (overwrite_type == OVERWRITE_PROMPT && FS::FileExists(dest))
        {
            snprintf(confirm_message, 512, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                usleep(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && FS::FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            prev_tick = GetMyTick();
            return remoteclient->Get(dest, src);
        }

        return 1;
    }

    int Download(const DirEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = remoteclient->ListDir(src.path);
            if (!FS::MkDirs(dest))
            {
                snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_CREATE_LOCAL_FOLDER_MSG], dest);
                return 0;
            }

            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    if (!FS::MkDirs(new_path))
                    {
                        snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_CREATE_LOCAL_FOLDER_MSG], new_path);
                        free(new_path);
                        return 0;
                    }

                    ret = Download(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DOWNLOADING], entries[i].path);
                    ret = DownloadFile(entries[i].path, new_path);
                    if (ret <= 0)
                    {
                        snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_DOWNLOAD_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DOWNLOADING], src.path);
            ret = DownloadFile(src.path, new_path);
            if (ret <= 0)
            {
                free(new_path);
                snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_DOWNLOAD_MSG], src.path);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *DownloadFilesThread(void *argp)
    {
        file_transfering = true;
        std::vector<DirEntry> files;
        if (multi_selected_remote_files.size() > 0)
            std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_remote_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (it->isDir)
            {
                char new_dir[512];
                snprintf(new_dir, 512, "%s%s%s", local_directory, FS::hasEndSlash(local_directory) ? "" : "/", it->name);
                Download(*it, new_dir);
            }
            else
            {
                Download(*it, local_directory);
            }
        }
        file_transfering = false;
        activity_inprogess = false;
        multi_selected_remote_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void DownloadFiles()
    {
        snprintf(status_message, 1024, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, DownloadFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            multi_selected_remote_files.clear();
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_LOCAL_FILES;
        }
    }

    void *ExtractZipThread(void *argp)
    {
        if (!FS::MkDirs(extract_zip_folder))
        {
            snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_CREATE_LOCAL_FOLDER_MSG], extract_zip_folder);
            return NULL;
        }

        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (stop_activity)
                break;
            if (!it->isDir)
            {
                int ret = ZipUtil::Extract(*it, extract_zip_folder);
                if (ret == 0)
                {
                    snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAILED_TO_EXTRACT], it->name);
                    usleep(100000);
                    break;
                }
            }
        }
        activity_inprogess = false;
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void ExtractLocalZips()
    {
        snprintf(status_message, 1024, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, ExtractZipThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *ExtractRemoteZipThread(void *argp)
    {
        if (!FS::MkDirs(extract_zip_folder))
        {
            snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_CREATE_LOCAL_FOLDER_MSG], extract_zip_folder);
            activity_inprogess = false;
            multi_selected_remote_files.clear();
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_LOCAL_FILES;
            return NULL;
        }

        std::vector<DirEntry> files;
        if (multi_selected_remote_files.size() > 0)
            std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_remote_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (stop_activity)
                break;
            if (!it->isDir)
            {
                int ret = ZipUtil::Extract(*it, extract_zip_folder, remoteclient);
                if (ret == 0)
                {
                    snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAILED_TO_EXTRACT], it->name);
                    usleep(100000);
                }
            }
        }

        activity_inprogess = false;
        multi_selected_remote_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void ExtractRemoteZips()
    {
        snprintf(status_message, 1024, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, ExtractRemoteZipThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            multi_selected_remote_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *MakeZipThread(void *argp)
    {
        zipFile zf = zipOpen64(zip_file_path, APPEND_STATUS_CREATE);
        if (zf != NULL)
        {
            std::vector<DirEntry> files;
            if (multi_selected_local_files.size() > 0)
                std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_local_file);

            for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
            {
                if (stop_activity)
                    break;
                int res = ZipUtil::ZipAddPath(zf, it->path, strlen(it->directory) + 1, Z_DEFAULT_COMPRESSION);
                if (res <= 0)
                {
                    snprintf(status_message, 1024, "%s", lang_strings[STR_ERROR_CREATE_ZIP]);
                    usleep(1000000);
                }
            }
            zipClose(zf, NULL);
        }
        else
        {
            snprintf(status_message, 1024, "%s", lang_strings[STR_ERROR_CREATE_ZIP]);
        }
        activity_inprogess = false;
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void MakeLocalZip()
    {
        snprintf(status_message, 1024, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, MakeZipThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void Connect()
    {
        CONFIG::SaveConfig();

        if (strncmp(remote_settings->server, "https://", 8) == 0 || strncmp(remote_settings->server, "http://", 7) == 0)
        {
            if (strcmp(remote_settings->http_server_type, HTTP_SERVER_APACHE) == 0)
                remoteclient = new ApacheClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_MS_IIS) == 0)
                remoteclient = new IISClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_NGINX) == 0)
                remoteclient = new NginxClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_NPX_SERVE) == 0)
                remoteclient = new NpxServeClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_RCLONE) == 0)
                remoteclient = new RCloneClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_ARCHIVEORG) == 0)
                remoteclient = new ArchiveOrgClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_MYRIENT) == 0)
                remoteclient = new MyrientClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_GITHUB) == 0)
                remoteclient = new GithubClient();
        }
        else if (strncmp(remote_settings->server, "webdavs://", 10) == 0 || strncmp(remote_settings->server, "webdav://", 9) == 0)
        {
            remoteclient = new WebDAVClient();
        }
        else if (strncmp(remote_settings->server, "smb://", 6) == 0)
        {
            remoteclient = new SmbClient();
        }
        else if (strncmp(remote_settings->server, "ftp://", 6) == 0)
        {
            FtpClient *client = new FtpClient();
            client->SetConnmode(FtpClient::pasv);
            client->SetCallbackBytes(1);
            client->SetCallbackXferFunction(FtpCallback);
            remoteclient = client;
        }
        else if (strncmp(remote_settings->server, "sftp://", 7) == 0)
        {
            remoteclient = new SFTPClient();
        }
        else if (strncmp(remote_settings->server, "nfs://", 6) == 0)
        {
            remoteclient = new NfsClient();
        }
        else
        {
            snprintf(status_message, 1024, "%s", lang_strings[STR_PROTOCOL_NOT_SUPPORTED]);
            selected_action = ACTION_NONE;
            return;
        }

        if (remoteclient->Connect(remote_settings->server, remote_settings->username, remote_settings->password))
        {
            RefreshRemoteFiles(false);

            if (remoteclient->clientType() == CLIENT_TYPE_FTP)
            {
                int res = pthread_create(&ftp_keep_alive_thid, NULL, KeepAliveThread, NULL);
            }
        }
        else
        {
            snprintf(status_message, 1024, "%s - %s", lang_strings[STR_FAIL_TIMEOUT_MSG], remoteclient->LastResponse());
            if (remoteclient != nullptr)
            {
                remoteclient->Quit();
                delete remoteclient;
                remoteclient = nullptr;
            }
        }
        selected_action = ACTION_NONE;
    }

    void Disconnect()
    {
        if (remoteclient != nullptr)
        {
            if (remoteclient->IsConnected())
                remoteclient->Quit();
            multi_selected_remote_files.clear();
            remote_files.clear();
            snprintf(status_message, 1024, "%s", "");
            delete remoteclient;
            remoteclient = nullptr;
        }
    }

    void SelectAllLocalFiles()
    {
        for (int i = 0; i < local_files.size(); i++)
        {
            if (strcmp(local_files[i].name, "..") != 0)
                multi_selected_local_files.insert(local_files[i]);
        }
    }

    void SelectAllRemoteFiles()
    {
        for (int i = 0; i < remote_files.size(); i++)
        {
            if (strcmp(remote_files[i].name, "..") != 0)
                multi_selected_remote_files.insert(remote_files[i]);
        }
    }

    void *KeepAliveThread(void *argp)
    {
        long idle;
        while (remoteclient != nullptr && remoteclient->clientType() == CLIENT_TYPE_FTP && remoteclient->IsConnected())
        {
            FtpClient *client = (FtpClient *)remoteclient;
            idle = client->GetIdleTime();
            if (idle > 60000000)
            {
                if (!client->Ping())
                {
                    client->Quit();
                    snprintf(status_message, 1024, "%s", lang_strings[STR_REMOTE_TERM_CONN_MSG]);
                    return NULL;
                }
            }
            usleep(500000);
        }
        return NULL;
    }

    int CopyOrMoveLocalFile(const char *src, const char *dest, bool isCopy)
    {
        int ret;
        if (overwrite_type == OVERWRITE_PROMPT && FS::FileExists(dest))
        {
            snprintf(confirm_message, 512, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                usleep(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && FS::FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            prev_tick = GetMyTick();
            // cannot copy file to folder
            if (FS::FolderExists(dest))
            {
                return 0;
            }

            if (isCopy)
                return FS::Copy(src, dest);
            else
                return FS::Move(src, dest);
        }

        return 1;
    }

    int CopyOrMove(const DirEntry &src, const char *dest, bool isCopy)
    {
        if (stop_activity)
            return 1;

        int ret;

        if (src.isDir)
        {
            int err;

            if (src.isLink)
            {
                int ret = CopyOrMoveLocalFile(src.path, dest, isCopy);
                if (ret != 0)
                {
                    snprintf(status_message, 1024, "%s %s", isCopy ? lang_strings[STR_FAIL_COPY_MSG] : lang_strings[STR_FAIL_MOVE_MSG], src.path);
                    return 0;
                }
            }
            else
            {
                // cannot copy directory over file
                if (FS::FileExists(dest))
                {
                    snprintf(status_message, 1024, "%s %s", isCopy ? lang_strings[STR_FAIL_COPY_MSG] : lang_strings[STR_FAIL_MOVE_MSG], src.path);
                    return 0;
                }

                std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
                if (!FS::MkDirs(dest))
                {
                    snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_CREATE_LOCAL_FOLDER_MSG], dest);
                    return 0;
                }

                for (int i = 0; i < entries.size(); i++)
                {
                    if (stop_activity)
                        return 2;

                    int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                    char *new_path = (char *)malloc(path_length);
                    snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                    if (entries[i].isDir)
                    {
                        if (strcmp(entries[i].name, "..") == 0)
                            continue;

                        if (!FS::MkDirs(new_path))
                        {
                            snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_CREATE_LOCAL_FOLDER_MSG], new_path);
                            free(new_path);
                            return 0;
                        }

                        ret = CopyOrMove(entries[i], new_path, isCopy);
                        if (ret == 0)
                        {
                            free(new_path);
                            return 0;
                        }
                    }
                    else
                    {
                        snprintf(activity_message, 1024, "%s %s", isCopy ? lang_strings[STR_COPYING] : lang_strings[STR_MOVING], entries[i].path);
                        bytes_to_download = entries[i].file_size;
                        bytes_transfered = 0;
                        prev_tick = GetMyTick();

                        ret = CopyOrMoveLocalFile(entries[i].path, new_path, isCopy);
                        if (ret == 0)
                        {
                            snprintf(status_message, 1024, "%s %s", isCopy ? lang_strings[STR_FAIL_COPY_MSG] : lang_strings[STR_FAIL_MOVE_MSG], entries[i].path);
                            free(new_path);
                            return 0;
                        }
                    }
                    free(new_path);
                }
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", isCopy ? lang_strings[STR_COPYING] : lang_strings[STR_MOVING], src.name);
            bytes_to_download = src.file_size;
            ret = CopyOrMoveLocalFile(src.path, new_path, isCopy);
            if (ret == 0)
            {
                free(new_path);
                snprintf(status_message, 1024, "%s %s", isCopy ? lang_strings[STR_FAIL_COPY_MSG] : lang_strings[STR_FAIL_MOVE_MSG], src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *MoveLocalFilesThread(void *argp)
    {
        file_transfering = true;
        for (std::vector<DirEntry>::iterator it = local_paste_files.begin(); it != local_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, local_directory) == 0)
                continue;

            if (it->isDir)
            {
                if (strncmp(local_directory, it->path, strlen(it->path)) == 0)
                {
                    snprintf(status_message, 1024, "%s", lang_strings[STR_CANT_MOVE_TO_SUBDIR_MSG]);
                    continue;
                }
                char new_dir[512];
                snprintf(new_dir, 512, "%s%s%s", local_directory, FS::hasEndSlash(local_directory) ? "" : "/", it->name);
                if (!CopyOrMove(*it, new_dir, false))
                    break;

                if (stop_activity)
                    break;
                
                if (!it->isLink)
                    FS::RmRecursive(it->path);
            }
            else
            {
                CopyOrMove(*it, local_directory, false);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        local_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void MoveLocalFiles()
    {
        snprintf(status_message, 1024, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, MoveLocalFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *CopyLocalFilesThread(void *argp)
    {
        file_transfering = true;
        for (std::vector<DirEntry>::iterator it = local_paste_files.begin(); it != local_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, local_directory) == 0)
                continue;

            if (it->isDir)
            {
                if (strncmp(local_directory, it->path, strlen(it->path)) == 0)
                {
                    snprintf(status_message, 1024, "%s", lang_strings[STR_CANT_COPY_TO_SUBDIR_MSG]);
                    continue;
                }
                char new_dir[512];
                snprintf(new_dir, 512, "%s%s%s", local_directory, FS::hasEndSlash(local_directory) ? "" : "/", it->name);
                CopyOrMove(*it, new_dir, true);
            }
            else
            {
                CopyOrMove(*it, local_directory, true);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        local_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void CopyLocalFiles()
    {
        snprintf(status_message, 1024, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, CopyLocalFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    int CopyOrMoveRemoteFile(const std::string &src, const std::string &dest, bool isCopy)
    {
        int ret;
        if (overwrite_type == OVERWRITE_PROMPT && remoteclient->FileExists(dest))
        {
            snprintf(confirm_message, 512, "%s %s?", lang_strings[STR_OVERWRITE], dest.c_str());
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                usleep(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && remoteclient->FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            prev_tick = GetMyTick();
            if (isCopy)
                return remoteclient->Copy(src, dest);
            else
                return remoteclient->Move(src, dest);
        }

        return 1;
    }

    void *MoveRemoteFilesThread(void *argp)
    {
        file_transfering = false;
        for (std::vector<DirEntry>::iterator it = remote_paste_files.begin(); it != remote_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, remote_directory) == 0)
                continue;

            char new_path[1024];
            snprintf(new_path, 1024, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory) ? "" : "/", it->name);
            if (it->isDir)
            {
                if (strncmp(remote_directory, it->path, strlen(it->path)) == 0)
                {
                    snprintf(status_message, 1024, "%s", lang_strings[STR_CANT_MOVE_TO_SUBDIR_MSG]);
                    continue;
                }
            }

            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_MOVING], it->path);
            int res = CopyOrMoveRemoteFile(it->path, new_path, false);
            if (res == 0)
                snprintf(status_message, 1024, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
        }
        activity_inprogess = false;
        file_transfering = false;
        remote_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void MoveRemoteFiles()
    {
        snprintf(status_message, 1024, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, MoveRemoteFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            remote_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    int CopyRemotePath(const DirEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = remoteclient->ListDir(src.path);
            remoteclient->Mkdir(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    remoteclient->Mkdir(new_path);
                    ret = CopyRemotePath(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_COPYING], entries[i].path);
                    bytes_to_download = entries[i].file_size;
                    bytes_transfered = 0;
                    prev_tick = GetMyTick();
                    ret = CopyOrMoveRemoteFile(entries[i].path, new_path, true);
                    if (ret <= 0)
                    {
                        snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_COPY_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_COPYING], src.name);
            ret = CopyOrMoveRemoteFile(src.path, new_path, true);
            if (ret <= 0)
            {
                free(new_path);
                snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_COPY_MSG], src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *CopyRemoteFilesThread(void *argp)
    {
        file_transfering = false;
        for (std::vector<DirEntry>::iterator it = remote_paste_files.begin(); it != remote_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, remote_directory) == 0)
                continue;

            char new_path[1024];
            snprintf(new_path, 1024, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory) ? "" : "/", it->name);
            if (it->isDir)
            {
                if (strncmp(remote_directory, it->path, strlen(it->path)) == 0)
                {
                    snprintf(status_message, 1024, "%s", lang_strings[STR_CANT_COPY_TO_SUBDIR_MSG]);
                    continue;
                }
                int res = CopyRemotePath(*it, new_path);
                if (res == 0)
                    snprintf(status_message, 1024, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
            }
            else
            {
                int res = CopyRemotePath(*it, remote_directory);
                if (res == 0)
                    snprintf(status_message, 1024, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        remote_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void CopyRemoteFiles()
    {
        snprintf(status_message, 1024, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, CopyRemoteFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            remote_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void CreateLocalFile(char *filename)
    {
        std::string new_file = FS::GetPath(local_directory, filename);
        std::string temp_file = new_file;
        int i = 1;
        while (true)
        {
            if (!FS::FileExists(temp_file))
                break;
            temp_file = new_file + "." + std::to_string(i);
            i++;
        }
        FILE* f = FS::Create(temp_file);
        if (f != nullptr)
        {
            FS::Close(f);
            RefreshLocalFiles(false);
            snprintf(local_file_to_select, 256, "%s", temp_file.c_str());
        }
        else
        {
            snprintf(status_message, 1024, "%s", lang_strings[STR_FAIL_CREATE_LOCAL_FILE_MSG]);
        }
    }

    void CreateRemoteFile(char *filename)
    {
        std::string new_file = FS::GetPath(remote_directory, filename);
        std::string temp_file = new_file;
        int i = 1;
        while (true)
        {
            if (!remoteclient->FileExists(temp_file))
                break;
            temp_file = new_file + "." + std::to_string(i);
            i++;
        }

        struct timespec tick;
        clock_gettime(CLOCK_MONOTONIC, &tick);
        std::string local_tmp = std::string(g_data_path) + "/" + std::to_string(tick.tv_nsec);
        FILE *f = FS::Create(local_tmp);
        FS::Close(f);
        remoteclient->Put(local_tmp, temp_file);
        FS::Rm(local_tmp);
        RefreshRemoteFiles(false);
        snprintf(remote_file_to_select, 256, "%s", temp_file.c_str());
    }

}
