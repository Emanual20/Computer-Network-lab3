//Author: Emanual20
//Date: 10/12/2020
#include<iostream>
#include<fstream>
#include<ctime>
#include<WinSock2.h>
#include<windows.h>
#include<string.h>
#include<cstring>
#include<string>
#include<map>
#include<iomanip>
#include<cmath>
#include<vector>
#include<mutex>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

//ofstream fdebug("debug3-3.txt");

// Note: don't know why but BUFFER_SIZE can't be 0xffff
const int BUFFER_SIZE = 0x5d0;
const int UDP_MAXSIZE = 0x5d0; // udp max size = 32768 byte
const int UDP_HEAD_SIZE = 0x10; // my designed udp head size = 16 byte
#define UDP_DATA_SIZE (UDP_MAXSIZE-UDP_HEAD_SIZE)
const int RTO_TIME = 1000; // the unit of RTO_TIME is ms
const int MAX_SEQ = 0x10000; // the valid seq shall keep in
const int DEFAULT_WINDOW_SIZE = 5;

// server ip and port number
char ROUTER_IP[] = "127.0.0.1";
int ROUTER_PORT = 14250;
char SERVER_IP[] = "192.168.43.180";
//int SERVER_PORT = 30000;
int SERVER_PORT = ROUTER_PORT;
char CLIENT_IP[] = "192.168.43.180";
int CLIENT_PORT = 1425;
char reserved_IP[] = "127.0.0.1";

sockaddr_in serveraddr, clientaddr;

int len_sockaddrin = sizeof(sockaddr_in);
int len_recv, len_send;

char sendBuffer[BUFFER_SIZE], recvBuffer[BUFFER_SIZE];
int SEND_LEN = sizeof(sendBuffer);
int RECV_LEN = sizeof(recvBuffer);

// sent seq for rdt2.1
u_short sent_seq = 1;

// timer for rdt3.0
int t_start, t_end;
int u_rto;
const int MINIMUM_RTO_TIME_LENGTH = 200;
const int DEFAULT_RTO_TIME_LENGTH = 1500;

// params for GBN
u_short nextseqnum = 1, base = 1;
double WINDOW_SIZE = DEFAULT_WINDOW_SIZE;
vector<string> dg_vec;
bool is_timeout = 0;
HANDLE timer_handle;
HANDLE recvfm_handle;

// params for RENO
enum CongestStatus {
	SLOW_START,
	AVOID_CONGEST,
	QUICK_RECOVERY
};
CongestStatus Congest_Status = SLOW_START;
double &cwnd = WINDOW_SIZE; // unit is MSS
double ssthresh; // unit is MSS
map<int, int> dup_cnt;
const double RENOINIT_CGWD_SIZE = 1;
const double RENOINIT_SSTHRESH_SIZE = 40;
const int RENO_MAXIMUM_DUMPLICATE_TIME = 3 + 1; // '+1' is cuz A new ACK will record its times as 1.
bool is_dup = 0;

// for debug
//void mydebug() {
//	fdebug.write(sendBuffer, BUFFER_SIZE);
//}

// to init_reno
void init_reno() {
	Congest_Status = SLOW_START;
	cwnd = RENOINIT_CGWD_SIZE;
	ssthresh = RENOINIT_SSTHRESH_SIZE;
	dup_cnt.clear();
	is_dup = false;
}

// to judge if an ACK is a dumplicate ACK
bool Is_Duplicate_ACK(int seq) {
	map<int, int>::iterator it = dup_cnt.find(seq);
	return it != dup_cnt.end();
}

void setisdup() {
	is_dup = true;
}

void clearisdup() {
	is_dup = false;
}

// SlowStartHandlers
void SlowStartDupACKHandler(int seq) {
	if (!Is_Duplicate_ACK(seq)) {
		cout << "seq " << seq << " is not a dup ACK! ABORT!" << endl;
		return;
	}
	dup_cnt[seq]++;
	if (dup_cnt[seq] >= RENO_MAXIMUM_DUMPLICATE_TIME) {
		ssthresh = cwnd / 2;
		cwnd = ssthresh + 3;
		is_dup = true; // need retransmission in the main func

		Congest_Status = QUICK_RECOVERY;
		cout << "the congest status has changed from SLOW_START to QUICK_RECOVERY.." << endl;
	}
}

