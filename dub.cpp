#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <atomic>
#include <netfw.h>
#include <comdef.h>
#include <shellapi.h>
#include <gdiplus.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// ---- Constants & State ----
std::atomic<bool> g_Lagging(false);
std::atomic<bool> g_AutoDupe(true);
int g_LagDuration = 6000;
int g_LobbyWait = 9000;

ULONG_PTR g_gdiplusToken;
HFONT g_hFontNormal = NULL;
HFONT g_hFontTitle = NULL;
HFONT g_hFontStatus = NULL;
HFONT g_hFontSmall = NULL;
HBRUSH g_hBackBrush = NULL;
HBRUSH g_hHeaderBrush = NULL;
HBRUSH g_hAccentBrush = NULL;
HBRUSH g_hCardBrush = NULL;

const int WIN_W = 380;
const int WIN_H = 420;
const int HEADER_H = 42;

const int BaseW = 1600;
const int BaseH = 900;
const int BaseExitX = 1344;
const int BaseExitY = 626;
const int BaseCancelX = 973;
const int BaseCancelY = 507;
const int BaseExitNowX = 586;
const int BaseExitNowY = 504;
const int BasePlayX = 1297;
const int BasePlayY = 751;

int g_ExitX, g_ExitY, g_CancelX, g_CancelY, g_ExitNowX, g_ExitNowY, g_PlayX, g_PlayY;

const int MenuOpenDelay = 280;
const int ClickHoldDown = 40;
const int PostClickDelay = 180;
const int FourthExitLead = 700;
const int ExitNowLead = 150;

HWND g_hwndStatus = NULL;
HWND g_hwndLagMs = NULL;
HWND g_hwndLobbyMs = NULL;
bool g_closeHovered = false;

// Custom Slider State
struct CustomSlider {
    int minVal;
    int maxVal;
    int* pVal;
    bool dragging;
    HWND labelHwnd;
};

struct CustomCheckbox {
    std::atomic<bool>* pVal;
    bool hovered;
};

CustomSlider g_LagSliderData = { 500, 12000, &g_LagDuration, false, NULL };
CustomSlider g_LobbySliderData = { 3000, 20000, &g_LobbyWait, false, NULL };
CustomCheckbox g_AutoDupeData = { &g_AutoDupe, false };

// ---- Custom Control Helpers ----
void DrawCustomCheckbox(HDC hdc, RECT rc, bool active, bool hovered) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    float w = (float)(rc.right - rc.left);
    float h = (float)(rc.bottom - rc.top);
    float switchW = 34.0f;
    float switchH = 18.0f;
    float thumbR = 7.0f;
    float centerY = h / 2.0f;

    // Track
    GraphicsPath path;
    path.AddArc((REAL)0.0f, (REAL)(centerY - (switchH / 2.0f)), (REAL)switchH, (REAL)switchH, 90.0f, 180.0f);
    path.AddArc((REAL)(switchW - switchH), (REAL)(centerY - (switchH / 2.0f)), (REAL)switchH, (REAL)switchH, 270.0f, 180.0f);
    path.CloseFigure();

    Color trackColor;
    if (active) {
        trackColor = hovered ? Color(255, 255, 80, 80) : Color(255, 235, 60, 60);
    } else {
        trackColor = hovered ? Color(255, 80, 80, 80) : Color(255, 60, 60, 60);
    }
    
    SolidBrush trackBrush(trackColor);
    g.FillPath(&trackBrush, &path);

    // Thumb
    float thumbX = active ? (switchW - (switchH / 2.0f)) : (switchH / 2.0f);
    SolidBrush thumbBrush(Color(255, 255, 255, 255));
    g.FillEllipse(&thumbBrush, (REAL)(thumbX - thumbR), (REAL)(centerY - thumbR), (REAL)(thumbR * 2.0f), (REAL)(thumbR * 2.0f));

    // Label Text
    std::wstring text = L"AUTO-REJOIN SEQUENCE";
    FontFamily ff(L"Segoe UI");
    Gdiplus::Font gdiFont(&ff, 9, FontStyleRegular, UnitPoint);
    SolidBrush textBrush(hovered ? Color(255, 230, 230, 230) : Color(255, 192, 192, 192));
    
    StringFormat format;
    format.SetAlignment(StringAlignmentNear);
    format.SetLineAlignment(StringAlignmentCenter);
    
    RectF layoutRect(switchW + 12.0f, 0, w - switchW - 12.0f, h);
    g.DrawString(text.c_str(), -1, &gdiFont, layoutRect, &format, &textBrush);
}

