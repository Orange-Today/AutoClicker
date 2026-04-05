/*
 * 自动按键器 - 游戏兼容版（键盘+鼠标连点）
 * 编译：gcc -static -mwindows -o AutoClicker.exe autoclicker.c -lcomctl32 -luser32 -lgdi32
 */

#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "comctl32.lib")

// 控件ID
#define ID_LIST_BOX        1001
#define ID_BTN_EDIT_CFG    1002
#define ID_BTN_RELOAD      1003
#define ID_BTN_START       1004
#define ID_BTN_STOP        1005
#define ID_CHK_LOOP        1006
#define ID_STATIC_HINT     1007
#define ID_STATIC_HOTKEY   1008

// 鼠标控件ID
#define ID_COMBO_MOUSE_TYPE   2001
#define ID_COMBO_MOUSE_SPEED  2002
#define ID_EDIT_MOUSE_DELAY   2003
#define ID_BTN_MOUSE_START    2004
#define ID_BTN_MOUSE_STOP     2005
#define ID_STATIC_MOUSE_HOTKEY 2006

// 按键项结构
typedef struct KeyItem {
    char szKey[64];
    DWORD dwDelay;
    struct KeyItem* next;
} KeyItem;

KeyItem* g_pHead = NULL;
KeyItem* g_pTail = NULL;
HWND     g_hListBox = NULL;
HWND     g_hMainWnd = NULL;
HANDLE   g_hThread = NULL;
volatile BOOL g_bRunning = FALSE;
BOOL     g_bLoopForever = FALSE;
char     g_szConfigPath[MAX_PATH];
UINT     g_uHotKey = VK_F6;           // 键盘默认 F6
char     g_szHotKeyName[32];

// 鼠标连点相关
volatile BOOL g_bMouseRunning = FALSE;
HANDLE   g_hMouseThread = NULL;
UINT     g_uMouseHotKey = VK_F4;      // 鼠标默认 F4
char     g_szMouseHotKeyName[32];
int      g_mouseClickType = 0;        // 0=左键,1=右键,2=中键
int      g_mouseDelay = 100;          // 毫秒
HWND     g_hComboMouseType = NULL;
HWND     g_hComboMouseSpeed = NULL;
HWND     g_hEditMouseDelay = NULL;
HWND     g_hBtnMouseStart = NULL;
HWND     g_hBtnMouseStop = NULL;
HWND     g_hStaticMouseHotkey = NULL;

// 函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void    LoadConfig();
void    SaveDefaultConfig();
void    RefreshListBox();
void    FreeKeyList();
void    EditConfigFile();
void    OnStart();
void    OnStop();
void    RegisterHotKeyCtrl();
void    UnregisterHotKeyCtrl();
DWORD WINAPI ThreadProc(LPVOID);
void    SendCombinedKey(const char* keySeq);
void    GetKeyNameByVK(UINT vk, char* out);
UINT    ParseHotkeyFromString(const char* str);
void    GetConfigPath();
BOOL    IsAdmin();

// 鼠标函数
void    OnMouseStart();
void    OnMouseStop();
DWORD WINAPI MouseThreadProc(LPVOID);
void    SendMouseClick(int type);
void    SaveMouseConfig(FILE* f);
void    LoadMouseConfigLine(const char* line);

// 检查管理员权限
BOOL IsAdmin() {
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    BOOL isAdmin = FALSE;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0,0,0,0,0,0, &AdministratorsGroup)) {
        CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin);
        FreeSid(AdministratorsGroup);
    }
    return isAdmin;
}

// 获取配置文件路径
void GetConfigPath() {
    GetModuleFileName(NULL, g_szConfigPath, MAX_PATH);
    char* p = strrchr(g_szConfigPath, '\\');
    if (p) p[1] = '\0';
    strcat(g_szConfigPath, "config.txt");
}

