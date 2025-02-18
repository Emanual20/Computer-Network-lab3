//Author: Emanual20
//Date: 10/12/2020
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
#include<mutex>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

//ofstream fdebug("debug3-3.txt");

// Note: don't know why but BUFFER_SIZE can't be 0xffff
const int BUFFER_SIZE = 0x3A90;
const int UDP_MAXSIZE = 0x3A90; // udp max size = 32768 byte
const int UDP_HEAD_SIZE = 0x10; // my designed udp head size = 16 byte
#define UDP_DATA_SIZE (UDP_MAXSIZE-UDP_HEAD_SIZE)
const int RTO_TIME = 1000; // the unit of RTO_TIME is ms
const int MAX_SEQ = 0x10000; // the valid seq shall keep in

// server ip and port number can be modified in "main"
char ROUTER_IP[] = "127.0.0.1";
int ROUTER_PORT = 14250;
char SERVER_IP[] = "192.168.43.180";
int SERVER_PORT = 30000;
char CLIENT_IP[] = "192.168.43.180";
//int CLIENT_PORT = 1425;
int CLIENT_PORT = ROUTER_PORT;
char reserved_IP[] = "127.0.0.1";

sockaddr_in serveraddr, clientaddr;

int len_sockaddrin = sizeof(sockaddr_in);
int len_recv, len_send;

char sendBuffer[BUFFER_SIZE], recvBuffer[BUFFER_SIZE];
int SEND_LEN = sizeof(sendBuffer);
int RECV_LEN = sizeof(recvBuffer);

// expected seq for rdt2.1
u_short expect_seq = 1;
ofstream fout;

// timer for tcp partial ACK
time_t t_start, t_end;
HANDLE sender_handle;
const int SENDER_SLEEP_INTERVAL = 20;
const int SENDER_SEND_INTERVAL = 100;

class Timer {
private:
	bool istimeout;
public:
	Timer() {
		this->istimeout = false;
	}

	void clear_timeout() {
		this->istimeout = false;
	}

	void set_timeout() {
		this->istimeout = true;
	}

	bool is_timeout() {
		return this->istimeout;
	}
};

class ACKSendManager {
private:
	bool isneedtosend;
public:
	ACKSendManager() {
		this->isneedtosend = false;
	}
	void set_isneedtosend() {
		this->isneedtosend = true;
	}
	void clear_isneedtosend() {
		this->isneedtosend = false;
	}
	bool is_need_send(){
		return this->isneedtosend;
	}
};

Timer timer;
ACKSendManager ACKSendmanager;

// to judge whether a ip user entered is valid 
bool is_ipvalid(string ip) {
	int l = ip.length();
	int res = 0, dot_cnt = 0;
	for (int i = 0; i < l; i++) {
		if (isdigit(ip[i])) {
			res *= 10;
			res += (ip[i] - '0');
		}
		else if (ip[i] == '.') {
			dot_cnt++;
			if (!(0 <= res && res <= 255)) return false;
			res = 0;
		}
		else return false;
	}
	if (!(0 <= res && res <= 255)) return false;
	return dot_cnt == 3;
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
	expect_seq = 1;
}

// expect_seq++
void plus_expectseq() {
	expect_seq++;
	expect_seq %= MAX_SEQ;
}

// fill the ack datagram's seq
int fill_seq(int ex_seq) {
	if (ex_seq > 0xffff) {
		return 1;
	}
	sendBuffer[8] = (char)((ex_seq >> 8) & 0xff);
	sendBuffer[9] = (char)(ex_seq & 0xff);
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

int read_fpathlength() {
	int ret = 0;
	ret += ((unsigned int)recvBuffer[12]) * 256;
	ret += (unsigned int)recvBuffer[13];
	return ret;
}

int read_synbit() {
	return recvBuffer[15] & 0x10;
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
		}
	}
}