void SlowStartNewACKHandler(int seq) {
	if (Is_Duplicate_ACK(seq)) {
		cout << "seq " << seq << " is not a new ACK! ABORT!" << endl;
		return;
	}
	// if accept an new ACK, cwnd = cwnd + MSS
	cwnd = cwnd + 1;
	dup_cnt.clear();
	dup_cnt[seq] = 1;

	if (cwnd >= ssthresh) {
		Congest_Status = AVOID_CONGEST;
		cout << "the congest status has changed from SLOW_START to AVOID_CONGEST.." << endl;
	}
}

void SlowStartTimeoutHandler() {
	ssthresh = cwnd / 2;
	cwnd = 1;
	dup_cnt.clear();
	is_dup = false;
	// need retransmission in main func
}

// QuickRecoveryHandlers
void QuickRecoveryDupACKHandler(int seq) {
	if (!Is_Duplicate_ACK(seq)) {
		cout << "seq " << seq << " is not a dup ACK! ABORT!" << endl;
		return;
	}
	cwnd = cwnd + 1;
	// need retransmission in main func
}

void QuickRecoveryNewACKHandler(int seq) {
	if (Is_Duplicate_ACK(seq)) {
		cout << "seq " << seq << " is not a new ACK! ABORT!" << endl;
		return;
	}
	cwnd = ssthresh;
	dup_cnt.clear();
	is_dup = false;
	dup_cnt[seq] = 1;

	Congest_Status = AVOID_CONGEST;
	cout << "the congest status has changed from QUICK_RECOVERY to AVOID_CONGEST.." << endl;
}

void QuickRecoveryTimeoutHandler() {
	ssthresh = cwnd / 2;
	cwnd = 1;
	dup_cnt.clear();
	is_dup = false;

	Congest_Status = SLOW_START;
	cout << "the congest status has changed from QUICK_RECOVERY to SLOW_START.." << endl;
	// need retransmission in main func
}

// AvoidCongestHandlers
void AvoidCongestDupACKHandler(int seq) {
	if (!Is_Duplicate_ACK(seq)) {
		cout << "seq " << seq << " is not a dup ACK! ABORT!" << endl;
		return;
	}
	dup_cnt[seq]++;
	if (dup_cnt[seq] >= RENO_MAXIMUM_DUMPLICATE_TIME) {
		ssthresh = cwnd / 2;
		cwnd = ssthresh + 3;

		Congest_Status = QUICK_RECOVERY;
		cout << "the congest status has changed from AVOID_CONGEST to QUICK_RECOVERY.." << endl;
	}
	// need retransmission in main func
}

void AvoidCongestNewACKHandler(int seq) {
	if (Is_Duplicate_ACK(seq)) {
		cout << "seq " << seq << " is not a new ACK! ABORT!" << endl;
		return;
	}
	// NOTE: TODO: fix this part into cwnd = cwnd + MSS * MSS / CWND
	double mss = 1;
	cwnd = cwnd + mss * mss / cwnd;
	dup_cnt.clear();
	is_dup = false;
	dup_cnt[seq] = 1;
}

void AvoidCongestTimeoutHandler() {
	ssthresh = cwnd / 2;
	cwnd = 1;
	dup_cnt.clear();
	is_dup = false;

	Congest_Status = SLOW_START;
	cout << "the congest status has changed from AVOID_CONGEST to SLOW_START.." << endl;
	// need retransmission in main func
}

// public handler function for outer use
void DupACKHandler(int seq) {
	switch (Congest_Status) {
	case SLOW_START: {
		return SlowStartDupACKHandler(seq);
	}
	case AVOID_CONGEST: {
		return AvoidCongestDupACKHandler(seq);
	}
	case QUICK_RECOVERY: {
		return QuickRecoveryDupACKHandler(seq);
	}
	default: {
		cout << "default error in DupACKHandler..!" << endl;
		return;
	}
	}
}

void NewACKHandler(int seq) {
	switch (Congest_Status) {
	case SLOW_START: {
		return SlowStartNewACKHandler(seq);
	}
	case AVOID_CONGEST: {
		return AvoidCongestNewACKHandler(seq);
	}
	case QUICK_RECOVERY: {
		return QuickRecoveryNewACKHandler(seq);
	}
	default: {
		cout << "default error in NewACKHandler..!" << endl;
		return;
	}
	}
}