// 虚拟键码转可读名称
void GetKeyNameByVK(UINT vk, char* out) {
    if (vk >= 'A' && vk <= 'Z') { out[0] = (char)vk; out[1] = '\0'; }
    else if (vk >= '0' && vk <= '9') { out[0] = (char)vk; out[1] = '\0'; }
    else if (vk >= VK_F1 && vk <= VK_F12) sprintf(out, "F%d", vk - VK_F1 + 1);
    else if (vk == VK_RETURN) strcpy(out, "Enter");
    else if (vk == VK_TAB) strcpy(out, "Tab");
    else if (vk == VK_SPACE) strcpy(out, "Space");
    else if (vk == VK_ESCAPE) strcpy(out, "Esc");
    else if (vk == VK_BACK) strcpy(out, "Backspace");
    else if (vk == VK_CONTROL) strcpy(out, "Ctrl");
    else if (vk == VK_MENU) strcpy(out, "Alt");
    else if (vk == VK_SHIFT) strcpy(out, "Shift");
    else if (vk == VK_LWIN) strcpy(out, "Win");
    else sprintf(out, "0x%02X", vk);
}

// 从字符串解析热键
UINT ParseHotkeyFromString(const char* str) {
    char* endptr; long val = strtol(str, &endptr, 10);
    if (*endptr == '\0' && val >= 1 && val <= 255) return (UINT)val;
    if (strlen(str) >= 2 && (str[0] == 'F' || str[0] == 'f')) {
        int num = atoi(str + 1);
        if (num >= 1 && num <= 12) return VK_F1 + (num - 1);
    }
    if (strlen(str) == 1 && isalpha(str[0])) return toupper(str[0]);
    if (stricmp(str, "Enter") == 0) return VK_RETURN;
    if (stricmp(str, "Tab") == 0) return VK_TAB;
    if (stricmp(str, "Space") == 0) return VK_SPACE;
    if (stricmp(str, "Esc") == 0) return VK_ESCAPE;
    if (stricmp(str, "Backspace") == 0) return VK_BACK;
    if (stricmp(str, "Ctrl") == 0) return VK_CONTROL;
    if (stricmp(str, "Alt") == 0) return VK_MENU;
    if (stricmp(str, "Shift") == 0) return VK_SHIFT;
    if (stricmp(str, "Win") == 0) return VK_LWIN;
    return VK_F4;
}

// 保存鼠标配置到文件
void SaveMouseConfig(FILE* f) {
    fprintf(f, "# 鼠标连点配置\n");
    char keyName[32];
    GetKeyNameByVK(g_uMouseHotKey, keyName);
    fprintf(f, "MouseHotkey=%s\n", keyName);
    const char* typeStr = (g_mouseClickType == 0) ? "left" : (g_mouseClickType == 1) ? "right" : "middle";
    fprintf(f, "MouseClickType=%s\n", typeStr);
    fprintf(f, "MouseDelay=%d\n", g_mouseDelay);
    fprintf(f, "# ===================================\n\n");
}

// 加载鼠标配置行
void LoadMouseConfigLine(const char* line) {
    if (strnicmp(line, "MouseHotkey=", 12) == 0) {
        const char* keyStr = line + 12;
        while (isspace(*keyStr)) keyStr++;
        g_uMouseHotKey = ParseHotkeyFromString(keyStr);
        GetKeyNameByVK(g_uMouseHotKey, g_szMouseHotKeyName);
    } else if (strnicmp(line, "MouseClickType=", 15) == 0) {
        const char* typeStr = line + 15;
        while (isspace(*typeStr)) typeStr++;
        if (stricmp(typeStr, "left") == 0) g_mouseClickType = 0;
        else if (stricmp(typeStr, "right") == 0) g_mouseClickType = 1;
        else if (stricmp(typeStr, "middle") == 0) g_mouseClickType = 2;
    } else if (strnicmp(line, "MouseDelay=", 11) == 0) {
        const char* valStr = line + 11;
        while (isspace(*valStr)) valStr++;
        int val = atoi(valStr);
        if (val > 0) g_mouseDelay = val;
    }
}

