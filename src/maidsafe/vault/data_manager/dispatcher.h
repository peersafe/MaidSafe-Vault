/*  Copyright 2013 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#ifndef MAIDSAFE_VAULT_DATA_MANAGER_DISPATCHER_H_
#define MAIDSAFE_VAULT_DATA_MANAGER_DISPATCHER_H_

#include <string>

#include "maidsafe/common/error.h"
#include "maidsafe/common/types.h"
#include "maidsafe/nfs/types.h"
#include "maidsafe/passport/types.h"
#include "maidsafe/data_types/data_type_values.h"
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/routing_api.h"

#include "maidsafe/nfs/message_types.h"
#include "maidsafe/nfs/types.h"

#include "maidsafe/vault/message_types.h"
#include "maidsafe/vault/types.h"
#include "maidsafe/vault/utils.h"
#include "maidsafe/vault/data_manager/data_manager.h"

namespace maidsafe {

namespace vault {

class DataManagerDispatcher {
 public:
  DataManagerDispatcher(routing::Routing& routing, const passport::Pmid& signing_fob)
      : routing_(routing), kSigningFob_(signing_fob) {}

  // =========================== Put section =======================================================
  // To PmidManager
  template <typename Data>
  void SendPutRequest(const PmidName& pmid_name, const Data& data, nfs::MessageId message_id);

  // To MaidManager
  template <typename Data>
  void SendPutResponse(const MaidName& account_name, const typename Data::Name& data_name,
                       int32_t cost, nfs::MessageId message_id);

  // To MaidManager
  template <typename Data>
  void SendPutFailure(const MaidName& maid_node, const typename Data::Name& data_name,
                      const maidsafe_error& error, nfs::MessageId message_id);

  // =========================== Get section (includes integrity checks) ===========================
  // To PmidNode
  template <typename Data>
  void SendGetRequest(const PmidName& pmid_node, const typename Data::Name& data_name,
                      nfs::MessageId message_id);

  // To PmidNode
  template <typename Data>
  void SendIntegrityCheck(const typename Data::Name& data_name, const NonEmptyString& random_string,
                          const PmidName& pmid_node, nfs::MessageId message_id);

  // To MaidNode or DataGetter
  template <typename RequestorIdType, typename Data>
  void SendGetResponseSuccess(const RequestorIdType& requestor_id, const Data& data,
                              nfs::MessageId message_id);

  // To MaidNode or DataGetter
  template <typename RequestorIdType, typename DataName>
  void SendGetResponseFailure(const RequestorIdType& requestor_id, const DataName& data_name,
                              const maidsafe_error& result, nfs::MessageId message_id);

  // To Self
  template<typename Data>
  void SendPutToCache(const Data& data);

  // To DataManager
  template<typename DataName>
  void SendGetFromCache(const DataName& data_name);

  // =========================== Delete section ====================================================
  // To PmidManager
  template <typename Data>
  void SendDeleteRequest(const PmidName& pmid_name, const typename Data::Name& data_name,
                         nfs::MessageId message_id);

  // =========================== Sync / AccountTransfer section ====================================
  void SendSync(const DataManager::Key& key, const std::string& serialised_sync);

  void SendAccountTransfer(const NodeId& destination_peer, const MaidName& account_name,
                           const std::string& serialised_account);

 private:
  DataManagerDispatcher();
  DataManagerDispatcher(const DataManagerDispatcher&);
  DataManagerDispatcher(DataManagerDispatcher&&);
  DataManagerDispatcher& operator=(DataManagerDispatcher);

  typedef std::true_type IsCacheable;
  typedef std::false_type IsNotCacheable;

  template<typename Data>
  void DoSendPutToCache(const Data& data, IsCacheable);

  template<typename Data>
  void DoSendPutToCache(const Data& /*data*/, IsNotCacheable) {}  // No-op

  template<typename DataName>
  void DoSendGetFromCache(const DataName& data_name, IsCacheable);

  template<typename DataName>
  void DoSendGetFromCache(const DataName& /*data_name*/, IsNotCacheable) {}  // No-op

  template <typename Message>
  void CheckSourcePersonaType() const;

  routing::Routing& routing_;
  const passport::Pmid kSigningFob_;
  const routing::SingleId kThisNodeAsSender_;
};

