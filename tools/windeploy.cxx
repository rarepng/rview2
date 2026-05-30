#include <windows.h>

__attribute__((constructor)) void initialize_win32_dll_search_path() {
    SetDllDirectoryW(L"libs");
}