// 生成默认配置文件（包含键盘和鼠标配置）
void SaveDefaultConfig() {
    FILE* f = fopen(g_szConfigPath, "w");
    if (!f) return;
    fprintf(f, "# ========== 自动按键器配置 ==========\n");
    fprintf(f, "# 键盘开关键：Hotkey=键名\n");
    fprintf(f, "# 支持的键名示例：F1-F12, A-Z, 0-9, Enter, Tab, Space, Esc, Backspace,\n");
    fprintf(f, "#                Ctrl, Alt, Shift, Win, Delete, Insert, Home, End,\n");
    fprintf(f, "#                PageUp, PageDown, PrintScreen, Pause, NumLock, ScrollLock,\n");
    fprintf(f, "#                小键盘数字键：Num0-Num9\n");
    fprintf(f, "Hotkey=F6\n\n");
    fprintf(f, "# 键盘按键序列格式：按键名 延时(ms)\n");
    fprintf(f, "# 支持单独按键或组合键（如 Ctrl+C, Alt+Tab, Win+R, Ctrl+Shift+Esc）\n");
    fprintf(f, "# ===================================\n");
    fprintf(f, "# 示例（可删除或修改）：\n");
    fprintf(f, "A 500\n");
    fprintf(f, "Num1 300\n");
    fprintf(f, "Ctrl+C 200\n");
    fprintf(f, "Delete 400\n");
    fprintf(f, "PrintScreen 100\n");
    fprintf(f, "# ===================================\n\n");
    SaveMouseConfig(f);
    fclose(f);
}

// 加载配置（键盘 + 鼠标）
void LoadConfig() {
    FreeKeyList();
    FILE* f = fopen(g_szConfigPath, "r");
    if (!f) {
        SaveDefaultConfig();
        f = fopen(g_szConfigPath, "r");
        if (!f) return;
    }

    UINT newHotKey = g_uHotKey;
    UINT newMouseHotKey = g_uMouseHotKey;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (len > 0 && line[len-1] == '\r') line[--len] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        // 键盘热键
        if (strnicmp(line, "Hotkey=", 7) == 0) {
            const char* keyStr = line + 7;
            while (isspace(*keyStr)) keyStr++;
            newHotKey = ParseHotkeyFromString(keyStr);
            continue;
        }
        // 鼠标配置
        if (strnicmp(line, "Mouse", 5) == 0) {
            LoadMouseConfigLine(line);
            continue;
        }
        // 键盘按键序列
        char key[64] = {0};
        int delay = 0;
        if (sscanf(line, "%63s %d", key, &delay) == 2 && delay > 0) {
            KeyItem* pNew = (KeyItem*)malloc(sizeof(KeyItem));
            if (pNew) {
                strncpy(pNew->szKey, key, 63);
                pNew->szKey[63] = '\0';
                pNew->dwDelay = delay;
                pNew->next = NULL;
                if (!g_pHead) g_pHead = g_pTail = pNew;
                else { g_pTail->next = pNew; g_pTail = pNew; }
            }
        }
    }
    fclose(f);

    // 更新键盘热键
    if (newHotKey != g_uHotKey) {
        UnregisterHotKeyCtrl();
        g_uHotKey = newHotKey;
        RegisterHotKeyCtrl();
    }
    GetKeyNameByVK(g_uHotKey, g_szHotKeyName);
    if (g_hMainWnd) {
        char disp[128];
        sprintf(disp, "当前键盘热键: %s", g_szHotKeyName);
        SetWindowText(GetDlgItem(g_hMainWnd, ID_STATIC_HOTKEY), disp);
    }

    // 更新鼠标热键（无条件重新注册，确保配置文件中的值生效）
    if (g_hMainWnd) {
        UnregisterHotKey(g_hMainWnd, 2);
        RegisterHotKey(g_hMainWnd, 2, 0, g_uMouseHotKey);
        GetKeyNameByVK(g_uMouseHotKey, g_szMouseHotKeyName);
        char disp[128];
        sprintf(disp, "当前鼠标热键: %s", g_szMouseHotKeyName);
        SetWindowText(g_hStaticMouseHotkey, disp);
    }

    RefreshListBox();
}

void EditConfigFile() { ShellExecute(NULL, "open", "notepad.exe", g_szConfigPath, NULL, SW_SHOWNORMAL); }
void RegisterHotKeyCtrl() { UnregisterHotKeyCtrl(); if (g_hMainWnd) RegisterHotKey(g_hMainWnd, 1, 0, g_uHotKey); }
void UnregisterHotKeyCtrl() { if (g_hMainWnd) UnregisterHotKey(g_hMainWnd, 1); }

