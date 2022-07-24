#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <mutex>
#include <queue>
#include <set>
#include <thread>


using std::string;

bool ack_flag = false;

class tsqueue {
    std::queue<string> q;
    std::mutex mux_q;

public:
    void push(string& data)
    {
        mux_q.lock();
        q.push(data);
        mux_q.unlock();
        return;
    }
    string front()
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

class client_side_connection
    : public std::enable_shared_from_this<client_side_connection> {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::socket socket_;
    std::thread context_thread;
    boost::asio::streambuf input_buffer;
    void start_reading()
    {
        boost::asio::async_read_until(
            socket_, input_buffer, '\n',
            boost::bind(&client_side_connection::read_handler, this,
                boost::asio::placeholders::error));
    }
    void read_handler(const boost::system::error_code& ec)
    {
        if (!ec) {
            std::string line;
            std::istream is(&input_buffer);
            std::getline(is, line);
            is >> line;
            client_msg_queue.push(line);
            ack_flag = true;
        } else {
            socket_.close();
            return;
        }
        start_reading();
    }

public:
    tsqueue client_msg_queue;
    client_side_connection(boost::asio::io_context& context)
        : io_context_(context)
        , socket_(context)
    {
        start_connection();
        context_thread = std::thread([this]() { io_context_.run(); });
    }
    boost::asio::ip::tcp::socket& socket() { return socket_; }
    void start_connection()
    {
        socket_.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 9999));
        if (socket_.is_open()) {
            std::cout << "Connection to Server Successfull" << std::endl;
            start_reading();
        }
    }
    void write_msg(string& msg)
    {
        if (msg.size() > 0) {
            boost::asio::post(io_context_, [this, msg]() {
                boost::asio::async_write(socket_, boost::asio::buffer(msg.data(), msg.size()), boost::bind(&client_side_connection::write_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
            });
        }
        return;
    }
    void write_handler(const boost::system::error_code& error, std::size_t bytes_transferred)
    {
        if (!error && (bytes_transferred > 0)) {
            return;
        } else {
            socket_.close();
            return;
        }
    }
    void close()
    {
        socket_.close();
    }
};