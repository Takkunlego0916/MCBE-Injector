#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <tlhelp32.h>
#include <commdlg.h>
#include <string>

const wchar_t* CONFIG_FILE = L".\\config.ini";

DWORD GetProcessIdByName(const std::wstring& name) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, name.c_str()) == 0) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

void SaveLastDLL(const std::wstring& path) {
    WritePrivateProfileStringW(L"Settings", L"LastDLL", path.c_str(), CONFIG_FILE);
}

std::wstring LoadLastDLL() {
    wchar_t buffer[MAX_PATH] = { 0 };
    GetPrivateProfileStringW(L"Settings", L"LastDLL", L"", buffer, MAX_PATH, CONFIG_FILE);
    return std::wstring(buffer);
}

void Inject(const std::wstring& dllPath, const std::wstring& targetProcess) {
    DWORD pid = GetProcessIdByName(targetProcess);
    if (pid == 0) {
        MessageBoxW(NULL, (L"�v���Z�X��������܂���: " + targetProcess).c_str(), L"�G���[", MB_ICONERROR);
        return;
    }

    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) {
        MessageBoxW(NULL, L"�v���Z�X�̓W�J�Ɏ��s���܂����B�Ǘ��Ҍ����Ŏ��s���Ă��������B", L"�G���[", MB_ICONERROR);
        return;
    }

    size_t pathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    void* loc = VirtualAllocEx(hProc, 0, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (loc) {
        WriteProcessMemory(hProc, loc, dllPath.c_str(), pathSize, 0);

        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        FARPROC loadLibAddr = GetProcAddress(hKernel32, "LoadLibraryW");

        HANDLE hThread = CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)loadLibAddr, loc, 0, 0);
        if (hThread) {
            MessageBoxW(NULL, L"�C���W�F�N�g�ɐ������܂����I", L"����", MB_ICONINFORMATION);
            CloseHandle(hThread);
        }
        else {
            MessageBoxW(NULL, L"�����[�g�X���b�h�̍쐬�Ɏ��s���܂����B", L"�G���[", MB_ICONERROR);
        }
    }
    CloseHandle(hProc);
}

std::wstring OpenFileDialog(HWND hwnd) {
    wchar_t szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) return std::wstring(szFile);
    return L"";
}

BOOL CALLBACK SetFontCallback(HWND hwnd, LPARAM lParam) {
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hBtnSelect, hBtnInject, hBtnUnlock, hEditProcess, hLabelTitle, hLabelFile;
    static std::wstring selectedPath;
    static bool isLocked = true;
    static HFONT hFont;

    switch (uMsg) {
    case WM_CREATE: {
        hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        hLabelTitle = CreateWindowW(L"STATIC", L"�^�[�Q�b�g�v���Z�X:", WS_VISIBLE | WS_CHILD, 20, 15, 120, 20, hwnd, NULL, NULL, NULL);
        hEditProcess = CreateWindowW(L"EDIT", L"Minecraft.Windows.exe", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_DISABLED, 20, 35, 190, 24, hwnd, NULL, NULL, NULL);
        hBtnUnlock = CreateWindowW(L"BUTTON", L"Unlock", WS_VISIBLE | WS_CHILD, 220, 34, 60, 26, hwnd, (HMENU)3, NULL, NULL);

        hLabelFile = CreateWindowW(L"STATIC", L"�I������Ă��܂���", WS_VISIBLE | WS_CHILD | SS_LEFT | SS_PATHELLIPSIS, 20, 75, 260, 20, hwnd, NULL, NULL, NULL);

        hBtnSelect = CreateWindowW(L"BUTTON", L"DLL��I��", WS_VISIBLE | WS_CHILD, 20, 105, 120, 32, hwnd, (HMENU)1, NULL, NULL);
        hBtnInject = CreateWindowW(L"BUTTON", L"Inject", WS_VISIBLE | WS_CHILD, 150, 105, 130, 32, hwnd, (HMENU)2, NULL, NULL);

        EnumChildWindows(hwnd, SetFontCallback, (LPARAM)hFont);

        selectedPath = LoadLastDLL();
        if (!selectedPath.empty()) {
            std::wstring fileName = selectedPath.substr(selectedPath.find_last_of(L"/\\") + 1);
            SetWindowTextW(hLabelFile, (L"DLL: " + fileName).c_str());
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            std::wstring temp = OpenFileDialog(hwnd);
            if (!temp.empty()) {
                selectedPath = temp;
                SaveLastDLL(selectedPath);
                std::wstring fileName = selectedPath.substr(selectedPath.find_last_of(L"/\\") + 1);
                SetWindowTextW(hLabelFile, (L"DLL: " + fileName).c_str());
            }
        }
        if (LOWORD(wParam) == 2) {
            wchar_t procName[MAX_PATH];
            GetWindowTextW(hEditProcess, procName, MAX_PATH);
            if (!selectedPath.empty()) {
                Inject(selectedPath, procName);
            }
            else {
                MessageBoxW(hwnd, L"���DLL��I�����Ă��������B", L"�x��", MB_ICONWARNING);
            }
        }
        if (LOWORD(wParam) == 3) {
            isLocked = !isLocked;
            EnableWindow(hEditProcess, !isLocked);
            SetWindowTextW(hBtnUnlock, isLocked ? L"Unlock" : L"Lock");
        }
        break;

    case WM_DESTROY:
        DeleteObject(hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    const wchar_t CLASS_NAME[] = L"MCBEInjectorClass";
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"MCBE Injector", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 200, NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nShow);
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
