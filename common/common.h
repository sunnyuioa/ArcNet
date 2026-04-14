#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <queue>
#include <map>
#include <vector>
#include <memory>

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::min;
using std::max;

typedef int SOCKET;

#endif
