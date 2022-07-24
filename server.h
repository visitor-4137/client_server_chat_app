#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <mutex>
#include <queue>
#include <set>

class Server;
class server_side_connection;

using std::string;

void disconnect(Server* server_ptr,
    server_side_connection* conn_ptr);

void call_server_writer(Server* server_ptr);

class tsqueue {
    std::queue<std::pair<string, server_side_connection*>> q;
    std::mutex mux_q;

public:
    void push(string& data, server_side_connection* conn_ptr)
    {
        mux_q.lock();
        // std::cout << "Pushed to queue." << std::endl;
        q.push(make_pair(data, conn_ptr));
        mux_q.unlock();
        return;
    }
    std::pair<string, server_side_connection*> front()
    {
        mux_q.lock();
        auto ans = q.front();
        mux_q.unlock();
        return ans;
    }
    void pop()
    {
        mux_q.lock();
        q.pop();
        mux_q.unlock();
        return;
    }
    bool is_empty()
    {
        mux_q.lock();
        bool ans = q.empty();
        mux_q.unlock();
        return ans;
    }
};

class server_side_connection
    : public std::enable_shared_from_this<server_side_connection> {
private:
    boost::asio::ip::tcp::socket socket_;
    int client_id;
    tsqueue& server_msg_queue;
    boost::asio::streambuf input_buffer;
    Server* server_ptr;
    void start_reading()
    {
        // std::cout << "Started reading from connection at endpoint : " << socket_.remote_endpoint() << std::endl;
        boost::asio::async_read_until(
            socket_, input_buffer, '\n',
            boost::bind(&server_side_connection::read_handler, this,
                boost::asio::placeholders::error));
    }
    void read_handler(const boost::system::error_code& ec)
    {
        // std::cout << "Read_Handler invoked!!!" << std::endl;
        if (!ec) {
            std::string line;
            std::istream is(&input_buffer);
            std::getline(is, line);
            is >> line;
            int total_size = input_buffer.size();
            input_buffer.consume(total_size);
            // std::cout << "[" << client_id << "]: " << line << std::endl;
            if (line.size() > 0) {
                if (server_msg_queue.is_empty()) {
                    server_msg_queue.push(line, this);
                    call_server_writer(server_ptr);
                } else {
                    server_msg_queue.push(line, this);
                }
            }
        } else {
            disconnect(server_ptr, this);
            delete this;
            return;
        }
        start_reading();
    }

public:
    server_side_connection(boost::asio::io_context& context, int c,
        tsqueue& msg_q, Server* ptr)
        : socket_(context)
        , client_id(c)
        , server_msg_queue(msg_q)
        , server_ptr(ptr)
    {
    }
    boost::asio::ip::tcp::socket& socket() { return socket_; }
    void start_connection()
    {
        // we push it to the queue of server and pointer to this connnection
        // then we broadcast from the queue to all other connections

        // here, queue struct will be -> {string -> message, shared_ptr
        // <server_side_connection> -> connection_link}
        start_reading();
    }
    void write_msg(string& msg)
    {
        // std::cout << "Writing the message : " << msg << " to client : " << client_id << std::endl;
        if (msg.size() > 1) {
            boost::asio::async_write(socket_, boost::asio::buffer(msg.data(), msg.size()), boost::bind(&server_side_connection::write_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }
        return;
    }
    void write_handler(const boost::system::error_code& error, std::size_t bytes_transferred)
    {
        if (!error && (bytes_transferred > 0)) {
            return;
        } else {
            // std::shared_ptr<server_side_connection> connection_ptr(this);
            disconnect(server_ptr, this);
            return;
        }
    }
    int get_id()
    {
        return client_id;
    }
    ~server_side_connection()
    {
        std::cout << "Erasing the server_side_connection object with client_id : " << client_id << std::endl;
    }
};

class Server : public std::enable_shared_from_this<Server> {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor connection_acceptor;
    std::map<int, server_side_connection*> accepted_conn;
    tsqueue server_msg_queue;
    int unique_conn_id = 1;

    void start_accepting_connection()
    {
        server_side_connection* conn_ptr = new server_side_connection(io_context_, unique_conn_id,
            server_msg_queue, this);
        unique_conn_id++;
        connection_acceptor.async_accept(
            conn_ptr->socket(),
            boost::bind(&Server::accept_handler, this,
                boost::asio::placeholders::error, conn_ptr));
    }
    void accept_handler(const boost::system::error_code& ec,
        server_side_connection* conn_ptr)
    {
        if (!ec) {
            std::cout << "Connection Accepted at endpoint : "
                      << (conn_ptr->socket()).remote_endpoint() << std::endl;
            accepted_conn[conn_ptr->get_id()] = conn_ptr;
            conn_ptr->start_connection();
            std::string msg = "[Server] : Connection Accepted. Client ID assigned is : " + std::to_string(conn_ptr->get_id());
            msg.push_back('\n');
            conn_ptr->write_msg(msg);
        } else {
            std::cout << "Error while connection" << std::endl;
            return;
        }
        start_accepting_connection();
    }

public:
    Server(boost::asio::io_context& io_context)
        : io_context_(io_context)
        , connection_acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 9999))
    {
        start_accepting_connection();
    }
    void disconnect(server_side_connection* conn_ptr)
    {
        accepted_conn.erase(conn_ptr->get_id());
    }
    void print_all_active_connections()
    {
        for (auto i : accepted_conn) {
            std::cout << "Active connection on endpoint : "
                      << ((i.second)->socket()).remote_endpoint() << std::endl;
        }
    }

    void write_to_all()
    {
        if (server_msg_queue.is_empty()) {
            // std::cout << "Server msg queue empty!!!" << std::endl;
            return;
        }
        // std::cout << "Writing to all" << std::endl;
        std::pair<string, server_side_connection*> to_write = server_msg_queue.front();
        std::string msg = to_write.first;
        msg = "[" + std::to_string((to_write.second)->get_id()) + "] : " + msg;
        msg.push_back('\n');
        for (auto conn : accepted_conn) {
            if (((to_write.second)->get_id()) != conn.first) {
                (conn.second)->write_msg(msg);
            }
        }
        server_msg_queue.pop();
        write_to_all();
        return;
    }
    void write_to_all_request()
    {
        // std::cout << "Posting request to write_to_all" << std::endl;
        boost::asio::post(io_context_,
            [this] {
                write_to_all();
            });
        // std::cout << "Post request Completed" << std::endl;
        return;
    }
};

void disconnect(Server* server_ptr,
    server_side_connection* conn_ptr)
{
    server_ptr->disconnect(conn_ptr);
}

void call_server_writer(Server* server_ptr)
{
    server_ptr->write_to_all_request();
}