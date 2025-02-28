#include <string>
#include <cstring>
#include <map>
#include <vector>
#include <regex>
#include <stdlib.h>
#include <pwd.h>
#include <libgen.h>

#include "server/http_server.h"
#include "config.h"
#include "fs.h"
#include "lang.h"
#include "crypt.h"
#include "base64.h"

extern "C"
{
#include "inifile.h"
}

bool swap_xo;
RemoteSettings *remote_settings;
char local_directory[PATH_MAX+ 1];
char remote_directory[PATH_MAX+ 1];
char app_ver[6];
char last_site[32];
char display_site[32];
std::vector<std::string> langs;
char language[128];
std::vector<std::string> sites;
std::vector<std::string> http_servers;
std::set<std::string> text_file_extensions;
std::set<std::string> image_file_extensions;
std::map<std::string, RemoteSettings> site_settings;
bool show_hidden_files;
char alldebrid_api_key[64];
char realdebrid_api_key[64];
char temp_folder[PATH_MAX+ 1];
char g_data_path[PATH_MAX+ 1];
char config_ini_path[PATH_MAX+ 1];
char g_root_path[PATH_MAX+ 1];

unsigned char cipher_key[32] = {'s', '5', 'v', '8', 'y', '/', 'B', '?', 'E', '(', 'H', '+', 'M', 'b', 'Q', 'e', 'T', 'h', 'W', 'm', 'Z', 'q', '4', 't', '7', 'w', '9', 'z', '$', 'C', '&', 'F'};
unsigned char cipher_iv[16] = {'Y', 'p', '3', 's', '6', 'v', '9', 'y', '$', 'B', '&', 'E', ')', 'H', '@', 'M'};

RemoteClient *remoteclient;

namespace CONFIG
{
    int Encrypt(const std::string &text, std::string &encrypt_text)
    {
        unsigned char tmp_encrypt_text[text.length() * 2];
        int encrypt_text_len;
        memset(tmp_encrypt_text, 0, sizeof(tmp_encrypt_text));
        int ret = openssl_encrypt((unsigned char *)text.c_str(), text.length(), cipher_key, cipher_iv, tmp_encrypt_text, &encrypt_text_len);
        if (ret == 0)
            return 0;
        return Base64::Encode(std::string((const char *)tmp_encrypt_text, encrypt_text_len), encrypt_text);
    }

    int Decrypt(const std::string &text, std::string &decrypt_text)
    {
        std::string tmp_decode_text;
        int ret = Base64::Decode(text, tmp_decode_text);
        if (ret == 0)
            return 0;

        unsigned char tmp_decrypt_text[tmp_decode_text.length() * 2];
        int decrypt_text_len;
        memset(tmp_decrypt_text, 0, sizeof(tmp_decrypt_text));
        ret = openssl_decrypt((unsigned char *)tmp_decode_text.c_str(), tmp_decode_text.length(), cipher_key, cipher_iv, tmp_decrypt_text, &decrypt_text_len);
        if (ret == 0)
            return 0;

        decrypt_text.clear();
        decrypt_text.append(std::string((const char *)tmp_decrypt_text, decrypt_text_len));

        return 1;
    }

    void SetClientType(RemoteSettings *setting)
    {
        if (strncmp(setting->server, "smb://", 6) == 0)
        {
            setting->type = CLIENT_TYPE_SMB;
        }
        else if (strncmp(setting->server, "ftp://", 6) == 0)
        {
            setting->type = CLIENT_TYPE_FTP;
        }
        else if (strncmp(setting->server, "sftp://", 7) == 0)
        {
            setting->type = CLIENT_TYPE_SFTP;
        }
        else if (strncmp(setting->server, "webdav://", 9) == 0 || strncmp(setting->server, "webdavs://", 10) == 0)
        {
            setting->type = CLIENT_TYPE_WEBDAV;
        }
        else if (strncmp(setting->server, "http://", 7) == 0 || strncmp(setting->server, "https://", 8) == 0)
        {
            setting->type = CLIENT_TYPE_HTTP_SERVER;
        }
        else if (strncmp(setting->server, "nfs://", 6) == 0)
        {
            setting->type = CLIENT_TYPE_NFS;
        }
        else
        {
            setting->type = CLINET_TYPE_UNKNOWN;
        }
    }

    void LoadCipherKeys()
    {
    }