void RefreshListBox() {
    SendMessage(g_hListBox, LB_RESETCONTENT, 0, 0);
    KeyItem* p = g_pHead;
    while (p) {
        char buf[128];
        sprintf(buf, "%s  [%d ms]", p->szKey, p->dwDelay);
        SendMessage(g_hListBox, LB_ADDSTRING, 0, (LPARAM)buf);
        p = p->next;
    }
}

void FreeKeyList() {
    KeyItem* p = g_pHead;
    while (p) {
        KeyItem* next = p->next;
        free(p);
        p = next;
    }
    g_pHead = g_pTail = NULL;
}

WORD GetScanCode(WORD vk) { return MapVirtualKey(vk, 0); }

void SendCombinedKey(const char* keySeq) {
    // 原有代码不变，省略...
    INPUT inputs[4] = {0};
    int nInputs = 0;
    BOOL bCtrl = FALSE, bAlt = FALSE, bShift = FALSE, bWin = FALSE;
    char mainKey[16] = {0};
    const char* p = keySeq;

    while (*p) {
        if (strncmp(p, "Ctrl+", 5) == 0) { bCtrl = TRUE; p += 5; }
        else if (strncmp(p, "Alt+", 4) == 0) { bAlt = TRUE; p += 4; }
        else if (strncmp(p, "Shift+", 6) == 0) { bShift = TRUE; p += 6; }
        else if (strncmp(p, "Win+", 4) == 0) { bWin = TRUE; p += 4; }
        else break;
    }
    strncpy(mainKey, p, sizeof(mainKey)-1);

    if (strlen(mainKey) == 0) {
        if (bCtrl) {
            INPUT ip[2] = {0};
            ip[0].type = INPUT_KEYBOARD; ip[0].ki.wVk = VK_CONTROL; ip[0].ki.wScan = GetScanCode(VK_CONTROL);
            ip[1].type = INPUT_KEYBOARD; ip[1].ki.wVk = VK_CONTROL; ip[1].ki.wScan = GetScanCode(VK_CONTROL); ip[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, ip, sizeof(INPUT));
        }
        if (bAlt) {
            INPUT ip[2] = {0};
            ip[0].type = INPUT_KEYBOARD; ip[0].ki.wVk = VK_MENU; ip[0].ki.wScan = GetScanCode(VK_MENU);
            ip[1].type = INPUT_KEYBOARD; ip[1].ki.wVk = VK_MENU; ip[1].ki.wScan = GetScanCode(VK_MENU); ip[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, ip, sizeof(INPUT));
        }
        if (bShift) {
            INPUT ip[2] = {0};
            ip[0].type = INPUT_KEYBOARD; ip[0].ki.wVk = VK_SHIFT; ip[0].ki.wScan = GetScanCode(VK_SHIFT);
            ip[1].type = INPUT_KEYBOARD; ip[1].ki.wVk = VK_SHIFT; ip[1].ki.wScan = GetScanCode(VK_SHIFT); ip[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, ip, sizeof(INPUT));
        }
        if (bWin) {
            INPUT ip[2] = {0};
            ip[0].type = INPUT_KEYBOARD; ip[0].ki.wVk = VK_LWIN; ip[0].ki.wScan = GetScanCode(VK_LWIN);
            ip[1].type = INPUT_KEYBOARD; ip[1].ki.wVk = VK_LWIN; ip[1].ki.wScan = GetScanCode(VK_LWIN); ip[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, ip, sizeof(INPUT));
        }
        return;
    }

    WORD vk = 0;
    if (strlen(mainKey) == 1 && isalpha(mainKey[0])) vk = VkKeyScanA(mainKey[0]) & 0xFF;
    else if (strlen(mainKey) == 1 && isdigit(mainKey[0])) vk = mainKey[0];
    else if (strcmp(mainKey, "Enter") == 0) vk = VK_RETURN;
    else if (strcmp(mainKey, "Tab") == 0) vk = VK_TAB;
    else if (strcmp(mainKey, "Space") == 0) vk = VK_SPACE;
    else if (strcmp(mainKey, "Esc") == 0) vk = VK_ESCAPE;
    else if (strcmp(mainKey, "Backspace") == 0) vk = VK_BACK;
    else if (strcmp(mainKey, "Ctrl") == 0) vk = VK_CONTROL;
    else if (strcmp(mainKey, "Alt") == 0) vk = VK_MENU;
    else if (strcmp(mainKey, "Shift") == 0) vk = VK_SHIFT;
    else if (strcmp(mainKey, "Win") == 0) vk = VK_LWIN;
    else if (strcmp(mainKey, "Num0") == 0) vk = VK_NUMPAD0;
    else if (strcmp(mainKey, "Num1") == 0) vk = VK_NUMPAD1;
    else if (strcmp(mainKey, "Num2") == 0) vk = VK_NUMPAD2;
    else if (strcmp(mainKey, "Num3") == 0) vk = VK_NUMPAD3;
    else if (strcmp(mainKey, "Num4") == 0) vk = VK_NUMPAD4;
    else if (strcmp(mainKey, "Num5") == 0) vk = VK_NUMPAD5;
    else if (strcmp(mainKey, "Num6") == 0) vk = VK_NUMPAD6;
    else if (strcmp(mainKey, "Num7") == 0) vk = VK_NUMPAD7;
    else if (strcmp(mainKey, "Num8") == 0) vk = VK_NUMPAD8;
    else if (strcmp(mainKey, "Num9") == 0) vk = VK_NUMPAD9;
    else if (strcmp(mainKey, "Delete") == 0) vk = VK_DELETE;
    else if (strcmp(mainKey, "Insert") == 0) vk = VK_INSERT;
    else if (strcmp(mainKey, "Home") == 0) vk = VK_HOME;
    else if (strcmp(mainKey, "End") == 0) vk = VK_END;
    else if (strcmp(mainKey, "PageUp") == 0) vk = VK_PRIOR;
    else if (strcmp(mainKey, "PageDown") == 0) vk = VK_NEXT;
    else if (strcmp(mainKey, "PrintScreen") == 0) vk = VK_SNAPSHOT;
    else if (strcmp(mainKey, "Pause") == 0) vk = VK_PAUSE;
    else if (strcmp(mainKey, "NumLock") == 0) vk = VK_NUMLOCK;
    else if (strcmp(mainKey, "ScrollLock") == 0) vk = VK_SCROLL;
    else if (strncmp(mainKey, "F", 1) == 0) {
        int num = atoi(mainKey + 1);
        if (num >= 1 && num <= 12) vk = VK_F1 + (num - 1);
    }
    if (vk == 0) return;

    WORD scan = GetScanCode(vk);
    if (bCtrl) { inputs[nInputs].type = INPUT_KEYBOARD; inputs[nInputs].ki.wVk = VK_CONTROL; inputs[nInputs].ki.wScan = GetScanCode(VK_CONTROL); nInputs++; }
    if (bAlt)  { inputs[nInputs].type = INPUT_KEYBOARD; inputs[nInputs].ki.wVk = VK_MENU; inputs[nInputs].ki.wScan = GetScanCode(VK_MENU); nInputs++; }
    if (bShift){ inputs[nInputs].type = INPUT_KEYBOARD; inputs[nInputs].ki.wVk = VK_SHIFT; inputs[nInputs].ki.wScan = GetScanCode(VK_SHIFT); nInputs++; }
    if (bWin)  { inputs[nInputs].type = INPUT_KEYBOARD; inputs[nInputs].ki.wVk = VK_LWIN; inputs[nInputs].ki.wScan = GetScanCode(VK_LWIN); nInputs++; }

    inputs[nInputs].type = INPUT_KEYBOARD;
    inputs[nInputs].ki.wScan = scan;
    inputs[nInputs].ki.dwFlags = KEYEVENTF_SCANCODE;
    nInputs++;

    inputs[nInputs].type = INPUT_KEYBOARD;
    inputs[nInputs].ki.wScan = scan;
    inputs[nInputs].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    nInputs++;

    if (bWin)  { inputs[nInputs].type = INPUT_KEYBOARD; inputs[nInputs].ki.wVk = VK_LWIN; inputs[nInputs].ki.wScan = GetScanCode(VK_LWIN); inputs[nInputs].ki.dwFlags = KEYEVENTF_KEYUP; nInputs++; }
    if (bShift){ inputs[nInputs].type = INPUT_KEYBOARD; inputs[nInputs].ki.wVk = VK_SHIFT; inputs[nInputs].ki.wScan = GetScanCode(VK_SHIFT); inputs[nInputs].ki.dwFlags = KEYEVENTF_KEYUP; nInputs++; }
    if (bAlt)  { inputs[nInputs].type = INPUT_KEYBOARD; inputs[nInputs].ki.wVk = VK_MENU; inputs[nInputs].ki.wScan = GetScanCode(VK_MENU); inputs[nInputs].ki.dwFlags = KEYEVENTF_KEYUP; nInputs++; }
    if (bCtrl) { inputs[nInputs].type = INPUT_KEYBOARD; inputs[nInputs].ki.wVk = VK_CONTROL; inputs[nInputs].ki.wScan = GetScanCode(VK_CONTROL); inputs[nInputs].ki.dwFlags = KEYEVENTF_KEYUP; nInputs++; }

    SendInput(nInputs, inputs, sizeof(INPUT));
}

