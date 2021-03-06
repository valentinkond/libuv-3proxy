/*
   3APA3A simpliest proxy server
   (c) 2002-2008 by ZARAZA <3APA3A@security.nnov.ru>

   please read License Agreement

   $Id: common.c,v 1.94 2014-04-07 21:24:45 vlad Exp $
*/



#include "proxy.h"


char * copyright = COPYRIGHT;

int randomizer = 1;

#ifndef _WIN32
 pthread_attr_t pa;
#endif

unsigned char **stringtable = NULL;

int myinet_ntop(int af, void *src, char *dst, socklen_t size){
#ifndef NOIPV6
 if(af != AF_INET6){
#endif 
	unsigned u = ntohl(((struct in_addr *)src)->s_addr);
 	return sprintf(dst, "%u.%u.%u.%u", 
		((u&0xFF000000)>>24), 
		((u&0x00FF0000)>>16),
		((u&0x0000FF00)>>8),
		((u&0x000000FF)));
#ifndef NOIPV6
 }
 *dst = 0;
 inet_ntop(af, src, dst, size);
 return strlen(dst);
#endif 
}

char *rotations[] = {
	"",
	"/min",
	"/hour",
	"/day",
	"/week",
	"/month",
	"/year",
	"",
};


struct extparam conf = {
	{1, 5, 30, 60, 180, 1800, 15, 60, 0, 0},
	NULL,
	NULL,
	NULL, NULL,
	NULL,
	NULL,
	0, -1, 0, 0, 0, 0, 0, MAX_PROXY_NUMBER, 0, 0, 0,
	6, 600,
	1048576,
	NULL, NULL,
	NONE, NONE,
	NULL,
	{AF_INET}, 
	INADDR_ANY, 
	0,
	NULL,
	NULL,
	doconnect,
	lognone,
	NULL,
	NULL,
	NULL, NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	(time_t)0, (time_t)0,
	0,0,
	'@'
};

int myrand(void * entropy, int len){
	int i;
	unsigned short init;

	init = randomizer;
	for(i=0; i < len/2; i++){
		init ^= ((unsigned short *)entropy)[i];
	}
	srand(init);
	randomizer = rand();
	return rand();
	
}

#ifndef WITH_POLL
int  
#ifdef _WIN32
 WINAPI
#endif

    mypoll(struct mypollfd *fds, unsigned int nfds, int timeout){
	fd_set readfd;
	fd_set writefd;
	fd_set oobfd;
	struct timeval tv;
	unsigned i;
	int num;
	SOCKET maxfd = 0;

	tv.tv_sec = timeout/1000;
	tv.tv_usec = (timeout%1000)*1000;
	FD_ZERO(&readfd);
	FD_ZERO(&writefd);
	FD_ZERO(&oobfd);
	for(i=0; i<nfds; i++){
		if((fds[i].events&POLLIN))FD_SET(fds[i].fd, &readfd);
		if((fds[i].events&POLLOUT))FD_SET(fds[i].fd, &writefd);
		if((fds[i].events&POLLPRI))FD_SET(fds[i].fd, &oobfd);
		fds[i].revents = 0;
		if(fds[i].fd > maxfd) maxfd = fds[i].fd;
	}
	if((num = select(((int)(maxfd))+1, &readfd, &writefd, &oobfd, &tv)) < 1) return num;
	for(i=0; i<nfds; i++){
		if(FD_ISSET(fds[i].fd, &readfd)) fds[i].revents |= POLLIN;
		if(FD_ISSET(fds[i].fd, &writefd)) fds[i].revents |= POLLOUT;
		if(FD_ISSET(fds[i].fd, &oobfd)) fds[i].revents |= POLLPRI;
	}
	return num;
}
#endif

struct sockfuncs so = {
	socket,
	accept,
	bind,
	listen,
	connect,
	getpeername,
	getsockname,
	getsockopt,
	setsockopt,
#ifdef WITH_POLL
	poll,
#else
	mypoll,
#endif
	send,
	sendto,
	recv,
	recvfrom,
	shutdown,
#ifdef _WIN32
	closesocket
#else
	close
#endif
};

#ifdef _WINCE

static char cebuf[1024];
static char ceargbuf[256];
char * ceargv[32];

