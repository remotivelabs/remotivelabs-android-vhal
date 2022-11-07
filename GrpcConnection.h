#include "grpc++/grpc++.h"
#include <network_api.grpc.pb.h>
#include <thread>

template<typename ENUM, typename U = typename std::underlying_type<ENUM>::type>
inline constexpr U toInt(ENUM const value) {
    return static_cast<U>(value);
}

class GrpcConnection
{
public:
    GrpcConnection() = default;
    GrpcConnection(std::shared_ptr<grpc::Channel> channel, std::atomic<bool>* shutdown);
    ~GrpcConnection();

    void subscriber();

    std::thread mSubscriber;

private:
    std::unique_ptr<base::NetworkService::Stub> stub;
    std::unique_ptr<base::ClientId> source;
    std::unique_ptr<base::NameSpace> name_space;
    std::atomic<bool>* shutdown;
};
