#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

HWND IpInput = NULL;
HWND PortInput = NULL;
HWND MessageInput = NULL;
HWND ConnectButton = NULL;
HWND SendButton = NULL;
HWND LogBox = NULL;

SOCKET ClientSocket = INVALID_SOCKET;
bool IsConnected = false;

void Log(const char* text) {
    int Length = GetWindowTextLength(LogBox);
    SendMessage(LogBox, EM_SETSEL, Length, Length);
    SendMessage(LogBox, EM_REPLACESEL, 0, (LPARAM)text);
    SendMessage(LogBox, EM_REPLACESEL, 0, (LPARAM)"\r\n");
}

DWORD WINAPI ReceiverThread(LPVOID) {
    char buffer[512];

    while (IsConnected) {

        int bytes = recv(ClientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
            Log("Disconnected from server.");
            IsConnected = false;
            break;
        }

        buffer[bytes] = 0;
        Log(buffer);
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    switch (msg) {

    case WM_CREATE:
        IpInput = CreateWindow("EDIT", "127.0.0.1",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            15, 20, 220, 25,
            hwnd, NULL, NULL, NULL);

        PortInput = CreateWindow("EDIT", "8080",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            240, 20, 230, 25,
            hwnd, NULL, NULL, NULL);

        ConnectButton = CreateWindow("BUTTON", "Connect",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            370, 50, 100, 25,
            hwnd, (HMENU)1, NULL, NULL);

        MessageInput = CreateWindow("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            15, 80, 455, 25,
            hwnd, NULL, NULL, NULL);

        SendButton = CreateWindow("BUTTON", "Send",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            370, 110, 100, 25,
            hwnd, (HMENU)2, NULL, NULL);

        LogBox = CreateWindow("EDIT", "",
            WS_CHILD | WS_VISIBLE  |
            ES_MULTILINE | ES_READONLY,
            15, 140, 460, 310,
            hwnd, NULL, NULL, NULL);
        break;

    case WM_COMMAND:

        if (LOWORD(wParam) == 1 && !IsConnected) {

            char ip[32], portStr[16];
            GetWindowText(IpInput, ip, sizeof(ip));
            GetWindowText(PortInput, portStr, sizeof(portStr));
            int port = atoi(portStr);

            WSADATA wsa;
            WSAStartup(MAKEWORD(2,2), &wsa);

            ClientSocket = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &serverAddr.sin_addr);

            Log("Connecting...");

            if (connect(ClientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                Log("Connection failed!");
                closesocket(ClientSocket);
            } else {
                IsConnected = true;
                Log("Connected!");

                CreateThread(NULL, 0, ReceiverThread, NULL, 0, NULL);
            }
        }

        if (LOWORD(wParam) == 2 && IsConnected) {

            char msg[256];
            GetWindowText(MessageInput, msg, sizeof(msg));

            if (strlen(msg) > 0) {
                send(ClientSocket, msg, strlen(msg), 0);
                Log((string("You: ") + msg).c_str());
            }
        }

        break;

    case WM_DESTROY:
        IsConnected = false;
        closesocket(ClientSocket);
        WSACleanup();
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {

    WNDCLASS window = { 0 };
    window.lpfnWndProc = WndProc;
    window.hInstance = hInst;
    window.lpszClassName = "SocketAndMultithreadingClient";
    window.hbrBackground = CreateSolidBrush(RGB(203, 204, 246));
    RegisterClass(&window);

    HWND hwnd = CreateWindowA("SocketAndMultithreadingClient", "Socket & Multithreading (Client)"
                , WS_CAPTION | WS_SYSMENU, 500, 200, 500, 500, NULL, NULL, hInst, NULL);


    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