// ==================== Put implementation =========================================================
template <typename Data>
void DataManagerDispatcher::SendPutRequest(const PmidName& pmid_name, const Data& data,
                                           nfs::MessageId message_id) {
  typedef PutRequestFromDataManagerToPmidManager VaultMessage;
  CheckSourcePersonaType<VaultMessage>();
  typedef routing::Message<VaultMessage::Sender, VaultMessage::Receiver> RoutingMessage;

  VaultMessage vault_message(message_id, nfs_vault::DataNameAndContent(data));
  RoutingMessage message(vault_message.Serialise(),
                         GroupSender<VaultMessage, typename Data::Name>(routing_, data.name()),
                         VaultMessage::Receiver(NodeId(pmid_name->string())));
  routing_.Send(message);
}

template <typename Data>
void DataManagerDispatcher::SendPutResponse(const MaidName& account_name,
                                            const typename Data::Name& data_name,
                                            int32_t cost, nfs::MessageId message_id) {
  typedef PutResponseFromDataManagerToMaidManager VaultMessage;
  CheckSourcePersonaType<VaultMessage>();
  typedef routing::Message<VaultMessage::Sender, VaultMessage::Receiver> RoutingMessage;

  VaultMessage vault_message(message_id, nfs_vault::DataNameAndCost(data_name, cost));
  RoutingMessage message(vault_message.Serialise(),
                         GroupSender<VaultMessage, typename Data::Name>(routing_, data_name),
                         VaultMessage::Receiver(NodeId(account_name->string())));
  routing_.Send(message);
}

template <typename Data>
void DataManagerDispatcher::SendPutFailure(
    const MaidName& maid_node, const typename Data::Name& data_name, const maidsafe_error& error,
    nfs::MessageId message_id) {
  typedef PutFailureFromDataManagerToMaidManager VaultMessage;
  CheckSourcePersonaType<VaultMessage>();
  typedef routing::Message<VaultMessage::Sender, VaultMessage::Receiver> RoutingMessage;

  VaultMessage vault_message(message_id,
                             nfs_client::DataNameAndReturnCode(data_name,
                                                               nfs_client::ReturnCode(error)));
  RoutingMessage message(vault_message.Serialise(),
                         GroupSender<VaultMessage, typename Data::Name>(routing_, data_name),
                         VaultMessage::Receiver(routing::GroupId(NodeId(maid_node->string()))));
  routing_.Send(message);
}

// ==================== Get / IntegrityCheck implementation ========================================
template <typename Data>
void DataManagerDispatcher::SendGetRequest(const PmidName& pmid_node,
                                           const typename Data::Name& data_name,
                                           nfs::MessageId message_id) {
  // NB - This should NOT be marked as cacheable - we want to force the message to go all the way to
  // PmidNode to check it's online.
  typedef GetRequestFromDataManagerToPmidNode VaultMessage;
  CheckSourcePersonaType<VaultMessage>();
  typedef routing::Message<VaultMessage::Sender, VaultMessage::Receiver> RoutingMessage;

  VaultMessage vault_message(message_id, VaultMessage::Contents(data_name));
  RoutingMessage message(
      vault_message.Serialise(),
      GroupSender<VaultMessage, typename Data::Name>(routing_, data_name),
      VaultMessage::Receiver(routing::SingleId(NodeId(pmid_node->string()))));
  routing_.Send(message);
}

template <typename Data>
void DataManagerDispatcher::SendIntegrityCheck(const typename Data::Name& data_name,
                                               const NonEmptyString& random_string,
                                               const PmidName& pmid_node,
                                               nfs::MessageId message_id) {
  typedef IntegrityCheckRequestFromDataManagerToPmidNode VaultMessage;
  CheckSourcePersonaType<VaultMessage>();
  typedef routing::Message<VaultMessage::Sender, VaultMessage::Receiver> RoutingMessage;

  VaultMessage vault_message(message_id, VaultMessage::Contents(data_name, random_string));
  RoutingMessage message(
      vault_message.Serialise(),
      VaultMessage::Sender(routing::SingleId(routing_.kNodeId())),
      VaultMessage::Receiver(routing::SingleId(NodeId(pmid_node->string()))));
  routing_.Send(message);
}

namespace detail {

template <typename RequestorIdType>
struct GetResponseMessage;

template <>
struct GetResponseMessage<Requestor<nfs::SourcePersona<nfs::Persona::kMaidNode>>> {
  typedef nfs::GetResponseFromDataManagerToMaidNode Type;
};

template <>
struct GetResponseMessage<Requestor<nfs::SourcePersona<nfs::Persona::kDataGetter>>> {
  typedef nfs::GetResponseFromDataManagerToDataGetter Type;
};

}  // namespace detail

