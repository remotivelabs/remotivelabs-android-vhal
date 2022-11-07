#include <android-base/logging.h>
#include <grpc++/grpc++.h>
#include <thread>

#include "GrpcConnection.h"

const char CUSTOM_CERTIFICATE[] = R"(
-----BEGIN CERTIFICATE-----
MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG
A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv
b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw
MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i
YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT
aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ
jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp
xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp
1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG
snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ
U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8
9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E
BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B
AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz
yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE
38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP
AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad
DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME
HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==
-----END CERTIFICATE-----
)";

std::atomic<bool> shutdown = false;

class MyCustomAuthenticator : public grpc::MetadataCredentialsPlugin
{
public:
  MyCustomAuthenticator(const grpc::string &ticket) : ticket_(ticket) {}

  grpc::Status GetMetadata(
      grpc::string_ref service_url, grpc::string_ref method_name,
      const grpc::AuthContext &channel_auth_context,
      std::multimap<grpc::string, grpc::string> *metadata) override
  {
    metadata->insert(std::make_pair("x-api-key", ticket_));
    return grpc::Status::OK;
  }

private:
  grpc::string ticket_;
};

void signalHandler(int sig) {
  LOG(INFO) << "Grpc-service stopping received signal " << sig;
  shutdown = true;
}

void registerSigHandler() {
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = signalHandler;
  sigaction(SIGQUIT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGINT, &sa, nullptr);
}


int main(int argc, char* argv[]) {

  registerSigHandler();

  // use local certificate
  grpc::SslCredentialsOptions sslops;
  sslops.pem_root_certs = CUSTOM_CERTIFICATE;

  auto channel_creds_ = ::grpc::SslCredentials(sslops);

  // or use certificate from host
  // auto channel_creds_ = ::grpc::SslCredentials(::grpc::SslCredentialsOptions());

  auto call_creds = grpc::MetadataCredentialsFromPlugin(
      std::unique_ptr<grpc::MetadataCredentialsPlugin>(
          new MyCustomAuthenticator(argv[2])));

  auto compsited_creds = ::grpc::CompositeChannelCredentials(channel_creds_, call_creds);

  grpc::ChannelArguments cargs;

  auto connector = new GrpcConnection(CreateCustomChannel(argv[1], compsited_creds, cargs), &shutdown);

  LOG(INFO) << "Start service";

  connector->mSubscriber.join();

  return 1;
}
