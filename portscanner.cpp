#include "portscanner.h"


void portscanner::scanPorts(const std::string& ip) {
        std::vector<std::thread> tasks;
        tasks.reserve(portEnd - portInit);

        for (int i = portInit; i < portEnd; ++i) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]() { return activeThreads < maxThreads; });
            ++activeThreads;

            tasks.emplace_back([this, ip, i]() {
                try {
                    task(ip, i);
                } catch (const std::exception& e) {
                    std::cerr << "Error scanning port " << i << ": " << e.what() << std::endl;
                }

                std::unique_lock<std::mutex> lock(mtx);
                --activeThreads;
                cv.notify_one();
            });
        }

        for (auto& t : tasks) {
            if (t.joinable()) {
                t.join();
            }
        }
}


bool portscanner::port_is_open(const std::string& ip, int port) {
        struct sockaddr_in addr_s;
        const char* addr = ip.c_str();
        int fd = -1;
        fd_set fdset;
        struct timeval tv;
        int rc;
        int so_error;
        socklen_t len;
        int seconds = 1;

        addr_s.sin_family = AF_INET;
        addr_s.sin_addr.s_addr = inet_addr(addr);
        addr_s.sin_port = htons(port);

        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            throw std::runtime_error("Socket creation failed");
        }

        fcntl(fd, F_SETFL, SOCK_NONBLOCK);

        rc = connect(fd, (struct sockaddr*)&addr_s, sizeof(addr_s));
        if ((rc == -1) && (errno != EINPROGRESS)) {
            close(fd);
            throw std::runtime_error("Connection failed: " + std::string(strerror(errno)));
        }
        if (rc == 0) {
            close(fd);
            return true;
        }

        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        tv.tv_sec = seconds;
        tv.tv_usec = 0;

        rc = select(fd + 1, NULL, &fdset, NULL, &tv);
        if (rc == -1) {
            close(fd);
            throw std::runtime_error("Select failed: " + std::string(strerror(errno)));
        } else if (rc == 0) {
            close(fd);
            return false;
        } else {
            len = sizeof(so_error);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
            close(fd);
            if (so_error == 0) {
                return true;
            } else {
                return false;
            }
        }
}


void portscanner::task(const std::string& ip, int port) {
        if (port_is_open(ip, port))
            std::cout << ip << ":" << port << " is open\n";
}