template <typename RequestorIdType, typename Data>
void DataManagerDispatcher::SendGetResponseSuccess(const RequestorIdType& requestor_id,
                                                   const Data& data, nfs::MessageId message_id) {
  typedef typename detail::GetResponseMessage<RequestorIdType>::Type NfsMessage;
  CheckSourcePersonaType<NfsMessage>();
  typedef routing::Message<typename NfsMessage::Sender, typename NfsMessage::Receiver>
      RoutingMessage;

  static const routing::Cacheable kCacheable(is_cacheable<Data>::value ? routing::Cacheable::kPut :
                                                                         routing::Cacheable::kNone);
  NfsMessage nfs_message(message_id, NfsMessage::Contents(data));
  RoutingMessage message(nfs_message.Serialise(),
                         GroupSender<NfsMessage, typename Data::Name>(routing_, data.name()),
                         NfsMessage::Receiver(routing::SingleId(requestor_id.node_id)), kCacheable);
  routing_.Send(message);
}

template <typename RequestorIdType, typename DataNameType>
void DataManagerDispatcher::SendGetResponseFailure(const RequestorIdType& requestor_id,
                                                   const DataNameType& data_name,
                                                   const maidsafe_error& result,
                                                   nfs::MessageId message_id) {
  typedef typename detail::GetResponseMessage<RequestorIdType>::Type NfsMessage;
  CheckSourcePersonaType<NfsMessage>();
  typedef routing::Message<typename NfsMessage::Sender, typename NfsMessage::Receiver>
      RoutingMessage;

  NfsMessage nfs_message(message_id,
                         NfsMessage::Contents(data_name, nfs_client::ReturnCode(result)));
  RoutingMessage message(nfs_message.Serialise(),
                         GroupSender<NfsMessage, DataNameType>(routing_, data_name),
                         NfsMessage::Receiver(routing::SingleId(requestor_id.node_id)));
  routing_.Send(message);
}

template<typename Data>
void DataManagerDispatcher::SendPutToCache(const Data& data) {
  DoSendPutToCache(data, is_cacheable<Data>());
}

template<typename Data>
void DataManagerDispatcher::DoSendPutToCache(const Data& data, IsCacheable) {
  typedef PutToCacheFromDataManagerToDataManager VaultMessage;
  CheckSourcePersonaType<VaultMessage>();
  typedef routing::Message<VaultMessage::Sender, VaultMessage::Receiver> RoutingMessage;

  VaultMessage vault_message((VaultMessage::Contents(data)));
  RoutingMessage message(vault_message.Serialise(),
                         VaultMessage::Sender(routing::SingleId(routing_.kNodeId())),
                         VaultMessage::Receiver(routing_.kNodeId()), routing::Cacheable::kPut);
  routing_.Send(message);
}

template<typename DataName>
void DataManagerDispatcher::SendGetFromCache(const DataName& data_name) {
  DoSendGetFromCache(data_name, is_cacheable<typename DataName::data_type>());
}

template<typename DataName>
void DataManagerDispatcher::DoSendGetFromCache(const DataName& data_name, IsCacheable) {
  typedef GetFromCacheFromDataManagerToDataManager VaultMessage;
  CheckSourcePersonaType<VaultMessage>();
  typedef routing::Message<VaultMessage::Sender, VaultMessage::Receiver> RoutingMessage;
  VaultMessage vault_message((VaultMessage::Contents(data_name)));
  RoutingMessage message(vault_message.Serialise(),
                         VaultMessage::Sender(routing::SingleId(routing_.kNodeId())),
                         VaultMessage::Receiver(routing::GroupId(NodeId(data_name->string()))),
                         routing::Cacheable::kGet);
  routing_.Send(message);
}

// ==================== Delete implementation ======================================================
template <typename Data>
void DataManagerDispatcher::SendDeleteRequest(const PmidName& pmid_node,
                                              const typename Data::Name& data_name,
                                              nfs::MessageId message_id) {
  typedef DeleteRequestFromDataManagerToPmidManager VaultMessage;
  CheckSourcePersonaType<VaultMessage>();
  typedef routing::Message<VaultMessage::Sender, VaultMessage::Receiver> RoutingMessage;

  VaultMessage vault_message(message_id, nfs_vault::DataName(data_name));
  RoutingMessage message(vault_message.Serialise(),
                         GroupSender<VaultMessage, typename Data::Name>(routing_, data_name),
                         VaultMessage::Receiver(NodeId(pmid_node->string())));
  routing_.Send(message);
}

// ==================== General implementation =====================================================

template<typename Message>
void DataManagerDispatcher::CheckSourcePersonaType() const {
  static_assert(Message::SourcePersona::value == nfs::Persona::kDataManager,
                "The source Persona must be kDataManager.");
}

}  // namespace vault

}  // namespace maidsafe

#endif  // MAIDSAFE_VAULT_DATA_MANAGER_DISPATCHER_H_
