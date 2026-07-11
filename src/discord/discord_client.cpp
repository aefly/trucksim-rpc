#include "discord_client.h"
#include "log.h"

DiscordClient::DiscordClient() : core_(nullptr), ready_(false) {}

DiscordClient::~DiscordClient() { shutdown(); }

bool DiscordClient::init(std::int64_t client_id) {
  if (core_)
    return true; // already initialized

  auto result =
      discord::Core::Create(client_id, DiscordCreateFlags_Default, &core_);
  if (result != discord::Result::Ok || !core_) {
    log_init_discord_fail();
    return false;
  }

  core_->SetLogHook(discord::LogLevel::Warn,
                    [](discord::LogLevel level, const char *message) {
                      (void)level;
                      log_warn("[Discord] %s", message);
                    });

  ready_ = true;
  log_init_discord_ok();
  return true;
}

void DiscordClient::run_callbacks() {
  if (!core_)
    return;
  auto result = core_->RunCallbacks();
  if (result != discord::Result::Ok && ready_) {
    log_warn("Discord RunCallbacks failed: %d", static_cast<int>(result));
  }
}

void DiscordClient::update_activity(const discord::Activity &activity) {
  if (!core_)
    return;
  core_->ActivityManager().UpdateActivity(activity, [](discord::Result) {});
}

void DiscordClient::shutdown() {
  if (core_) {
    ready_ = false; // prevent callbacks after deletion
    delete core_;
    core_ = nullptr;
    log_discord_shutdown();
  }
}

bool DiscordClient::is_ready() const { return ready_ && core_; }
