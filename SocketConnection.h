#include <thread>
#include <vector>

class SocketConnection {
 public:
    SocketConnection();

    bool connect();
    int send(std::vector<uint8_t>& data);
    std::vector<uint8_t> read();
    void close();

 private:
    int mSocketFd;
    int32_t readInt(int fd);
    std::vector<uint8_t> readExactly(int fd, int numBytes);

};
