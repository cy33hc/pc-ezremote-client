# ezRemote Client for Steam Gamemode

ezRemote Client is a controller friendly File Manager application that allows you to connect your Linux Handheld to remote FTP/SFTP, SMB(Windows Share), NFS, WebDAV, HTTP servers(Apache/Nginx/IIS/RClone etc...), Archive.org and Myrient to transfer files. The interface is inspired by Filezilla client which provides a commander like GUI. This is a port from the vita, ps3, ps4 and switch.

App is designed to run in Steam GameScope mode, so you can download/upload/copy/delete/paste/extract/compress files just using the controller and touch screen.

NOTE: This is just a hobby of mine and app is certainly not suitable for Production usage. It's single threaded, meaning you can do only 1 operation at a time, and transfer speed may not be as good since it doesn't download multiple parts simultaneously.

![Preview](/screenshot.png)

![Preview](/ezremote_client_web.png)

## Usage
To distinguish between FTP, SFTP, SMB, NFS, WebDAV or HTTP, the URL must be prefix with **ftp://**, **sftp://**, **smb://**, **nfs://**, **webdav://**, **webdavs://**, **http://** and **https://**

 - The url format for FTP/SFTP is
   ```
   ftp://hostname[:port]
   sftp://hostname[:port]

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 21(ftp) and 22(sftp) if not provided
   ```
   For Secure FTP (sftp), use of identity files is possible. Put both the **id_rsa** and **id_rsa.pub** into a the same folder. Then in the password field in the UI, instead of putting a password reference the folder where id_rsa and id_rsa.pub is place. Prefix the folder with **"file://"** and **do not** password protect the identity file.
   ```
   Example: If you had placed the id_rsa and id_rsa.pub files into the folder /home/user/.ssh,
   then in the password field enter file:///home/user/.ssh
   ```

 - The url format for SMB(Windows Share) is
   ```
   smb://hostname[:port]/sharename

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 445 if not provided
     - sharename is required
   ```

 - The url format for NFS is
   ```
   nfs://hostname[:port]/export_path[?uid=<UID>&gid=<GID>]

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 2049 if not provided
     - export_path is required
     - uid is the UID value to use when talking to the server. Defaults to 65534 if not specified.
     - gid is the GID value to use when talking to the server. Defaults to 65534 if not specified.

     Special characters in 'path' are escaped using %-hex-hex syntax.

     For example '?' must be escaped if it occurs in a path as '?' is also used to
     separate the path from the optional list of url arguments.

     Example:
     nfs://192.168.0.1/my?path?uid=1000&gid=1000
     must be escaped as
     nfs://192.168.0.1/my%3Fpath?uid=1000&gid=1000
   ```

 - The url format for WebDAV is
   ```
   webdav://hostname[:port]/[url_path]
   webdavs://hostname[:port]/[url_path]

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 80(webdav) and 443(webdavs) if not provided
     - url_path is optional based on your WebDAV hosting requiremets
   ```

- The url format for HTTP Server is
   ```
   http://hostname[:port]/[url_path]
   https://hostname[:port]/[url_path]
     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 80(http) and 443(https) if not provided
     - url_path is optional based on your HTTP Server hosting requiremets
   ```
- For Internet Archive repos
  - Only supports parsing of the download URL (ie the URL where you see a list of files). Example
    |      |           |  |
    |----------|-----------|---|
    | ![archive_org_screen1](https://github.com/user-attachments/assets/b129b6cf-b938-4d7c-a3fa-61e1c633c1f6) | ![archive_org_screen2](https://github.com/user-attachments/assets/646106d1-e60b-4b35-b153-3475182df968)| ![image](https://github.com/user-attachments/assets/cad94de8-a694-4ef5-92a8-b87468e30adb) |

- For Myrient, enter the following URL https://myrient.erista.me/files in the server field

- Support for browse and download  release artifacts from github repos. Under the server just enter the git repo of the homebrew eg https://github.com/cy33hc/ps4-ezremote-client
  ![image](https://github.com/user-attachments/assets/f8e931ea-f1d1-4af8-aed5-b0dfe661a230)

Tested with following WebDAV server:
 - **(Recommeded)** [Dufs](https://github.com/sigoden/dufs) - For hosting your own WebDAV server.
 - [Rclone](https://rclone.org/)
 - [SFTPgo](https://github.com/drakkan/sftpgo) - For local hosted WebDAV server. Can also be used as a webdav frontend for Cloud Storage like AWS S3, Azure Blob or Google Storage.
 - box.com (Note: delete folder does not work. This is an issue with box.com and not the app)
 - mega.nz (via the [megacmd tool](https://mega.io/cmd))
 - 4shared.com
 - drivehq.com

## Features Native Application##
 - Manage/Transfer files back and forth on FTP/SFTP/SMB/NFS/WebDAV/HTTP server
 - Support for connecting to Http Servers like (Apache/Nginx,Microsoft IIS, Serve) with html directory listings to download files.
 - Support for listing and downloading files from Archive.org and Myrient
 - Create Zip files
 - Extract from zip, 7zip, rar, tar, tar.gz files
 - File management function include cut/copy/paste/rename/delete/new folder/file

## Features in Web Interface ##
 - Copy/Move/Delete/Rename/Create files/folders
 - Extract 7zip, rar and zip files
 - Compress files into zip directly
 - Edit text files
 - View all common image formats
 - Upload files
 - Download files from URL directly or via (All-Debrid and Real-Debrid)
 
## How to access the Web Interface ##
You need to launch the "ezRemote Client" app. Then on any device(laptop, tablet, phone etc..) with web browser goto to http://<ip_address_of_device>:8090 . That's all.

The port# can be changed from the "Global Settings" dialog. Any changes to the web server settings needs a restart of the application to take effect.

## Gamepad Controls
```
A - Select Button/TextBox
B - Un-Select the file list to navigate to other widgets or Close Dialog window in most cases
Y - Menu (after a file(s)/folder(s) is selected)
X - Multi select files
RB - Navigate to the Remote list of files
LB - Navigate to the Local list of files
LT - To go up a directory from current directory
Start Button - Exit Application
```
## Keyboard Controls
```
Space - Select Button/TextBox
Esc - Un-Select the file list to navigate to other widgets or Close Dialog window in most cases
Alt - Menu (after a file(s)/folder(s) is selected)
Insert - Multi select files
Ctrl-1 - Navigate to the Local list of files
Ctrl-2 - Navigate to the Remote list of files
Backspace - To go up a directory from current directory
Ctrl-Q - Exit Application
```

## Multi Language Support
The appplication support following languages.

**Note:** Translations are not complete for some languagess. Please help by downloading this [Template](https://github.com/cy33hc/pc-ezremote-client/blob/master/data/ezremote-client/assets/langs/English.ini), make your changes and submit an issue with the file attached for the language.

The following languages are supported.
```
Dutch
English
French
German
Italiano
Japanese
Korean
Polish
Portuguese_BR
Russian
Spanish
Simplified Chinese
Traditional Chinese
Arabic
Catalan
Croatian
Euskera
Galego
Greek
Hungarian
Indonesian
Romanian
Ryukyuan
Thai
Turkish
Ukrainian
Vietnamese
```
