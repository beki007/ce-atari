#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <errno.h>  
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <iostream>

#include "utils.h"
#include "debug.h"
#include "timesync.h"
#include "settings.h"

TimeSync::TimeSync()
{
    iInitState = INIT_NONE;
}

bool TimeSync::sync(void)
{
	Debug::out(LOG_DEBUG, "TimeSync::sync -- start");

    BYTE tmp[10];
    Utils::getIpAdds(tmp);                  // try to get IP addresses of network interfaces

    if(tmp[0] == 0 && tmp[5] == 0) {        // no network interface has IP? then no network, failed
        Debug::out(LOG_DEBUG, "TimeSync::sync -- not syncing, network is down...");
        return false;
    }
 
    bool res = syncByNtp();                 // first try by NTP
    
    if(!res) {
        return false;
    }
    
    
  	Debug::out(LOG_DEBUG, "TimeSync::sync -- success!");
    return true;
}

bool TimeSync::syncByNtp(void)
{
    refreshNetworkDateNtp();

    if(iInitState == INIT_NTP_FAILED) {       // if failed, then failed ;)
        return false;
    }
  
    timeval tv;
    tv.tv_sec=lTime;
    tv.tv_usec=0;

    Debug::out(LOG_DEBUG, "TimeSync: trying to set date to %d", lTime);
    int ret=settimeofday(&tv,NULL);

    if( ret<0 ){
        iInitState=INIT_DATE_NOT_SET;
        Debug::out(LOG_DEBUG, "TimeSync: could not set date: %d", errno);
        return false;
    }
    
    Debug::out(LOG_DEBUG, "TimeSync: date set to %d",lTime);

    iInitState=INIT_OK;
    Debug::out(LOG_DEBUG, "TimeSync: init done.");
    return true;
}

//from http://stackoverflow.com/questions/9326677/is-there-any-c-c-library-to-connect-with-a-remote-ntp-server/19835285#19835285
void TimeSync::refreshNetworkDateNtp(void) 
{
  Settings settings;
  std::string ntpServer = settings.getString((char *) "TIME_NTP_SERVER", (char *) "200.20.186.76");

  char hostname[64];
  strcpy(hostname, (char *) ntpServer.c_str());

  int portno=123;     //NTP is port 123
  int maxlen=1024;        //check our buffers
  int i;          // misc var i
  unsigned char msg[48]={010,0,0,0,0,0,0,0,0};    // the packet we send
  unsigned long  buf[maxlen]; // the buffer we get back
  //struct in_addr ipaddr;        //  
  struct protoent *proto;     //
  struct sockaddr_in server_addr;
  int s;  // socket
  long int tmit;   // the time -- This is a time_t sort of

  //use Socket;
  //
  //#we use the system call to open a UDP socket
  proto=getprotobyname("udp");
  s=socket(PF_INET, SOCK_DGRAM, proto->p_proto);
  if( s<0 ){
    iInitState=INIT_NTP_FAILED;
    Debug::out(LOG_DEBUG, "TimeSync: could not open NTP UDP socket: ",strerror(errno));
    return;
  }
  
  //
  //#convert hostname to ipaddress if needed
  memset( &server_addr, 0, sizeof( server_addr ));
  server_addr.sin_family=AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(hostname);
 
  server_addr.sin_port=htons(portno);
  
  /*
   * build a message.  Our message is all zeros except for a one in the
   * protocol version field
   * msg[] in binary is 00 001 000 00000000 
   * it should be a total of 48 bytes long
  */
  // send the data
  Debug::out(LOG_DEBUG, "TimeSync: requesting date from NTP %s:%d", ntpServer.c_str(),123);
  struct timeval tv;
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  i=setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv));
  if( i<0 ) {
    iInitState=INIT_NTP_FAILED;
    Debug::out(LOG_DEBUG, "TimeSync: could not set options on UDP socket: %s",strerror(errno));
    return;
  }
  i=sendto(s,msg,sizeof(msg),0,(struct sockaddr *)&server_addr,sizeof(server_addr));
  if( i<0 ){
    iInitState=INIT_NTP_FAILED;
    Debug::out(LOG_DEBUG, "TimeSync: could not set UDP packet: %s",strerror(errno));
    return;
  }
  // get the data back
  struct sockaddr saddr;
  socklen_t saddr_l = sizeof (saddr);
  i=recvfrom(s,buf,48,0,&saddr,&saddr_l);
  if( i<0 ){
    iInitState=INIT_NTP_FAILED;
    Debug::out(LOG_DEBUG, "TimeSync: could not recieve packet: %s",strerror(errno));
    return;
  }
  
  /*
   * The high word of transmit time is the 10th word we get back
   * tmit is the time in seconds not accounting for network delays which
   * should be way less than a second if this is a local NTP server
   */
  tmit=ntohl((time_t)buf[4]);    //# get transmit time
  
  /*
   * Convert time to unix standard time NTP is number of seconds since 0000
   * UT on 1 January 1900 unix time is seconds since 0000 UT on 1 January
   * 1970 There has been a trend to add a 2 leap seconds every 3 years.
   * Leap seconds are only an issue the last second of the month in June and
   * December if you don't try to set the clock then it can be ignored but
   * this is importaint to people who coordinate times with GPS clock sources.
   */
  
  tmit-= 2208988800U; 
  //printf("tmit=%d\n",tmit);
  /* use unix library function to show me the local time (it takes care
   * of timezone issues for both north and south of the equator and places
   * that do Summer time/ Daylight savings time.
   */
  
  //#compare to system time
  //std::cout << "time is " << ctime(&tmit)  << std::endl;
  Debug::out(LOG_DEBUG, "TimeSync: NTP time is %s",ctime(&tmit));
  i=time(0);
  //printf("%d-%d=%d\n",i,tmit,i-tmit);
  //printf("System time is %d seconds off\n",(i-tmit));
  //>std::cout << "System time is " << (i-tmit) << " seconds off" << std::endl;
  Debug::out(LOG_DEBUG, "TimeSync: System time is %d seconds off",(i-tmit));

  lTime=tmit;
}
