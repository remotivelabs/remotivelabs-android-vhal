#include "SocketConnection.h"

#include <android-base/logging.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <vector>

static constexpr int VHAL_PORT = 33452;

SocketConnection::SocketConnection() {
}

bool SocketConnection::connect() {
  struct sockaddr_in servAddr;

  mSocketFd = socket(AF_INET, SOCK_STREAM, 0);
  if (mSocketFd < 0) {
    LOG(ERROR) << "ERROR open socket" << mSocketFd;
    return false;
  }
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(VHAL_PORT);

  if (::connect(mSocketFd, (sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
    LOG(ERROR) << "ERROR connect socket";
    return false;
  }
  return true;
}

int SocketConnection::send(std::vector<uint8_t>& data) {
    static constexpr int MSG_HEADER_LEN = 4;
    int retVal = 0;
    union {
        uint32_t msgLen;
        uint8_t msgLenBytes[MSG_HEADER_LEN];
    };

    // Prepare header for the message
    msgLen = static_cast<uint32_t>(data.size());
    msgLen = htonl(msgLen);

    if (mSocketFd > 0) {
        retVal = ::write(mSocketFd, msgLenBytes, MSG_HEADER_LEN);

        if (retVal == MSG_HEADER_LEN) {
            retVal = ::write(mSocketFd, data.data(), data.size());
        }
    }

    return retVal;
}

std::vector<uint8_t> SocketConnection::readExactly(int fd, int numBytes) {
    std::vector<uint8_t> buffer(numBytes);
    int totalRead = 0;
    int offset = 0;
    while (totalRead < numBytes) {
        int numRead = ::read(fd, &buffer.data()[offset], numBytes - offset);
        if (numRead == 0) {
            buffer.resize(0);
            return buffer;
        }

        totalRead += numRead;
    }
    return buffer;
}

int32_t SocketConnection::readInt(int fd) {
    std::vector<uint8_t> buffer = readExactly(fd, sizeof(int32_t));
    if (buffer.size() == 0) {
        return -1;
    }

    int32_t value = *reinterpret_cast<int32_t*>(buffer.data());
    return ntohl(value);
}

std::vector<uint8_t> SocketConnection::read() {
    int32_t msgSize = readInt(mSocketFd);
    if (msgSize <= 0) {
      LOG(DEBUG) <<  __FUNCTION__ << "Connection terminated on socket " <<  mSocketFd;
      return std::vector<uint8_t>();
    }

    return readExactly(mSocketFd, msgSize);
}

void SocketConnection::close() {
  ::close(mSocketFd);
  mSocketFd = -1;
}
