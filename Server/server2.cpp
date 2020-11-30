#include<iostream>
#include<ctime>
#include<WinSock2.h>
#include<string.h>
#include<cstring>
#include<string>
#include<map>
#include<iomanip>
#include<cmath>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

const int BUFFER_SIZE = 1024;
char SERVER_IP[] = "192.168.43.180";
int SERVER_PORT = 30000;

sockaddr_in serveraddr, clientaddr;

int len_sockaddrin = sizeof(sockaddr_in);
int len_recv, len_send;

char sendBuffer[BUFFER_SIZE], recvBuffer[BUFFER_SIZE];

int main() {
	WORD wVersionRequested = MAKEWORD(2, 0);
	WSADATA wsaData;
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		cout << "failed to load socket libs.." << endl;
	}
	else cout << "load socket libs successfully.." << endl;

	SOCKET ser_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (ser_socket == INVALID_SOCKET) {
		cout << "failed to create a new server socket.." << endl;
	}
	else cout << "create a new server socket successfully.." << endl;

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVER_IP);//转换成二进制字节流
	serveraddr.sin_port = htons(SERVER_PORT); // 16位TCP端口号

	bind(ser_socket, (sockaddr*)&serveraddr, sizeof(sockaddr));

	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr(SERVER_IP);//转换成二进制字节流
	clientaddr.sin_port = htons(SERVER_PORT); // 16位TCP端口号

	while (1) {
		recvfrom(ser_socket, recvBuffer, sizeof(recvBuffer), 0, (sockaddr*)&clientaddr, &len_sockaddrin);
		cout << recvBuffer << endl;
		sendto(ser_socket, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&clientaddr, len_sockaddrin);
	}

	closesocket(ser_socket);
	WSACleanup();
}