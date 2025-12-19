#include <windows.h>
#include <string>
#include <cstdio>

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

HWND EditInput = NULL;
HWND ListBox = NULL;
HWND SendButton = NULL;

HANDLE FileMappingHandle = NULL;
HANDLE MutualExclusionHandle = NULL;
HANDLE EventHandle = NULL;

SharedMemoryData* SharedData = nullptr;
unsigned long LastSequence = 0;
bool IsRunning = true;

HANDLE ReceiverThreadHandle = NULL;

void AddToListBox(const char* msg) {
    if (ListBox) SendMessageA(ListBox, LB_ADDSTRING, 0, (LPARAM)msg);
}

DWORD WINAPI ReceiverThread(LPVOID) {
    while (IsRunning) {
        WaitForSingleObject(EventHandle, INFINITE);
        WaitForSingleObject(MutualExclusionHandle, INFINITE);

        while (LastSequence < SharedData->Sequence) {
            LastSequence++;
            unsigned index = (unsigned)(LastSequence % MaxMessagesCount);
            AddToListBox(SharedData->Messages[index]);
        }

        ReleaseMutex(MutualExclusionHandle);
        ResetEvent(EventHandle);
    }
    return 0;
}

void SendServerMessage() {
    char text[MessageSize] = {0};
    GetWindowTextA(EditInput, text, MessageSize);
    if (strlen(text) == 0) return;

    WaitForSingleObject(MutualExclusionHandle, INFINITE);

    SharedData->Sequence++;
    unsigned index = (unsigned)(SharedData->Sequence % MaxMessagesCount);
    string msg = string("Server: ") + text;
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
        if (LOWORD(wParam) == 1) SendServerMessage();
        break;
    case WM_DESTROY:
        IsRunning = false;
        if (ReceiverThreadHandle) { WaitForSingleObject(ReceiverThreadHandle, 500); CloseHandle(ReceiverThreadHandle); ReceiverThreadHandle = NULL; }
        if (SharedData) UnmapViewOfFile(SharedData);
        if (FileMappingHandle) CloseHandle(FileMappingHandle);
        if (MutualExclusionHandle) CloseHandle(MutualExclusionHandle);
        if (EventHandle) CloseHandle(EventHandle);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    FileMappingHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedMemoryData), SharedMemory);
    if (!FileMappingHandle) { MessageBoxA(NULL, "CreateFileMapping failed", "Error", MB_OK | MB_ICONERROR); return 1; }

    SharedData = (SharedMemoryData*)MapViewOfFile(FileMappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemoryData));
    if (!SharedData) { MessageBoxA(NULL, "MapViewOfFile failed", "Error", MB_OK | MB_ICONERROR); CloseHandle(FileMappingHandle); return 1; }

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        SharedData->Sequence = 0;
        for (int i = 0; i < MaxMessagesCount; ++i) SharedData->Messages[i][0] = '\0';
    }

    MutualExclusionHandle = CreateMutexA(NULL, FALSE, MutualExclusion);
    EventHandle = CreateEventA(NULL, TRUE, FALSE, Event);

    WNDCLASSA window = { 0 };
    window.lpfnWndProc = WndProc;
    window.hInstance = hInstance;
    window.lpszClassName = "ChatServerWindow";
    window.hbrBackground = CreateSolidBrush(RGB(203, 204, 246));
    RegisterClassA(&window);

    HWND hwnd = CreateWindowA("ChatServerWindow", "Shared Memory (Server)", WS_CAPTION | WS_SYSMENU, 500, 200, 500, 500, NULL, NULL, hInstance, NULL);

    EditInput = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP , 15, 20, 460, 25, hwnd, NULL, hInstance, NULL);
    SendButton = CreateWindowA("BUTTON", "Broadcast", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 355, 50, 120, 25, hwnd, (HMENU)1, hInstance, NULL);
    ListBox = CreateWindowA("LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 15, 80, 460, 380, hwnd, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    ReceiverThreadHandle = CreateThread(NULL, 0, ReceiverThread, NULL, 0, NULL);

    MSG message;
    while (GetMessage(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return 0;
}