DWORD WINAPI ThreadProc(LPVOID lpParam) {
    while (g_bRunning) {
        KeyItem* p = g_pHead;
        while (p && g_bRunning) {
            SendCombinedKey(p->szKey);
            Sleep(p->dwDelay);
            p = p->next;
        }
        if (!g_bLoopForever) break;
    }
    g_bRunning = FALSE;
    g_hThread = NULL;
    EnableWindow(GetDlgItem(g_hMainWnd, ID_BTN_START), TRUE);
    EnableWindow(GetDlgItem(g_hMainWnd, ID_BTN_STOP), FALSE);
    return 0;
}

void OnStart() {
    if (g_bRunning || !g_pHead) return;
    g_bLoopForever = (IsDlgButtonChecked(g_hMainWnd, ID_CHK_LOOP) == BST_CHECKED);
    g_bRunning = TRUE;
    EnableWindow(GetDlgItem(g_hMainWnd, ID_BTN_START), FALSE);
    EnableWindow(GetDlgItem(g_hMainWnd, ID_BTN_STOP), TRUE);
    g_hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, NULL);
}

void OnStop() {
    if (!g_bRunning) return;
    g_bRunning = FALSE;
    if (g_hThread) {
        WaitForSingleObject(g_hThread, 1000);
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }
    EnableWindow(GetDlgItem(g_hMainWnd, ID_BTN_START), TRUE);
    EnableWindow(GetDlgItem(g_hMainWnd, ID_BTN_STOP), FALSE);
}

