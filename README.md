# Port-scanner

This project is a simple scanner based on scanning using syn flags. The utility is written using the standard sys/socket library for linux.
This program uses the creation of 1000 streams for simultaneous scanning. Stack overflow with so many threads may occur on some machines, so you can change the value in the program if necessary.

To install, run the following commands:

1. git clone git@github.com:comp1ler1/Port-scanner.git

2. cd Port-scanner

3. g++ main.cpp

To start, use:

./a.out <IP address> <start port> <end port>

For example: ./a.out 192.168.31.10 1 10000
