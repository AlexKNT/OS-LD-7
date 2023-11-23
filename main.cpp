#undef UNICODE // Pārslēgt projektu ASCII režīmā
#include <windows.h>
#include <stdio.h>
#include "resource.h"
#include <strsafe.h>
#include <tchar.h>
#pragma warning(disable : 4996)


// Global variables
HANDLE hListenerThread;
HANDLE hCompletionEvent;
HANDLE hMailSlot;

DWORD WINAPI ListenerThread(LPVOID lpParam) {
    HWND hWnd = (HWND)lpParam;

    DWORD cbMessage, cMessage, cbRead;
    BOOL fResult;
    char lpszBuffer[1024];

    while (1) {
        // Check for completion request
        if (WaitForSingleObject(hCompletionEvent, 0) == WAIT_OBJECT_0) {
            break;
        }

        // Check for new message
        fResult = GetMailslotInfo(hMailSlot, NULL, &cbMessage, &cMessage, NULL);

        if (!fResult) {
            // Handle error
            return 1;
        }

        if (cbMessage != MAILSLOT_NO_MESSAGE) {
            // Create a buffer for the message
            char* pBuffer = new char[cbMessage];
            // Read the message from the mailslot
            fResult = ReadFile(hMailSlot, pBuffer, cbMessage, &cbRead, NULL);

            if (fResult && cbRead > 0) {
                // Send the message to the main thread using a Windows message
                SendMessage(GetDlgItem(hWnd, IDC_HUBBUB), WM_SETTEXT, 0, (LPARAM)pBuffer);
            }

            delete[] pBuffer;
        }

        // Sleep for a short interval (adjust as needed)
        Sleep(1000);
    }

    CloseHandle(hMailSlot);

    return 0;
}


