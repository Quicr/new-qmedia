#include "../../new-qmedia/include/qmedia/mls_session.h"

#include <iostream>
#include <numeric>
#include <set>

using namespace mls;

MLSInitInfo::MLSInitInfo(CipherSuite suite_in, uint32_t endpoint_id_in)
  : suite(suite_in)
  , init_key(HPKEPrivateKey::generate(suite))
  , encryption_key(HPKEPrivateKey::generate(suite))
  , signature_key(SignaturePrivateKey::generate(suite))
  , credential(Credential::basic(tls::marshal(endpoint_id_in)))
{
  auto leaf_node = LeafNode{ suite,
                             encryption_key.public_key,
                             signature_key.public_key,
                             credential,
                             Capabilities::create_default(),
                             Lifetime::create_default(),
                             ExtensionList{},
                             signature_key };

  key_package = KeyPackage{
    suite, init_key.public_key, leaf_node, ExtensionList{}, signature_key
  };
}

const MessageOpts MLSSession::message_opts = {
  .encrypt = true,
  .authenticated_data = {},
  .padding_size = 0,
};

MLSSession
MLSSession::create(const MLSInitInfo& info, uint64_t group_id)
{
  auto mls_state = State{ tls::marshal(group_id),     info.suite,
                          info.encryption_key,        info.signature_key,
                          info.key_package.leaf_node, {} };
  return { std::move(mls_state) };
}

MLSSession
MLSSession::join(const MLSInitInfo& info, const mls::Welcome& welcome)
{
  auto state = State{ info.init_key,
                      info.encryption_key,
                      info.signature_key,
                      info.key_package,
                      welcome,
                      std::nullopt,
                      {} };
  return { { std::move(state) } };
}

ParsedJoinRequest
MLSSession::parse_join(delivery::JoinRequest&& join)
{
  const auto endpoint_id =
    endpoint_id_from_cred(join.key_package.leaf_node.credential);
  return { endpoint_id, join.key_package };
}

bool
MLSSession::obsolete(const ParsedJoinRequest& req) const
{
  return !!leaf_for_endpoint_id(req.endpoint_id);
}

mls::MLSMessage
MLSSession::leave()
{
  const auto index = get_state().index();
  return get_state().remove(index, message_opts);
}

std::optional<ParsedLeaveRequest>
MLSSession::parse_leave(delivery::LeaveRequest&& leave)
{
  // Import the message
  const auto epoch = leave.proposal.epoch();
  if (!has_state(epoch)) {
    return std::nullopt;
  }

  auto mls_state = get_state(epoch);
  if (leave.proposal.group_id() != mls_state.group_id()) {
    return std::nullopt;
  }

  const auto leave_auth_content = mls_state.unwrap(leave.proposal);
  const auto& leave_content = leave_auth_content.content;
  const auto& leave_sender = leave_content.sender.sender;

  // Verify that this is a self-remove proposal
  const auto& remove_proposal = var::get<Proposal>(leave_content.content);
  const auto& remove = var::get<Remove>(remove_proposal.content);
  const auto& sender = var::get<MemberSender>(leave_sender).sender;
  if (remove.removed != sender) {
    return std::nullopt;
  }

  // Verify that the self-removed user has the indicated user ID
  const auto leaf = mls_state.tree().leaf_node(remove.removed).value();
  const auto endpoint_id = endpoint_id_from_cred(leaf.credential);

  return { { endpoint_id, epoch } };
}

bool
MLSSession::obsolete(const ParsedLeaveRequest& req) const
{
  return !leaf_for_endpoint_id(req.endpoint_id);
}

std::tuple<mls::MLSMessage, mls::Welcome>
MLSSession::commit(bool force_path,
                   const std::vector<ParsedJoinRequest>& joins,
                   const std::vector<ParsedLeaveRequest>& leaves)
{
  auto& mls_state = get_state();
  auto proposals = std::vector<Proposal>{};

  std::transform(
    joins.begin(),
    joins.end(),
    std::back_inserter(proposals),
    [&](const auto& req) { return mls_state.add_proposal(req.key_package); });

  std::transform(leaves.begin(),
                 leaves.end(),
                 std::back_inserter(proposals),
                 [&](const auto& req) {
                   const auto index = leaf_for_endpoint_id(req.endpoint_id);
                   return mls_state.remove_proposal(index.value());
                 });

  const auto commit_opts = CommitOpts{ proposals, true, force_path, {} };
  const auto [commit, welcome, next_state] =
    mls_state.commit(fresh_secret(), commit_opts, message_opts);

  cached_commit = commit;
  cached_next_state = std::move(next_state);
  return { commit, welcome };
}

