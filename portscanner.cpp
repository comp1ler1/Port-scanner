#include "portscanner.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unordered_set>
#include <chrono>
#include <map>
#include <sys/resource.h>

// Устанавливаем лимиты на количество открытых файловых дескрипторов
bool set_fd_limit(int max_fds) {
    struct rlimit rl;
    rl.rlim_cur = max_fds;
    rl.rlim_max = max_fds;
    if (setrlimit(RLIMIT_NOFILE, &rl) != 0) {
        std::cerr << "Failed to set file descriptor limit\n";
        return false;
    }
    return true;
}

// Устанавливаем сокет в неблокирующий режим
bool set_non_blocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return false;
    }
    return true;
}

bool portscanner::port_is_open(const std::string& ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed\n";
        return false;
    }

    if (!set_non_blocking(sockfd)) {
        std::cerr << "Failed to set non-blocking mode\n";
        close(sockfd);
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    int result = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (result < 0 && errno != EINPROGRESS) {
        close(sockfd);
        return false;
    }

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        std::cerr << "Failed to create epoll\n";
        close(sockfd);
        return false;
    }

    struct epoll_event event;
    event.events = EPOLLOUT | EPOLLET;
    event.data.fd = sockfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
        std::cerr << "Failed to add socket to epoll\n";
        close(sockfd);
        close(epollfd);
        return false;
    }

    struct epoll_event events[1];
    int nfds = epoll_wait(epollfd, events, 1, 1000); // Тайм-аут 1 секунда

    if (nfds == -1) {
        std::cerr << "Epoll wait failed\n";
        close(sockfd);
        close(epollfd);
        return false;
    }

    bool is_open = false;

    if (nfds > 0) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        is_open = (so_error == 0);
    }

    close(sockfd);
    close(epollfd);
    return is_open;
}

void portscanner::scanPorts(const std::string& ip) {
    // Попытка установить лимиты на количество открытых файловых дескрипторов
    if (!set_fd_limit(66000)) {
        std::cerr << "Could not set file descriptor limit to 66000. Continuing with default limit.\n";
    }

    constexpr int MAX_EVENTS = 65535;
    constexpr int TIMEOUT_MS = 2000; // Тайм-аут в миллисекундах
    constexpr int BATCH_SIZE = 65535; // Количество портов для обработки в одном батче

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        std::cerr << "Failed to create epoll\n";
        return;
    }

    epoll_event event;
    event.events = EPOLLOUT | EPOLLET; // Ждем готовности к записи (т.е. соединение установлено)
    std::unordered_set<int> open_ports;
    std::map<int, int> socket_to_port; // Отслеживаем порт для каждого сокета
    std::mutex mtx;

    auto start_time = std::chrono::steady_clock::now();

    for (int port_start = portInit; port_start <= portEnd; port_start += BATCH_SIZE) {
        std::vector<int> sockets;
        for (int port = port_start; port < port_start + BATCH_SIZE && port <= portEnd; ++port) {
            int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
            if (sockfd < 0) {
                continue;
            }

            sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

            int result = connect(sockfd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
            if (result == 0) {
                std::lock_guard<std::mutex> lock(mtx);
                std::cout << ip << ":" << port << " is open\n";
                close(sockfd);
                continue;
            } else if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
                event.data.fd = sockfd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
                    std::cerr << "Failed to add socket to epoll\n";
                    close(sockfd);
                    continue;
                }
                socket_to_port[sockfd] = port; // Запоминаем порт для этого сокета
                sockets.push_back(sockfd);
            } else {
                close(sockfd);
                continue;
            }
        }

        epoll_event events[MAX_EVENTS];
        int ready_fds;
        while ((ready_fds = epoll_wait(epollfd, events, MAX_EVENTS, TIMEOUT_MS)) > 0) {
            for (int i = 0; i < ready_fds; ++i) {
                int sockfd = events[i].data.fd;
                if (events[i].events & EPOLLOUT) {
                    int optval;
                    socklen_t optlen = sizeof(optval);
                    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1) {
                        std::cerr << "Failed to get socket error\n";
                        continue;
                    }
                    if (optval == 0) {
                        std::lock_guard<std::mutex> lock(mtx);
                        std::cout << ip << ":" << socket_to_port[sockfd] << " is open\n";
                    }
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                }
            }
        }

        // Закрываем все оставшиеся сокеты в батче
        for (int sockfd : sockets) {
            epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
            close(sockfd);
        }
    }

    close(epollfd);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << "Scanning finished in " << duration << " milliseconds\n";
}
