#ifndef EZ_CONFIG_H
#define EZ_CONFIG_H

#include <string>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <set>

#include "clients/remote_client.h"

#define APP_ID ".ezremote-client"

#define CONFIG_GLOBAL "Global"

#define CONFIG_SHOW_HIDDEN_FILES "show_hidden_files"

#define CONFIG_HTTP_SERVER "HttpServer"
#define CONFIG_HTTP_SERVER_PORT "http_server_port"
#define CONFIG_HTTP_SERVER_ENABLED "http_server_enabled"
#define CONFIG_HTTP_SERVER_COMPRESSED_FILE_PATH "compressed_files_path"

#define CONFIG_REMOTE_SERVER_NAME "remote_server_name"
#define CONFIG_REMOTE_SERVER_URL "remote_server_url"
#define CONFIG_REMOTE_SERVER_USER "remote_server_user"
#define CONFIG_REMOTE_SERVER_PASSWORD "remote_server_password"
#define CONFIG_REMOTE_SERVER_HTTP_PORT "remote_server_http_port"
#define CONFIG_REMOTE_HTTP_SERVER_TYPE "remote_server_http_server_type"
#define CONFIG_REMOTE_DEFAULT_DIRECTORY "remote_server_default_directory"

#define CONFIG_ALLDEBRID_API_KEY "alldebrid_api_key"
#define CONFIG_REALDEBRID_API_KEY "realdebrid_api_key"

#define CONFIG_VERSION "config_version"
#define CONFIG_VERSION_NUM 1

#define CONFIG_LAST_SITE "last_site"

#define CONFIG_LOCAL_DIRECTORY "local_directory"
#define CONFIG_TMP_FOLDER_PATH "temp_folder"

#define CONFIG_LANGUAGE "language"

#define HTTP_SERVER_APACHE "Apache"
#define HTTP_SERVER_MS_IIS "Msft IIS"
#define HTTP_SERVER_NGINX "Nginx"
#define HTTP_SERVER_NPX_SERVE "Serve"
#define HTTP_SERVER_RCLONE "RClone"
#define HTTP_SERVER_ARCHIVEORG "Archive.org"
#define HTTP_SERVER_MYRIENT "Myrient"
#define HTTP_SERVER_GITHUB "Github"

struct RemoteSettings
{
    char site_name[32];
    char server[256];
    char username[33];
    char password[128];
    ClientType type;
    uint32_t supported_actions;
    char http_server_type[24];
    char default_directory[256];
};

extern std::vector<std::string> sites;
extern std::vector<std::string> http_servers;
extern std::set<std::string> text_file_extensions;
extern std::set<std::string> image_file_extensions;
extern std::map<std::string, RemoteSettings> site_settings;
extern char local_directory[PATH_MAX+ 1];
extern char remote_directory[PATH_MAX+ 1];
extern char app_ver[6];
extern char last_site[32];
extern char display_site[32];
extern std::vector<std::string> langs;
extern char language[128];
extern RemoteSettings *remote_settings;
extern RemoteClient *remoteclient;
extern unsigned char cipher_key[32];
extern unsigned char cipher_iv[16];
extern bool show_hidden_files;
extern char alldebrid_api_key[64];
extern char realdebrid_api_key[64];
extern char temp_folder[PATH_MAX+ 1];
extern char g_data_path[PATH_MAX+ 1];
extern char g_root_path[PATH_MAX+ 1];

namespace CONFIG
{
    void LoadConfig();
    void SaveConfig();
    void SaveGlobalConfig();
    void SaveLocalDirecotry(const std::string &path);
    void SaveFavoriteUrl(int index, char *url);
    void SetClientType(RemoteSettings *settings);
}
#endif