char * CEToUnicode (const char *str){
	int i;

	for(i=0; i<510 && str[i]; i++){
		cebuf[(i*2)] = str[i];
		cebuf[(i*2)+1] = 0;
	}
	cebuf[(i*2)] = 0;
	cebuf[(i*2)+1] = 0;
	return cebuf;
};

int cesystem(const char *str){
	STARTUPINFO startupInfo = {0};
	startupInfo.cb = sizeof(startupInfo);

	PROCESS_INFORMATION processInformation;

	return CreateProcessW((LPWSTR)CEToUnicode(str), NULL, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &startupInfo, &processInformation);
}

int ceparseargs(const char *str){
	int argc = 0, i;
	int space = 1;

	for(i=0; i<250 && argc<30 && str[2*i]; i++){
		ceargbuf[i] = str[2*i];
		if(space && ceargbuf[i]!=' '&& ceargbuf[i]!='\t'&& ceargbuf[i]!='\r'&& ceargbuf[i]!='\n'){
			ceargv[argc++] = ceargbuf + i;
			space = 0;
		}
		else if(!space && (ceargbuf[i]==' ' || ceargbuf[i]=='\t' || ceargbuf[i]=='\r' || ceargbuf[i]=='\n')){
			ceargbuf[i] = 0;
			space = 1;
		}
	}
	return argc;
}

#endif

int parsehostname(char *hostname, struct clientparam *param, unsigned short port){
	char *sp;

	if(!hostname || !*hostname)return 1;
	if ( (sp = strchr(hostname, ':')) ) *sp = 0;
	if ((unsigned char *)hostname != param->hostname){
		if(param->hostname) myfree(param->hostname);
		param->hostname = (unsigned char *)mystrdup(hostname);
	}
	if(sp){
		port = atoi(sp+1);
		*sp = ':';
	}
	param->req.sin_port=htons(port);
	param->req.sin_addr.s_addr = getip(param->hostname);
	param->sins.sin_addr.s_addr = 0;
	param->sins.sin_port = 0;
	return 0;
}

int parseusername(char *username, struct clientparam *param, int extpasswd){
	char *sb = NULL, *se = NULL, *sp = NULL;

	if(!username || !*username) return 1;
	if(!param->srv->nouser && (sb = strchr(username, ':')) && (se = strchr(sb + 1, ':')) && (!extpasswd || (sp = strchr(se + 1, ':')))){
		*sb = 0;
		*se = 0;
		if(sp) *sp = 0;
		if(*(sb+1)) {
			if(param->password) myfree(param->password);
			param->password = (unsigned char *)mystrdup(sb+1);
		}
		if(*username) {
			if(param->username) myfree(param->username);
			param->username = (unsigned char *)mystrdup(username);
		}
		username = se+1;
	 }
	if(extpasswd){
		if(!sp) sp = strchr(username, ':');
		if(sp){
			*sp = 0;
			if(param->extpassword) myfree(param->extpassword);
			param->extpassword = (unsigned char *) mystrdup(sp+1);
		}
	}
	if(param->extusername) myfree(param->extusername);
	param->extusername = (unsigned char *)mystrdup(username);
	if(sb) *sb = ':';
	if(se) *se = ':';
	if(sp) *sp = ':';
	return 0;
}

int parseconnusername(char *username, struct clientparam *param, int extpasswd, unsigned short port){
	char *sb, *se;
	if(!username || !*username) return 1;
        if ((sb=strchr(username, conf.delimchar)) == NULL){
		if(!param->hostname && param->remote_conn.socket == INVALID_SOCKET) return 2;
		return parseusername(username, param, extpasswd);
	}
	while ((se=strchr(sb+1, conf.delimchar)))sb=se;
	*(sb) = 0;
	if(parseusername(username, param, extpasswd)) return 3;
	*(sb) = conf.delimchar;
	if(parsehostname(sb+1, param, port)) return 4;
	return 0;
}

void clearstat(struct clientparam * param) {

#ifdef _WIN32
	struct timeb tb;

	ftime(&tb);
	param->time_start = (time_t)tb.time;
	param->msec_start = (unsigned)tb.millitm;

#else
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);

	param->time_start = (time_t)tv.tv_sec;
	param->msec_start = (tv.tv_usec / 1000);
#endif
	param->statscli64 = param->statssrv64 = param->nreads = param->nwrites =
		param->nconnects = 0;
}


