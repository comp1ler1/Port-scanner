#include <string>
#include <iostream>
#include <regex>
#include "portscanner.cpp"

bool validateData(int portInit, int portEnd, std::string ip){

    if (portInit < 1 || portInit > 65535){
        std::cout << "Incorrect start port argument" << std::endl;
        return false;
    }


    if (portEnd < 1 || portEnd > 65535 || portEnd < portInit){
        std::cout << "Incorrect end port argument" << std::endl;
        return false;
    }


    regex rgx("^([0-9]|[1-9][0-9]|1([0-9][0-9])|2([0-4][0-9]|5[0-5]))."
         "([0-9]|[1-9][0-9]|1([0-9][0-9])|2([0-4][0-9]|5[0-5]))."
         "([0-9]|[1-9][0-9]|1([0-9][0-9])|2([0-4][0-9]|5[0-5]))."
         "([0-9]|[1-9][0-9]|1([0-9][0-9])|2([0-4][0-9]|5[0-5]))$");


    if(!std::regex_match(ip, rgx)){
        std::cout << "Incorrect IP argument" << std::endl;
        return false;
    }

    return true;
}


int main(int argc, char** argv) {
    
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <IP address> <start port> <end port>" << std::endl;
        return 1;
    }

    std::string ip = argv[1];
    int portInit = std::stoi(argv[2]);
    int portEnd = std::stoi(argv[3]);


    if(!validateData(portInit, portEnd, ip)){
        return -1;
    }


    portscanner scanner(1000, portInit, portEnd);
    scanner.scanPorts(ip);

    return 0;
}

