#include <string>
#include <json-c/json.h>
#include "http/httplib.h"
#include "server/http_server.h"
#include "clients/sftpclient.h"
#include "clients/smbclient.h"
#include "clients/ftpclient.h"
#include "clients/nfsclient.h"
#include "clients/webdav.h"
#include "clients/apache.h"
#include "clients/archiveorg.h"
#include "clients/iis.h"
#include "clients/nginx.h"
#include "clients/npxserve.h"
#include "clients/rclone.h"
#include "filehost/filehost.h"
#include "config.h"
#include "fs.h"
#include "windows.h"
#include "lang.h"
#include "zip_util.h"
#include "util.h"

#define SERVER_CERT_FILE "/usr/share/ezremote-client/assets/certs/domain.crt"
#define SERVER_PRIVATE_KEY_FILE "/usr/share/ezremote-client/assets/certs/domain.key"
#define SERVER_PRIVATE_KEY_PASSWORD "12345678"
#define SUCCESS_MSG "{ \"result\": { \"success\": true, \"error\": null } }"
#define FAILURE_MSG "{ \"result\": { \"success\": false, \"error\": \"%s\" } }"
#define SUCCESS_MSG_LEN 48

using namespace httplib;

struct RemoteDownloadData
{
    RemoteClient *client = nullptr;
    std::map<std::string, void *> fp_handles;
};

static RemoteDownloadData remote_data[100];

Server *svr;
int http_server_port = 8080;
char compressed_file_path[1024];
bool web_server_enabled = false;

namespace HttpServer
{
    std::string dump_headers(const Headers &headers)
    {
        std::string s;
        char buf[BUFSIZ];

        for (auto it = headers.begin(); it != headers.end(); ++it)
        {
            const auto &x = *it;
            snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
            s += buf;
        }

        return s;
    }

    std::string log(const Request &req, const Response &res)
    {
        std::string s;
        char buf[BUFSIZ];

        s += "================================\n";

        snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(),
                 req.version.c_str(), req.path.c_str());
        s += buf;

        std::string query;
        for (auto it = req.params.begin(); it != req.params.end(); ++it)
        {
            const auto &x = *it;
            snprintf(buf, sizeof(buf), "%c%s=%s",
                     (it == req.params.begin()) ? '?' : '&', x.first.c_str(),
                     x.second.c_str());
            query += buf;
        }
        snprintf(buf, sizeof(buf), "%s\n", query.c_str());
        s += buf;

        s += dump_headers(req.headers);

        s += "--------------------------------\n";

        snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
        s += buf;
        s += dump_headers(res.headers);
        s += "\n";

        if (!res.body.empty())
        {
            s += res.body;
        }

        s += "\n";

