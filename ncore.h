#pragma once

#include "json/json.h"

#include <hash_map>
#include <math.h>
#include <WinSock2.h>
#include <string>

#include <stdio.h>
#include <iostream>

using namespace std;

#pragma comment(lib, "json/jsoncpp.lib")

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
    struct user_table*u;
    char ip[20];
} SOCK_INFO;

typedef struct user_table {
    double x;
    double y;
    double z;
    string username;
    string roomname;
    struct sock_info*s;
} USER_TABLE;

typedef struct room_table {
    int user_count;
    USER_TABLE *users[MAX_ROOM_MEMBER];
} ROOM_TABLE;

hash_map<string, ROOM_TABLE> room_list;
int threshold = 1;

Json::Value json_from_buffer(char* buffer)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(buffer, root)) {
        printf("jSON parse error\n");
        return Json::nullValue;
    }

    return root;
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
    int d = 1e9;
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

            cout << "shoot by " << client->u->username << " to " << room_list[roomname].users[victim]->username << endl;

            for (int j = i + 1; j < room_list[roomname].user_count; j++)
            {
                room_list[roomname].users[j - 1] = room_list[roomname].users[j];
            }
            room_list[roomname].user_count -= 1;
            break;
        }
    }

    return victim;
}

void erase_room(string room_name) {
    room_list.erase(room_name);
}

int event_handler(Json::Value data, SOCK_INFO* client) {
    string event_name;
    Json::Value event_value;

    event_name = data["event"].asString();
    event_value = data["value"];
    
    cout << "Event (" << event_name << ") is happened!" << endl;

    if (event_name == "enter") {
        client->u->username = event_value["username"].asString();
        string tmp_roomname = event_value["roomname"].asString();
        
        if (room_list[tmp_roomname].user_count >= MAX_ROOM_MEMBER) {
            cout << "Room " << tmp_roomname << "is already fulled!" << endl;
        }
        else {
            client->u->roomname = tmp_roomname;
            room_list[tmp_roomname].users[room_list[tmp_roomname].user_count] = client->u;
            room_list[tmp_roomname].user_count += 1;
        }
    }
    else if (event_name == "position") {
        client->u->x = event_value["x"].asDouble();
        client->u->y = event_value["y"].asDouble();
        client->u->z = event_value["z"].asDouble();
    }
    else if (event_name == "shoot") {
        double phi = event_value["phi"].asDouble();
        double theta = event_value["theta"].asDouble();

        int victim = shoot_handler(client, phi, theta);

        if (victim != -1) {
            Json::Value response;
            Json::Value value;

            response["event"] = "result";
            value["username"] = client->u->username;
            value["victim"] = room_list[client->u->roomname].users[victim]->username;
            response["value"] = value;

            string response_string = response.toStyledString();

            for (int i = 0; i < room_list[client->u->roomname].user_count; i++) {
                send(room_list[client->u->roomname].users[i]->s->socket, response_string.c_str(), response_string.size(), 0);
            }
        }

    }
    else if (event_name == "disconnect") {
        bool flag = false;
        for (int i = 0; i < room_list[client->u->roomname].user_count; i++) {
            if (client == room_list[client->u->roomname].users[i]->s) {
                room_list[client->u->roomname].users[i]->x = 0;
                room_list[client->u->roomname].users[i]->y = 0;
                room_list[client->u->roomname].users[i]->z = 0;
                room_list[client->u->roomname].users[i]->roomname = "";
                room_list[client->u->roomname].users[i]->username = "";

                flag = true;
            }

            if (flag) {
                if (i == room_list[client->u->roomname].user_count - 1) break;

                room_list[client->u->roomname].users[i] = room_list[client->u->roomname].users[i + 1];
            }
        }

        room_list[client->u->roomname].user_count -= 1;
        
        if (room_list[client->u->roomname].user_count == 0) {
            erase_room(client->u->roomname);
        }
        else {
            room_list[client->u->roomname].users[room_list[client->u->roomname].user_count] = NULL;
        }

        Json::Value response;
        Json::Value value;

        response["event"] = "disconnect";
        value["username"] = client->u->username;
        response["value"] = value;

        string response_string = response.toStyledString();

        for (int i = 0; i < room_list[client->u->roomname].user_count; i++) {
            send(room_list[client->u->roomname].users[i]->s->socket, response_string.c_str(), response_string.size(), 0);
        }
    }

    return 0;
}