#include "fs.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <stdio.h>
#include <unistd.h>
#include <vector>

#include "util.h"
#include "lang.h"
#include "windows.h"

namespace FS
{
    int hasEndSlash(const char *path)
    {
        return path[strlen(path) - 1] == '/';
    }

    int MkDirs(const std::string &ppath, bool prev)
    {
        std::string path = ppath;
        if (!prev)
        {
            path.push_back('/');
        }
        auto ptr = path.begin();
        int ret = 0;
        while (true)
        {
            ptr = std::find(ptr, path.end(), '/');
            if (ptr == path.end())
                break;

            char last = *ptr;
            *ptr = 0;
            errno = 0;

            if (strcmp(path.c_str(), "") != 0)
            {
                ret = mkdir(path.c_str(), 0750);
                if (ret != 0 && errno != EEXIST)
                {
                    return 0;
                }
            }
            *ptr = last;
            ++ptr;
        }

        return 1;
    }

    int Rm(const std::string &file)
    {
        if (!remove(file.c_str()))
            return 0;
        return 1;
    }

    int RmDir(const std::string &path)
    {
        if (!remove(path.c_str()))
            return 0;
        return 1;
    }

    int64_t GetSize(const std::string &path)
    {
        struct stat file_stat = {0};
        int err = stat(path.c_str(), &file_stat);
        if (err < 0)
        {
            return -1;
        }
        return file_stat.st_size;
    }

    bool FileExists(const std::string &path)
    {
        struct stat file_stat = {0};
        return (stat(path.c_str(), std::addressof(file_stat)) == 0 && S_ISREG(file_stat.st_mode));
    }

    bool FolderExists(const std::string &path)
    {
        struct stat dir_stat = {0};
        return (stat(path.c_str(), &dir_stat) == 0 && S_ISDIR(dir_stat.st_mode));
    }

    int IsFolder(const std::string &path)
    {
        struct stat dir_stat = {0};
        if (stat(path.c_str(), &dir_stat) != 0)
            return -1;
        if (S_ISDIR(dir_stat.st_mode))
            return 1;
        return 0;
    
    }

    int IsLink(const std::string &path)
    {
        struct stat link_stat = {0};
        if (lstat(path.c_str(), &link_stat) != 0)
            return -1;
        if (S_ISLNK(link_stat.st_mode))
            return 1;
        return 0;
    
    }

    int Rename(const std::string &from, const std::string &to)
    {
        int res = rename(from.c_str(), to.c_str());
        return res;
    }

    FILE *Create(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "w");

