#ifndef __EZ_LANG_H__
#define __EZ_LANG_H__

#include "imgui.h"
#include "config.h"

#define FOREACH_STR(FUNC)                   \
	FUNC(STR_CONNECTION_SETTINGS)           \
	FUNC(STR_SITE)                          \
	FUNC(STR_LOCAL)                         \
	FUNC(STR_REMOTE)                        \
	FUNC(STR_MESSAGES)                      \
	FUNC(STR_UPDATE_SOFTWARE)               \
	FUNC(STR_CONNECT)                       \
	FUNC(STR_DISCONNECT)                    \
	FUNC(STR_SEARCH)                        \
	FUNC(STR_REFRESH)                       \
	FUNC(STR_SERVER)                        \
	FUNC(STR_USERNAME)                      \
	FUNC(STR_PASSWORD)                      \
	FUNC(STR_PORT)                          \
	FUNC(STR_PASV)                          \
	FUNC(STR_DIRECTORY)                     \
	FUNC(STR_FILTER)                        \
	FUNC(STR_YES)                           \
	FUNC(STR_NO)                            \
	FUNC(STR_CANCEL)                        \
	FUNC(STR_CONTINUE)                      \
	FUNC(STR_CLOSE)                         \
	FUNC(STR_FOLDER)                        \
	FUNC(STR_FILE)                          \
	FUNC(STR_TYPE)                          \
	FUNC(STR_NAME)                          \
	FUNC(STR_SIZE)                          \
	FUNC(STR_DATE)                          \
	FUNC(STR_NEW_FOLDER)                    \
	FUNC(STR_RENAME)                        \
	FUNC(STR_DELETE)                        \
	FUNC(STR_UPLOAD)                        \
	FUNC(STR_DOWNLOAD)                      \
	FUNC(STR_SELECT_ALL)                    \
	FUNC(STR_CLEAR_ALL)                     \
	FUNC(STR_UPLOADING)                     \
	FUNC(STR_DOWNLOADING)                   \
	FUNC(STR_OVERWRITE)                     \
	FUNC(STR_DONT_OVERWRITE)                \
	FUNC(STR_ASK_FOR_CONFIRM)               \
	FUNC(STR_DONT_ASK_CONFIRM)              \
	FUNC(STR_ALLWAYS_USE_OPTION)            \
	FUNC(STR_ACTIONS)                       \
	FUNC(STR_CONFIRM)                       \
	FUNC(STR_OVERWRITE_OPTIONS)             \
	FUNC(STR_PROPERTIES)                    \
	FUNC(STR_PROGRESS)                      \
	FUNC(STR_UPDATES)                       \
	FUNC(STR_DEL_CONFIRM_MSG)               \
	FUNC(STR_CANCEL_ACTION_MSG)             \
	FUNC(STR_FAIL_UPLOAD_MSG)               \
	FUNC(STR_FAIL_DOWNLOAD_MSG)             \
	FUNC(STR_FAIL_READ_LOCAL_DIR_MSG)       \
	FUNC(STR_CONNECTION_CLOSE_ERR_MSG)      \
	FUNC(STR_REMOTE_TERM_CONN_MSG)          \
	FUNC(STR_FAIL_LOGIN_MSG)                \
	FUNC(STR_FAIL_TIMEOUT_MSG)              \
	FUNC(STR_FAIL_DEL_DIR_MSG)              \
	FUNC(STR_DELETING)                      \
	FUNC(STR_FAIL_DEL_FILE_MSG)             \
	FUNC(STR_DELETED)                       \
	FUNC(STR_LINK)                          \
	FUNC(STR_SHARE)                         \
	FUNC(STR_FAILED)                        \
	FUNC(STR_FAIL_CREATE_LOCAL_FILE_MSG)    \
	FUNC(STR_INSTALL)                       \
	FUNC(STR_INSTALLING)                    \
	FUNC(STR_INSTALL_SUCCESS)               \
	FUNC(STR_INSTALL_FAILED)                \
	FUNC(STR_INSTALL_SKIPPED)               \
	FUNC(STR_CHECK_HTTP_MSG)                \
	FUNC(STR_FAILED_HTTP_CHECK)             \
	FUNC(STR_REMOTE_NOT_HTTP)               \
	FUNC(STR_INSTALL_FROM_DATA_MSG)         \
	FUNC(STR_ALREADY_INSTALLED_MSG)         \
	FUNC(STR_INSTALL_FROM_URL)              \
	FUNC(STR_CANNOT_READ_PKG_HDR_MSG)       \
	FUNC(STR_FAVORITE_URLS)                 \
	FUNC(STR_SLOT)                          \
	FUNC(STR_EDIT)                          \
	FUNC(STR_ONETIME_URL)                   \
	FUNC(STR_NOT_A_VALID_PACKAGE)           \
	FUNC(STR_WAIT_FOR_INSTALL_MSG)          \
	FUNC(STR_FAIL_INSTALL_TMP_PKG_MSG)      \
	FUNC(STR_FAIL_TO_OBTAIN_GG_DL_MSG)      \
	FUNC(STR_AUTO_DELETE_TMP_PKG)           \
	FUNC(STR_PROTOCOL_NOT_SUPPORTED)        \
	FUNC(STR_COULD_NOT_RESOLVE_HOST)        \
	FUNC(STR_EXTRACT)                       \
	FUNC(STR_EXTRACTING)                    \
	FUNC(STR_FAILED_TO_EXTRACT)             \
	FUNC(STR_EXTRACT_LOCATION)              \
	FUNC(STR_COMPRESS)                      \
	FUNC(STR_ZIP_FILE_PATH)                 \
	FUNC(STR_COMPRESSING)                   \
	FUNC(STR_ERROR_CREATE_ZIP)              \
	FUNC(STR_UNSUPPORTED_FILE_FORMAT)       \
	FUNC(STR_CUT)                           \
	FUNC(STR_COPY)                          \
	FUNC(STR_PASTE)                         \
	FUNC(STR_MOVING)                        \
	FUNC(STR_COPYING)                       \
	FUNC(STR_FAIL_MOVE_MSG)                 \
	FUNC(STR_FAIL_COPY_MSG)                 \
	FUNC(STR_CANT_MOVE_TO_SUBDIR_MSG)       \
	FUNC(STR_CANT_COPY_TO_SUBDIR_MSG)       \
	FUNC(STR_UNSUPPORTED_OPERATION_MSG)     \
	FUNC(STR_HTTP_PORT)                     \
	FUNC(STR_REINSTALL_CONFIRM_MSG)         \
	FUNC(STR_REMOTE_NOT_SUPPORT_MSG)        \
	FUNC(STR_CANNOT_CONNECT_REMOTE_MSG)     \
	FUNC(STR_DOWNLOAD_INSTALL_MSG)          \
	FUNC(STR_CHECKING_REMOTE_SERVER_MSG)    \
	FUNC(STR_ENABLE_RPI)                    \
	FUNC(STR_ENABLE_RPI_FTP_SMB_MSG)        \
	FUNC(STR_ENABLE_RPI_WEBDAV_MSG)         \
	FUNC(STR_FILES)                         \
	FUNC(STR_EDITOR)                        \
	FUNC(STR_SAVE)                          \
	FUNC(STR_MAX_EDIT_FILE_SIZE_MSG)        \
	FUNC(STR_DELETE_LINE)                   \
	FUNC(STR_INSERT_LINE)                   \
	FUNC(STR_MODIFIED)                      \
	FUNC(STR_FAIL_GET_TOKEN_MSG)            \
	FUNC(STR_GET_TOKEN_SUCCESS_MSG)         \
	FUNC(STR_PERM_DRIVE)                    \
	FUNC(STR_PERM_DRIVE_APPDATA)            \
	FUNC(STR_PERM_DRIVE_FILE)               \
	FUNC(STR_PERM_DRIVE_METADATA)           \
	FUNC(STR_PERM_DRIVE_METADATA_RO)        \
	FUNC(STR_GOOGLE_LOGIN_FAIL_MSG)         \
	FUNC(STR_GOOGLE_LOGIN_TIMEOUT_MSG)      \
	FUNC(STR_NEW_FILE)                      \
	FUNC(STR_SETTINGS)                      \
	FUNC(STR_CLIENT_ID)                     \
	FUNC(STR_CLIENT_SECRET)                 \
	FUNC(STR_GLOBAL)                        \
	FUNC(STR_GOOGLE)                        \
	FUNC(STR_COPY_LINE)                     \
	FUNC(STR_PASTE_LINE)                    \
	FUNC(STR_SHOW_HIDDEN_FILES)             \
	FUNC(STR_SET_DEFAULT_DIRECTORY)         \
	FUNC(STR_SET_DEFAULT_DIRECTORY_MSG)     \
	FUNC(STR_VIEW_IMAGE)                    \
	FUNC(STR_VIEW_PKG_INFO)                 \
	FUNC(STR_NFS_EXP_PATH_MISSING_MSG)      \
	FUNC(STR_FAIL_INIT_NFS_CONTEXT)         \
	FUNC(STR_FAIL_MOUNT_NFS_MSG)            \
	FUNC(STR_WEB_SERVER)                    \
	FUNC(STR_ENABLE)                        \
	FUNC(STR_COMPRESSED_FILE_PATH)          \
	FUNC(STR_COMPRESSED_FILE_PATH_MSG)      \
	FUNC(STR_ALLDEBRID)                     \
	FUNC(STR_API_KEY)                       \
	FUNC(STR_CANT_EXTRACT_URL_MSG)          \
	FUNC(STR_FAIL_INSTALL_FROM_URL_MSG)     \
	FUNC(STR_INVALID_URL)                   \
	FUNC(STR_ALLDEBRID_API_KEY_MISSING_MSG) \
	FUNC(STR_LANGUAGE)                      \
	FUNC(STR_TEMP_DIRECTORY)                \
	FUNC(STR_REALDEBRID)                    \
	FUNC(STR_BACKGROUND_INSTALL_INPROGRESS) \
	FUNC(STR_ENABLE_DISC_CACHE_MSG)         \
	FUNC(STR_ENABLE_DISK_CACHE)             \
	FUNC(STR_ENABLE_ALLDEBRID_MSG)          \
	FUNC(STR_ENABLE_REALDEBRID_MSG)         \
	FUNC(STR_ENABLE_DISKCACHE_DESC)         \
	FUNC(STR_FAIL_CREATE_LOCAL_FOLDER_MSG)  \
	FUNC(STR_PERMISSIONS)                   \
	FUNC(STR_READ)                          \
	FUNC(STR_WRITE)                         \
	FUNC(STR_EXECUTE)                       \
	FUNC(STR_OWNER)                         \
	FUNC(STR_GROUP)                         \
	FUNC(STR_OTHER)                         \
	FUNC(STR_UPDATE)                        \
	FUNC(STR_FAIL_UPDATE_PERMISSION_MSG)

