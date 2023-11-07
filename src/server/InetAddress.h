#ifndef _INET_ADDRESS_H_
#define _INET_ADDRESS_H_
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string>
#include <sys/socket.h>

class Ipv4Address {
public:
	Ipv4Address();
	Ipv4Address(std::string ip, uint16_t port);
	void setAddr(std::string ip, uint16_t port);
	std::string getIp();
	uint16_t getPort();
	struct sockaddr *getAddr();

private:
	std::string mIp;
	uint16_t mPort;
	struct sockaddr_in mAddr;
};

#endif //_INET_ADDRESS_H_