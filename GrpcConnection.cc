#include "GrpcConnection.h"
#include "SocketConnection.h"

#include <android-base/logging.h>
#include <thread>

#include <grpc++/grpc++.h>

#include <network_api.grpc.pb.h>
#include <common.pb.h>

#include <android/hardware/automotive/vehicle/2.0/types.h>
#include <VehicleHalProto.pb.h>
#include <VehicleHalProto.grpc.pb.h>

using ::android::hardware::automotive::vehicle::V2_0::StatusCode;
using ::android::hardware::automotive::vehicle::V2_0::VehicleProperty;
using ::android::hardware::automotive::vehicle::V2_0::VehiclePropertyStatus;
using ::android::hardware::automotive::vehicle::V2_0::VehiclePropValue;

using namespace grpc;
using namespace base;

std::map<std::string, int> signalMap
  {
   {"SteeringAngle129", toInt(VehicleProperty::PERF_STEERING_ANGLE)},
     // This is not visible in kitchensinkapp so map DI_uiSpeed to PERF_VEHICLE_SPEED
     //{"DI_uiSpeed", toInt(VehicleProperty::PERF_VEHICLE_SPEED_DISPLAY)},
   {"DI_uiSpeed", toInt(VehicleProperty::PERF_VEHICLE_SPEED)},
  };

GrpcConnection::GrpcConnection(std::shared_ptr<Channel> channel, std::atomic<bool>* shutdown) 
    : stub(NetworkService::NewStub(channel))
  {
    source = std::make_unique<ClientId>();
    name_space = std::make_unique<NameSpace>();
    source->set_id("my_unique_client_id");
    name_space->set_name("ChassiBus");
    this->shutdown = shutdown;

    subscriber();
  }

GrpcConnection::~GrpcConnection() {
  if (mSubscriber.joinable()) {
    mSubscriber.join();
  }
}


int writePropvalue(int signal, float value, uint timestamp, SocketConnection* s) {
  vhal_proto::EmulatorMessage msg;
  vhal_proto::VehiclePropValue* pValue = msg.add_value();
  pValue->set_prop(signal);
  pValue->add_float_values(value);
  pValue->set_status(vhal_proto::AVAILABLE);
  pValue->set_timestamp(timestamp);
  msg.set_msg_type(vhal_proto::SET_PROPERTY_CMD);

  int numBytes = msg.ByteSize();
  std::vector<uint8_t> buffer(static_cast<size_t>(numBytes));
  if (!msg.SerializeToArray(buffer.data(), numBytes)) {
    LOG(ERROR) << __func__ << "SerializeToString failed!";
    return -1;
  }
  s->send(buffer);

  std::vector<uint8_t> read_buffer;
  read_buffer = s->read();
  if (read_buffer.size() == 0) {
    LOG(ERROR) << __func__ <<"Read returned empty message";
    return -1;
  }

  vhal_proto::EmulatorMessage respMsg;
  if (respMsg.ParseFromArray(read_buffer.data(), static_cast<int32_t>(read_buffer.size()))) {
    if (respMsg.status() == vhal_proto::RESULT_OK) {
      return 0;
    } else {
      LOG(ERROR) << __func__ << " Not expected response type=" << respMsg.msg_type() << " status=" << respMsg.status();
    }
  }

  return -1;
}

void GrpcConnection::subscriber() {
    mSubscriber = std::thread([this]() {
      auto signals = new SignalIds();
      // add any number of signals...
      {
        auto handle = signals->add_signalid();
        handle->set_allocated_name(new std::string("SteeringAngle129"));
        handle->set_allocated_namespace_(new NameSpace(*name_space));
      }

      {
        auto handle = signals->add_signalid();
        handle->set_allocated_name(new std::string("DI_uiSpeed"));
        handle->set_allocated_namespace_(new NameSpace(*name_space));
      }

      SubscriberConfig sub_info;
      sub_info.set_allocated_clientid(new ClientId(*source));
      sub_info.set_allocated_signals(signals);
      sub_info.set_onchange(true);

      ClientContext ctx;
      Empty empty;

      auto socket = new SocketConnection();
      socket->connect();

      LOG(INFO) << "Subscribing";

      Signals signalsreturned;

      std::unique_ptr<ClientReader<Signals>> reader(stub->SubscribeToSignals(&ctx, sub_info));

      LOG(INFO) << "Start reader";

      while (reader->Read(&signalsreturned)){

	if (*shutdown == true) {
	  LOG(INFO) << "Exit reader";
	  ctx.TryCancel();
	  break;
	}

        for (int i = 0; i < signalsreturned.signal_size(); i++){
          auto name = signalsreturned.signal(i).id().name();
          int propId = signalMap.at(name);
          switch (propId) {
          case toInt(VehicleProperty::PERF_STEERING_ANGLE):
	    LOG(INFO) << "Got signal " << propId << " " << name << " : " << signalsreturned.signal(i).double_();
            writePropvalue(propId, (float)signalsreturned.signal(i).double_(),
                           signalsreturned.signal(i).timestamp(), socket);
            break;
          case toInt(VehicleProperty::PERF_VEHICLE_SPEED):
	  case toInt(VehicleProperty::PERF_VEHICLE_SPEED_DISPLAY):
	    LOG(INFO) << "Got signal " << propId << " " << name << " : " << signalsreturned.signal(i).integer();
            writePropvalue(propId, (float)signalsreturned.signal(i).integer(),
                           signalsreturned.signal(i).timestamp(), socket);
	    break;
          default:
            LOG(ERROR) << __func__ <<" Not supporting prop id " << propId;
          }
        }
      }

      Status status = reader->Finish();

      socket->close();

      LOG(INFO) << "Exit " <<__func__;
   });
}

