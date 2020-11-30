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

	SOCKET cli_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (cli_socket == INVALID_SOCKET) {
		cout << "failed to create a new client socket.." << endl;
	}
	else cout << "create a new client socket successfully.." << endl;

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVER_IP);//转换成二进制字节流
	serveraddr.sin_port = htons(SERVER_PORT); // 16位TCP端口号

	//bind(ser_socket, (sockaddr*)&serveraddr, sizeof(sockaddr));

	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr(SERVER_IP);//转换成二进制字节流
	clientaddr.sin_port = htons(SERVER_PORT); // 16位TCP端口号
	bind(cli_socket, (sockaddr*)&serveraddr, sizeof(sockaddr));

	while (1) {
		strcpy(sendBuffer, "hello world!");
		sendto(cli_socket, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&serveraddr, len_sockaddrin);
		recvfrom(cli_socket, recvBuffer, sizeof(recvBuffer), 0, (sockaddr*)&serveraddr, &len_sockaddrin);
		cout << recvBuffer << endl;
	}

	closesocket(cli_socket);
	WSACleanup();
}