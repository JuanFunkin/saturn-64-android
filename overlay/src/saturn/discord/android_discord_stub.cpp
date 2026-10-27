// Discord Rich Presence does not exist on Android: empty replacements for the symbols saturn_imgui.cpp and
// saturn.cpp reference, so saturn_discord.cpp (and the Discord Game SDK) are not compiled.
#ifdef __ANDROID__

#include "saturn/discord/saturn_discord.h"

struct DiscordApplication app;
struct DiscordActivity gCurActivity;
struct DiscordUser gCurUser;

std::string discord_state = "";
std::string model_details = "";
std::string cc_details = "";

extern "C" {
    void sdiscord_init(void) {}
    void sdiscord_update(void) {}
    void sdiscord_shutdown(void) {}
}

#endif
