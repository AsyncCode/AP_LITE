﻿#include "common.h"  
#include "AP.h"
#include <windowsx.h>

#include <wlanapi.h>
#pragma comment(lib, "Wlanapi.lib")

static INT_PTR CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);
static void DialogInit(void);
static void Switch(void);
static void Config(void);
static void RefreshInfo(void);
static void ShowMenu(void);
static bool QuitWithCheck(void);

static HWND gDialog;
static HINSTANCE gInstance;
static NOTIFYICONDATA gNid;

static AP *gpAP = NULL;

int WINAPI AP_WinMain(HINSTANCE instance)
{
    gInstance = instance;
    gpAP = new AP();

    HWND gDialog = CreateDialog(gInstance,
        MAKEINTRESOURCE(IDD_AP), GetDesktopWindow(), (DLGPROC)DlgProc);
    if (!gDialog) {
        return 0;
    }
    ShowWindow(gDialog, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

static INT_PTR CALLBACK DlgProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam)
{
    gDialog = dialog;
    CStringW str;
    CStringW strContent;

    switch (msg) {
    case WM_INITDIALOG:
        DialogInit();
        break;

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE) {
            QuitWithCheck();
        }
        break;

    case WM_TIMER:
        RefreshInfo();
        break;

    case WM_SHOWTASK:
        if (LOWORD(lParam) == WM_RBUTTONDOWN) {
            ShowMenu();
        } else if (LOWORD(lParam) == WM_LBUTTONDOWN) {
            ShowWindow(dialog, SW_SHOW);
            Shell_NotifyIcon(NIM_DELETE, &gNid);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SWITCH:
        case ID_MENU_SWITCH:
            Switch();
            break;

        case IDC_CONFIG:
            Config();
            break;

        case IDC_TRAY:
            Shell_NotifyIcon(NIM_ADD, &gNid);
            ShowWindow(dialog, SW_HIDE);
            break;

        case ID_MENU_ABOUT:
            str.LoadStringW(IDS_ABOUT_TITLE);
            strContent.LoadStringW(IDS_ABOUT_CONTENT);
            MessageBox(gDialog, strContent, str, MB_OK);
            break;

        case ID_MENU_SHOW:
            ShowWindow(dialog, SW_SHOW);
            Shell_NotifyIcon(NIM_DELETE, &gNid);
            break;

        case ID_MENU_QUIT:
            QuitWithCheck();
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }
    return 0;
}

static void DialogInit(void)
{
    CStringW str;

    str.LoadStringW(IDS_AP_DIALOG_TITLT);
    gNid.cbSize = sizeof(NOTIFYICONDATA);
    gNid.hWnd = gDialog;
    gNid.uID = IDI_ICON;
    gNid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    gNid.uCallbackMessage = WM_SHOWTASK;
    gNid.hIcon = LoadIcon(gInstance, MAKEINTRESOURCE(IDI_ICON));
    StrCpy(gNid.szTip, str);

    str.LoadStringW(IDS_AP_DIALOG_TITLT);
    SendMessage(gDialog, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(GetModuleHandleW(NULL),
        MAKEINTRESOURCE(IDI_ICON)));
    SetWindowText(gDialog, str);

    str.LoadStringW(IDS_SSID_TITLE);
    SetWindowText(GetDlgItem(gDialog, IDC_SSID_TITLE), str);
    str.LoadStringW(IDS_KEY_TITLE);
    SetWindowText(GetDlgItem(gDialog, IDC_KEY_TITLE), str);

    SetWindowText(GetDlgItem(gDialog, IDC_SSID), gConfigData.ssid);
    SetWindowText(GetDlgItem(gDialog, IDC_KEY), gConfigData.key);

    str.LoadStringW(IDS_CONFIG);
    SetWindowText(GetDlgItem(gDialog, IDC_CONFIG), str);
    str.LoadStringW(IDS_TRAY);
    SetWindowText(GetDlgItem(gDialog, IDC_TRAY), str);

    str.LoadStringW(IDS_SHARED_TITLE);
    SetWindowText(GetDlgItem(gDialog, IDC_SHARED_TITLE), str);

    HWND combo = GetDlgItem(gDialog, IDC_SHARED);
    const vector<Connection> *pConnections = gpAP->getOtherConnections();
    for (vector<LPWSTR>::size_type i = 0; i < pConnections->size(); ++i) {
        if ((*pConnections)[i].pNP->Status == NCS_CONNECTED) {
            ComboBox_AddString(combo, (*pConnections)[i].pNP->pszwName);
        }
    }
    str.LoadStringW(IDS_NONE);
    ComboBox_AddString(combo, str);
    ComboBox_SetCurSel(combo, 0);

    RefreshInfo();
    SetTimer(gDialog, 1, 5000, NULL);
}

static void Switch(void)
{
    CStringW str;
    if (gpAP->getStatus() == AP::STATUS_OFF) {
        str.LoadStringW(IDS_IS_TURNING_ON);
        SetWindowText(GetDlgItem(gDialog, IDC_INFO), str);
    }

    HWND combo = GetDlgItem(gDialog, IDC_SHARED);
    const vector<Connection> *pConnections = gpAP->getOtherConnections();
    int len = ComboBox_GetTextLength(combo) + 1;
    int which = pConnections->size();
    LPWSTR buffer = new WCHAR[len];
    ComboBox_GetText(combo, buffer, len);
    for (vector<LPWSTR>::size_type i = 0; i < pConnections->size(); ++i) {
        if (!StrCmp((*pConnections)[i].pNP->pszwName, buffer)) {
            which = i;
        }
    }
    delete[] buffer;
    gpAP->switchStatus(which);
    RefreshInfo();
}

static void Config(void)
{
    if (QuitWithCheck()) {
        WCHAR selfFilename[PATH_SIZE];
        WCHAR tempPath[PATH_SIZE];
        WCHAR cmd[CMD_SIZE];
        GetModuleFileName(NULL, selfFilename, PATH_SIZE);
        GetTempPath(PATH_SIZE, tempPath);
        GetTempFileName(tempPath, L"AP_LITE", 0, cmd);
        PathRenameExtension(cmd, L".exe");

        if (CopyFile(selfFilename, cmd, false)) {
            StrCat(cmd, L" " CONFIG_OPTION " ");
            StrCat(cmd, selfFilename);

            STARTUPINFO si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi;
            ZeroMemory(&pi, sizeof(pi));

            if (!CreateProcess(NULL, cmd,
                NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                return;
            }

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
}

static void RefreshInfo(void)
{
    CStringW str;
    int mStatus = gpAP->getStatus();

    if (mStatus != AP::STATUS_ERROR) {
        str.LoadStringW(mStatus == AP::STATUS_OFF ? IDS_SWITCH_ON : IDS_SWITCH_OFF);
        SetWindowText(GetDlgItem(gDialog, IDC_SWITCH), str);
    } else {
        str.LoadStringW(IDS_ERROR);
        SetWindowText(GetDlgItem(gDialog, IDC_SWITCH), str);
        EnableWindow(GetDlgItem(gDialog, IDC_SWITCH), false);
    }

    if (mStatus == AP::STATUS_ON) {
        str.LoadStringW(IDS_USERINFO_PREFIX);
        str.AppendFormat(L"%d", gpAP->getPeerNumber());
        SetWindowText(GetDlgItem(gDialog, IDC_INFO), str);
    } else {
        str.LoadStringW(IDS_IS_NOT_ON);
        SetWindowText(GetDlgItem(gDialog, IDC_INFO), str);
    }

    if (mStatus == AP::STATUS_OFF) {
        EnableWindow(GetDlgItem(gDialog, IDC_SHARED), true);
    } else {
        EnableWindow(GetDlgItem(gDialog, IDC_SHARED), false);
    }
}

static void ShowMenu(void)
{
    UINT uFlag = MF_BYPOSITION | MF_STRING;
    POINT clickPoint;
    GetCursorPos(&clickPoint);
    HMENU menu = CreatePopupMenu();
    CStringW str;

    str.LoadStringW(IDS_ABOUT);
    InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_MENU_ABOUT, str);

    if (gpAP->getStatus() != AP::STATUS_ERROR) {
        str.LoadStringW(gpAP->getStatus() == AP::STATUS_OFF ? IDS_SWITCH_ON : IDS_SWITCH_OFF);
        InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_MENU_SWITCH, str);
    } else {
        str.LoadStringW(IDS_ERROR);
        InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_GRAYED, ID_MENU_SWITCH, str);
    }

    str.LoadStringW(IDS_SHOW);
    InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_MENU_SHOW, str);

    str.LoadStringW(IDS_QUIT);
    InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_MENU_QUIT, str);

    SetForegroundWindow(gDialog);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN,
        clickPoint.x, clickPoint.y, 0, gDialog, NULL);
}

static bool QuitWithCheck(void)
{
    CStringW str;
    str.LoadStringW(IDS_QUIT_WARNNING);

    if (gpAP->getStatus() == AP::STATUS_ON && gConfigData.askBeforeQuit) {
        if (MessageBox(gDialog, str, L"", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
            PostQuitMessage(0);
            delete gpAP;
            return true;
        } else {
            return false;
        }
    } else {
        PostQuitMessage(0);
        return true;
    }
}