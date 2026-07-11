#pragma once

#include "core.h"
#include "types.h"
#include <cstdint>

class DiscordClient {
public:
  DiscordClient();
  ~DiscordClient();

  bool init(std::int64_t client_id);
  void run_callbacks();
  void update_activity(const discord::Activity &activity);
  void shutdown();
  bool is_ready() const;

private:
  discord::Core *core_;
  bool ready_;
};
