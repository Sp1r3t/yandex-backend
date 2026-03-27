#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;

using namespace std::literals;

int main(int argc, char** argv) {
    static const int port = 3333;

    // Проверка аргументов
    if (argc != 2) {
        std::cout << "Usage: "sv << argv[0] << " <server IP>"sv << std::endl;
        return 1;
    }

    // Парсим IP
    boost::system::error_code ec;
    auto address = net::ip::make_address(argv[1], ec);

    if (ec) {
        std::cout << "Wrong IP format"sv << std::endl;
        return 1;
    }

    // Создаём endpoint
    tcp::endpoint endpoint(address, port);

    // Создаём контекст и сокет
    net::io_context io_context;
    tcp::socket socket{io_context};

    // Подключаемся к серверу
    socket.connect(endpoint, ec);

    if (ec) {
        std::cout << "Can't connect to server"sv << std::endl;
        return 1;
    }

    std::cout << "Connected to server!" << std::endl;

    // 📤 Отправка данных
    std::string message = "Hello, I'm client!\n";
    socket.write_some(net::buffer(message), ec);

    if (ec) {
        std::cout << "Error sending data"sv << std::endl;
        return 1;
    }

    // 📥 Получение ответа
    net::streambuf stream_buf;
    net::read_until(socket, stream_buf, '\n', ec);

    if (ec) {
        std::cout << "Error reading data"sv << std::endl;
        return 1;
    }

    std::string server_data{
        std::istreambuf_iterator<char>(&stream_buf),
        std::istreambuf_iterator<char>()
    };

    std::cout << "Server responded: "sv << server_data << std::endl;

    return 0;
}
