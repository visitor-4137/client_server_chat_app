#include "client.h"

using std::string;

int main()
{
    boost::asio::io_service io_context;
    client_side_connection client(io_context);
    while (1) {
        if (ack_flag) {
            string msg;
            std::cout << ">> ";
            getline(std::cin, msg);
            if (msg == "quit") {
                break;
            }
            msg.push_back('\n');
            if (msg.size() > 1) {
                client.write_msg(msg);
            }
        }
        while (!client.client_msg_queue.is_empty()) {
            string msg = client.client_msg_queue.front();
            client.client_msg_queue.pop();
            std::cout << msg << std::endl;
        }
    }
    client.close();
    return 0;
}