// Ziņojumu apstrādātāja prototips
BOOL CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
// Galvenā funkcija
// ja galvenā loga šablonam ir piešķirts resursa identifikators IDD_MAINDIALOG
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    DialogBox(
        hInstance,
        MAKEINTRESOURCE(IDD_MAINDIALOG),
        NULL,
        DLGPROC(MainWndProc)
    );
    return 0;
}
// Ziņojumu apstrādātājs
BOOL CALLBACK MainWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    switch (Msg) { // pārbaudīt ziņojuma identifikatoru
    case WM_INITDIALOG: {
        // Create an event for completion signaling
        hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        // Create a mailslot
        hMailSlot = CreateMailslot(("\\\\.\\mailslot\\Shouter"), 0, MAILSLOT_WAIT_FOREVER, NULL);

        // Create the listener thread
        DWORD dwThreadId;
        hListenerThread = CreateThread(NULL, 0, ListenerThread, hWnd, 0, &dwThreadId);

        return TRUE;
    }
    case WM_COMMAND: {// ja tas ir paziņojums no vadības elementa
        switch (LOWORD(wParam)) { // pārbaudīt vadības elementa ID
        case IDCANCEL: {// ja bija uzspiesta OK poga
            DestroyWindow(hWnd); // aizvērt logu
            return TRUE;
        }

                     // IDC_SHOUT button handler
        case IDC_SHOUT: {
            WCHAR szComputerName[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD dwSize = MAX_COMPUTERNAME_LENGTH + 1;
            GetComputerNameW(szComputerName, &dwSize);

            WCHAR szUserName[MAX_PATH];
            GetDlgItemTextW(hWnd, IDC_MYNAME, szUserName, MAX_PATH);

            WCHAR szMessage[256];
            GetDlgItemTextW(hWnd, IDC_MYSCREAM, szMessage, 256);

            WCHAR szFinalMessage[512];

            // Check if the robot is activated
            if (IsDlgButtonChecked(hWnd, IDC_ROBOT) == BST_CHECKED) {
                // If the robot is activated, respond with a predefined message
                StringCbPrintfW(szFinalMessage, sizeof(szFinalMessage), L"%s from %s shout: STOP SPAMMING!!!", szUserName, szComputerName);
            }
            else {
                // Otherwise, use the original shout format
                StringCbPrintfW(szFinalMessage, sizeof(szFinalMessage), L"%s from %s shout: %s", szUserName, szComputerName, szMessage);
            }

            // Create a file for the mailbox entry
            HANDLE hMailbox = CreateFileW(L"\\\\.\\mailslot\\Shouter", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            // Send a text message to the mailslot by writing it to the created file
            DWORD dwBytesWritten;
            WriteFile(hMailbox, szFinalMessage, sizeof(szFinalMessage), &dwBytesWritten, NULL);

            // Close the file handle
            CloseHandle(hMailbox);

            return TRUE;
        }

        case IDC_ROBOT: {
            if (IsDlgButtonChecked(hWnd, IDC_ROBOT) == BST_CHECKED) {
                // Activate the robot
                SendMessage(GetDlgItem(hWnd, IDC_HUBBUB), WM_SETTEXT, 0, (LPARAM)L"Robot activated. It will respond with 'STOP SPAMMING!!!'.");
            }
            else {
                // Deactivate the robot
                SendMessage(GetDlgItem(hWnd, IDC_HUBBUB), WM_SETTEXT, 0, (LPARAM)L"Robot deactivated.");
            }
            return TRUE;
        }
        }
    }
        return FALSE;
    
    case WM_USER + 1: {
        char* pBuffer = (char*)wParam;
        // Write the received text message to the IDC_HUBBUB field
        SendMessage(GetDlgItem(hWnd, IDC_HUBBUB), WM_SETTEXT, 0, (LPARAM)pBuffer);
        // Delete the buffer
        delete[] pBuffer;
        return TRUE;
    }

    case WM_DESTROY: {// ja tas ir pabeigšanas pieprasījums
        SetEvent(hCompletionEvent);
        WaitForSingleObject(hListenerThread, INFINITE);
        CloseHandle(hCompletionEvent);
        EndDialog(hWnd, 0);
        return TRUE;
    }
    }
    return FALSE;
}

/*
And resource:
Dialog IDD_MAINDIALOG, Edit control IDC_MYSCREAM, Edit control IDC_HUBBUB, Button IDC_EXIT, Edit control IDC_MYNAME, Check box IDC_ROBOT, Button IDC_SHOUT

Write a local network broadcast chat. In this chat, a text message sent by any local network user will be received by all other users. Windows mail slots must be used for sending broadcasts. Therefore, in order for all programs to be able to send and receive information, they must use a mail slot with the common name Shouter.

Structure: The program must contain the main thread, which serves the form and organizes the dialogue with the user, and the listener thread, which waits for text messages from the network and sends them to the main thread. The listening thread creates a mail slot called Shouter and waits for a message in that mail slot, periodically checking the slot for new messages and, if there are, reading them and sending them to the main form using the Windows message mechanism.

Use HANDLE WINAPI CreateMailslot() Example: \.\mailslot\Shouter. Use GetMailslotInfo func. Use ReadFile func. Use CreateFile func. Use WriteFile func.

Implement listener thread:
1. HANDLE WINAPI CreateMailslot(),
2. In the main loop, while no completion request,
- determine the size of the last text message in the mail slot;
- if there is a short message in the slot (the size of the last short message is not equal to MAILSLOT_NO_MESSAGE), create a buffer of required size in the stack, read the short message from the mailslot to this buffer and send the buffer address to the main thread using a user WINAPI windows message;
3. Close the hanle of the mail lot after the loop

Implement processing user WINAPI windows message from listener thread:
1. Read the buffered address from the message parameters.
2. Write the received text message to the IDC_HUBBUB field.
3. Delete the row from the stack

Implement IDC_SHOUT button:
1. Create a short message by connecting the computer name (requested with the GetComputerName function), the user name (from the field IDC_MYNAME) and the text of the short message (from the field IDC_MYSCREAM) using sprintf_s, in the following way: USER_NAME from COMPUTER_NAME shout: TEXT
2. Create a file for the mailbox entry.
3. Send a text message to the mailslot by writing it to the created file.
4. Close the file handle.

Implement WM_INITDIALOG:
Run the listening thread and save its handle on a global variable.

Implement WM_DESTROY:
1. Set the completion request flag to listener thread.
2. Wait for the thread to complete using the WaitForSingleObject function.
3. Close the thread handle.

Implement IDC_ROBOT checkbox:
Add a bot to the program that could automatically answer some questions if checkbox enabled. The robot can be turned on or off with the control element (check box) on the main window.
If the robot is activated, it analyzes all incoming text messages, forms and sends reply: "STOP SPAMMING!!!"
*/