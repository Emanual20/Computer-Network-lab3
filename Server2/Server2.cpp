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

ofstream fdebug("debug.txt");

// Note: don't know why but BUFFER_SIZE can't be 0xffff
const int BUFFER_SIZE = 0x8000;
int UDP_MAXSIZE = 0x8000; // udp max size = 32768 byte
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

// expected seq for rdt2.1
u_short expect_seq = 0;
ofstream fout;

void mydebug() {
	fdebug.write(recvBuffer, BUFFER_SIZE);
}

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

void clear_expectseq() {
	expect_seq = 0;
}

// expect_seq++
void plus_expectseq() {
	expect_seq++;
	expect_seq &= 0xffff;
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

void clear_nakbit() {
	sendBuffer[15] &= 0xf7;
}

void fill_nakbit() {
	sendBuffer[15] |= 0x8;
}

void clear_ackbit() {
	sendBuffer[15] &= 0xfb;
}

void fill_ackbit() {
	sendBuffer[15] |= 0x4;
}

// call all sub-fill func() to fill_udphead
void fill_udphead(int sz) {
	fill_fmtoports();
	if (fill_length(sz)) {
		cout << "error in fill_udphead's fill_length..!" << endl;
	}
	if (fill_cksum(sz)) {
		cout << "error in fill_udphead's fill_cksum..!" << endl;
	}
}

int read_fitemlength() {
	int ret = 0;
	ret += (((unsigned short)recvBuffer[4]) % 0x100) * 256;
	ret += ((unsigned short)recvBuffer[5] % 0x100);
	return ret;
}

// via the checksum of recvBuffer to check if the datagram is corrupted
bool is_corrupt() {
	int l = read_fitemlength();
	u_short now_cksum = cksum((u_short*)&sendBuffer[0], l / 2);
	cout << now_cksum << endl;
	return now_cksum != 0xffff;
}

// note: unsigned short must % 0x100
u_short read_checksum() {
	u_short ret = 0;
	ret += (((unsigned short)recvBuffer[6]) % 0x100) * 256;
	ret += ((unsigned short)recvBuffer[7] % 0x100);
	return ret;
}

// note: unsigned short must % 0x100
u_short read_seq() {
	u_short ret = 0;
	ret += (((unsigned short)recvBuffer[8]) % 0x100) * 256;
	ret += ((unsigned short)recvBuffer[9] % 0x100);
	return ret;
}

// via read_seq to judge if is_seq
bool is_seq() {
	return expect_seq == read_seq();
}

int read_fpathlength() {
	int ret = 0;
	ret += ((unsigned int)recvBuffer[12]) * 256;
	ret += (unsigned int)recvBuffer[13];
	return ret;
}

int read_fileendbit() {
	return recvBuffer[15] & 0x2;
}

int read_filebit() {
	return recvBuffer[15] & 0x1;
}

// to analyze the datagram
void anal_datagram() {
	// if the datagram's type is a file
	if (read_filebit()) {
		int flength = read_fpathlength();
		if (flength > 0) {
			string file_name = "";
			for (int i = 0; i < flength; i++) {
				file_name += recvBuffer[UDP_HEAD_SIZE + i];
			}
			cout << "stated receiving " << file_name << "..!" << endl;
			fout.open(file_name, ios_base::out | ios_base::app | ios_base::binary);
		}

		int fitemlength = read_fitemlength();
		//cout << fitemlength << endl;
		fitemlength -= (flength + UDP_HEAD_SIZE);
		// write file to disk
		fout.write(&recvBuffer[UDP_HEAD_SIZE + flength], fitemlength);
		
		if (read_fileendbit()) {
			fout.close();
			clear_expectseq();
		}
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

		bool IsCorrupted = is_corrupt();
		while (IsCorrupted) {
			// send a NAK datagram
			fill_nakbit();
			fill_udphead(UDP_HEAD_SIZE);
			sendto(ser_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&clientaddr, len_sockaddrin);
			memset(sendBuffer, 0, sizeof(sendBuffer));

			// recieve a retransmission(maybe redundant) datagram
			recvfrom(ser_socket, recvBuffer, RECV_LEN, 0, (sockaddr*)&clientaddr, &len_sockaddrin);
			IsCorrupted = is_corrupt();
		}

		// send a ACK datagram (has not finish)
		fill_ackbit();
		fill_udphead(UDP_HEAD_SIZE);
		sendto(ser_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&clientaddr, len_sockaddrin);
		memset(sendBuffer, 0, sizeof(sendBuffer));

		// if the seq not fits
		if (!is_seq()) {
			continue;
		}
		
		// if the seq fits, analyze the datagram
		plus_expectseq();
		anal_datagram();
	}

	closesocket(ser_socket);
	WSACleanup();
}