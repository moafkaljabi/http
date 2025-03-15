/*

in .h :
    Creae class.
    declare constructor and destructor.

    create int m_socket = socket()
    create socket for our member function.
    wrap the socket in a function e.g. startServer.

in .cpp
    define the constructor and destructor.


in server.cpp: 
    make instance of the class TCPServer


*/      

#ifndef INCLUDED_HTTP_TCPSERVER_LINUX
#define INCLUDED_HTTP_TCPSERVER_LINUX


/* includes */
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string>

namespace http {
    class TCPServer {

    public:
        TCPServer(std::string IPAddress, int port);
        ~TCPServer();
        void startListen();

    private:
        std::string m_ip_address;
        int m_socket;
        int m_port;
        int m_new_socket;
        long m_incoming_message;
        struct sockaddr_in m_socket_address;
        unsigned int m_socket_address_len;
        std::string m_server_message;

        int startServer();
        void closeServer();
        void acceptConnection(int &new_socket);
        std::string buildResponse();
        void sendResponse();
    };

}


#endif