LRESULT CALLBACK CheckboxSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData) {
    CustomCheckbox* cc = (CustomCheckbox*)dwRefData;
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        SelectObject(memDC, memBmp);

        HBRUSH br = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(memDC, &rc, br);
        DeleteObject(br);

        DrawCustomCheckbox(memDC, rc, cc->pVal->load(), cc->hovered);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        cc->pVal->store(!cc->pVal->load());
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!cc->hovered) {
            cc->hovered = true;
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        cc->hovered = false;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ---- Firewall Logic ----
void SetRuleEnabled(bool enable) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    INetFwPolicy2* pNetFwPolicy2 = NULL;
    
    hr = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2), (void**)&pNetFwPolicy2);
    if (SUCCEEDED(hr)) {
        INetFwRules* pRules = NULL;
        hr = pNetFwPolicy2->get_Rules(&pRules);
        if (SUCCEEDED(hr)) {
            IUnknown* pEnumerator = NULL;
            hr = pRules->get__NewEnum(&pEnumerator);
            if (SUCCEEDED(hr)) {
                IEnumVARIANT* pVariantEnum = NULL;
                hr = pEnumerator->QueryInterface(__uuidof(IEnumVARIANT), (void**)&pVariantEnum);
                if (SUCCEEDED(hr)) {
                    VARIANT var;
                    VariantInit(&var);
                    while (pVariantEnum->Next(1, &var, NULL) == S_OK) {
                        if (var.vt == VT_DISPATCH) {
                            INetFwRule* pRule = NULL;
                            hr = var.pdispVal->QueryInterface(__uuidof(INetFwRule), (void**)&pRule);
                            if (SUCCEEDED(hr)) {
                                BSTR bstrName = NULL;
                                pRule->get_Name(&bstrName);
                                if (bstrName && wcscmp(bstrName, L"Rule") == 0) {
                                    pRule->put_Enabled(enable ? VARIANT_TRUE : VARIANT_FALSE);
                                }
                                SysFreeString(bstrName);
                                pRule->Release();
                            }
                        }
                        VariantClear(&var);
                    }
                    pVariantEnum->Release();
                }
                pEnumerator->Release();
            }
            pRules->Release();
        }
        pNetFwPolicy2->Release();
    }
    CoUninitialize();

    std::string cmd = "netsh advfirewall firewall set rule name=\"Rule\" new enable=";
    cmd += (enable ? "yes" : "no");
    WinExec(cmd.c_str(), SW_HIDE);
}

// ---- Input Helpers ----
void SendKeyVK(BYTE vk, BYTE sc) {
    keybd_event(0, sc, KEYEVENTF_SCANCODE, 0);
    Sleep(ClickHoldDown);
    keybd_event(0, sc, KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP, 0);
}

void SendEsc() {
    SendKeyVK(VK_ESCAPE, 0x01);
}