    void LoadConfig()
    {
        char default_temp_folder[PATH_MAX + 1];
        char default_compress_folder[PATH_MAX + 1];

        char *root_path_p = realpath("/proc/self/exe", nullptr);
        if (root_path_p)
        {
            snprintf(g_root_path, PATH_MAX, "%s/..", dirname(root_path_p));
            free(root_path_p);
        }
        else
        {
            sprintf(g_root_path, "%s", "/usr");
        }

        LoadCipherKeys();

        struct passwd *pw = getpwuid(getuid());
        snprintf(g_data_path, 256, "%s/%s", pw->pw_dir, APP_ID);
        snprintf(config_ini_path, 256, "%s/%s/config.ini", pw->pw_dir, APP_ID);
        snprintf(default_temp_folder, 256, "%s/Downloads", pw->pw_dir);
        snprintf(default_compress_folder, 256, "%s/Downloads", pw->pw_dir);

        if (!FS::FolderExists(g_data_path))
        {
            FS::MkDirs(g_data_path);
        }

        sites = {"Site 1", "Site 2", "Site 3", "Site 4", "Site 5", "Site 6", "Site 7", "Site 8", "Site 9", "Site 10",
                 "Site 11", "Site 12", "Site 13", "Site 14", "Site 15", "Site 16", "Site 17", "Site 18", "Site 19", "Site 20"};

        langs = { "Arabic", "Catalan", "Croatian", "Dutch", "English", "Euskera", "French", "Galego", "German", "Greek", 
                  "Hungarian", "Indonesian", "Italiano", "Japanese", "Korean", "Polish", "Portuguese_BR", "Russian", "Romanian", "Ryukyuan", "Spanish", "Turkish",
                  "Simplified Chinese", "Traditional Chinese", "Thai", "Ukrainian", "Vietnamese"};

        http_servers = {HTTP_SERVER_APACHE, HTTP_SERVER_MS_IIS, HTTP_SERVER_NGINX, HTTP_SERVER_NPX_SERVE, HTTP_SERVER_RCLONE, HTTP_SERVER_ARCHIVEORG, HTTP_SERVER_MYRIENT, HTTP_SERVER_GITHUB};

        OpenIniFile(config_ini_path);

        int version = ReadInt(CONFIG_GLOBAL, CONFIG_VERSION, 0);
        bool conversion_needed = false;
        if (version < CONFIG_VERSION_NUM)
        {
            conversion_needed = true;
        }
        WriteInt(CONFIG_GLOBAL, CONFIG_VERSION, CONFIG_VERSION_NUM);

        // Load global config
        snprintf(language, 128, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LANGUAGE, "English"));
        WriteString(CONFIG_GLOBAL, CONFIG_LANGUAGE, language);

