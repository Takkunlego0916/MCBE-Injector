#include <windows.h>
#include <tlhelp32.h>
#include <commdlg.h>
#include <string>
#include <fstream>

// ワイド文字列変換
std::wstring s2ws(const std::string& s) {
    int slength = (int)s.length() + 1;
    int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    std::wstring r(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &r[0], len);
    return r;
}

// プロセスID取得
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

// 記憶用のパス保存/読み込み
void SaveLastDLL(std::string path) {
    std::ofstream f("config.txt");
    if (f.is_open()) f << path;
}

std::string LoadLastDLL() {
    std::ifstream f("config.txt");
    std::string path;
    if (f.is_open()) std::getline(f, path);
    return path;
}

// インジェクト処理
void Inject(std::string dllPath, std::wstring targetProcess) {
    DWORD pid = GetProcessIdByName(targetProcess);
    if (pid == 0) {
        MessageBoxW(NULL, (L"Target process not found: " + targetProcess).c_str(), L"Error", MB_ICONERROR);
        return;
    }
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        MessageBoxA(NULL, "OpenProcess failed. Run as Admin!", "Error", MB_ICONERROR);
        return;
    }
    void* loc = VirtualAllocEx(hProc, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProc, loc, dllPath.c_str(), dllPath.length() + 1, 0);
    HANDLE hThread = CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, loc, 0, 0);
    if (hThread) {
        MessageBoxA(NULL, "Successfully Injected!", "Success", MB_ICONINFORMATION);
        CloseHandle(hThread);
    } else {
        MessageBoxA(NULL, "Injection Failed.", "Error", MB_ICONERROR);
    }
    CloseHandle(hProc);
}

std::string OpenFileDialog() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "DLL Files\0*.dll\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(szFile);
    return "";
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hBtnSelect, hBtnInject, hBtnUnlock, hEditProcess, hLabel;
    static std::string selectedPath;
    static bool isLocked = true;

    switch (uMsg) {
    case WM_CREATE:
        CreateWindowA("STATIC", "Target Process:", WS_VISIBLE | WS_CHILD, 20, 15, 100, 20, hwnd, NULL, NULL, NULL);
        hEditProcess = CreateWindowA("EDIT", "Minecraft.Windows.exe", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_DISABLED, 20, 35, 180, 22, hwnd, NULL, NULL, NULL);
        hBtnUnlock = CreateWindowA("BUTTON", "Unlock", WS_VISIBLE | WS_CHILD, 210, 35, 60, 22, hwnd, (HMENU)3, NULL, NULL);
        
        hLabel = CreateWindowA("STATIC", "No file selected", WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 70, 260, 20, hwnd, NULL, NULL, NULL);
        hBtnSelect = CreateWindowA("BUTTON", "Select DLL", WS_VISIBLE | WS_CHILD, 20, 100, 110, 30, hwnd, (HMENU)1, NULL, NULL);
        hBtnInject = CreateWindowA("BUTTON", "Inject", WS_VISIBLE | WS_CHILD, 150, 100, 110, 30, hwnd, (HMENU)2, NULL, NULL);

        // 起動時に最後に使ったDLLを読み込む
        selectedPath = LoadLastDLL();
        if (!selectedPath.empty()) {
            std::string fileName = selectedPath.substr(selectedPath.find_last_of("/\\") + 1);
            SetWindowTextA(hLabel, (std::string("Last DLL: ") + fileName).c_str());
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // Select
            std::string temp = OpenFileDialog();
            if (!temp.empty()) {
                selectedPath = temp;
                SaveLastDLL(selectedPath); // パスを保存
                SetWindowTextA(hLabel, (std::string("File: ") + selectedPath.substr(selectedPath.find_last_of("/\\") + 1)).c_str());
            }
        }
        if (LOWORD(wParam) == 2) { // Inject
            char procName[256];
            GetWindowTextA(hEditProcess, procName, 256);
            if (!selectedPath.empty()) Inject(selectedPath, s2ws(procName));
            else MessageBoxA(hwnd, "Please select a DLL first.", "Warning", MB_ICONWARNING);
        }
        if (LOWORD(wParam) == 3) { // Unlock
            isLocked = !isLocked;
            EnableWindow(hEditProcess, !isLocked);
            SetWindowTextA(hBtnUnlock, isLocked ? "Unlock" : "Lock");
        }
        break;

    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    const char CLASS_NAME[] = "MCBEInjectorClass";
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, CLASS_NAME, "MCBE Injector", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 200, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nShow);
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}