char months[12][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


int dobuf2(struct clientparam * param, unsigned char * buf, const unsigned char *s, const unsigned char * doublec, struct tm* tm, char * format){
	int i, j;
	int len;
	time_t sec;
	unsigned msec;

	long timezone;
	unsigned delay;
	struct in_addr tmpia;



#ifdef _WIN32
	struct timeb tb;

	ftime(&tb);
	sec = (time_t)tb.time;
	msec = (unsigned)tb.millitm;
	timezone = tm->tm_isdst*60 - tb.timezone;

#else
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);

	sec = (time_t)tv.tv_sec;
	msec = tv.tv_usec / 1000;
#ifdef _SOLARIS
	timezone = -altzone / 60;
#else
	timezone = tm->tm_gmtoff / 60;
#endif
#endif

	delay = param->time_start?((unsigned) ((sec - param->time_start))*1000 + msec) - param->msec_start : 0;
	*buf = 0;
	for(i=0, j=0; format[j] && i < 4040; j++){
		if(format[j] == '%' && format[j+1]){
			j++;
			switch(format[j]){
				case '%':
				 buf[i++] = '%';
				 break;
				case 'y':
				 sprintf((char *)buf+i, "%.2d", tm->tm_year%100);
				 i+=2;
				 break;
				case 'Y':
				 sprintf((char *)buf+i, "%.4d", tm->tm_year+1900);
				 i+=4;
				 break;
				case 'm':
				 sprintf((char *)buf+i, "%.2d", tm->tm_mon+1);
				 i+=2;
				 break;
				case 'o':
				 sprintf((char *)buf+i, "%s", months[tm->tm_mon]);
				 i+=3;
				 break;
				case 'd':
				 sprintf((char *)buf+i, "%.2d", tm->tm_mday);
				 i+=2;
				 break;
				case 'H':
				 sprintf((char *)buf+i, "%.2d", tm->tm_hour);
				 i+=2;
				 break;
				case 'M':
				 sprintf((char *)buf+i, "%.2d", tm->tm_min);
				 i+=2;
				 break;
				case 'S':
				 sprintf((char *)buf+i, "%.2d", tm->tm_sec);
				 i+=2;
				 break;
				case 't':
				 sprintf((char *)buf+i, "%.10u", (unsigned)sec);
				 i+=10;
				 break;
				case 'b':
				 i+=sprintf((char *)buf+i, "%u", delay?(unsigned)(param->statscli64 * 1000./delay):0);
				 break;
				case 'B':
				 i+=sprintf((char *)buf+i, "%u", delay?(unsigned)(param->statssrv64 * 1000./delay):0);
				 break;				 
				case 'D':
				 i+=sprintf((char *)buf+i, "%u", delay);
				 break;
				case '.':
				 sprintf((char *)buf+i, "%.3u", msec);
				 i+=3;
				 break;
				case 'z':
				 sprintf((char *)buf+i, "%+.2ld%.2u", timezone / 60, (unsigned)(timezone%60));
				 i+=5;
				 break;
				case 'U':
				 if(param->username && *param->username){
					for(len = 0; i< 4000 && param->username[len]; len++){
					 buf[i] = param->username[len];
					 if(param->srv->nonprintable && (buf[i] < 0x20 || strchr((char *)param->srv->nonprintable, buf[i]))) buf[i] = param->srv->replace;
					 if(doublec && strchr((char *)doublec, buf[i])) {
						buf[i+1] = buf[i];
						i++;
					 }
					 i++;
					}
				 }
				 else {
					buf[i++] = '-';
				 }
				 break;
				case 'n':
					len = param->hostname? (int)strlen((char *)param->hostname) : 0;
					if (len > 0) for(len = 0; param->hostname[len] && i < 4000; len++, i++){
						buf[i] = param->hostname[len];
					 	if(param->srv->nonprintable && (buf[i] < 0x20 || strchr((char *)param->srv->nonprintable, buf[i]))) buf[i] = param->srv->replace;
						if(doublec && strchr((char *)doublec, buf[i])) {
							buf[i+1] = buf[i];
							i++;
						}
					}
					else i += myinet_ntop(*SAFAMILY(&param->sins), SAADDR(&param->sins), (char *)buf + i, 64);
					break;

				case 'N':
				 if(param->service >=0 && param->service < 15) {
					 len = (conf.stringtable)? (int)strlen((char *)conf.stringtable[SERVICES + param->service]) : 0;
					 if(len > 20) len = 20;
					 memcpy(buf+i, (len)?conf.stringtable[SERVICES + param->service]:(unsigned char*)"-", (len)?len:1);
					 i += (len)?len:1;
				 }
				 break;
				case 'E':
				 sprintf((char *)buf+i, "%.05d", param->res);
				 i += 5;
				 break;
				case 'T':
				 if(s){
					for(len = 0; i<4000 && s[len]; len++){
					 buf[i] = s[len];
					 if(param->srv->nonprintable && (buf[i] < 0x20 || strchr((char *)param->srv->nonprintable, buf[i]))) buf[i] = param->srv->replace;
					 if(doublec && strchr((char *)doublec, buf[i])) {
						buf[i+1] = buf[i];
						i++;
					 }
					 i++;
					}
				 }
				 break;
				case 'e':
				 tmpia.s_addr = param->extip;
				 i += myinet_ntop(AF_INET, &tmpia, (char *)buf + i, 64);
				 break;
				case 'C':
				 i += myinet_ntop(*SAFAMILY(&param->sincr), SAADDR(&param->sincr), (char *)buf + i, 64);
				 break;
				case 'R':
				 i += myinet_ntop(*SAFAMILY(&param->sins), SAADDR(&param->sins), (char *)buf + i, 64);
				 break;
				case 'Q':
				 i += myinet_ntop(*SAFAMILY(&param->req), SAADDR(&param->req), (char *)buf + i, 64);
				 break;
				case 'p':
				 sprintf((char *)buf+i, "%hu", ntohs(*SAPORT(&param->srv->intsa)));
				 i += (int)strlen((char *)buf+i);
				 break;
				case 'c':
				 sprintf((char *)buf+i, "%hu", ntohs(*SAPORT(&param->sincr)));
				 i += (int)strlen((char *)buf+i);
				 break;
				case 'r':
				 sprintf((char *)buf+i, "%hu", ntohs(param->sins.sin_port));
				 i += (int)strlen((char *)buf+i);
				 break;
				case 'q':
				 sprintf((char *)buf+i, "%hu", ntohs(param->req.sin_port));
				 i += (int)strlen((char *)buf+i);
				 break;
				case 'I':
				 sprintf((char *)buf+i, "%"PRINTF_INT64_MODIFIER"u", param->statssrv64);
				 i += (int)strlen((char *)buf+i);
				 break;
				case 'O':
				 sprintf((char *)buf+i, "%"PRINTF_INT64_MODIFIER"u", param->statscli64);
				 i += (int)strlen((char *)buf+i);
				 break;
				case 'h':
				 sprintf((char *)buf+i, "%d", param->redirected);
				 i += (int)strlen((char *)buf+i);
				 break;
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					{
						int k, pmin=0, pmax=0;
						for (k = j; isnumber(format[k]); k++);
						if(format[k] == '-' && isnumber(format[k+1])){
							pmin = atoi(format + j) - 1;
							k++;
							pmax = atoi(format + k) -1;
							for (; isnumber(format[k]); k++);
							j = k;
						}
						if(!s || format[k]!='T') break;
						for(k = 0, len = 0; s[len] && i < 4000; len++){
							if(isspace(s[len])){
								k++;
								while(isspace(s[len+1]))len++;
								if(k == pmin) continue;
							}
							if(k>=pmin && k<=pmax) {
								buf[i] = s[len];
								if(param->srv->nonprintable && (buf[i] < 0x20 || strchr((char *)param->srv->nonprintable, buf[i]))) buf[i] = param->srv->replace;
								if(doublec && strchr((char *)doublec, buf[i])) {
									buf[i+1] = buf[i];
									i++;
				 				}
								i++;
							}
						}
						break;

					}
				default:
				 buf[i++] = format[j];
			}
		}
		else buf[i++] = format[j];
	}
	buf[i] = 0;
	return i;
}

