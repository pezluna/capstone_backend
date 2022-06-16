#pragma once

#include <hash_map>
#include <math.h>
#include <WinSock2.h>
#include <string>
#include <sstream>
#include <vector>

#include <stdio.h>
#include <iostream>

using namespace std;

#define MAX_ROOM_MEMBER 3
#define M_PI 3.14159265358979323846

/*
* SOCK_INFO : handler�� client ������ ������Ÿ��
* SOCK_INFO->u �� ���� �ش� Ŭ���̾�Ʈ�� ����� USER_TABLE�� ���� ����
*
* USER_TABLE : Ŭ���̾�Ʈ�� 1:1 ���� ������, ������ ����� ������ ����(!=Ŭ���̾�Ʈ�� ����)
* USER_TABLE.x, USER_TABLE.y, USER_TABLE.z�� ���� ������ ��ġ ���� ȹ�� ����
* USER_TABLE.username, USER_TABLE.roomname�� ���� ���� �̸��� �� �̸� ȹ�� ����
* USER_TABLE.ip : ���Ͽ� �ִ°� ���� ����
* USER_TABLE->s : 1:1 ������ socket ������ ���� ����
*
* ROOM_TABLE : ���� ������ 1:N ���� ������, ���� ���� ����
* ROOM_TABLE.user_count�� ���� ���� in-game�� ���� �������� �� �� ����
* ROOM_TABLE->users[idx]�� ���� ���� �������� ������ ������ ���� ����
*/

typedef struct sock_info {
    SOCKET socket;
    HANDLE ev;
    struct user_table* u;
    char ip[20];
} SOCK_INFO;

typedef struct user_table {
    double x;
    double y;
    double z;
    string username;
    string roomname;
    struct sock_info* s;
} USER_TABLE;

typedef struct room_table {
    int user_count;
    USER_TABLE* users[MAX_ROOM_MEMBER];
} ROOM_TABLE;

hash_map<string, ROOM_TABLE> room_list;
int threshold = 1;

vector<string> split(string str, char Delimiter) {
    istringstream iss(str);
    string buf;
    vector<string> result;

    while (getline(iss, buf, Delimiter)) {
        result.push_back(buf);
    }

    return result;
}

vector<string> get_event(char* buffer)
{
    /*
    * get_event : buffer ������ ���� �̺�Ʈ ������ �����س��� �Լ�
    * get_event[0] : �̺�Ʈ�� ����
    * get_event[1:] : �̺�Ʈ�� ������(��ǥ�� ���е�)
    */
    string tmp = "";
    vector<string> event_data;
    tmp += buffer[0];
    event_data.push_back(tmp);
    string tmp_data(buffer);
    
    tmp_data = tmp_data.substr(1);

    for (string data : split(tmp_data, ',')) {
        event_data.push_back(data);
    }

    return event_data;
}

double* add_pos(double x[], double y[]) {
    double* result = new double[3];

    result[0] = x[0] + y[0];
    result[1] = x[1] + y[1];
    result[2] = x[2] + y[2];

    return result;
}

double* sub_pos(double x[], double y[]) {
    double* result = new double[3];

    result[0] = x[0] - y[0];
    result[1] = x[1] - y[1];
    result[2] = x[2] - y[2];

    return result;
}

double product_pos(double x[], double y[]) {
    return x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
}

double* mul_pos(double c, double x[]) {
    double* result = new double[3];

    result[0] = c * x[0];
    result[1] = c * x[1];
    result[2] = c * x[2];

    return result;
}

double mag_pos(double x[]) {
    return sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
}

int shoot_handler(SOCK_INFO* client, double phi, double theta) {
    int d = (int)1e9;
    int victim = -1;
    double dphi = phi * 180 / M_PI;
    double dtheta = theta * 180 / M_PI;

    double u[3] = { sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta) };
    double origin[3] = { client->u->x, client->u->y, client->u->z };

    string roomname = client->u->roomname;

    for (int i = 0; i < room_list[roomname].user_count; i++) {
        if (room_list[roomname].users[i] == client->u) {
            continue;
        }

        double v[3] = { room_list[roomname].users[i]->x, room_list[roomname].users[i]->y, room_list[roomname].users[i]->z };
        double* p = sub_pos(v, origin);
        if (product_pos(p, u) < 0) {
            continue;
        }

        double* proj = mul_pos(product_pos(p, u), u);

        if (mag_pos(sub_pos(p, proj)) > threshold) {
            continue;
        }

        if (d > mag_pos(proj)) {
            victim = i;
        }
    }

    return victim;
}

void erase_room(string room_name) {
    room_list.erase(room_name);
}

int event_handler(vector<string> data, SOCK_INFO* client) {
    string event_name = data[0];

    if (event_name == "E") {
        /*
        * ������ �뿡 ������ �� �߻��ϴ� �̺�Ʈ(Enter)
        * data : {"E", "<username>", "<roomname>"}
        */
        client->u->username = data[1];
        string tmp_roomname = data[2];

        if (room_list[tmp_roomname].user_count >= MAX_ROOM_MEMBER) {
            cout << "Room " << tmp_roomname << "is already fulled!" << endl;
        }
        else {
            client->u->roomname = tmp_roomname;
            room_list[tmp_roomname].users[room_list[tmp_roomname].user_count] = client->u;
            room_list[tmp_roomname].user_count += 1;
        }
    }
    else if (event_name == "P") {
        /*
        * ������ ��ġ ������ �޴� �̺�Ʈ(Position)
        * data : {"P", "<x>", "<y>", "<z>"}
        */
        client->u->x = stod(data[1]);
        client->u->y = stod(data[2]);
        client->u->z = stod(data[3]);
    }
    else if (event_name == "shoot") {
        double phi = stod(data[1]);
        double theta = stod(data[2]);

        int victim = shoot_handler(client, phi, theta);

        if (victim != -1) {
            cout << "[Kill] " << client->u->username << " kills " << room_list[client->u->roomname].users[victim]->username << endl;

            string v_name = room_list[client->u->roomname].users[victim]->username;
            string k_name = client->u->username;

            send(client->socket, ("W" + v_name).c_str(), v_name.length() + 1, 0);
            send(room_list[client->u->roomname].users[victim]->s->socket, ("L" + k_name).c_str(), k_name.length() + 1, 0);

            for (int i = victim; i < room_list[client->u->roomname].user_count - 1; i++) {
                room_list[client->u->roomname].users[i] = room_list[client->u->roomname].users[i + 1];
            }

            room_list[client->u->roomname].user_count -= 1;

            if (room_list[client->u->roomname].user_count <= 1) {
                send(client->socket, "D", 1, 0);
            }
            else {
                for (int i = 0; i < room_list[client->u->roomname].user_count; i++) {
                    send(client->socket, ("I" + v_name).c_str(), v_name.length() + 1, 0);
                }
            }
        }
    }

    return 0;
}