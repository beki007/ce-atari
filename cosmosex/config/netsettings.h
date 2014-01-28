#ifndef _NETSETTINGS_H_#define _NETSETTINGS_H_#include <string>typedef struct {	bool		dhcpNotStatic;	std::string address;    std::string netmask;    std::string gateway;		std::string wpaSsid;	std::string wpaPsk;} TNetInterface;class NetworkSettings {public:	NetworkSettings(void);	void load(void);	void save(void);	TNetInterface	eth0;	TNetInterface	wlan0;	std::string		nameserver;	private:	void initNetSettings(TNetInterface *neti);	void readString(char *line, char *tag, std::string &val);	void dumpSettings(void);		void loadNameserver(void);	void saveNameserver(void);};#endif