void SendClick(int x, int y) {
    SetCursorPos(x, y);
    Sleep(35);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(ClickHoldDown);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

void ApplyResolution(int w, int h) {
    float sx = (float)w / BaseW;
    float sy = (float)h / BaseH;
    g_ExitX = (int)(BaseExitX * sx);
    g_ExitY = (int)(BaseExitY * sy);
    g_CancelX = (int)(BaseCancelX * sx);
    g_CancelY = (int)(BaseCancelY * sy);
    g_ExitNowX = (int)(BaseExitNowX * sx);
    g_ExitNowY = (int)(BaseExitNowY * sy);
    g_PlayX = (int)(BasePlayX * sx);
    g_PlayY = (int)(BasePlayY * sy);
}

// ---- Main Logic ----
void DupeSequence() {
    ULONGLONG startTick = GetTickCount64();
    SetRuleEnabled(true);
    Beep(523, 130);
    InvalidateRect(g_hwndStatus, NULL, TRUE);
    SetWindowTextA(g_hwndStatus, "LAGGING");

    if (g_AutoDupe) {
        Sleep(50);
        for (int i = 0; i < 3; ++i) {
            if (!g_Lagging) break;
            SendEsc();
            Sleep(MenuOpenDelay);
            SendClick(g_ExitX, g_ExitY);
            Sleep(PostClickDelay);
            SendClick(g_CancelX, g_CancelY);
            Sleep(PostClickDelay);
        }

        ULONGLONG target = startTick + g_LagDuration - FourthExitLead;
        while (g_Lagging && GetTickCount64() < target) {
            Sleep(20);
        }
        if (g_Lagging) {
            SendEsc();
            Sleep(MenuOpenDelay);
            SendClick(g_ExitX, g_ExitY);
        }

        target = startTick + g_LagDuration - ExitNowLead;
        while (g_Lagging && GetTickCount64() < target) {
            Sleep(20);
        }
        if (g_Lagging) {
            SendClick(g_ExitNowX, g_ExitNowY);
        }
    }

    ULONGLONG elapsed = GetTickCount64() - startTick;
    if (elapsed < (ULONGLONG)g_LagDuration) {
        Sleep((DWORD)(g_LagDuration - elapsed));
    }

    g_Lagging = false;
    SetRuleEnabled(false);
    Beep(223, 120);

    if (g_AutoDupe) {
        SetWindowTextA(g_hwndStatus, "IN LOBBY");
        InvalidateRect(g_hwndStatus, NULL, TRUE);
        Sleep(g_LobbyWait);
        Beep(700, 100);
        SendClick(g_PlayX, g_PlayY);
        SetWindowTextA(g_hwndStatus, "REJOINING");
        InvalidateRect(g_hwndStatus, NULL, TRUE);
        Sleep(3000);
    }
    SetWindowTextA(g_hwndStatus, "READY");
    InvalidateRect(g_hwndStatus, NULL, TRUE);
}

void StartLag() {
    if (g_Lagging) {
        g_Lagging = false;
        return;
    }
    g_Lagging = true;
    std::thread(DupeSequence).detach();
}

// ---- Custom Slider Helpers ----
void DrawCustomSlider(HDC hdc, RECT rc, int val, int minV, int maxV, bool dragging) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    float w = (float)(rc.right - rc.left);
    float h = (float)(rc.bottom - rc.top);
    float trackH = 4.0f;
    float thumbR = 7.0f;
    float padding = 10.0f;

    float usableW = w - (padding * 2);
    float ratio = (float)(val - minV) / (maxV - minV);
    float thumbX = padding + (usableW * ratio);
    float centerY = h / 2.0f;

    // Track Background
    SolidBrush trackBg(Color(255, 60, 60, 60));
    g.FillRectangle(&trackBg, padding, centerY - (trackH / 2.0f), usableW, trackH);

    // Track Fill (Accent)
    SolidBrush accent(Color(255, 235, 60, 60));
    g.FillRectangle(&accent, padding, centerY - (trackH / 2.0f), thumbX - padding, trackH);

    // Thumb
    SolidBrush thumbBrush(dragging ? Color(255, 255, 80, 80) : Color(255, 235, 60, 60));
    g.FillEllipse(&thumbBrush, thumbX - thumbR, centerY - thumbR, thumbR * 2, thumbR * 2);
    
    // Thumb Outline
    Pen thumbPen(Color(255, 40, 40, 40), 1.0f);
    g.DrawEllipse(&thumbPen, thumbX - thumbR, centerY - thumbR, thumbR * 2, thumbR * 2);
}

