#include "seabattle.h"

#include <arpa/inet.h>
#include <cctype>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <utility>

static std::string MoveToString(std::pair<int, int> move) {
    char buff[] = {
        static_cast<char>(move.first + 'A'),
        static_cast<char>(move.second + '1')
    };
    return std::string(buff, 2);
}

static bool StringToMove(const std::string& s, std::pair<int, int>& move) {
    if (s.size() != 2) {
        return false;
    }

    char row = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    char col = s[1];

    if (row < 'A' || row > 'H' || col < '1' || col > '8') {
        return false;
    }

    move.first = row - 'A';
    move.second = col - '1';
    return true;
}

static void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << "     ";
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << '\n';

    for (size_t y = 0; y < SeabattleField::field_size; ++y) {
        left.PrintLine(std::cout, y);
        std::cout << "   ";
        right.PrintLine(std::cout, y);
        std::cout << '\n';
    }

    SeabattleField::PrintDigitLine(std::cout);
    std::cout << "     ";
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << '\n';
}

static void SendAll(int socket_fd, const std::string& data) {
    const char* ptr = data.data();
    size_t total = 0;

    while (total < data.size()) {
        ssize_t sent = send(socket_fd, ptr + total, data.size() - total, 0);
        if (sent <= 0) {
            throw std::runtime_error("send failed");
        }
        total += static_cast<size_t>(sent);
    }
}

static std::string RecvLine(int socket_fd) {
    std::string result;
    char ch = '\0';

    while (true) {
        ssize_t received = recv(socket_fd, &ch, 1, 0);
        if (received <= 0) {
            throw std::runtime_error("recv failed");
        }

        if (ch == '\n') {
            break;
        }
        if (ch != '\r') {
            result.push_back(ch);
        }
    }

    return result;
}

class SocketGuard {
public:
    explicit SocketGuard(int fd = -1)
        : fd_(fd) {
    }

    ~SocketGuard() {
        if (fd_ != -1) {
            close(fd_);
        }
    }

    int get() const {
        return fd_;
    }