/*
	Create a sender to send ACKs
*/
DWORD WINAPI ACKsender(LPVOID param) {
	SOCKET Ser_socket = (SOCKET)(LPVOID)param;
	mutex mtx;
	while (1) {
		mtx.lock();
		if (ACKSendmanager.is_need_send() && timer.is_timeout()) {
			// send a ACK datagram
			fill_ackbit();
			fill_seq(expect_seq - 1); // fill the ACK datagram's seq for GBN
			fill_udphead(UDP_HEAD_SIZE);
			cout << "send ACK" << expect_seq - 1 << endl;
			sendto(Ser_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&clientaddr, len_sockaddrin);
			memset(sendBuffer, 0, sizeof(sendBuffer));

			ACKSendmanager.clear_isneedtosend();
			timer.clear_timeout();
		}
		mtx.unlock();
		mtx.lock();
		t_end = clock();
		mtx.unlock();
		mtx.lock();
		if (t_end - t_start > SENDER_SEND_INTERVAL) {
			timer.set_timeout();
		}
		mtx.unlock();
		Sleep(SENDER_SLEEP_INTERVAL);
	}
}

int main() {
	ios::sync_with_stdio(false);
	cin.tie(0);
	cout.tie(0);

	// SET SERVER_IP & CLIENT_IP
	string sip, cip;
	cout << "please input your server IP, notvalid IP or input 0 will be set as 127.0.0.1 !" << endl;
	cin >> sip;
	if (is_ipvalid(sip)) {
		cout << "server ip is set as: " << sip << endl;
		strcpy(SERVER_IP, sip.c_str());
	}
	else {
		cout << "server ip is set as: " << reserved_IP << endl;
		strcpy(SERVER_IP, reserved_IP);
	}

	cout << "please input your client IP, notvalid IP or input 0 will be set as 127.0.0.1 !" << endl;
	cin >> cip;
	if (is_ipvalid(cip)) {
		cout << "client ip is set as: " << cip << endl;
		strcpy(CLIENT_IP, cip.c_str());
	}
	else {
		cout << "client ip is set as: " << reserved_IP << endl;
		strcpy(CLIENT_IP, reserved_IP);
	}

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

	int tot = 0;
	// create a sendto thread
	sender_handle = CreateThread(NULL, NULL, ACKsender, LPVOID(ser_socket), 0, 0);
	// recv from in main thread
	mutex mtx;
	while (1) {
		recvfrom(ser_socket, recvBuffer, RECV_LEN, 0, (sockaddr*)&clientaddr, &len_sockaddrin);

		// check the checksum
		bool IsCorrupted = is_corrupt();
		if (IsCorrupted) {
			cout << "the datagram has corrupted, will be abandoned..!" << endl;
			continue;
		}

		// if the datagram is a syn
		if (read_synbit()) {
			cout << "retrieve a syn datagram to request connection..!" << endl;
			fill_ackbit();
			fill_seq(0);
			fill_udphead(UDP_HEAD_SIZE);
			sendto(ser_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&clientaddr, len_sockaddrin);
			memset(sendBuffer, 0, sizeof(sendBuffer));
			continue;
		}

		// check the seq, if the seq fits, analyze the datagram
		cout << "expected seq: " << expect_seq << " ; received seq: " << read_seq() << endl;
		bool is_seq_match = read_seq() == expect_seq;
		if (is_seq_match) {
			cout << "accept" << endl;
			plus_expectseq();

			mtx.lock();
			timer.clear_timeout();
			t_start = clock();
			ACKSendmanager.set_isneedtosend();
			mtx.unlock();

			anal_datagram();
		}

		// if the file transmission finish, clear the seq status
		if (is_seq_match && read_fileendbit()) {
			Sleep((SENDER_SEND_INTERVAL + SENDER_SLEEP_INTERVAL)*1.5);

			clear_expectseq();
			mtx.lock();
			timer.clear_timeout();
			ACKSendmanager.clear_isneedtosend();
			mtx.unlock();
			cout << "receive successful..!" << endl;
		}
	}

	CloseHandle(sender_handle);
	closesocket(ser_socket);
	WSACleanup();
}