LRESULT CALLBACK SliderSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData) {
    CustomSlider* cs = (CustomSlider*)dwRefData;
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        SelectObject(memDC, memBmp);

        HBRUSH br = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(memDC, &rc, br);
        DeleteObject(br);

        DrawCustomSlider(memDC, rc, *cs->pVal, cs->minVal, cs->maxVal, cs->dragging);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        cs->dragging = true;
        SetCapture(hwnd);
        // Fallthrough
    }
    case WM_MOUSEMOVE: {
        if (cs->dragging || (msg == WM_LBUTTONDOWN)) {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rc; GetClientRect(hwnd, &rc);
            float padding = 10.0f;
            float usableW = (float)(rc.right - rc.left) - (padding * 2);
            float x = (float)pt.x - padding;
            if (x < 0) x = 0;
            if (x > usableW) x = usableW;
            
            float ratio = x / usableW;
            int rawVal = cs->minVal + (int)(ratio * (cs->maxVal - cs->minVal));
            *cs->pVal = ((rawVal + 5) / 10) * 10; // Snap to nearest 10
            
            std::string s = std::to_string(*cs->pVal) + " ms";
            SetWindowTextA(cs->labelHwnd, s.c_str());
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (cs->dragging) {
            cs->dragging = false;
            ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ---- GUI Proc ----
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        g_hFontNormal = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        g_hFontTitle = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Tahoma");
        g_hFontStatus = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        g_hFontSmall = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        
        g_hBackBrush = CreateSolidBrush(RGB(26, 26, 26));
        g_hHeaderBrush = CreateSolidBrush(RGB(17, 17, 17));
        g_hAccentBrush = CreateSolidBrush(RGB(235, 60, 60));
        g_hCardBrush = CreateSolidBrush(RGB(30, 30, 30));

        g_hwndStatus = CreateWindowA("STATIC", "READY", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 75, 340, 35, hwnd, NULL, NULL, NULL);
        SendMessage(g_hwndStatus, WM_SETFONT, (WPARAM)g_hFontStatus, TRUE);
        
        HWND hHotkeys = CreateWindowA("STATIC", "HOTKEYS: CapsLock (Toggle) / End (Quit)", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 115, 340, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hHotkeys, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

        // Settings Card
        HWND hLagLabel = CreateWindowA("STATIC", "LAG DURATION", WS_VISIBLE | WS_CHILD | SS_LEFT, 35, 170, 200, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hLagLabel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hwndLagMs = CreateWindowA("STATIC", "6000 ms", WS_VISIBLE | WS_CHILD | SS_RIGHT, 145, 170, 200, 20, hwnd, NULL, NULL, NULL);
        SendMessage(g_hwndLagMs, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        g_LagSliderData.labelHwnd = g_hwndLagMs;

        HWND hLagSlider = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD | SS_NOTIFY, 30, 195, 320, 30, hwnd, (HMENU)101, NULL, NULL);
        SetWindowSubclass(hLagSlider, SliderSubclass, 101, (DWORD_PTR)&g_LagSliderData);

        HWND hLobbyLabel = CreateWindowA("STATIC", "LOBBY WAIT", WS_VISIBLE | WS_CHILD | SS_LEFT, 35, 250, 200, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hLobbyLabel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hwndLobbyMs = CreateWindowA("STATIC", "9000 ms", WS_VISIBLE | WS_CHILD | SS_RIGHT, 145, 250, 200, 20, hwnd, NULL, NULL, NULL);
        SendMessage(g_hwndLobbyMs, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        g_LobbySliderData.labelHwnd = g_hwndLobbyMs;

        HWND hLobbySlider = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD | SS_NOTIFY, 30, 275, 320, 30, hwnd, (HMENU)102, NULL, NULL);
        SetWindowSubclass(hLobbySlider, SliderSubclass, 102, (DWORD_PTR)&g_LobbySliderData);

        HWND hCheck = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD | SS_NOTIFY, 35, 335, 310, 25, hwnd, (HMENU)103, NULL, NULL);
        SetWindowSubclass(hCheck, CheckboxSubclass, 103, (DWORD_PTR)&g_AutoDupeData);

        HWND hFooter = CreateWindowA("STATIC", "GHAXLABS - DUPE UTILITY V7", WS_VISIBLE | WS_CHILD | SS_CENTER, 10, 390, 360, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hFooter, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

        ApplyResolution(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        return 0;
    }
    case WM_NCHITTEST: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);
        if (pt.x >= WIN_W - 36 && pt.x < WIN_W && pt.y >= 0 && pt.y < 30) return HTCLIENT; 
        if (pt.y < HEADER_H) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (pt.x >= WIN_W - 36 && pt.x < WIN_W && pt.y >= 0 && pt.y < 30) {
            PostMessageA(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        bool over = (pt.x >= WIN_W - 36 && pt.x < WIN_W && pt.y >= 0 && pt.y < 30);
        if (over != g_closeHovered) {
            g_closeHovered = over;
            RECT r = { WIN_W - 36, 0, WIN_W, 30 };
            InvalidateRect(hwnd, &r, FALSE);
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (g_closeHovered) {
            g_closeHovered = false;
            RECT r = { WIN_W - 36, 0, WIN_W, 30 };
            InvalidateRect(hwnd, &r, FALSE);
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(192, 192, 192));
        
        if (hCtrl == g_hwndStatus) {
            char status[32];
            GetWindowTextA(g_hwndStatus, status, 32);
            if (strcmp(status, "READY") == 0) SetTextColor(hdc, RGB(0, 255, 127));
            else if (strcmp(status, "LAGGING") == 0) SetTextColor(hdc, RGB(235, 60, 60));
            else if (strcmp(status, "IN LOBBY") == 0) SetTextColor(hdc, RGB(30, 144, 255));
            else SetTextColor(hdc, RGB(255, 215, 0));
        } else {
            RECT rc; GetWindowRect(hCtrl, &rc);
            MapWindowPoints(NULL, hwnd, (LPPOINT)&rc, 2);
            if (rc.top > 380) SetTextColor(hdc, RGB(102, 102, 102));
        }
        return (LRESULT)g_hBackBrush;
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(192, 192, 192));
        SetBkColor(hdc, RGB(26, 26, 26));
        return (LRESULT)g_hBackBrush;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        FillRect(hdc, &rc, g_hBackBrush);
        RECT headerRect = { 0, 0, rc.right, HEADER_H };
        FillRect(hdc, &headerRect, g_hHeaderBrush);
        RECT accentRect = { 0, HEADER_H - 2, rc.right, HEADER_H };
        FillRect(hdc, &accentRect, g_hAccentBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(235, 60, 60));
        SelectObject(hdc, g_hFontTitle);
        RECT titleRect = { 0, 10, rc.right, HEADER_H };
        DrawTextA(hdc, "GHAXLABS", -1, &titleRect, DT_CENTER | DT_SINGLELINE);

        if (g_closeHovered) {
            RECT closeRect = { WIN_W - 36, 0, WIN_W, 30 };
            HBRUSH hHover = CreateSolidBrush(RGB(235, 60, 60));
            FillRect(hdc, &closeRect, hHover);
            DeleteObject(hHover);
        }
        SetTextColor(hdc, RGB(240, 235, 235));
        RECT xRect = { WIN_W - 36, 0, WIN_W, 30 };
        DrawTextA(hdc, "X", -1, &xRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(51, 51, 51));
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, 0, 0, rc.right, rc.bottom);
        
        RECT cardRect = { 20, 150, rc.right - 20, 375 };
        FillRect(hdc, &cardRect, g_hCardBrush);
        Rectangle(hdc, 20, 150, rc.right - 20, 375);

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hPen);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 103) {
            g_AutoDupe = (IsDlgButtonChecked(hwnd, 103) == BST_CHECKED);
        }
        return 0;
    }
    case WM_DESTROY:
        DeleteObject(g_hFontNormal);
        DeleteObject(g_hFontTitle);
        DeleteObject(g_hFontStatus);
        DeleteObject(g_hFontSmall);
        DeleteObject(g_hBackBrush);
        DeleteObject(g_hHeaderBrush);
        DeleteObject(g_hAccentBrush);
        DeleteObject(g_hCardBrush);
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        SetRuleEnabled(false);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDPIAware();
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    BOOL bIsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &bIsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    if (!bIsAdmin) {
        char szPath[MAX_PATH];
        GetModuleFileNameA(NULL, szPath, MAX_PATH);
        ShellExecuteA(NULL, "runas", szPath, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }

    const char CLASS_NAME[] = "UtilityAppClass";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExA(0, CLASS_NAME, "Network Utility", WS_POPUP,
        (sw - WIN_W) / 2, (sh - WIN_H) / 2, WIN_W, WIN_H,
        NULL, NULL, hInstance, NULL);
    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);
    RegisterHotKey(hwnd, 1, 0, VK_CAPITAL);
    RegisterHotKey(hwnd, 2, 0, VK_END);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == 1) StartLag();
            if (msg.wParam == 2) {
                SetRuleEnabled(false);
                DestroyWindow(hwnd);
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