int dobuf(struct clientparam * param, unsigned char * buf, const unsigned char *s, const unsigned char * doublec){
	struct tm* tm;
	int i;
	char * format;
	time_t t;

	time(&t);
	if(!param) return 0;
	if(param->trafcountfunc)(*param->trafcountfunc)(param);
	format = (char *)param->srv->logformat;
	if(!format) format = "G%y%m%d%H%M%S.%. %p %E %U %C:%c %R:%r %O %I %h %T";
	tm = (*format == 'G' || *format == 'g')?
		gmtime(&t) : localtime(&t);
	i = dobuf2(param, buf, s, doublec, tm, format + 1);
	clearstat(param);
	return i;
}

void lognone(struct clientparam * param, const unsigned char *s) {
	if(param->trafcountfunc)(*param->trafcountfunc)(param);
	clearstat(param);
}


void logstdout(struct clientparam * param, const unsigned char *s) {
	unsigned char buf[4096];
	FILE *log;

	log = param->srv->stdlog?param->srv->stdlog:conf.stdlog?conf.stdlog:stdout;
	dobuf(param, buf, s, NULL);
	if(!param->nolog)if(fprintf(log, "%s\n", buf) < 0) {
		perror("printf()");
	};
	if(log != conf.stdlog)fflush(log);
}
#ifndef _WIN32
void logsyslog(struct clientparam * param, const unsigned char *s) {
	unsigned char buf[4096];

	dobuf(param, buf, s, NULL);
	if(!param->nolog)syslog(LOG_INFO, "%s", buf);
}
#endif

