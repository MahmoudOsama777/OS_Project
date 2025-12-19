#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <string>
#include <cstdlib>

using namespace std;

HWND LogBox = NULL;
HWND PortInput = NULL;
HWND StartButton = NULL;

SOCKET ServerSocket;
vector<SOCKET> Clients;
mutex ClientsMutex;
bool IsRunning = false;

void Log(const char* Message) {
    int Length = GetWindowTextLength(LogBox);
    SendMessage(LogBox, EM_SETSEL, Length, Length);
    SendMessage(LogBox, EM_REPLACESEL, FALSE, (LPARAM)Message);
    SendMessage(LogBox, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
}

void Broadcast(const string& Message, SOCKET exclude = INVALID_SOCKET) {
    lock_guard<mutex> lock(ClientsMutex);
    for (SOCKET c : Clients) {
        if (c != exclude)
            send(c, Message.c_str(), (int)Message.size(), 0);
    }
}

void ClientThread(SOCKET Clientsock) {
    char buffer[512];
    Log("Client connected!");

    while (true) {
        int bytes = recv(Clientsock, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) break;

        buffer[bytes] = 0;
        string Message = string("Client: ") + buffer;
        Log(Message.c_str());
        Broadcast(Message, Clientsock);
    }

    {
        lock_guard<mutex> lock(ClientsMutex);
        Clients.erase(remove(Clients.begin(), Clients.end(), Clientsock), Clients.end());
    }

    closesocket(Clientsock);
    Log("Client disconnected!");
}

void ServerThread() {
    Log("Server listening...");
    while (IsRunning) {
        SOCKET Clientsock = accept(ServerSocket, NULL, NULL);
        if (Clientsock == INVALID_SOCKET) continue;

        {
            lock_guard<mutex> lock(ClientsMutex);
            Clients.push_back(Clientsock);
        }

        thread t(ClientThread, Clientsock);
        t.detach();
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {

    switch (Message) {

    case WM_CREATE:
        PortInput = CreateWindow("EDIT", "8080",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            15, 20, 460, 25, hwnd, NULL, NULL, NULL);

        StartButton = CreateWindow("BUTTON", "Start Server",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            355, 50, 120, 25, hwnd, (HMENU)1, NULL, NULL);

        LogBox = CreateWindow("EDIT", "",
            WS_CHILD | WS_VISIBLE  |
            ES_MULTILINE | ES_READONLY,
            15, 80, 460, 380, hwnd, NULL, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1 && !IsRunning) {
            char portStr[10];
            GetWindowText(PortInput, portStr, sizeof(portStr));
            int port = atoi(portStr);

            WSADATA wsa;
            WSAStartup(MAKEWORD(2,2), &wsa);

            ServerSocket = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            serverAddr.sin_addr.s_addr = INADDR_ANY;

            if (bind(ServerSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                Log("Bind failed!");
                break;
            }

            listen(ServerSocket, SOMAXCONN);
            IsRunning = true;

            thread(ServerThread).detach();
            Log("Server Started!");
        }
        break;

    case WM_DESTROY:
        IsRunning = false;
        closesocket(ServerSocket);
        WSACleanup();
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, Message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {

    WNDCLASS window = {0};
    window.lpfnWndProc = WndProc;
    window.hInstance = hInst;
    window.lpszClassName = "SocketAndMultithreadingServer";
    window.hbrBackground = CreateSolidBrush(RGB(203, 204, 246));
    RegisterClass(&window);



    HWND hwnd = CreateWindowA("SocketAndMultithreadingServer", "Socket & Multithreading (Server)"
                    , WS_CAPTION | WS_SYSMENU, 500, 200, 500, 500, NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nCmdShow);

    MSG Message;
    while (GetMessage(&Message, NULL, 0, 0)) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    return 0;
}
