#include <windows.h>
#include <string>
#include <vector>
#include <thread>

using namespace std;

#define SharedMemory    "Global\\ChatMemory"
#define MutualExclusion "Global\\ChatMutex"
#define Event           "Global\\ChatEvent"

#define MaxMessagesCount 10
#define MessageSize 60

struct SharedMemoryData {
    unsigned long Sequence;
    char Messages[MaxMessagesCount][MessageSize];
};

HWND EditInput  = NULL;
HWND ListBox = NULL;
HWND SendButton = NULL;

HANDLE FileMappingHandle = NULL;
HANDLE MutualExclusionHandle = NULL;
HANDLE EventHandle = NULL;

SharedMemoryData* SharedData;
unsigned long LastSequence = 0;
bool IsRunning = true;

string username;

HWND NameEdit;
bool gotUsername = false;

LRESULT CALLBACK NameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            char buf[64];
            GetWindowTextA(NameEdit, buf, 64);
            username = buf;
            if (username.empty()) username = "Client";
            gotUsername = true;
            DestroyWindow(hwnd);
        }
        break;
    case WM_CLOSE:
        if (!gotUsername) username = "Client";
        gotUsername = true;
        DestroyWindow(hwnd);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void UsernameThread(HINSTANCE hInst) {
    WNDCLASSA window = {0};
    window.lpfnWndProc = NameWndProc;
    window.hInstance = hInst;
    window.lpszClassName = "ChatClientNameWindow";
    window.hbrBackground = CreateSolidBrush(RGB(203, 204, 246));
    RegisterClassA(&window);

    HWND hwnd = CreateWindowA("ChatClientNameWindow", "Client Name",
        WS_CAPTION | WS_SYSMENU, 600, 350, 300, 120,
        NULL, NULL, hInst, NULL);

    NameEdit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        15, 20, 260, 25, hwnd, NULL, hInst, NULL);

    CreateWindowA("BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        90, 60, 100, 25, hwnd, (HMENU)1, hInst, NULL);

    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (!gotUsername) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10);
    }
}

void AddToListBox(const char* msg) {
    SendMessageA(ListBox, LB_ADDSTRING, 0, (LPARAM)msg);
}

DWORD WINAPI ReceiverThread(LPVOID) {
    while (IsRunning) {
        WaitForSingleObject(EventHandle, INFINITE);
        WaitForSingleObject(MutualExclusionHandle, INFINITE);

        while (LastSequence < SharedData->Sequence) {
            LastSequence++;
            unsigned index = LastSequence % MaxMessagesCount;
            AddToListBox(SharedData->Messages[index]);
        }

        ReleaseMutex(MutualExclusionHandle);
    }
    return 0;
}

void SendMessageToServer() {
    char text[MessageSize];
    GetWindowTextA(EditInput, text, MessageSize);

    if (strlen(text) == 0) return;

    WaitForSingleObject(MutualExclusionHandle, INFINITE);

    SharedData->Sequence++;
    unsigned index = SharedData->Sequence % MaxMessagesCount;

    string msg = username + ": " + text;
    size_t len = msg.size();
    if (len >= MessageSize) len = MessageSize - 1;

    memcpy(SharedData->Messages[index], msg.c_str(), len);
    SharedData->Messages[index][len] = '\0';

    ReleaseMutex(MutualExclusionHandle);
    SetEvent(EventHandle);

    SetWindowTextA(EditInput, "");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            SendMessageToServer();
        }
        break;
    case WM_DESTROY:
        IsRunning = false;
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {

    thread t(UsernameThread, hInstance);
    while (!gotUsername) Sleep(10);
    t.join();

    FileMappingHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
        sizeof(SharedMemoryData), SharedMemory);
    SharedData = (SharedMemoryData*)MapViewOfFile(FileMappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemoryData));
    MutualExclusionHandle = CreateMutexA(NULL, FALSE, MutualExclusion);
    EventHandle = CreateEventA(NULL, TRUE, FALSE, Event);

    WNDCLASSA window = {0};
    window.lpfnWndProc = WndProc;
    window.hInstance = hInstance;
    window.lpszClassName = "ChatClientWindow";
    window.hbrBackground = CreateSolidBrush(RGB(203, 204, 246));
    RegisterClassA(&window);

    HWND hwnd = CreateWindowA("ChatClientWindow", "Shared Memory Client",
        WS_OVERLAPPEDWINDOW, 500, 200, 500, 500,
        NULL, NULL, hInstance, NULL);

    EditInput = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP ,
        15, 20, 460, 25, hwnd, NULL, hInstance, NULL);

    SendButton = CreateWindowA("BUTTON", "Send",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        355, 50, 120, 25, hwnd, (HMENU)1, hInstance, NULL);

    ListBox = CreateWindowA("LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY,
        15, 80, 460, 380, hwnd, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    CreateThread(NULL, 0, ReceiverThread, NULL, 0, NULL);

    MSG message;
    while (GetMessage(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    IsRunning = false;
    return 0;
}
