#include <windows.h>
#include <stdio.h>
#include "resource.h"
#include <strsafe.h>
#include <tchar.h>
#include <thread>

// Global variables
HANDLE g_hListenerThread = nullptr;
bool g_bCompletionRequest = false;
HWND g_hWnd = nullptr;
bool g_bRobotActivated = false;

// Listener thread function
DWORD WINAPI ListenerThread(LPVOID lpParam) {
    HANDLE hMailSlot = CreateMailslot(L"\\\\.\\mailslot\\Shouter", 0, MAILSLOT_WAIT_FOREVER, nullptr);

    while (!g_bCompletionRequest) {
        DWORD cbMessage, cMessage, cbRead;
        BOOL fResult;

        fResult = GetMailslotInfo(hMailSlot, nullptr, &cbMessage, &cMessage, nullptr);

        if (fResult && cbMessage != MAILSLOT_NO_MESSAGE) {
            wchar_t* pBuffer = new wchar_t[cbMessage / sizeof(wchar_t)];
            ReadFile(hMailSlot, pBuffer, cbMessage, &cbRead, nullptr);

            // Send the buffer address to the main thread using a user Windows message
            PostMessage(g_hWnd, WM_USER + 1, reinterpret_cast<WPARAM>(pBuffer), 0);
        }

        // Sleep for a short interval to avoid high CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    CloseHandle(hMailSlot);
    return 0;
}

// Other function prototypes
void ProcessIncomingMessage(wchar_t* pMessage);
void SendMessageToMailSlot(const wchar_t* pMessage);

// Main window procedure
BOOL CALLBACK MainWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    switch (Msg) {
    case WM_INITDIALOG: {
        g_hWnd = hWnd;

        // Run the listening thread and save its handle
        g_hListenerThread = CreateThread(nullptr, 0, ListenerThread, nullptr, 0, nullptr);
        return TRUE;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDCANCEL: {
            DestroyWindow(hWnd);
            return TRUE;
        }
        case IDC_SHOUT: {
            wchar_t szComputerName[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD dwSize = sizeof(szComputerName) / sizeof(wchar_t);
            GetComputerName(szComputerName, &dwSize);

            wchar_t szUserName[256];
            GetDlgItemText(hWnd, IDC_MYNAME, szUserName, ARRAYSIZE(szUserName));

            wchar_t szMessage[512];
            GetDlgItemText(hWnd, IDC_MYSCREAM, szMessage, ARRAYSIZE(szMessage));

            wchar_t szShortMessage[1024];
            StringCbPrintf(szShortMessage, sizeof(szShortMessage), L"%s from %s shout: %s\n", szUserName, szComputerName, szMessage);

            SendMessageToMailSlot(szShortMessage);
            return TRUE;
        }
        case IDC_ROBOT: {
            g_bRobotActivated = IsDlgButtonChecked(hWnd, IDC_ROBOT) == BST_CHECKED;
            return TRUE;
        }
        }
        return FALSE;
    }
    case WM_USER + 1: {
        // Process the incoming message
        ProcessIncomingMessage(reinterpret_cast<wchar_t*>(wParam));
        return TRUE;
    }
    case WM_DESTROY: {
        // Set the completion request flag to the listener thread
        g_bCompletionRequest = true;

        // Wait for the thread to complete
        WaitForSingleObject(g_hListenerThread, INFINITE);

        // Close the thread handle
        CloseHandle(g_hListenerThread);

        // Post quit message
        PostQuitMessage(0);
        return TRUE;
    }
    }
    return FALSE;
}

// Function to process incoming message
void ProcessIncomingMessage(wchar_t* pMessage) {
    // Set the text in IDC_HUBBUB
    SetDlgItemText(g_hWnd, IDC_HUBBUB, pMessage);

    if (g_bRobotActivated) {
        // Check if the incoming message contains a question
        if (wcsstr(pMessage, L"?") != nullptr) {
            // If a question is detected, send a robot response
            wchar_t szRobotResponse[] = L"STOP SPAMMING!!!";
            SendMessageToMailSlot(szRobotResponse);
        }
    }

    // Delete the allocated buffer
    delete[] pMessage;
}

// Function to send a text message to the mailslot
void SendMessageToMailSlot(const wchar_t* pMessage) {
    HANDLE hMailSlot = CreateFile(L"\\\\.\\mailslot\\Shouter", GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hMailSlot != INVALID_HANDLE_VALUE) {
        DWORD cbWritten;
        WriteFile(hMailSlot, pMessage, (wcslen(pMessage) + 1) * sizeof(wchar_t), &cbWritten, nullptr);
        CloseHandle(hMailSlot);
        PostMessage(g_hWnd, WM_USER + 1, reinterpret_cast<WPARAM>(_wcsdup(pMessage)), 0);
    }
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    DialogBox(
        hInstance,
        MAKEINTRESOURCE(IDD_MAINDIALOG),
        nullptr,
        DLGPROC(MainWndProc)
    );
    return 0;
}