void TimeoutHandler() {
	switch (Congest_Status) {
	case SLOW_START: {
		return SlowStartTimeoutHandler();
	}
	case AVOID_CONGEST: {
		return AvoidCongestTimeoutHandler();
	}
	case QUICK_RECOVERY: {
		return QuickRecoveryTimeoutHandler();
	}
	default: {
		cout << "default error in TimeoutHandler..!" << endl;
		return;
	}
	}
}

// to init dg_vec
void init_dgvec() {
	dg_vec.clear();
	string takeplace = "#";
	dg_vec.push_back(takeplace);
}

// clear all the status
void clear_status() {
	init_dgvec();
	nextseqnum = base = 1;
}

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

// to judge whether a rto user entered is valid
bool is_rtovalid(int rto) {
	return rto >= MINIMUM_RTO_TIME_LENGTH;
}

// fill the port bits with global var 'SERVER_PORT' & 'CLIENT_PORT'
void fill_fmtoports() {
	// fm_port
	sendBuffer[0] = (char)((CLIENT_PORT >> 8) % 0x100);
	sendBuffer[1] = (char)(CLIENT_PORT % 0x100);
	// to_port
	sendBuffer[2] = (char)((SERVER_PORT >> 8) % 0x100);
	sendBuffer[3] = (char)(SERVER_PORT % 0x100);
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
	sendBuffer[5] = (char)((length & 0xff) % 0x100);
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
	sendBuffer[6] = sendBuffer[7] = 0; // clear checksum bits
	u_short x = cksum((u_short*)&sendBuffer[0], count);
	sendBuffer[6] = (char)((x >> 8) % 0x100);
	sendBuffer[7] = (char)(x % 0x100);
	return 0;
}

void clear_sentseq() {
	sent_seq = 1;
}

// sent_seq++
void plus_sentseq() {
	sent_seq++;
	sent_seq %= MAX_SEQ;
}