void on_remote_connect(uv_connect_t* req, int status);
int doconnect(struct clientparam * param){
 SASIZETYPE size = sizeof(param->sins);
 struct sockaddr_in bindsa;
 if (param->operation == ADMIN || param->operation == DNSRESOLVE || param->operation == BIND || param->operation == UDPASSOC)
	return 0;
 if (param->remote_conn.socket != INVALID_SOCKET){
	 if (so._getpeername(param->remote_conn.socket, (struct sockaddr *)&param->sins, &size) == -1) { return (15); }
 }
 else {
	 if (!param->sins.sin_addr.s_addr)
		 if (!(param->sins.sin_addr.s_addr = param->req.sin_addr.s_addr)) return 100;
	 if (!param->sins.sin_port)param->sins.sin_port = param->req.sin_port;

	 param->sins.sin_family = AF_INET;

//	 struct linger lg;
// 	 SASIZETYPE size = sizeof(param->sins);
// 	 so._setsockopt(param->remote_conn.socket, SOL_SOCKET, SO_LINGER, (char *)&lg, sizeof(lg));
// 	 struct sockaddr_in bindsa;
// 	 memset(&bindsa, 0, sizeof(bindsa));
// 	 bindsa.sin_family = AF_INET;
// 	 bindsa.sin_port = param->extport;
// 	 bindsa.sin_addr.s_addr = param->extip;
// 	 if (param->srv->targetport && !bindsa.sin_port && ntohs(*SAPORT(&param->sincr)) > 1023) bindsa.sin_port = *SAPORT(&param->sincr);
// 	 if (so._bind(param->remote_conn.socket, (struct sockaddr*)&bindsa, sizeof(bindsa)) == -1)
// 	 {
// 		 memset(&bindsa, 0, sizeof(bindsa));
// 		 bindsa.sin_family = AF_INET;
// 		 bindsa.sin_addr.s_addr = param->extip;
// 		 bindsa.sin_port = 0;
// 		 if (so._bind(param->remote_conn.socket, (struct sockaddr*)&bindsa, sizeof(bindsa)) == -1)
// 		 {
// 			 return (12);
// 		 }
// 	 }

	int r = uv_tcp_connect(&param->remote_connect_req,
		&param->remote_conn,
		(const struct sockaddr*) &param->sins,
		on_remote_connect);
	if (r!=0)
	{
		return (11);
	}
 }
 return 0;
}

int scanaddr(const unsigned char *s, unsigned long * ip, unsigned long * mask) {
	unsigned d1, d2, d3, d4, m;
	int res;
	if ((res = sscanf((char *)s, "%u.%u.%u.%u/%u", &d1, &d2, &d3, &d4, &m)) < 4) return 0;
	if(mask && res == 4) *mask = 0xFFFFFFFF;
	else if (mask) *mask = htonl(0xFFFFFFFF << (32 - m));
	*ip = htonl ((d1<<24) ^ (d2<<16) ^ (d3<<8) ^ d4);
	return res;
}

RESOLVFUNC resolvfunc = NULL;
#ifndef _WIN32
pthread_mutex_t gethostbyname_mutex;
int ghbn_init = 0;
#endif


#ifdef GETHOSTBYNAME_R
struct hostent * my_gethostbyname(char *name, char *buf, struct hostent *hp){
	struct hostent *result;
	int gherrno;

#ifdef _SOLARIS
	return gethostbyname_r(name, hp, buf, 1024, &gherrno);
#else
	if(gethostbyname_r(name, hp, buf, 1024, &result, &gherrno) != 0)
		return NULL;
	return result;
#endif
}
#endif

