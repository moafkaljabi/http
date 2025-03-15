#include "../include/http_TCPServer.h"

#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>


namespace {

    const int BUFFER_SIZE = 30720;

    void log(const std::string &message){
        std::cout << message << std::endl;
    }

    void exitWithError(const std::string &errorMessage){
        log("ERROR: " + errorMessage);
        exit(1);
    }
}

namespace http{

    TCPServer::TCPServer(std::string ip_address, int port)
    : m_ip_address(ip_address), m_port(port), m_socket(),
      m_new_socket(), m_incoming_message(), m_socket_address(),
      m_socket_address_len(sizeof(m_socket_address)),
      m_server_message(buildResponse())
      
      {
        startServer();
      }

      TCPServer::~TCPServer(){
        closeServer();
      }

      int TCPServer::startServer (){
        m_socket = socket(AF_INET,SOCK_STREAM,0);
        if(m_socket < 1) {
            exitWithError("Cannot create socket");
            return 1;
        }
        return 0;
      }

      void TCPServer::closeServer(){
        close(m_socket);
        close(m_new_socket);
        exit (0);
      }

}