// fill the sent-seq for rdt2.1
int fill_seq(int e_seq) {
	if (e_seq > 0xffff) {
		return 1;
	}
	sendBuffer[8] = (char)((e_seq >> 8) & 0xff);
	sendBuffer[9] = (char)(e_seq & 0xff);
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

void clear_synbit() {
	sendBuffer[15] &= 0xef;
}

void fill_synbit() {
	sendBuffer[15] |= 0x10;
}

void clear_fileendbit() {
	sendBuffer[15] &= 0xfd;
}

void fill_fileendbit() {
	sendBuffer[15] |= 0x2;
}

void fill_filebit() {
	sendBuffer[15] |= 0x1;
}

// call all sub-fill func() to fill_udphead
void fill_udphead(int sz, int seq) {
	fill_fmtoports();
	if (fill_length(sz)) {
		cout << "error in fill_udphead's fill_length..!" << endl;
	}
	if (fill_cksum(sz)) {
		cout << "error in fill_udphead's fill_cksum..!" << endl;
	}
	if (fill_seq(seq)) {
		cout << "error in fill_udphead's fill_seq..!" << endl;
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
	//cout << now_cksum << endl;
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

int read_fpathlength(char* buffer_ptr) {
	int ret = 0;
	ret += ((unsigned int)buffer_ptr[12]) * 256;
	ret += (unsigned int)buffer_ptr[13];
	return ret;
}

int read_nakbit() {
	return recvBuffer[15] & 0x8;
}

int read_ackbit() {
	return recvBuffer[15] & 0x4;
}

int read_filebit() {
	return recvBuffer[15] & 0x1;
}

void set_istimeout() {
	is_timeout = 1;
}

void clear_istimeout() {
	is_timeout = 0;
}

/*
  Create a timer to test RTO
*/
DWORD WINAPI myTimer(LPVOID param) {
	int limit = (int)(LPVOID)param;
	int st = clock(), et;
	mutex mtx;
	// clear the global is_timeout bit
	clear_istimeout();

	while (1) {
		et = clock();
		if (et - st > limit) {
			cout << "time is up...!" << endl;
			mtx.lock();
			set_istimeout();
			mtx.unlock();
			return 1;
		}
		Sleep(200);
	}
	return 0;
}

/*
  Create a thread to recvfrom ack from server.
*/
DWORD WINAPI handlerACK(LPVOID param) {
	SOCKET Cli_Socket = (SOCKET)(LPVOID)param;
	mutex mtx;
	while (1) {
		recvfrom(Cli_Socket, recvBuffer, RECV_LEN, 0, (sockaddr*)&serveraddr, &len_sockaddrin);
		if (!is_corrupt()) {
			mtx.lock();
			int received_ack = read_seq();
			base = received_ack + 1;
			cout << "base received " << received_ack << endl;

			// reno handler
			if (Is_Duplicate_ACK(received_ack)) {
				DupACKHandler(received_ack);
			}
			else {
				NewACKHandler(received_ack);
			}

			if (base == nextseqnum) {
				// close timer
				TerminateThread(timer_handle, 0);
				CloseHandle(timer_handle);
			}
			else {
				// restart timer!

				// (1) close timer
				TerminateThread(timer_handle, 0);
				CloseHandle(timer_handle);
				// (2) start timer
				timer_handle = CreateThread(NULL, NULL, myTimer, LPVOID(u_rto), 0, 0);
			}
			mtx.unlock();
		}
		else {
			cout << "our ack" << read_seq() <<
				" datagram is corrupted..!" << endl;
		}
	}
}

int main() {
	// SET SERVER_IP & CLIENT_IP
	string sip, cip;
	{
		cout << "please input your server IP, notvalid IP or input 0 will be set as 127.0.0.1 !" << endl;
		cin >> sip;
		if (is_ipvalid(sip)) {
			cout << "server ip is set as: " << sip << endl;
			strcpy(SERVER_IP, sip.c_str());
		}
		else {
			cout << "server ip be set as: " << reserved_IP << endl;
			strcpy(SERVER_IP, reserved_IP);
		}

		cout << "please input your client IP, notvalid IP or input 0 will be set as 127.0.0.1 !" << endl;
		cin >> cip;
		if (is_ipvalid(cip)) {
			cout << "client ip is set as: " << cip << endl;
			strcpy(CLIENT_IP, cip.c_str());
		}
		else {
			cout << "client ip be set as: " << reserved_IP << endl;
			strcpy(CLIENT_IP, reserved_IP);
		}
	}

	// set the timer's RTO for rdt3.0
	// note: the timeval type used in setsocketopt will be read as milliseconds
	cout << "please input the timer's RTO (the unit is ms, at least will be 200ms):" << endl;
	cin >> u_rto;
	if (is_rtovalid) {
		cout << "RTO is set as: " << u_rto << " ms" << endl;
	}
	else {
		u_rto = DEFAULT_RTO_TIME_LENGTH;
		cout << "input invalid.. RTO is set as DEFAULT_RTO_TIME_LENGTH: " << u_rto << endl;
	}

	// set the windowsize for GBN
	// note: if GBN's windowsize = 1, it will become stop-wait rdt3.0
	//cout << "please input your GBN's windowsize(it must > 0):" << endl;
	//int windowsize;
	//cin >> windowsize;
	//if (windowsize > 0) {
	//	WINDOW_SIZE = windowsize;
	//}
	//else {
	//	WINDOW_SIZE = DEFAULT_WINDOW_SIZE;
	//}
	cout << "WINDOW_SIZE will be set by renoinit() before a file send.." << endl;

	// load libs
	WORD wVersionRequested = MAKEWORD(2, 0);
	WSADATA wsaData;
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		cout << "failed to load socket libs.." << endl;
	}
	else cout << "load socket libs successfully.." << endl;

	// create a datagram socket
	SOCKET cli_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (cli_socket == INVALID_SOCKET) {
		cout << "failed to create a new client socket.." << endl;
	}
	else cout << "create a new client socket successfully.." << endl;

	// bind serveraddr to the datagram socket
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVER_IP);
	serveraddr.sin_port = htons(SERVER_PORT);

	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr(CLIENT_IP);
	clientaddr.sin_port = htons(CLIENT_PORT);
	int ercode = bind(cli_socket, (sockaddr*)&clientaddr, sizeof(sockaddr));
	if (ercode != 0) {
		cout << "bind error..!" << endl;
		return -1;
	}
	else cout << "bind success..!" << endl;

	// recvfrom & sendto
	string option;
	while (1) {
		cout << endl << "PLEASE INPUT YOUR OPTION:" << endl;
		cout << "text;<text info>  file;<file path>  exit; reset;" << endl;
		cout << "for example, text;hello ,  file;1.jpg , reset;" << endl << endl;

		cin >> option;
		if (option.substr(0, 4) == "text") {
			//string text_to_send = option.substr(5, option.length() - 5);
			//strcpy(&sendBuffer[UDP_HEAD_SIZE], text_to_send.c_str());

			//// miss fill_udphead
			//sendto(cli_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&serveraddr, len_sockaddrin);
			//memset(sendBuffer, 0, sizeof(sendBuffer));

			//recvfrom(cli_socket, recvBuffer, RECV_LEN, 0, (sockaddr*)&serveraddr, &len_sockaddrin);
			//cout << recvBuffer << endl;
			cout << "sorry, this option is not available..!" << endl;
		}
		else if (option.substr(0, 4) == "file") {
			string file_path = option.substr(5, option.length() - 5);
			int file_length = file_path.length();
			ifstream fin(file_path, ios_base::in | ios_base::binary);
			// move the ifstream pointer to the end of the binary file
			fin.seekg(0, ios::end);

			int len = fin.tellg();
			if (len <= 0) {
				cout << "failed to open the file, please check..!" << endl;
				continue;
			}

			// send a syn datagram to build connection..
			fill_udphead(UDP_HEAD_SIZE, 0);
			fill_synbit();
			sendto(cli_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&serveraddr, len_sockaddrin);
			memset(sendBuffer, 0, sizeof(sendBuffer));

			if (int it = recvfrom(cli_socket, recvBuffer, RECV_LEN, 0, (sockaddr*)&serveraddr, &len_sockaddrin) <= 0) {
				if (it == 0) {
					cout << "we receive an empty ack..?!" << endl;
				}
				int errorcode = WSAGetLastError();
				if (errorcode == 10060) {
					cout << "over RTO, server didn't respond us.. transmission corrupt..!" << endl;
					continue;
				}
			}
			else {
				if (read_ackbit()) {
					cout << "build connection successful..! we will begin to send your file..!" << endl;
				}
				else {
					cout << "we didn't receive server's ack.. transmission interrupt..!" << endl;
					continue;
				}
			}

			// prepare to begin to send files
			int send_times = ceil((len * 1.0) / UDP_DATA_SIZE);
			cout << "for this file, we will send " << send_times << " times..!" << endl;

			// save the datagrams
			init_dgvec();
			int tot_read_size = 0; // bytes have been read
			int reserved_size = 0;
			int this_written_size = 0;
			for (int i = 0; i < send_times; i++) {
				reserved_size = UDP_DATA_SIZE;
				this_written_size = 0;

				// if it's the first piece, it shall send the file's name
				if (file_length > 0) {
					fill_flength(file_length);
					reserved_size -= file_length;
					this_written_size += file_length;
					// write file name into datagram
					for (int j = 0; j < file_length; j++) {
						sendBuffer[UDP_HEAD_SIZE + j] = file_path[j];
					}
					file_length = 0;
				}
				else {
					fill_flength(0);
				}

				fin.seekg(tot_read_size, ios::beg);

				// determine optimal file size to send
				int sendsize = min(reserved_size, len - tot_read_size);

				// read from file
				fin.read(&sendBuffer[UDP_HEAD_SIZE + (UDP_DATA_SIZE - reserved_size)], sendsize);
				tot_read_size += sendsize;
				this_written_size += sendsize;

				// fill the udphead
				fill_udphead(UDP_HEAD_SIZE + this_written_size, sent_seq);

				if (i == send_times - 1) fill_fileendbit();
				fill_filebit();

				// push the datagram into dg_vec
				string temp_str = "";
				for (int j = 0; j < UDP_MAXSIZE; j++) {
					temp_str += sendBuffer[j];
				}
				memset(sendBuffer, 0, sizeof(sendBuffer));
				dg_vec.push_back(temp_str);
			}
			fin.close();
			cout << "calc datagrams complete..!" << endl;

			// init reno params and funcs
			init_reno();

			// open sub-thread to recvfrom ack
			recvfm_handle = CreateThread(NULL, NULL, handlerACK, LPVOID(cli_socket), 0, 0);
			mutex mtx;
			int t_start = clock();
			while (1) {

				// if window is available, transmission package which seqnum is var<nextseqnum>
				mtx.lock();
				if (nextseqnum < base + WINDOW_SIZE && nextseqnum <= send_times) {
					for (int j = 0; j < dg_vec[nextseqnum].length(); j++) {
						sendBuffer[j] = dg_vec[nextseqnum][j];
					}
					fill_seq(nextseqnum);
					//cout << nextseqnum << " " << read_fpathlength(sendBuffer) << endl;
					sendto(cli_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&serveraddr, len_sockaddrin);
					memset(sendBuffer, 0, sizeof(sendBuffer));
					if (base == nextseqnum) {
						timer_handle = CreateThread(NULL, NULL, myTimer, LPVOID(u_rto), 0, 0);
					}
					nextseqnum++;
					mtx.unlock();
				}
				else {
					mtx.unlock();
					cout << "nextseqnum beyond the window, will try later.." << endl;
					// NOTE: later will be annotation
					Sleep(300);
				}

				// NOTE: TODO: maybe need another thread
				// if is_dup, retransmission all the packages which haven't received ACKs
				if (is_dup) {
					mtx.lock();
					for (int i = base; i < nextseqnum; i++) {
						for (int j = 0; j < dg_vec[i].length(); j++) {
							sendBuffer[j] = dg_vec[i][j];
						}
						fill_seq(i);
						sendto(cli_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&serveraddr, len_sockaddrin);
						memset(sendBuffer, 0, sizeof(sendBuffer));
					}
					TerminateThread(timer_handle, 0);
					CloseHandle(timer_handle);
					clear_istimeout();
					mtx.unlock();
				}

				// if timeout, you should restart timer and retranmission all the datagrams between var<base> and var<nextseqnum>
				if (is_timeout) {
					// restart timer
					mtx.lock();
					TerminateThread(timer_handle, 0);
					CloseHandle(timer_handle);
					timer_handle = CreateThread(NULL, NULL, myTimer, LPVOID(u_rto), 0, 0);
					mtx.unlock();
					mtx.lock();
					// reno timeout handler
					TimeoutHandler();
					for (int i = base; i < nextseqnum; i++) {
						for (int j = 0; j < dg_vec[i].length(); j++) {
							sendBuffer[j] = dg_vec[i][j];
						}
						fill_seq(i);
						sendto(cli_socket, sendBuffer, SEND_LEN, 0, (sockaddr*)&serveraddr, len_sockaddrin);
						memset(sendBuffer, 0, sizeof(sendBuffer));
					}
					mtx.unlock();
				}

				mtx.lock();
				cout << "base: " << base << " ;nextseqnum: " << nextseqnum << " " << endl;
				mtx.unlock();

				// send_times == base - 1 means finish datagrams transmission
				if (send_times == base - 1) {
					mtx.lock();
					int t_end = clock();
					t_end = clock();
					cout << "we send " << len << " (bytes), cost " << (t_end - t_start) << "(ms)";
					cout << " throughout: " << len * 8 * 1.0 / (t_end - t_start) * CLOCKS_PER_SEC << " bps" << endl;
					clear_status();
					TerminateThread(recvfm_handle, 0);
					CloseHandle(recvfm_handle);
					TerminateThread(timer_handle, 0);
					CloseHandle(timer_handle);
					mtx.unlock();
					break;
				}
			}
			TerminateThread(recvfm_handle, 0);
			CloseHandle(recvfm_handle);
			TerminateThread(timer_handle, 0);
			CloseHandle(timer_handle);
		}
		else if (option.substr(0, 5) == "reset") {
			cout << "choose your option:" << endl;
			cout << "1 for RTO; 2 for WINDOW_SIZE;" << endl;
			int opt;
			cin >> opt;
			switch (opt) {
			case 1: {
				// set the timer's RTO for rdt3.0
				// note: the timeval type used in setsocketopt will be read as milliseconds
				cout << "please input the timer's RTO (the unit is ms):" << endl;
				cin >> u_rto;
				continue;
			}
			case 2: {
				cout << "please input your GBN's windowsize(it must > 0):" << endl;
				int windowsize;
				cin >> windowsize;
				if (windowsize > 0) {
					WINDOW_SIZE = windowsize;
				}
				else {
					WINDOW_SIZE = DEFAULT_WINDOW_SIZE;
				}
				continue;
			}
			default: {
				cout << "invalid option..please check..!" << endl;
				continue;
			}
			}
		}
		else if (option.substr(0, 4) == "exit") {
			break;
		}
	}

	// close the cli_socket
	closesocket(cli_socket);
	WSACleanup();
}