    void reset(int fd = -1) {
        if (fd_ != -1) {
            close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_;
};

class SeabattleAgent {
public:
    explicit SeabattleAgent(const SeabattleField& field)
        : my_field_(field)
        , other_field_(SeabattleField::State::UNKNOWN) {
    }

    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    void StartGame(int socket_fd, bool is_client) {
        bool my_turn = !is_client;

        std::cout << (is_client ? "You are CLIENT\n" : "You are SERVER\n");

        while (true) {
            std::cout << "\n========================================\n";
            PrintFields();
            std::cout << "========================================\n";

            if (my_field_.IsLoser()) {
                std::cout << "You lose.\n";
                return;
            }
            if (other_field_.IsLoser()) {
                std::cout << "You win.\n";
                return;
            }

            if (my_turn) {
                std::pair<int, int> move = ReadMoveFromConsole();
                std::string move_str = MoveToString(move);

                SendAll(socket_fd, move_str + "\n");
                std::cout << "You shot: " << move_str << '\n';

                std::string response = RecvLine(socket_fd);

                if (response == "MISS") {
                    other_field_.MarkMiss(static_cast<size_t>(move.second), static_cast<size_t>(move.first));
                    std::cout << "Result: MISS\n";
                    my_turn = false;
                } else if (response == "HIT") {
                    other_field_.MarkHit(static_cast<size_t>(move.second), static_cast<size_t>(move.first));
                    std::cout << "Result: HIT\n";
                    my_turn = true;
                } else if (response == "KILL") {
                    other_field_.MarkKill(static_cast<size_t>(move.second), static_cast<size_t>(move.first));
                    std::cout << "Result: KILL\n";
                    my_turn = true;
                } else if (response == "LOSE") {
                    other_field_.MarkKill(static_cast<size_t>(move.second), static_cast<size_t>(move.first));
                    std::cout << "You win!\n";
                    return;
                } else {
                    throw std::runtime_error("unknown response from peer");
                }
            } else {
                std::cout << "Waiting for enemy move...\n";

                std::string move_str = RecvLine(socket_fd);
                std::pair<int, int> move;
                if (!StringToMove(move_str, move)) {
                    throw std::runtime_error("invalid move received");
                }

                std::cout << "Enemy shot: " << move_str << '\n';

                SeabattleField::ShotResult shot_result =
                    my_field_.Shoot(static_cast<size_t>(move.second), static_cast<size_t>(move.first));

                if (shot_result == SeabattleField::ShotResult::MISS) {
                    SendAll(socket_fd, "MISS\n");
                    std::cout << "Enemy missed\n";
                    my_turn = true;
                } else if (shot_result == SeabattleField::ShotResult::HIT) {
                    SendAll(socket_fd, "HIT\n");
                    std::cout << "Enemy hit your ship\n";
                    my_turn = false;
                } else {
                    if (my_field_.IsLoser()) {
                        SendAll(socket_fd, "LOSE\n");
                        std::cout << "All your ships are destroyed. You lose.\n";
                        return;
                    } else {
                        SendAll(socket_fd, "KILL\n");
                        std::cout << "Enemy killed your ship\n";
                        my_turn = false;
                    }
                }
            }
        }
    }

private:
    std::pair<int, int> ReadMoveFromConsole() const {
        while (true) {
            std::cout << "Enter move (A1-H8): ";
            std::string s;
            std::cin >> s;

            std::pair<int, int> move;
            if (!StringToMove(s, move)) {
                std::cout << "Invalid move format.\n";
                continue;
            }

            SeabattleField::State state =
                other_field_(static_cast<size_t>(move.second), static_cast<size_t>(move.first));

            if (state != SeabattleField::State::UNKNOWN) {
                std::cout << "You already shot there.\n";
                continue;
            }

            return move;
        }
    }

private:
    SeabattleField my_field_;
    SeabattleField other_field_;
};

void StartServer(const SeabattleField& field, unsigned short port) {
    SocketGuard listen_socket(socket(AF_INET, SOCK_STREAM, 0));
    if (listen_socket.get() == -1) {
        throw std::runtime_error("cannot create listen socket");
    }

    int opt = 1;
    if (setsockopt(listen_socket.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        throw std::runtime_error("setsockopt failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_socket.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        throw std::runtime_error("bind failed");
    }

    if (listen(listen_socket.get(), 1) == -1) {
        throw std::runtime_error("listen failed");
    }

    std::cout << "Server is listening on port " << port << "...\n";

    SocketGuard client_socket(accept(listen_socket.get(), nullptr, nullptr));
    if (client_socket.get() == -1) {
        throw std::runtime_error("accept failed");
    }

    std::cout << "Client connected.\n";

    SeabattleAgent agent(field);
    agent.StartGame(client_socket.get(), false);
}

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    SocketGuard sock(socket(AF_INET, SOCK_STREAM, 0));
    if (sock.get() == -1) {
        throw std::runtime_error("cannot create socket");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("invalid IP address");
    }

    std::cout << "Connecting to " << ip_str << ":" << port << "...\n";

    if (connect(sock.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        throw std::runtime_error("connect failed");
    }

    std::cout << "Connected to server.\n";

    SeabattleAgent agent(field);
    agent.StartGame(sock.get(), true);
}

int main(int argc, const char** argv) {
    try {
        if (argc != 3 && argc != 4) {
            std::cout << "Usage:\n";
            std::cout << "  Server: ./program <seed> <port>\n";
            std::cout << "  Client: ./program <seed> <ip> <port>\n";
            return 1;
        }

        std::mt19937 engine(std::stoi(argv[1]));
        SeabattleField field = SeabattleField::GetRandomField(engine);

        if (argc == 3) {
            StartServer(field, static_cast<unsigned short>(std::stoi(argv[2])));
        } else {
            StartClient(field, argv[2], static_cast<unsigned short>(std::stoi(argv[3])));
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