        return s;
    }

    void failed(Response &res, int status, const std::string &msg)
    {
        res.status = status;
        char response_msg[msg.length() + strlen(FAILURE_MSG) + 2];
        snprintf(response_msg, sizeof(response_msg), "{ \"result\": { \"success\": false, \"error\": \"%s\" } }", msg.c_str());
        res.set_content(response_msg, strlen(response_msg), "application/json");
        return;
    }

    void bad_request(Response &res, const std::string &msg)
    {
        failed(res, 200, msg);
        return;
    }

    void success(Response &res)
    {
        res.status = 200;
        res.set_content(SUCCESS_MSG, SUCCESS_MSG_LEN, "application/json");
        return;
    }

    int CopyOrMoveLocalFile(const char *src, const char *dest, bool isCopy)
    {
        if (FS::FolderExists(dest))
        {
            return 0;
        }

        if (isCopy)
            return FS::Copy(src, dest);
        else
            return FS::Move(src, dest);
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
                    return 0;
                }
            }
            else
            {
                // cannot copy directory over file
                if (FS::FileExists(dest))
                {
                    return 0;
                }

                std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
                if (!FS::MkDirs(dest))
                {
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
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *ServerThread(void *argp)
    {
        svr->Get("/", [&](const Request &req, Response &res)
        {
            res.set_redirect("/index.html");
        });

        svr->Get("/index.html", [&](const Request &req, Response &res)
        {
            FILE *in = FS::OpenRead("/usr/share/ezremote-client/assets/index.html");
            size_t size = FS::GetSize("/usr/share/ezremote-client/assets/index.html");
            res.set_content_provider(
                size, "text/html",
                [in](size_t offset, size_t length, DataSink &sink) {
                    size_t size_to_read = std::min(static_cast<size_t>(length), (size_t)1048576);
                    char buff[size_to_read];
                    size_t read_len;
                    FS::Seek(in, offset);
                    read_len = FS::Read(in, buff, size_to_read);
                    sink.write(buff, read_len);
                    return read_len == size_to_read;
                },
                [in](bool success) {
                    FS::Close(in);
                });
        });

        svr->Get("/favicon.ico", [&](const Request &req, Response &res)
        {
            FILE *in = FS::OpenRead("/usr/share/ezremote-client/assets/favicon.ico");
            size_t size = FS::GetSize("/usr/share/ezremote-client/assets/favicon.ico");
            res.set_content_provider(
                size, "image/vnd.microsoft.icon",
                [in](size_t offset, size_t length, DataSink &sink) {
                    size_t size_to_read = std::min(static_cast<size_t>(length), (size_t)1048576);
                    char buff[size_to_read];
                    size_t read_len;
                    FS::Seek(in, offset);
                    read_len = FS::Read(in, buff, size_to_read);
                    sink.write(buff, read_len);
                    return read_len == size_to_read;
                },
                [in](bool success) {
                    FS::Close(in);
                });
        });

        svr->Post("/__local__/list", [&](const Request &req, Response &res)
        {
            const char *path;
            bool onlyFolders = false;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                path = json_object_get_string(json_object_object_get(jobj, "path"));
                const char *onlyFolders_text = json_object_get_string(json_object_object_get(jobj, "onlyFolders"));
                if (onlyFolders_text != nullptr && strcasecmp(onlyFolders_text, "true")==0)
                    onlyFolders = true;
                if (path == nullptr)
                {
                    bad_request(res, "Required path parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            int err;
            std::vector<DirEntry> files = FS::ListDir(path, &err);
            DirEntry::Sort(files);
            json_object *json_files = json_object_new_array();
            for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end();)
            {
                if (((onlyFolders && it->isDir) || !onlyFolders) && strcmp(it->name, "..") != 0)
                {
                    json_object *new_file = json_object_new_object();
                    char display_date[32];
                    snprintf(display_date, 32, "%04d-%02d-%02d %02d:%02d:%02d", it->modified.year, it->modified.month, it->modified.day, it->modified.hours, it->modified.minutes, it->modified.seconds);
                    json_object_object_add(new_file, "name", json_object_new_string(it->name));
                    json_object_object_add(new_file, "rights", json_object_new_string(it->display_perm));
                    json_object_object_add(new_file, "date", json_object_new_string(display_date));
                    json_object_object_add(new_file, "size", json_object_new_string(it->isDir ? "" : std::to_string(it->file_size).c_str()));
                    json_object_object_add(new_file, "type", json_object_new_string(it->isDir ? "dir" : "file"));
                    json_object_array_add(json_files, new_file);
                }
                it++;
            }
            json_object *results = json_object_new_object();
            json_object_object_add(results, "result", json_files);
            const char *results_str = json_object_to_json_string(results);
            res.status = 200;
            res.set_content(results_str, strlen(results_str), "application/json");
        });

        svr->Post("/__local__/rename", [&](const Request &req, Response &res)
        {
            const char *item;
            const char *newItemPath;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                item = json_object_get_string(json_object_object_get(jobj, "item"));
                newItemPath = json_object_get_string(json_object_object_get(jobj, "newItemPath"));
                if (item == nullptr || newItemPath == nullptr)
                {
                    bad_request(res, "Required item or newItemPath parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            FS::Rename(item, newItemPath);
            success(res);
            return;
        });

        svr->Post("/__local__/move", [&](const Request &req, Response &res)
        {
            const json_object *items;
            const char *newPath;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                items = json_object_object_get(jobj, "items");
                newPath = json_object_get_string(json_object_object_get(jobj, "newPath"));
                if (items == nullptr || newPath == nullptr)
                {
                    bad_request(res, "Required items or newPath parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            std::string failed_items;
            size_t len = json_object_array_length(items);
            for (size_t i=0; i < len; i++)
            {
                const char *item = json_object_get_string(json_object_array_get_idx(items, i));
                DirEntry entry;
                std::string temp = std::string(item);
                size_t slash_pos = temp.find_last_of("/");
                snprintf(entry.name, 256, "%s", temp.substr(slash_pos+1).c_str());
                snprintf(entry.path, 768, "%s", item);
                entry.isDir = FS::IsFolder(item);
                entry.isLink = FS::IsLink(item);
                std::string new_path = std::string(newPath);
                if (entry.isDir)
                    new_path =  new_path + "/" + entry.name;
                bool ret = CopyOrMove(entry, new_path.c_str(), false);
                if (!ret)
                {
                    failed_items += std::string(item) + ",";
                }
                else
                {
                    if (entry.isDir)
                        FS::RmRecursive(entry.path);
                }
            }

            if (failed_items.length() > 0)
            {
                std::string error_msg = std::string("One or more file(s) failed to move. ") + failed_items;
                failed(res, 200, error_msg);
            }
            else
                success(res);
        });

        svr->Post("/__local__/copy", [&](const Request &req, Response &res)
        {
            const json_object *items;
            const char *newPath;
            const char *singleFilename;

            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                items = json_object_object_get(jobj, "items");
                newPath = json_object_get_string(json_object_object_get(jobj, "newPath"));
                singleFilename = json_object_get_string(json_object_object_get(jobj, "singleFilename"));

                if (items == nullptr || newPath == nullptr)
                {
                    bad_request(res, "Required items or newPath or singleFilename parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            std::string failed_items;
            if (singleFilename != nullptr)
            {
                const char *src = json_object_get_string(json_object_array_get_idx(items, 0));
                std::string dest = std::string(newPath) + "/" + singleFilename;

                std::string temp = std::string(src);
                size_t slash_pos = temp.find_last_of("/");
                DirEntry entry;
                snprintf(entry.name, 256, "%s", temp.substr(slash_pos+1).c_str());
                snprintf(entry.path, 768, "%s", src);
                entry.isDir = FS::IsFolder(src);
                entry.isLink = FS::IsLink(src);

                if (dest.compare(src) != 0 && !CopyOrMove(entry, dest.c_str(), true))
                {
                    failed_items += src;
                }
            }
            else
            {
                size_t len = json_object_array_length(items);
                for (size_t i=0; i < len; i++)
                {
                    const char *item = json_object_get_string(json_object_array_get_idx(items, i));
                    DirEntry entry;
                    std::string temp = std::string(item);
                    size_t slash_pos = temp.find_last_of("/");
                    snprintf(entry.name, 256, "%s", temp.substr(slash_pos+1).c_str());
                    snprintf(entry.path, 768, "%s", item);
                    entry.isDir = FS::IsFolder(item);
                    std::string new_path = std::string(newPath);
                    if (entry.isDir)
                        new_path =  new_path + "/" + entry.name;
                    bool ret = CopyOrMove(entry, new_path.c_str(), true);
                    if (!ret)
                    {
                        failed_items += std::string(item) + ",";
                    }
                }
            }

            if (failed_items.length() > 0)
            {
                std::string error_msg = std::string("One or more file(s) failed to copy. ") + failed_items;
                failed(res, 200, error_msg);
            }
            else
                success(res);
        });

        svr->Post("/__local__/remove", [&](const Request &req, Response &res)
        {
            json_object *items;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                items = json_object_object_get(jobj, "items");
                if (items == nullptr)
                {
                    bad_request(res, "Required items parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            std::string failed_items;
            size_t len = json_object_array_length(items);
            for (size_t i=0; i < len; i++)
            {
                const char *item = json_object_get_string(json_object_array_get_idx(items, i));
                bool ret = FS::RmRecursive(item);
                if (!ret)
                {
                    failed_items += std::string(item) + ",";
                }
            }

            if (failed_items.length() > 0)
            {
                std::string error_msg = std::string("One or more file(s) failed to delete. ") + failed_items;
                failed(res, 200, error_msg);
            }
            else
                success(res);
        });

        svr->Post("/__local__/edit", [&](const Request &req, Response &res)
        {
            const char *item;
            const char *content;
            size_t content_len;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                item = json_object_get_string(json_object_object_get(jobj, "item"));
                json_object *content_obj = json_object_object_get(jobj, "content");
                content = json_object_get_string(content_obj);
                content_len = json_object_get_string_len(content_obj);
                if (item == nullptr || content == nullptr)
                {
                    bad_request(res, "Required item or content parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            bool ret = FS::Save(item, content, content_len);
            if (!ret)
            {
                failed(res, 200, "Failed to save content to file.");
                return;
            }

            success(res);
        });

        svr->Post("/__local__/getContent", [&](const Request &req, Response &res)
        {
            const char *item;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                item = json_object_get_string(json_object_object_get(jobj, "item"));
                if (item == nullptr)
                {
                    bad_request(res, "Required item parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            std::vector<char> content = FS::Load(item);
            json_object *result = json_object_new_object();
            json_object_object_add(result, "result", json_object_new_string(content.data()));
            const char *result_str = json_object_to_json_string(result);
            res.status = 200;
            res.set_content(result_str, strlen(result_str), "application/json");
        });

        svr->Post("/__local__/createFolder", [&](const Request &req, Response &res)
        {
            const char *newPath;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                newPath = json_object_get_string(json_object_object_get(jobj, "newPath"));
                if (newPath == nullptr)
                {
                    bad_request(res, "Required newPath parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            int ret = FS::MkDirs(newPath);
            if (ret == 0)
            {
                failed(res, 200, "Failed to create folder");
                return;
            }
            success(res);
        });

        svr->Post("/__local__/permission", [&](const Request &req, Response &res)
        {
            json_object *items;
            const char* permCode;
            uint32_t perm_code;
            int ret;
            
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                items = json_object_object_get(jobj, "items");
                permCode = json_object_get_string(json_object_object_get(jobj, "permsCode"));

                if (items == nullptr || permCode == nullptr)
                {
                    bad_request(res, "Required items,permsCode parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            perm_code = strtol(permCode, 0, 8);
            size_t len = json_object_array_length(items);
            for (size_t i=0; i < len; i++)
            {
                const char *item = json_object_get_string(json_object_array_get_idx(items, i));
                ret = chmod(item, perm_code);
                if (ret != 0)
                {
                    std::string message = std::string("Failed to change permission for ") + item;
                    failed(res, 200, message);
                    return;
                }
            }
            
            success(res);
        });


        svr->Post("/__local__/compress", [&](const Request &req, Response &res)
        {
            json_object *items;
            const char* destination;
            const char* compressedFilename;
            
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                items = json_object_object_get(jobj, "items");
                destination = json_object_get_string(json_object_object_get(jobj, "destination"));
                compressedFilename = json_object_get_string(json_object_object_get(jobj, "compressedFilename"));

                if (items == nullptr || destination == nullptr || compressedFilename == nullptr)
                {
                    bad_request(res, "Required items,destination,compressedFilename parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            if (!FS::FolderExists(compressed_file_path))
            {
                if (!FS::MkDirs(compressed_file_path))
                {
                    failed(res, 200, "Failed to create zip");
                    return;
                }
            }

            std::string zip_file = std::string(compressed_file_path) + "/" + compressedFilename;
            zipFile zf = zipOpen64(zip_file.c_str(), APPEND_STATUS_CREATE);
            if (zf != NULL)
            {
                size_t len = json_object_array_length(items);
                for (size_t i=0; i < len; i++)
                {
                    const char *item = json_object_get_string(json_object_array_get_idx(items, i));
                    std::string src = std::string(item);
                    size_t slash_pos = src.find_last_of("/");
                    int ret = ZipUtil::ZipAddPath(zf, src, (slash_pos != std::string::npos ? slash_pos + 1 : 1), Z_DEFAULT_COMPRESSION);
                    if (ret != 1)
                    {
                        zipClose(zf, NULL);
                        FS::Rm(zip_file);
                        failed(res, 200, "Failed to create zip");
                    }
                }
                zipClose(zf, NULL);
                success(res);
            }
            else
            {
                failed(res, 200, "Failed to create zip");
            }
        });

        svr->Post("/__local__/extract", [&](const Request &req, Response &res)
        {
            const char* item;
            const char* destination;
            const char* folderName;
            
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                item = json_object_get_string(json_object_object_get(jobj, "item"));
                destination = json_object_get_string(json_object_object_get(jobj, "destination"));
                folderName = json_object_get_string(json_object_object_get(jobj, "folderName"));

                if (item == nullptr || destination == nullptr || folderName == nullptr)
                {
                    bad_request(res, "Required item,destination,folderName parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            std::string extract_zip_folder = std::string(destination) + "/" + folderName;
            DirEntry entry;
            snprintf(entry.name, 256, "%s", "");
            snprintf(entry.path, 768, "%s", item);
            entry.isDir = false;
            if (!FS::MkDirs(extract_zip_folder))
            {
                failed(res, 200, "Failed to extract file");
                return;
            }

            int ret = ZipUtil::Extract(entry, extract_zip_folder);
            if (ret == 0)
                failed(res, 200, "Failed to extract file");
            else if (ret == -1)
                failed(res, 200, "Unsupported compressed file format");
            else
                success(res);
        });

        svr->Get("/__local__/uploadResumeSize", [&](const Request &req, Response &res)
        {
            std::string destination = req.get_param_value("destination");
            std::string filename = req.get_param_value("filename");
            std::string file_path = destination + "/" + filename;
            int64_t size = 0;
            if (FS::FileExists(file_path))
                size = FS::GetSize(file_path);
            std::string result_str = "{\"size\":" + std::to_string(size) + "}";
            res.status = 200;
            res.set_content(result_str.c_str(), result_str.length(), "application/json");
        });

        svr->Post("/__local__/upload", [&](const Request &req, Response &res, const ContentReader &content_reader)
        {
            MultipartFormDataItems items;
            std::string destination;
            size_t chunk_size = 0;
            size_t chunk_number = -1;
            size_t total_size = 0;
            size_t currentChunkSize = 0;
            FILE *out = nullptr;
            std::string new_file;
            content_reader(
                [&](const MultipartFormData &item)
                {
                    items.push_back(item);
                    if (item.name == "file")
                    {
                        new_file = destination + "/" + item.filename;
                        if (out != nullptr)
                        {
                            FS::Close(out);
                        }

                        if (chunk_number == 0)
                            out = FS::Create(new_file);
                        else if (chunk_number > 0)
                            out = FS::Append(new_file);
                    }
                    return true;
                },
                [&](const char *data, size_t data_length)
                {
                    items.back().content.append(data, data_length);
                    if (items.back().name == "destination")
                    {
                        destination = items.back().content;
                    }
                    else if (items.back().name == "_chunkSize")
                    {
                        std::stringstream ss(items.back().content);
                        ss >> chunk_size;
                    }
                    else if (items.back().name == "_chunkNumber")
                    {
                        std::stringstream ss(items.back().content);
                        ss >> chunk_number;
                    }
                    else if (items.back().name == "_totalSize")
                    {
                        std::stringstream ss(items.back().content);
                        ss >> total_size;
                    }
                    else if (items.back().name == "_currentChunkSize")
                    {
                        std::stringstream ss(items.back().content);
                        ss >> currentChunkSize;
                    }
                    else
                    {
                        if (out != nullptr)
                            FS::Write(out, data, data_length);
                    }
                    return true;
                });
            if (out != nullptr)
            {
                FS::Close(out);
            }
            success(res);
        });

        // Download multiple files as ZIP
        svr->Get("/__local__/downloadMultiple", [&](const Request &req, Response &res)
        {
            if (req.get_param_value_count("items") == 0 || req.get_param_value_count("toFilename") == 0)
            {
                failed(res, 200, "Required items and toFilename parameter missing");
                return;
            }

            if (!FS::FolderExists(compressed_file_path) && !FS::MkDirs(compressed_file_path))
            {
                failed(res, 200, "Failed to create zip file");
                return;
            }

            std::string toFilename = req.get_param_value("toFilename");
            std::string zip_file = std::string(compressed_file_path) + "/" + toFilename;
            zipFile zf = zipOpen64(zip_file.c_str(), APPEND_STATUS_CREATE);
            if (zf != NULL)
            {
                int items_count = req.get_param_value_count("items");
                for (size_t i=0; i < items_count; i++)
                {
                    std::string src = req.get_param_value("items", i);
                    size_t slash_pos = src.find_last_of("/");
                    int ret = ZipUtil::ZipAddPath(zf, src, (slash_pos != std::string::npos ? slash_pos + 1 : 1), Z_DEFAULT_COMPRESSION);
                    if (ret != 1)
                    {
                        zipClose(zf, NULL);
                        FS::Rm(zip_file);
                        failed(res, 200, "Failed to create zip file");
                        return;
                    }
                }
                zipClose(zf, NULL);

                // start stream the zip
                FILE *in = FS::OpenRead(zip_file);
                uint64_t size = FS::GetSize(zip_file);
                res.set_header("Content-Disposition", "attachment; filename=\"" + std::string(toFilename) + "\"");
                res.set_content_provider(
                    size, "application/octet-stream",
                    [in](size_t offset, size_t length, DataSink &sink) {
                        size_t size_to_read = std::min(static_cast<size_t>(length), (size_t)1048576);
                        char buff[size_to_read];
                        size_t read_len;
                        FS::Seek(in, offset);
                        read_len = FS::Read(in, buff, size_to_read);
                        sink.write(buff, read_len);
                        return read_len == size_to_read;
                    },
                    [in, zip_file](bool success) {
                        FS::Close(in);
                        FS::Rm(zip_file);
                    });
            }
            else
            {
                failed(res, 200, "Failed to create zip");
            }
        });

        // Download single file
        svr->Get("/__local__/downloadFile", [&](const Request &req, Response &res)
        {
            std::string path = req.get_param_value("path", 0);
            if (path.empty())
            {
                bad_request(res, "Failed to download");
                return;
            }

            int64_t size = FS::GetSize(path);
            FILE *in = FS::OpenRead(path);

            size_t slash_pos = path.find_last_of("/");
            std::string name = path;
            if (slash_pos != std::string::npos)
                name = path.substr(slash_pos+1);

            res.set_header("Content-Disposition", "attachment; filename=\"" + name + "\"");
            res.set_content_provider(
                size, "application/octet-stream",
                [in](size_t offset, size_t length, DataSink &sink) {
                    size_t size_to_read = std::min(static_cast<size_t>(length), (size_t)1048576);
                    char buff[size_to_read];
                    size_t read_len;
                    FS::Seek(in, offset);
                    read_len = FS::Read(in, buff, size_to_read);
                    sink.write(buff, read_len);
                    return read_len == size_to_read;
                },
                [in](bool success) {
                    FS::Close(in);
                }); 
        });

        svr->Post("/__local__/download_url", [&](const Request &req, Response &res)
                  {
            std::string url;
            const char *url_param;
            bool use_alldebrid = false;
            bool use_realdebrid = false;

            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                url_param = json_object_get_string(json_object_object_get(jobj, "url"));
                use_alldebrid  = json_object_get_boolean(json_object_object_get(jobj, "use_alldebrid"));
                use_realdebrid = json_object_get_boolean(json_object_object_get(jobj, "use_realdebrid"));

                if (url_param == nullptr)
                {
                    bad_request(res, "Required url_param parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            if ((use_alldebrid && strlen(alldebrid_api_key) == 0) || (use_realdebrid && strlen(realdebrid_api_key) == 0))
            {
                failed(res, 200, lang_strings[STR_ALLDEBRID_API_KEY_MISSING_MSG]);
                return;
            }

            url = std::string(url_param);
            FileHost *filehost = FileHost::getFileHost(url, use_alldebrid, use_realdebrid);

            if (!filehost->IsValidUrl())
            {
                failed(res, 200, lang_strings[STR_INVALID_URL]);
                return;
            }

            snprintf(activity_message, 1023, "%s %s", lang_strings[STR_DOWNLOADING], filehost->GetUrl().c_str());
            activity_inprogess = true;
            file_transfering = true;
            bytes_to_download = 100;
            bytes_transfered = 0;
            prev_tick = GetMyTick();

            Windows::SetModalMode(true);

            std::string download_url = filehost->GetDownloadUrl();
            if (download_url.empty())
            {
                failed(res, 200, lang_strings[STR_CANT_EXTRACT_URL_MSG]);
                activity_inprogess = false;
                file_transfering = false;
                Windows::SetModalMode(false);
                return;
            }
            delete(filehost);

			size_t scheme_pos = download_url.find("://");
			size_t root_pos = download_url.find("/", scheme_pos + 3);
			std::string host = download_url.substr(0, root_pos);
			std::string path = download_url.substr(root_pos);
            std::string filename = BaseClient::UnEscape( path.substr(path.find_last_of("/")+1, path.find_first_of("?")-(path.find_last_of("/")+1)));
            std::string file_path = std::string(temp_folder) + "/" + filename;

            BaseClient *baseclient = new BaseClient();
            baseclient->Connect(host, "", "");
            
            if (!baseclient->FileExists(path))
            {
                failed(res, 200, baseclient->LastResponse());
                activity_inprogess = false;
                file_transfering = false;
                Windows::SetModalMode(false);
                return;
            }

            bytes_transfered = 0;
            baseclient->Size(path, &bytes_to_download);

            if (!baseclient->Get(file_path, path, 0))
            {
                failed(res, 200, baseclient->LastResponse());
                activity_inprogess = false;
                file_transfering = false;
                Windows::SetModalMode(false);
                return;
            }

            activity_inprogess = false;
            file_transfering = false;
            Windows::SetModalMode(false);
            success(res); });


        svr->Get("/stop", [&](const Request & /*req*/, Response & /*res*/)
        {
            svr->stop();
        });

        svr->set_error_handler([](const Request & /*req*/, Response &res)
        {
            const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
            char buf[BUFSIZ];
            snprintf(buf, sizeof(buf), fmt, res.status);
            res.set_content(buf, "text/html");
        });

        /*
        svr->set_logger([](const Request &req, const Response &res)
        {
            dbglogger_log("%s", log(req, res).c_str());
        });
        */
       
        svr->set_payload_max_length(1024 * 1024 * 12);
        svr->set_tcp_nodelay(true);
        svr->set_mount_point("/", std::string(g_root_path) + "/share/ezremote-client/assets");

        if (web_server_enabled)
            svr->listen("0.0.0.0", http_server_port);
        else
            svr->listen("127.0.0.1", http_server_port);

        return NULL;
    }

    void Start()
    {
        if (svr == nullptr)
            svr = new Server();
        if (!svr->is_valid())
        {
            return;
        }
        int ret = pthread_create(&http_server_thid, NULL, ServerThread, NULL);
    }

    void Stop()
    {
        if (svr != nullptr)
            svr->stop();
    }
}
