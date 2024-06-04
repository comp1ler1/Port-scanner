#include "portscanner.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

bool portscanner::port_is_open(const std::string& ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed\n";
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    int result = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    close(sockfd);

    return (result == 0);
}

void portscanner::scanPorts(const std::string& ip) {
    std::vector<std::thread> threads;
    std::mutex mtx;
    const int numThreads = 75; // Можно настроить количество потоков

    for (int port = portInit; port <= portEnd; ++port) {
        threads.emplace_back([&, ip, port]() {
            if (port_is_open(ip, port)) {
                std::lock_guard<std::mutex> lock(mtx);
                std::cout << ip << ":" << port << " is open\n";
            }
        });

        // Поддерживаем не более numThreads потоков одновременно
        if (threads.size() >= numThreads) {
            for (auto& thread : threads) {
                thread.join();
            }
            threads.clear();
        }
    }

    // Дожидаемся завершения оставшихся потоков
    for (auto& thread : threads) {
        thread.join();
    }
}