bool
MLSSession::current(const MLSMessage& message) const
{
  return message.epoch() == get_state().epoch();
}

bool
MLSSession::future(const MLSMessage& msage) const
{
  return msage.epoch() > get_state().epoch();
}

static std::vector<LeafIndex>
add_locations(size_t n_adds, const TreeKEMPublicKey& tree)
{
  auto to_place = n_adds;
  auto places = std::vector<LeafIndex>{};
  for (auto i = LeafIndex{ 0 }; to_place > 0; i.val++) {
    if (i < tree.size && !tree.node_at(i).blank()) {
      continue;
    }

    places.push_back(i);
    to_place -= 1;
  }

  return places;
}

static uint32_t
total_distance(LeafIndex a, const std::vector<LeafIndex>& b)
{
  return std::accumulate(b.begin(), b.end(), 0, [&](auto last, auto bx) {
    const auto topological_distance = a.ancestor(bx).level();
    return last + topological_distance;
  });
}

uint32_t
MLSSession::distance_from(size_t n_adds,
                          const std::vector<ParsedLeaveRequest>& leaves) const
{
  auto& mls_state = get_state();
  auto removed = std::set<mls::LeafIndex>{};
  std::transform(leaves.begin(),
                 leaves.end(),
                 std::inserter(removed, removed.begin()),
                 [&](const auto& req) {
                   return leaf_for_endpoint_id(req.endpoint_id).value();
                 });

  auto affected = add_locations(n_adds, mls_state.tree());
  affected.insert(affected.end(), removed.begin(), removed.end());

  return total_distance(mls_state.index(), affected);
}

MLSSession::HandleResult
MLSSession::handle(const mls::MLSMessage& commit)
{
  if (commit == cached_commit) {
    add_state(std::move(cached_next_state.value()));
    cached_commit.reset();
    cached_next_state.reset();
    return HandleResult::ok;
  }

  // The caller should assure that any handled commits are timely
  if (commit.epoch() != get_state().epoch()) {
    return HandleResult::fail;
  }

  // Attempt to handle the Commit
  // XXX(richbarn): It would be nice to unwrap the Commit here and explicitly
  // check whether there is a Remove proposal removing this client.  However,
  // this causes a double-decrypt, which fails because decrypting causes keys to
  // be erased.  Instead we assume that any failure due to an invalid proposal
  // list is this type of failure
  try {
    add_state(tls::opt::get(get_state().handle(commit)));
  } catch (const ProtocolError& exc) {
    if (std::string(exc.what()) == "Invalid proposal list") {
      return HandleResult::removes_me;
    }

    return HandleResult::fail;
  }

  return HandleResult::ok;
}

void
MLSSession::add_state(State&& state)
{
  while (history.size() > max_history_depth) {
    history.pop_back();
  }
  history.push_front(std::move(state));
}

const mls::State&
MLSSession::get_state() const
{
  return history.front();
}

mls::State&
MLSSession::get_state()
{
  return history.front();
}

bool
MLSSession::has_state(epoch_t epoch)
{
  auto it =
    std::find_if(history.begin(), history.end(), [&](const auto& state) {
      return state.epoch() == epoch;
    });
  return it != history.end();
}

mls::State&
MLSSession::get_state(epoch_t epoch)
{
  auto it =
    std::find_if(history.begin(), history.end(), [&](const auto& state) {
      return state.epoch() == epoch;
    });
  if (it == history.end()) {
    throw std::runtime_error("No state for epoch");
  }

  return *it;
}

size_t
MLSSession::member_count() const
{
  size_t members = 0;
  get_state().tree().all_leaves([&](auto /* i */, const auto& /* leaf */) {
    members += 1;
    return true;
  });
  return members;
}

MLSSession::MLSSession(mls::State&& state)
{
  add_state(std::move(state));
}

bytes
MLSSession::fresh_secret() const
{
  return random_bytes(get_state().cipher_suite().secret_size());
}

uint32_t
MLSSession::endpoint_id_from_cred(const Credential& cred)
{
  const auto& basic_cred = cred.get<BasicCredential>();
  return tls::get<uint32_t>(basic_cred.identity);
}

std::optional<LeafIndex>
MLSSession::leaf_for_endpoint_id(uint32_t endpoint_id) const
{
  auto out = std::optional<LeafIndex>{};
  get_state().tree().any_leaf([&](auto i, const auto& leaf) {
    auto match = endpoint_id_from_cred(leaf.credential) == endpoint_id;
    if (match) {
      out = i;
    }

    return match;
  });

  return out;
}