        snprintf(local_directory, 255, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LOCAL_DIRECTORY, "/"));
        WriteString(CONFIG_GLOBAL, CONFIG_LOCAL_DIRECTORY, local_directory);

        show_hidden_files = ReadBool(CONFIG_GLOBAL, CONFIG_SHOW_HIDDEN_FILES, false);
        WriteBool(CONFIG_GLOBAL, CONFIG_SHOW_HIDDEN_FILES, show_hidden_files);

        snprintf(temp_folder, 256, ReadString(CONFIG_GLOBAL, CONFIG_TMP_FOLDER_PATH, default_temp_folder));
        WriteString(CONFIG_GLOBAL, CONFIG_TMP_FOLDER_PATH, temp_folder);

        if (!FS::FolderExists(temp_folder))
        {
            FS::MkDirs(temp_folder);
        }

        // alldebrid api key
        char tmp_api_key[512];
        snprintf(tmp_api_key, 512, "%s", ReadString(CONFIG_GLOBAL, CONFIG_ALLDEBRID_API_KEY, ""));
        std::string encrypted_api_key;
        if (strlen(tmp_api_key) > 0)
        {
            std::string decrypted_api_key;
            int ret = Decrypt(tmp_api_key, decrypted_api_key);
            if (ret == 0)
                snprintf(alldebrid_api_key, 64, "%s", tmp_api_key);
            else
                snprintf(alldebrid_api_key, 64, "%s", decrypted_api_key.c_str());
            Encrypt(alldebrid_api_key, encrypted_api_key);
        }
        WriteString(CONFIG_GLOBAL, CONFIG_ALLDEBRID_API_KEY, encrypted_api_key.c_str());

        // realdebrid api key
        snprintf(tmp_api_key, 512, "%s", ReadString(CONFIG_GLOBAL, CONFIG_REALDEBRID_API_KEY, ""));
        encrypted_api_key = "";
        if (strlen(tmp_api_key) > 0)
        {
            std::string decrypted_api_key;
            int ret = Decrypt(tmp_api_key, decrypted_api_key);
            if (ret == 0)
                snprintf(realdebrid_api_key, 64, "%s", tmp_api_key);
            else
                snprintf(realdebrid_api_key, 64, "%s", decrypted_api_key.c_str());
            Encrypt(realdebrid_api_key, encrypted_api_key);
        }
        WriteString(CONFIG_GLOBAL, CONFIG_REALDEBRID_API_KEY, encrypted_api_key.c_str());

        // Http Server Info
        http_server_port = ReadInt(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_PORT, 8090);
        WriteInt(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_PORT, http_server_port);

        snprintf(compressed_file_path, 1024, "%s", ReadString(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_COMPRESSED_FILE_PATH, default_compress_folder));
        WriteString(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_COMPRESSED_FILE_PATH, compressed_file_path);
        if (!FS::FolderExists(compressed_file_path))
        {
            FS::MkDirs(compressed_file_path);
        }

        web_server_enabled = ReadBool(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_ENABLED, true);
        WriteBool(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_ENABLED, web_server_enabled);

        for (int i = 0; i < sites.size(); i++)
        {
            RemoteSettings setting;
            memset(&setting, 0, sizeof(RemoteSettings));
            snprintf(setting.site_name, 32, "%s", sites[i].c_str());

            snprintf(setting.server, 256, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_URL, ""));
            if (conversion_needed && strlen(setting.server) > 0)
            {
                std::string tmp = std::string(setting.server);
                tmp = std::regex_replace(tmp, std::regex("http://"), "webdav://");
                tmp = std::regex_replace(tmp, std::regex("https://"), "webdavs://");
                snprintf(setting.server, 256, "%s", tmp.c_str());
            }
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_URL, setting.server);

            snprintf(setting.username, 33, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_USER, ""));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_USER, setting.username);

            char tmp_password[128];
            snprintf(tmp_password, 128, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_PASSWORD, ""));
            std::string encrypted_password;
            if (strlen(tmp_password) > 0)
            {
                std::string decrypted_password;
                int ret = Decrypt(tmp_password, decrypted_password);
                if (ret == 0)
                    snprintf(setting.password, 128, "%s", tmp_password);
                else
                    snprintf(setting.password, 128, "%s", decrypted_password.c_str());
                Encrypt(setting.password, encrypted_password);
            }
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_PASSWORD, encrypted_password.c_str());

            snprintf(setting.http_server_type, 24, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_HTTP_SERVER_TYPE, HTTP_SERVER_APACHE));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_HTTP_SERVER_TYPE, setting.http_server_type);

            snprintf(setting.default_directory, 256, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_DEFAULT_DIRECTORY, "/"));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_DEFAULT_DIRECTORY, setting.default_directory);

            SetClientType(&setting);
            site_settings.insert(std::make_pair(sites[i], setting));
        }

        snprintf(last_site, 32, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LAST_SITE, sites[0].c_str()));
        WriteString(CONFIG_GLOBAL, CONFIG_LAST_SITE, last_site);

        remote_settings = &site_settings[std::string(last_site)];
        snprintf(remote_directory, 256, "%s", remote_settings->default_directory);

        WriteIniFile(config_ini_path);
        CloseIniFile();
    }

    void SaveConfig()
    {
        OpenIniFile(config_ini_path);

        std::string encrypted_text;
        if (strlen(remote_settings->password) > 0)
            Encrypt(remote_settings->password, encrypted_text);
        else
            encrypted_text = std::string(remote_settings->password);
        WriteString(last_site, CONFIG_REMOTE_SERVER_URL, remote_settings->server);
        WriteString(last_site, CONFIG_REMOTE_SERVER_USER, remote_settings->username);
        WriteString(last_site, CONFIG_REMOTE_SERVER_PASSWORD, encrypted_text.c_str());
        WriteString(last_site, CONFIG_REMOTE_HTTP_SERVER_TYPE, remote_settings->http_server_type);
        WriteString(last_site, CONFIG_REMOTE_DEFAULT_DIRECTORY, remote_settings->default_directory);
        WriteString(CONFIG_GLOBAL, CONFIG_LAST_SITE, last_site);
        
        WriteIniFile(config_ini_path);
        CloseIniFile();
    }

    void SaveGlobalConfig()
    {
        OpenIniFile(config_ini_path);

        std::string encrypted_api_key;
        if (strlen(alldebrid_api_key) > 0)
            Encrypt(alldebrid_api_key, encrypted_api_key);
        else
            encrypted_api_key = std::string(alldebrid_api_key);
        WriteString(CONFIG_GLOBAL, CONFIG_ALLDEBRID_API_KEY, encrypted_api_key.c_str());

        if (strlen(realdebrid_api_key) > 0)
            Encrypt(realdebrid_api_key, encrypted_api_key);
        else
            encrypted_api_key = std::string(realdebrid_api_key);
        WriteString(CONFIG_GLOBAL, CONFIG_REALDEBRID_API_KEY, encrypted_api_key.c_str());

        if (!FS::FolderExists(temp_folder))
        {
            FS::MkDirs(temp_folder);
        }
        
        WriteString(CONFIG_GLOBAL, CONFIG_TMP_FOLDER_PATH, temp_folder);
        WriteBool(CONFIG_GLOBAL, CONFIG_SHOW_HIDDEN_FILES, show_hidden_files);
        WriteString(CONFIG_GLOBAL, CONFIG_LANGUAGE, language);
        WriteInt(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_PORT, http_server_port);
        WriteString(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_COMPRESSED_FILE_PATH, compressed_file_path);
        WriteBool(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_ENABLED, web_server_enabled);

        WriteIniFile(config_ini_path);
        CloseIniFile();

        if (!FS::FolderExists(temp_folder))
        {
            FS::MkDirs(temp_folder);
        }

        if (!FS::FolderExists(compressed_file_path))
        {
            FS::MkDirs(compressed_file_path);
        }
    }

    void SaveLocalDirecotry(const std::string &path)
    {
        OpenIniFile(config_ini_path);
        WriteString(CONFIG_GLOBAL, CONFIG_LOCAL_DIRECTORY, path.c_str());
        WriteIniFile(config_ini_path);
        CloseIniFile();
    }

}
