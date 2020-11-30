//Author: Emanual20
//Date: 29/11/2020
#include<iostream>
#include<fstream>
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

// Note: don't know why but BUFFER_SIZE can't be 0xffff
const int BUFFER_SIZE = 0x7fff;
int UDP_MAXSIZE = 0x7fff; // udp max size = 65535 byte
int UDP_HEAD_SIZE = 0x10; // my designed udp head size = 16 byte
#define UDP_DATA_SIZE (UDP_MAXSIZE-UDP_HEAD_SIZE)

// server ip and port number
char SERVER_IP[] = "192.168.43.180";
int SERVER_PORT = 30000;
char CLIENT_IP[] = "192.168.43.180";
int CLIENT_PORT = 1425;

sockaddr_in serveraddr, clientaddr;

int len_sockaddrin = sizeof(sockaddr_in);
int len_recv, len_send;

char sendBuffer[BUFFER_SIZE], recvBuffer[BUFFER_SIZE];
int SEND_LEN = sizeof(sendBuffer);
int RECV_LEN = sizeof(recvBuffer);
ofstream fout;

// fill the port bits with global var 'SERVER_PORT' & 'CLIENT_PORT'
void fill_fmtoports() {
	// fm_port
	sendBuffer[0] = (char)((SERVER_PORT >> 8) % 0x100);
	sendBuffer[1] = (char)(SERVER_PORT % 0x100);
	// to_port
	sendBuffer[2] = (char)((CLIENT_PORT >> 8) % 0x100);
	sendBuffer[3] = (char)(CLIENT_PORT % 0x100);
}

// fill the sendBuffer with var 'length'
int fill_length(int length) {
	// if length > 0xffffffff, there must be something wrong
	if (length >> 16) {
		cout << "error in fill length.. for the length can't longer than 0xffff, but its length is"
			<< length
			<< "..!" << endl;
		return 1;
	}
	sendBuffer[4] = (char)((length >> 8) % 0x100);
	sendBuffer[5] = (char)(length % 0x100);
	return 0;
}

// calc checksum 
u_short cksum(u_short* buf, int count) {
	register u_long sum = 0;
	while (count--) {
		sum += *(buf++);
		// if cksum overflow 16 bits, it will keep its carry-bit
		if (sum & 0xffff0000) {
			sum &= 0xffff;
			sum++;
		}
	}
	return ~(sum & 0xffff);
}

// fill the sendBuffer cksum
int fill_cksum(int count) {
	if (count < 0) {
		return 1;
	}
	u_short x = cksum((u_short*)&sendBuffer[0], count);
	sendBuffer[6] = (char)((x >> 8) % 0x100);
	sendBuffer[7] = (char)(x % 0x100);
	return 0;
}

// fill the sendBuffer file_length;
int fill_flength(int fl) {
	// if fl > 0xffffffff, there must be something wrong
	if (fl >> 16) {
		cout << "error in fill flength.. for the length can't longer than 0xffff, but its length is"
			<< fl
			<< "..!" << endl;
		return 1;
	}
	sendBuffer[12] = (char)((fl >> 8) % 0x100);
	sendBuffer[13] = (char)(fl % 0x100);
	return 0;
}

int read_fitemlength() {
	int ret = 0;
	ret += ((unsigned int)recvBuffer[4]) * 256;
	ret += (unsigned int)recvBuffer[5];
	return ret;
}

int read_fpathlength() {
	int ret = 0;
	ret += ((unsigned int)recvBuffer[12]) * 256;
	ret += (unsigned int)recvBuffer[13];
	return ret;
}

int read_filebit() {
	return recvBuffer[15] & 0x1;
}

void anal_datagram() {
	if (read_filebit()) {
		int flength = read_fpathlength();
		if (flength > 0) {
			string file_name = "";
			for (int i = 0; i < flength; i++) {
				file_name += recvBuffer[UDP_HEAD_SIZE + i];
			}
			fout.open(file_name, ios_base::out | ios_base::app | ios_base::binary);
		}

		int fitemlength = read_fitemlength();
		// write file to disk
		fout.write(&recvBuffer[UDP_HEAD_SIZE + flength], fitemlength);
	}
}

int main() {
	// load libs
	WORD wVersionRequested = MAKEWORD(2, 0);
	WSADATA wsaData;
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		cout << "failed to load socket libs.." << endl;
	}
	else cout << "load socket libs successfully.." << endl;

	// create a datagram socket
	SOCKET ser_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (ser_socket == INVALID_SOCKET) {
		cout << "failed to create a new server socket.." << endl;
	}
	else cout << "create a new server socket successfully.." << endl;

	// bind serveraddr to the datagram socket
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVER_IP);
	serveraddr.sin_port = htons(SERVER_PORT);
	int ercode = bind(ser_socket, (sockaddr*)&serveraddr, sizeof(sockaddr));
	if (ercode != 0) {
		cout << "bind error..!" << endl;
		return -1;
	}
	else cout << "bind success..!" << endl;

	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr(CLIENT_IP);
	clientaddr.sin_port = htons(CLIENT_PORT);

	// recvfrom & sendto
	int tot = 0;
	while (1) {
		recvfrom(ser_socket, recvBuffer, RECV_LEN, 0, (sockaddr*)&clientaddr, &len_sockaddrin);
		anal_datagram();
		_itoa(tot++, sendBuffer, 10);
		sendto(ser_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&clientaddr, len_sockaddrin);
	}

	closesocket(ser_socket);
	WSACleanup();
}