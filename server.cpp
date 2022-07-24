#include "server.h"
#include <iostream>

using std::cout;
using std::endl;
using std::string;

int main()
{
    boost::asio::io_context io_context; // server context
    Server s(io_context);
    io_context.run();
    return 0;
}
