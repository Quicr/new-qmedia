#pragma once
#include <bytes/bytes.h>
#include <hpke/random.h>
#include <mls/common.h>
#include <mls/state.h>

#include "delivery.h"

#include <deque>
#include <memory>

// Information needed per user to populate MLS state
struct MLSInitInfo
{
  mls::CipherSuite suite;
  mls::KeyPackage key_package;
  mls::HPKEPrivateKey init_key;
  mls::HPKEPrivateKey encryption_key;
  mls::SignaturePrivateKey signature_key;
  mls::Credential credential;

  MLSInitInfo(mls::CipherSuite suite, uint32_t endpoint_id);
};

struct ParsedJoinRequest
{
  uint32_t endpoint_id;
  mls::KeyPackage key_package;
};

struct ParsedLeaveRequest
{
  uint32_t endpoint_id;
  mls::epoch_t epoch;
};

class MLSSession
{
public:
  // Set up MLS state for the creator
  static MLSSession create(const MLSInitInfo& info, uint64_t group_id);

  // Join logic
  static MLSSession join(const MLSInitInfo& info, const mls::Welcome& welcome);
  static ParsedJoinRequest parse_join(delivery::JoinRequest&& join);
  bool obsolete(const ParsedJoinRequest& req) const;

  // Leave logic
  mls::MLSMessage leave();
  std::optional<ParsedLeaveRequest> parse_leave(delivery::LeaveRequest&& leave);
  bool obsolete(const ParsedLeaveRequest& req) const;

  // Form a commit
  std::tuple<mls::MLSMessage, mls::Welcome> commit(
    bool force_path,
    const std::vector<ParsedJoinRequest>& joins,
    const std::vector<ParsedLeaveRequest>& leaves);

  // Whether a given MLSMessage is for the current epoch
  bool current(const mls::MLSMessage& message) const;
  bool future(const mls::MLSMessage& message) const;

  // Measure the proximity of this member to a set of changes
  uint32_t distance_from(size_t n_adds,
                         const std::vector<ParsedLeaveRequest>& leaves) const;

  // Commit handling
  enum struct HandleResult : uint8_t
  {
    ok,
    fail,
    removes_me,
  };
  HandleResult handle(const mls::MLSMessage& commit);

  // Access to the underlying MLS state
  uint32_t index() const { return get_state().index().val; }
  uint64_t epoch() const { return get_state().epoch(); }
  bytes epoch_authenticator() const
  {
    return get_state().epoch_authenticator();
  }
  size_t member_count() const;

private:
  MLSSession(mls::State&& state);
  bytes fresh_secret() const;

  const mls::State& get_state() const;
  mls::State& get_state();

  static uint32_t endpoint_id_from_cred(const mls::Credential& cred);
  std::optional<mls::LeafIndex> leaf_for_endpoint_id(
    uint32_t endpoint_id) const;

  void add_state(mls::State&& state);
  bool has_state(mls::epoch_t epoch);
  mls::State& get_state(mls::epoch_t epoch);

  std::deque<mls::State> history;
  std::optional<mls::MLSMessage> cached_commit;
  std::optional<mls::State> cached_next_state;

  static constexpr size_t max_history_depth = 10;
  static const mls::MessageOpts message_opts;
};
