#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS

#include "./c_core.h"

#include <thread>

#define CLIENT_COUNT 5
#define PACKET_SIZE 512
#define PORT_NUMBER 23
#define IP_ADDRESS "127.0.0.1"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

SOCK_INFO clients[CLIENT_COUNT + 1];
unsigned int socket_count;

// 서버 생성 함수
SOCKET init_server() {
    WSADATA wsaData;
    SOCKET server_socket;
    SOCKADDR_IN server_addr;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed" << endl;
        return 1;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT_NUMBER);

    if (bind(server_socket, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cout << "bind failed" << endl;
        return 1;
    }

    if (listen(server_socket, CLIENT_COUNT) == SOCKET_ERROR) {
        cout << "listen failed" << endl;
        return 1;
    }

    return server_socket;
}

void add_client(int idx) {
    SOCKET client_socket;
    SOCKADDR_IN client_addr;
    HANDLE ev;
    int client_addr_len;

    client_addr_len = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));
    client_socket = accept(clients[idx].socket, (SOCKADDR*)&client_addr, &client_addr_len);

    ev = WSACreateEvent();

    clients[socket_count].socket = client_socket;
    clients[socket_count].ev = ev;
    strcpy_s(clients[socket_count].ip, inet_ntoa(client_addr.sin_addr));

    WSAEventSelect(client_socket, ev, FD_READ | FD_CLOSE);



    socket_count += 1;

    cout << "Client Connected : " << clients[socket_count - 1].ip << endl;
}

unsigned int WINAPI recv_data(void* params) {
    int idx = (int)params;
    char buf[PACKET_SIZE];
    SOCKADDR_IN client_addr;
    int recv_len;
    int addr_len;
    Json::Value data;

    memset(&client_addr, 0, sizeof(client_addr));

    if ((recv_len = recv(clients[idx].socket, buf, PACKET_SIZE, 0)) > 0) {
        addr_len = sizeof(client_addr);
        getpeername(clients[idx].socket, (SOCKADDR*)&client_addr, &addr_len);

        data = json_from_buffer(buf);

        if(data) cout << data << endl;

        int ret = event_handler(data, &clients[idx]);
    }

    _endthreadex(0);

    return 0;
}

void read_client(int idx) {
    unsigned int tid;

    HANDLE read_thread = (HANDLE)_beginthreadex(NULL, 0, recv_data, (void*)idx, 0, &tid);
    WaitForSingleObject(read_thread, INFINITE);

    CloseHandle(read_thread);
}

void rm_client(int idx) {
    cout << "Client Disconnected : " << clients[idx].ip << endl;

    closesocket(clients[idx].socket);
    WSACloseEvent(clients[idx].ev);

    socket_count -= 1;

    clients[idx].socket = 0;
    clients[idx].ev = 0;

    for (unsigned int i = idx; i < socket_count; i++) {
        clients[i] = clients[i + 1];
    }
}

unsigned int WINAPI core_thread(void* params) {
    SOCKET server_socket;
    WSANETWORKEVENTS ev;
    HANDLE event;
    WSAEVENT events[CLIENT_COUNT + 1];

    int idx;

    server_socket = init_server();

    if (server_socket == INVALID_SOCKET) {
        cout << "server init failed" << endl;
        return 1;
    }

    event = WSACreateEvent();

    clients[0].socket = server_socket;
    clients[0].ev = event;
    WSAEventSelect(server_socket, event, FD_ACCEPT);

    socket_count += 1;

    while (true) {
        memset(&events, 0, sizeof(events));

        for (unsigned int i = 0; i < socket_count; i++) {
            events[i] = clients[i].ev;
        }

        idx = WSAWaitForMultipleEvents(socket_count, events, false, WSA_INFINITE, false);

        if (idx == WSA_WAIT_FAILED) {
            cout << "WSAWaitForMultipleEvents failed" << endl;
            return 1;
        }

        if (idx == WSA_WAIT_TIMEOUT) {
            continue;
        }

        WSAEnumNetworkEvents(clients[idx].socket, clients[idx].ev, &ev);

        if (ev.lNetworkEvents == FD_ACCEPT) {
            // 새로운 클라이언트 접속
            add_client(idx);
        }
        else if (ev.lNetworkEvents == FD_READ) {
            read_client(idx);
            // 클라이언트로부터 데이터 수신
        }
        else if (ev.lNetworkEvents == FD_CLOSE) {
            rm_client(idx);
            // 클라이언트 접속 종료
        }
    }

    _endthreadex(0);

    return NULL;
}

int main() {
    unsigned int thread_id;
    HANDLE thread_handle;

    thread_handle = (HANDLE)_beginthreadex(NULL, 0, core_thread, NULL, 0, &thread_id);

    if (thread_handle) {
        cout << "server started" << endl;

        WaitForSingleObject(thread_handle, INFINITE);
        CloseHandle(thread_handle);
    }

    return 0;
}