        return fd;
    }

    FILE *OpenRW(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "w+");
        return fd;
    }

    FILE *OpenRead(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "rb");
        return fd;
    }

    FILE *Append(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "a");
        return fd;
    }

    int64_t Seek(FILE *f, uint64_t offset)
    {
        auto const pos = fseek(f, offset, SEEK_SET);
        return pos;
    }

    int Read(FILE *f, void *buffer, uint32_t size)
    {
        const auto read = fread(buffer, 1, size, f);
        return read;
    }

    int Write(FILE *f, const void *buffer, uint32_t size)
    {
        int write = fwrite(buffer, 1, size, f);
        return write;
    }

    void Close(FILE *fd)
    {
        int err = fclose(fd);
    }

    std::vector<char> Load(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "rb");
        if (fd == nullptr)
            return std::vector<char>(0);
        const auto size = GetSize(path);
        std::vector<char> data(size);

        const auto read = fread(data.data(), 1, data.size(), fd);
        fclose(fd);
        if (read < 0)
            return std::vector<char>(0);

        data.resize(read+1);
        data[data.size()-1]=0;
        return data;
    }

    bool LoadText(std::vector<std::string> *lines, const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "rb");
        if (fd == nullptr)
            return false;

        lines->clear();

        char buffer[1024];
        short bytes_read;
        std::vector<char> line = std::vector<char>(0);
        do
        {
            bytes_read = fread(buffer, sizeof(char), 1024, fd);
            if (bytes_read < 0)
            {
                fclose(fd);
                return false;
            }

            for (short i = 0; i < bytes_read; i++)
            {
                if (buffer[i] != '\r' && buffer[i] != '\n')
                {
                    line.push_back(buffer[i]);
                }
                else
                {
                    lines->push_back(std::string(line.data(), line.size()));
                    line = std::vector<char>(0);
                    if (buffer[i] == '\r' && buffer[i+1] == '\n')
                        i++;
                }
            }
        } while (bytes_read == 1024);
        if (line.size()>0)
            lines->push_back(std::string(line.data(), line.size()));
 
        fclose(fd);
        if (lines->size() == 0)
            lines->push_back("");
        return true;
    }

    bool SaveText(std::vector<std::string> *lines, const std::string &path)
    {
        FILE *fd = OpenRW(path);
        if (fd == nullptr)
            return false;

        char nl[1] = {'\n'};
        for (int i=0; i < lines->size(); i++)
        {
            Write(fd, lines->at(i).c_str(), lines->at(i).length());
            Write(fd, nl, 1);
        }

        fclose(fd);

        return true;
    }

    bool Save(const std::string &path, const void *data, uint32_t size)
    {
        FILE *fd = fopen(path.c_str(), "w+");
        if (fd == nullptr)
            return false;

        const char *data8 = static_cast<const char *>(data);
        while (size != 0)
        {
            int written = fwrite(data8, 1, size, fd);
            fclose(fd);
            if (written <= 0)
                return false;
            data8 += written;
            size -= written;
        }
        return true;
    }

    std::vector<DirEntry> ListDir(const std::string &ppath, int *err)
    {
        std::vector<DirEntry> out;
        DirEntry entry;
        std::string path = ppath;

        memset(&entry, 0, sizeof(DirEntry));
        snprintf(entry.directory, 512, "%s", path.c_str());
        snprintf(entry.name, 256, "%s", "..");
        snprintf(entry.display_size, 48, "%s", lang_strings[STR_FOLDER]);
        snprintf(entry.path, 768, "%s", path.c_str());
        entry.file_size = 0;
        entry.isDir = true;
        entry.selectable = false;
        out.push_back(entry);

        DIR *fd = opendir(path.c_str());
        *err = 0;
        if (fd == NULL)
        {
            *err = 1;
            return out;
        }

        while (true)
        {
            struct dirent *dirent;
            DirEntry entry = {0};
            dirent = readdir(fd);
            if (dirent == NULL)
            {
                closedir(fd);
                return out;
            }
            else
            {
                if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0 || (!show_hidden_files && dirent->d_name[0] == '.'))
                {
                    continue;
                }

                snprintf(entry.directory, 512, "%s", path.c_str());
                snprintf(entry.name, 256, "%s", dirent->d_name);
                entry.selectable = true;

                if (hasEndSlash(path.c_str()))
                {
                    snprintf(entry.path, 768, "%s%s", path.c_str(), dirent->d_name);
                }
                else
                {
                    snprintf(entry.path, 768, "%s/%s", path.c_str(), dirent->d_name);
                }
                struct stat file_stat = {0};
                stat(entry.path, &file_stat);
                struct tm tm = *localtime(&file_stat.st_mtim.tv_sec);

                entry.modified.day = tm.tm_mday;
                entry.modified.month = tm.tm_mon + 1;
                entry.modified.year = tm.tm_year + 1900;
                entry.modified.hours = tm.tm_hour;
                entry.modified.minutes = tm.tm_min;
                entry.modified.seconds = tm.tm_sec;
                entry.file_size = file_stat.st_size;
                entry.st_mode = file_stat.st_mode;
                DirEntry::SetDisplayPerm(&entry);

                if (dirent->d_type == DT_LNK)
                {
                    entry.isLink = true;
                    entry.isDir = false;
                    entry.file_size =0;

                    if (S_ISDIR(file_stat.st_mode))
                    {
                        snprintf(entry.display_size, 48, "[%s]", lang_strings[STR_FOLDER]);
                        entry.isDir = true;
                    }
                    else
                    {
                        snprintf(entry.display_size, 48, "%s", lang_strings[STR_LINK]);
                    }
                }
                else if (dirent->d_type == DT_DIR)
                {
                    entry.isDir = true;
                    entry.file_size = 0;
                    snprintf(entry.display_size, 48, "%s", lang_strings[STR_FOLDER]);
                }
                else
                {
                    if (entry.file_size < 1024)
                    {
                        snprintf(entry.display_size, 48, "%luB", entry.file_size);
                    }
                    else if (entry.file_size < 1024 * 1024)
                    {
                        snprintf(entry.display_size, 48, "%.2fKB", entry.file_size * 1.0f / 1024);
                    }
                    else if (entry.file_size < 1024 * 1024 * 1024)
                    {
                        snprintf(entry.display_size, 48, "%.2fMB", entry.file_size * 1.0f / (1024 * 1024));
                    }
                    else
                    {
                        snprintf(entry.display_size, 48, "%.2fGB", entry.file_size * 1.0f / (1024 * 1024 * 1024));
                    }
                    entry.isDir = false;
                }
                out.push_back(entry);
            }
        }
        closedir(fd);

        return out;
    }

    std::vector<std::string> ListFiles(const std::string &path)
    {
        DIR *fd = opendir(path.c_str());
        if (fd == NULL)
            return std::vector<std::string>(0);

        std::vector<std::string> out;
        while (true)
        {
            struct dirent *dirent;
            dirent = readdir(fd);
            if (dirent == NULL)
            {
                closedir(fd);
                return out;
            }

            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
            {
                continue;
            }

            if (dirent->d_type & DT_DIR)
            {
                std::vector<std::string> files = FS::ListFiles(path + "/" + dirent->d_name);
                for (std::vector<std::string>::iterator it = files.begin(); it != files.end();)
                {
                    out.push_back(std::string(dirent->d_name) + "/" + *it);
                    ++it;
                }
            }
            else
            {
                out.push_back(dirent->d_name);
            }
        }
        closedir(fd);
        return out;
    }

    int RmRecursive(const std::string &path)
    {
        if (stop_activity)
            return 1;

        struct stat file_stat = {0};
        lstat(path.c_str(), &file_stat);

        if (!S_ISLNK(file_stat.st_mode))
        {
            DIR *dfd = opendir(path.c_str());
            if (dfd != NULL)
            {
                struct dirent *dir = NULL;
                do
                {
                    dir = readdir(dfd);
                    if (dir == NULL || strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                    {
                        continue;
                    }

                    char new_path[512];
                    snprintf(new_path, 512, "%s%s%s", path.c_str(), hasEndSlash(path.c_str()) ? "" : "/", dir->d_name);

                    if (dir->d_type & DT_DIR)
                    {
                        int ret = RmRecursive(new_path);
                        if (ret <= 0)
                        {
                            snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_DEL_DIR_MSG], new_path);
                            closedir(dfd);
                            return ret;
                        }
                    }
                    else
                    {
                        snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETING], new_path);
                        int ret = remove(new_path);
                        if (ret < 0)
                        {
                            snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_DEL_FILE_MSG], new_path);
                            closedir(dfd);
                            return ret;
                        }
                    }
                } while (dir != NULL && !stop_activity);

                closedir(dfd);

                if (stop_activity)
                    return 0;

                int ret = rmdir(path.c_str());
                if (ret < 0)
                {
                    snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_DEL_DIR_MSG], path.c_str());
                    return ret;
                }
                snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETED], path.c_str());
            }
            else
            {
                int ret = remove(path.c_str());
                if (ret < 0)
                {
                    snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_DEL_FILE_MSG], path.c_str());
                    return ret;
                }
                snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETED], path.c_str());
            }
        }
        else
        {
            int ret = remove(path.c_str());
            if (ret < 0)
            {
                snprintf(status_message, 1024, "%s %s", lang_strings[STR_FAIL_DEL_FILE_MSG], path.c_str());
                return ret;
            }
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETED], path.c_str());
        }
        return 1;
    }

    std::string GetPath(const std::string &ppath1, const std::string &ppath2)
    {
        std::string path1 = ppath1;
        std::string path2 = ppath2;
        path2 = Util::Rtrim(Util::Trim(path2, " "), "/");
        return path1 + "/" + path2;
    }

    int Head(const std::string &path, void *buffer, uint16_t len)
    {
        FILE *file = OpenRead(path);
        if (file == nullptr)
            return 0;
        int ret = Read(file, buffer, len);
        if (ret != len)
        {
            Close(file);
            return 0;
        }
        Close(file);
        return 1;
    }

    bool Copy(const std::string &from, const std::string &to)
    {
        int ret;

        if (!MkDirs(to, true))
        {
            return false;
        }

        if (from.compare(to) == 0)
            return true;

        struct stat file_stat = {0};
        ret = lstat(from.c_str(), &file_stat);
        if (ret != 0)
        {
            return false;
        }

        if (S_ISLNK(file_stat.st_mode))
        {
            char link_path[PATH_MAX+1];
            int link_path_length = readlink(from.c_str(), link_path, PATH_MAX);
            if (link_path_length > -1)
            {
                link_path[link_path_length] = 0;
                errno = 0;
                Rm(to);
                ret = symlink(link_path, to.c_str());
                if (ret != 0)
                {
                    return false;
                }
            }
        }
        else
        {
            FILE *src = fopen(from.c_str(), "rb");
            if (!src)
            {
                return false;
            }

            struct stat file_stat = {0};
            if (stat(from.c_str(), &file_stat) != 0)
            {
                return false;
            }

            bytes_to_download = file_stat.st_size;

            FILE *dest = fopen(to.c_str(), "wb");
            if (!dest)
            {
                fclose(src);
                return false;
            }

            size_t bytes_read = 0;
            bytes_transfered = 0;
            prev_tick = GetMyTick();

            const size_t buf_size = 0x10000;
            unsigned char *buf = new unsigned char[buf_size];

            do
            {
                bytes_read = fread(buf, sizeof(unsigned char), buf_size, src);
                if (bytes_read < 0)
                {
                    delete[] buf;
                    fclose(src);
                    fclose(dest);
                    return false;
                }

                size_t bytes_written = fwrite(buf, sizeof(unsigned char), bytes_read, dest);
                if (bytes_written != bytes_read)
                {
                    delete[] buf;
                    fclose(src);
                    fclose(dest);
                    return false;
                }

                bytes_transfered += bytes_read;
            } while (bytes_transfered < bytes_to_download);

            delete[] buf;
            fclose(src);
            fclose(dest);
        }

        return true;
    }

    bool Move(const std::string &from, const std::string &to)
    {
        if (from.compare(to) == 0)
            return true;

        errno = 0;
        int ret = rename(from.c_str(), to.c_str());
        if (ret != 0 && (errno == EXDEV || errno == EEXIST))
        {
            bool res = Copy(from, to);
            if (res)
            {
                Rm(from);
                return 1;
            }
            else
                return res;
        }
        
        return ret == 0;
    }
}
