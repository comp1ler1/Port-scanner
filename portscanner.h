#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <cstring>
#include <regex>


using namespace std;


class portscanner {
public:
    portscanner(int maxThreads, int startPort, int endPort) : maxThreads(maxThreads), activeThreads(0), portInit(startPort), portEnd(endPort) {}

    void scanPorts(const std::string& ip);

private:
    static bool port_is_open(const std::string& ip, int port);


    void scanPortRange(const std::string& ip, int startPort, int endPort);
    void task(const std::string& ip, int port);


    int portInit, portEnd;
    string ip;


    int maxThreads;
    int activeThreads;
    std::mutex mtx;
    std::condition_variable cv;
};