// 鼠标连点功能
void SendMouseClick(int type) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    switch (type) {
        case 0: input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP; break;
        case 1: input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP; break;
        case 2: input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP; break;
        default: return;
    }
    SendInput(1, &input, sizeof(INPUT));
}

DWORD WINAPI MouseThreadProc(LPVOID lpParam) {
    while (g_bMouseRunning) {
        SendMouseClick(g_mouseClickType);
        Sleep(g_mouseDelay);
    }
    return 0;
}

void OnMouseStart() {
    if (g_bMouseRunning) return;
    g_bMouseRunning = TRUE;
    EnableWindow(g_hBtnMouseStart, FALSE);
    EnableWindow(g_hBtnMouseStop, TRUE);
    g_hMouseThread = CreateThread(NULL, 0, MouseThreadProc, NULL, 0, NULL);
}

void OnMouseStop() {
    if (!g_bMouseRunning) return;
    g_bMouseRunning = FALSE;
    if (g_hMouseThread) {
        WaitForSingleObject(g_hMouseThread, 1000);
        CloseHandle(g_hMouseThread);
        g_hMouseThread = NULL;
    }
    EnableWindow(g_hBtnMouseStart, TRUE);
    EnableWindow(g_hBtnMouseStop, FALSE);
}

// 主窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hMainWnd = hwnd;

            // ========== 键盘区域 ==========
            g_hListBox = CreateWindow("LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                       10, 10, 250, 200, hwnd, (HMENU)ID_LIST_BOX, GetModuleHandle(NULL), NULL);
            CreateWindow("BUTTON", "编辑配置", WS_CHILD | WS_VISIBLE, 270, 10, 90, 30, hwnd, (HMENU)ID_BTN_EDIT_CFG, NULL, NULL);
            CreateWindow("BUTTON", "重新加载", WS_CHILD | WS_VISIBLE, 270, 50, 90, 30, hwnd, (HMENU)ID_BTN_RELOAD, NULL, NULL);
            CreateWindow("BUTTON", "开始键盘", WS_CHILD | WS_VISIBLE, 10, 220, 80, 30, hwnd, (HMENU)ID_BTN_START, NULL, NULL);
            CreateWindow("BUTTON", "停止键盘", WS_CHILD | WS_VISIBLE, 100, 220, 80, 30, hwnd, (HMENU)ID_BTN_STOP, NULL, NULL);
            CreateWindow("BUTTON", "循环执行", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 200, 220, 80, 30, hwnd, (HMENU)ID_CHK_LOOP, NULL, NULL);
            CreateWindow("STATIC", "使用说明：点击[编辑配置]修改按键序列和开关键，保存后点[重新加载]。按开关键开始/停止。",
                         WS_CHILD | WS_VISIBLE | SS_LEFT, 10, 270, 560, 50, hwnd, (HMENU)ID_STATIC_HINT, NULL, NULL);
            char hotkeyDisplay[128];
            GetKeyNameByVK(g_uHotKey, g_szHotKeyName);
            sprintf(hotkeyDisplay, "当前键盘热键: %s", g_szHotKeyName);
            CreateWindow("STATIC", hotkeyDisplay, WS_CHILD | WS_VISIBLE | SS_LEFT,
                         10, 320, 200, 20, hwnd, (HMENU)ID_STATIC_HOTKEY, NULL, NULL);

            // ========== 鼠标区域（右侧） ==========
            // 分组框
            CreateWindow("BUTTON", "鼠标连点", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                         370, 10, 220, 170, hwnd, NULL, GetModuleHandle(NULL), NULL);

            // 点击类型
            CreateWindow("STATIC", "点击类型:", WS_CHILD | WS_VISIBLE | SS_LEFT, 385, 35, 80, 20, hwnd, NULL, NULL, NULL);
            g_hComboMouseType = CreateWindow("COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                                             470, 33, 100, 100, hwnd, (HMENU)ID_COMBO_MOUSE_TYPE, GetModuleHandle(NULL), NULL);
            SendMessage(g_hComboMouseType, CB_ADDSTRING, 0, (LPARAM)"左键");
            SendMessage(g_hComboMouseType, CB_ADDSTRING, 0, (LPARAM)"右键");
            SendMessage(g_hComboMouseType, CB_ADDSTRING, 0, (LPARAM)"中键");
            SendMessage(g_hComboMouseType, CB_SETCURSEL, g_mouseClickType, 0);

            // 速度模式
            CreateWindow("STATIC", "速度模式:", WS_CHILD | WS_VISIBLE | SS_LEFT, 385, 65, 80, 20, hwnd, NULL, NULL, NULL);
            g_hComboMouseSpeed = CreateWindow("COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                                              470, 63, 100, 100, hwnd, (HMENU)ID_COMBO_MOUSE_SPEED, GetModuleHandle(NULL), NULL);
            SendMessage(g_hComboMouseSpeed, CB_ADDSTRING, 0, (LPARAM)"极速(10ms)");
            SendMessage(g_hComboMouseSpeed, CB_ADDSTRING, 0, (LPARAM)"高效(100ms)");
            SendMessage(g_hComboMouseSpeed, CB_ADDSTRING, 0, (LPARAM)"普通(500ms)");
            SendMessage(g_hComboMouseSpeed, CB_ADDSTRING, 0, (LPARAM)"自定义");
            SendMessage(g_hComboMouseSpeed, CB_SETCURSEL, 1, 0); // 默认高效

            // 手动输入框
            CreateWindow("STATIC", "手动(ms):", WS_CHILD | WS_VISIBLE | SS_LEFT, 385, 95, 80, 20, hwnd, NULL, NULL, NULL);
            g_hEditMouseDelay = CreateWindow("EDIT", "100", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                             470, 93, 80, 22, hwnd, (HMENU)ID_EDIT_MOUSE_DELAY, NULL, NULL);
            EnableWindow(g_hEditMouseDelay, FALSE); // 初始禁用

            // 鼠标按钮
            g_hBtnMouseStart = CreateWindow("BUTTON", "开始鼠标", WS_CHILD | WS_VISIBLE,
                                            385, 125, 80, 25, hwnd, (HMENU)ID_BTN_MOUSE_START, NULL, NULL);
            g_hBtnMouseStop = CreateWindow("BUTTON", "停止鼠标", WS_CHILD | WS_VISIBLE,
                                           475, 125, 80, 25, hwnd, (HMENU)ID_BTN_MOUSE_STOP, NULL, NULL);
            EnableWindow(g_hBtnMouseStop, FALSE);

            // 鼠标热键显示（合并为一行）
            char mouseHotkeyDisplay[128];
            GetKeyNameByVK(g_uMouseHotKey, g_szMouseHotKeyName);
            sprintf(mouseHotkeyDisplay, "当前鼠标热键: %s", g_szMouseHotKeyName);
            g_hStaticMouseHotkey = CreateWindow("STATIC", mouseHotkeyDisplay, WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    385, 155, 200, 20, hwnd, (HMENU)ID_STATIC_MOUSE_HOTKEY, NULL, NULL);

            EnableWindow(GetDlgItem(hwnd, ID_BTN_STOP), FALSE);
            GetConfigPath();
            LoadConfig();

            // 注册键盘热键（ID 1）
            RegisterHotKeyCtrl();

            if (!IsAdmin()) {
                MessageBox(hwnd, "建议以管理员权限运行此程序，否则某些游戏可能无法接收模拟按键。\n\n"
                                 "可以右键点击 exe -> 以管理员身份运行。", "提示", MB_OK | MB_ICONINFORMATION);
            }
            break;
        }
        case WM_HOTKEY: {
            if (wParam == 1) {
                if (g_bRunning) OnStop(); else OnStart();
            } else if (wParam == 2) {
                if (g_bMouseRunning) OnMouseStop(); else OnMouseStart();
            }
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_BTN_EDIT_CFG: EditConfigFile(); break;
                case ID_BTN_RELOAD:   LoadConfig(); break;
                case ID_BTN_START:    OnStart(); break;
                case ID_BTN_STOP:     OnStop(); break;
                case ID_BTN_MOUSE_START: OnMouseStart(); break;
                case ID_BTN_MOUSE_STOP:  OnMouseStop(); break;
                case ID_COMBO_MOUSE_TYPE:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        g_mouseClickType = SendMessage(g_hComboMouseType, CB_GETCURSEL, 0, 0);
                    }
                    break;
                case ID_COMBO_MOUSE_SPEED:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        int sel = SendMessage(g_hComboMouseSpeed, CB_GETCURSEL, 0, 0);
                        if (sel == 0) g_mouseDelay = 10;
                        else if (sel == 1) g_mouseDelay = 100;
                        else if (sel == 2) g_mouseDelay = 500;
                        else if (sel == 3) {
                            EnableWindow(g_hEditMouseDelay, TRUE);
                            char buf[16];
                            GetWindowText(g_hEditMouseDelay, buf, sizeof(buf));
                            int val = atoi(buf);
                            if (val > 0) g_mouseDelay = val;
                        } else {
                            EnableWindow(g_hEditMouseDelay, FALSE);
                        }
                    }
                    break;
                case ID_EDIT_MOUSE_DELAY:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        char buf[16];
                        GetWindowText(g_hEditMouseDelay, buf, sizeof(buf));
                        int val = atoi(buf);
                        if (val > 0) g_mouseDelay = val;
                        // 自动将速度模式设为“自定义”
                        SendMessage(g_hComboMouseSpeed, CB_SETCURSEL, 3, 0);
                        EnableWindow(g_hEditMouseDelay, TRUE);
                    }
                    break;
            }
            break;
        case WM_DESTROY:
            OnStop();
            OnMouseStop();
            UnregisterHotKeyCtrl();
            UnregisterHotKey(hwnd, 2);
            FreeKeyList();
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = "AutoClickerClass";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("AutoClickerClass", "自动按键器 - 键盘+鼠标连点",
                             WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                             CW_USEDEFAULT, CW_USEDEFAULT, 600, 420,
                             NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}