#define GET_VALUE(x) x,
#define GET_STRING(x) #x,

enum
{
	FOREACH_STR(GET_VALUE)
};

#define LANG_STRINGS_NUM 177
#define LANG_ID_SIZE 64
#define LANG_STR_SIZE 384
extern char lang_identifiers[LANG_STRINGS_NUM][LANG_ID_SIZE];
extern char lang_strings[LANG_STRINGS_NUM][LANG_STR_SIZE];
extern bool needs_extended_font;

	static const ImWchar latin[] = { // 
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x0100, 0x024F, // Latin Extended
		0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
		0x1E00, 0x1EFF, // Latin Extended Additional
		0x2000, 0x206F, // General Punctuation
		0x2100, 0x214F, // Letterlike Symbols
		0x2460, 0x24FF, // Enclosed Alphanumerics
		0x2DE0, 0x2DFF, // Cyrillic Extended-A
		0x31F0, 0x31FF, // Katakana Phonetic Extensions
		0xA640, 0xA69F, // Cyrillic Extended-B
		0xFF00, 0xFFEF, // Half-width characters
		0,
	};

	static const ImWchar arabic[] = { // Arabic
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x0100, 0x024F, // Latin Extended
		0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
		0x0600, 0x06FF, // Arabic
		0x0750, 0x077F, // Arabic Supplement
		0x0870, 0x089F, // Arabic Extended-B
		0x08A0, 0x08FF, // Arabic Extended-A
		0x1E00, 0x1EFF, // Latin Extended Additional
		0x2000, 0x206F, // General Punctuation
		0x2100, 0x214F, // Letterlike Symbols
		0x2460, 0x24FF, // Enclosed Alphanumerics
		0x2DE0, 0x2DFF, // Cyrillic Extended-A
		0x31F0, 0x31FF, // Katakana Phonetic Extensions
		0xA640, 0xA69F, // Cyrillic Extended-B
		0xFB50, 0xFDFF, // Arabic Presentation Forms-A
		0xFE70, 0xFEFF, // Arabic Presentation Forms-B
		0xFF00, 0xFFEF, // Half-width characters
		0,
	};

	static const ImWchar fa_icons[] {
		0xF07B, 0xF07B, // folder
		0xF65E, 0xF65E, // new folder
		0xF15B, 0xF15B, // file
		0xF021, 0xF021, // refresh
		0xF0CA, 0xF0CA, // select all
		0xF0C9, 0xF0C9, // unselect all
		0x2700, 0x2700, // cut
		0xF0C5, 0xF0C5, // copy
		0xF0EA, 0xF0EA, // paste
		0xF31C, 0xF31C, // edit
		0xE0AC, 0xE0AC, // rename
		0xE5A1, 0xE5A1, // delete
		0xF002, 0xF002, // search
		0xF013, 0xF013, // settings
		0xF0ED, 0xF0ED, // download
		0xF0EE, 0xF0EE, // upload
		0xF56E, 0xF56E, // extract
		0xF56F, 0xF56F, // compress
		0xF0F6, 0xF0F6, // properties
		0xF112, 0xF112, // cancel
		0xF0DA, 0xF0DA, // arrow right
		0x0031, 0x0031, // 1
		0x004C, 0x004C, // L
		0x0052, 0x0052, // R
		0,
	};

	static const ImWchar of_icons[] {
		0xE0CB, 0xE0CB, // square
		0xE0DE, 0xE0DE, // triangle
		0,
	};


namespace Lang
{
	void SetTranslation();
}

#endif