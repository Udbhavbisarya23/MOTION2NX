#include "abynbackend.h"

#define BOOST_FILESYSTEM_NO_DEPRECATED

#include <fmt/format.h>

#include "utility/constants.h"
#include "communication/message.h"
#include "communication/hellomessage.h"

namespace ABYN {

  ABYNBackend::ABYNBackend(ABYNConfigurationPtr &abyn_config) : abyn_config_(abyn_config) {
    abyn_core_ = std::make_shared<ABYN::ABYNCore>(abyn_config);

    for (auto i = 0u; i < abyn_config_->GetNumOfParties(); ++i) {
      if (abyn_config_->GetParty(i) == nullptr) { continue; }
      abyn_config_->GetParty(i)->InitializeMyRandomnessGenerator();
      abyn_config_->GetParty(i)->SetLogger(abyn_core_->GetLogger());
    }
  }

  void ABYNBackend::InitializeCommunicationHandlers() {
    using PartyCommunicationHandler = ABYN::Communication::PartyCommunicationHandler;
    communication_handlers_.resize(abyn_config_->GetNumOfParties(), nullptr);
    for (auto i = 0u; i < abyn_config_->GetNumOfParties(); ++i) {
      if (i == abyn_config_->GetMyId()) { continue; }
      auto message = fmt::format(
          "Party #{} creates CommHandler for Party #{} with end ip {}, local port {} and remote port {}",
          abyn_config_->GetMyId(), i,
          abyn_config_->GetParty(i)->GetIp(),
          abyn_config_->GetParty(i)->GetSocket()->local_endpoint().port(),
          abyn_config_->GetParty(i)->GetSocket()->remote_endpoint().port());
      abyn_core_->GetLogger()->LogDebug(message);

      communication_handlers_.at(i) =
          std::make_shared<PartyCommunicationHandler>(abyn_config_->GetParty(i), abyn_core_->GetLogger());
    }
  }

  void ABYNBackend::SendHelloToOthers() {
    abyn_core_->GetLogger()->LogInfo("Send hello message to other parties");
    for (auto destination_id = 0u; destination_id < abyn_config_->GetNumOfParties(); ++destination_id) {
      if (destination_id == abyn_config_->GetMyId()) { continue; }
      std::vector<u8> seed;
      if (share_inputs_) {
        seed = std::move(abyn_config_->GetParty(destination_id)->GetMyRandomnessGenerator()->GetSeed());
      }

      auto seed_ptr = share_inputs_ ? &seed : nullptr;
      auto hello_message = ABYN::Communication::BuildHelloMessage(abyn_config_->GetMyId(), destination_id,
                                                                  abyn_config_->GetNumOfParties(),
                                                                  seed_ptr,
                                                                  abyn_config_->OnlineAfterSetup(),
                                                                  ABYN::ABYN_VERSION);
      Send(destination_id, hello_message);
    }
  }

  void ABYNBackend::Send(size_t party_id, flatbuffers::FlatBufferBuilder &message) {
    if (party_id == abyn_config_->GetMyId()) { throw (std::runtime_error("Want to send message to myself")); }
    communication_handlers_.at(party_id)->SendMessage(message);
  }

  void ABYNBackend::RegisterInputGate(Gates::Interfaces::InputGatePtr &input_gate) {
    input_gates_.push_back(input_gate);
  }

  void ABYNBackend::EvaluateSequential() {
    //TODO
    for (auto &gate : input_gates_) {
      auto wires = gate->GetOutputShare()->GetWires();
      for (auto &wire : wires) {
        auto waiting_gates = wire->GetWaitingGatesIds();
      }
    }
  }

  void ABYNBackend::EvaluateParallel() {
//TODO
  }


  void ABYNBackend::TerminateCommunication() {
    for (auto party_id = 0u; party_id < communication_handlers_.size(); ++party_id) {
      if (communication_handlers_.at(party_id)) { communication_handlers_.at(party_id)->TerminateCommunication(); }
    }
  }

  void ABYNBackend::WaitForConnectionEnd() {
    for (auto &handler : communication_handlers_) {
      if (handler) { handler->WaitForConnectionEnd(); }
    }
  }

  void ABYNBackend::VerifyHelloMessages() {
    bool success = true;
    for (auto &handler : communication_handlers_) {
      if (handler) { success &= handler->VerifyHelloMessage(); }
    }

    if (!success) { abyn_core_->GetLogger()->LogError("Hello message verification failed"); }
    else { abyn_core_->GetLogger()->LogInfo("Successfully verified hello messages"); }
  }
}