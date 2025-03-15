#include "../include/http_TCPServer.h"



int main() {

using namespace http;

   TCPServer server = TCPServer("0.0.0.0",8080);
    server.startListen();

    return 0;
}