unsigned long getip(unsigned char *name){
	unsigned long retval;
	int i;
	int ndots = 0;
	struct hostent *hp=NULL;

#ifdef GETHOSTBYNAME_R
	struct hostent he;
	char ghbuf[1024];
#define gethostbyname(NAME) my_gethostbyname(NAME, ghbuf, &he)
#endif

	if(strlen((char *)name)>255)name[255] = 0;
	for(i=0; name[i]; i++){
		if(name[i] == '.'){
			if(++ndots > 3) break;
			continue;
		}
		if(name[i] <'0' || name[i] >'9') break;
	}
	if(!name[i] && ndots == 3){
		if(scanaddr(name, &retval, NULL) == 4){
			return retval;
		}
	}
	if(resolvfunc){
		if((retval = (*resolvfunc)(name))) return retval;
		if(conf.demanddialprog) system(conf.demanddialprog);
		return (*resolvfunc)(name);
	}
#if !defined(_WIN32) && !defined(GETHOSTBYNAME_R)
	if(!ghbn_init){
		pthread_mutex_init(&gethostbyname_mutex, NULL);
		ghbn_init++;
	}
	pthread_mutex_lock(&gethostbyname_mutex);
#endif
	hp=gethostbyname((char *)name);
	if (!hp && conf.demanddialprog) {
		system(conf.demanddialprog);
		hp=gethostbyname((char *)name);
	}
	retval = hp?*(unsigned long *)hp->h_addr:0;
#if !defined(_WIN32) && !defined(GETHOSTBYNAME_R)
	pthread_mutex_unlock(&gethostbyname_mutex);
#endif
#ifdef GETHOSTBYNAME_R
#undef gethostbyname
#endif
	return retval;
}

unsigned long getip46(int family, unsigned char *name,  struct sockaddr *sa){
#ifndef NOIPV6
	int ndots=0, ncols=0, nhex=0;
	struct addrinfo *ai, *iter;
	struct sockaddr *sa4=NULL, *sa6=NULL;
	int i;

	if(!sa) return 0;
	if(!family) {
#endif
		((struct sockaddr_in *)sa)->sin_family = AF_INET;
		return (((struct sockaddr_in *)sa)->sin_addr.s_addr = getip(name))? AF_INET:0;
#ifndef NOIPV6
	}
	for(i=0; name[i]; i++){
		if(name[i] == '.'){
			if(++ndots > 3) {
				break;
			}
		}
		else if(name[i] == ':'){
			if(++ncols > 7) {
				break;
			}
		}
		else if(name[i] == '%' || (name[i] >= 'a' && name[i] <= 'f') || (name[i] >= 'A' && name[i] <= 'F')){
			nhex++;
		}
		else if(name[i] <'0' || name[i] >'9') {
			break;
		}
	}
	if(!name[i]){
		if(ndots == 3 && ncols == 0 && nhex == 0){
			*SAFAMILY(sa)=AF_INET;
			return inet_pton(AF_INET, name, SAADDR(sa))? AF_INET : 0; 
		}
		if(ncols >= 2) {
			*SAFAMILY(sa)=AF_INET6;
			return inet_pton(AF_INET6, name, SAADDR(sa))? AF_INET6 : 0;
		}
	}
	if (getaddrinfo(name, NULL, NULL, &ai)) return 0;
	for(iter = ai; iter; iter = iter->ai_next){
		if(!sa4 && iter->ai_addr->sa_family == AF_INET) sa4 = iter->ai_addr;
		if(!sa6 && iter->ai_addr->sa_family == AF_INET6) sa6 = iter->ai_addr;
	}
	if(sa6 && ((family == 6) || (family == 64) || (family == 46 && !sa4))){
		*SAFAMILY(sa)=AF_INET6;
		memcpy(SAADDR(sa), SAADDR(sa6), SAADDRLEN(sa));
		freeaddrinfo(ai);
		return AF_INET6;
	}
	else if(sa4 && family != 6){
		*SAFAMILY(sa)=AF_INET;
		memcpy(SAADDR(sa), SAADDR(sa4), SAADDRLEN(sa));
		freeaddrinfo(ai);
		return AF_INET;
	}	
	freeaddrinfo(ai);
	return 0;
#endif
}
