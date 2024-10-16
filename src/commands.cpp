#include <sstream>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/chat/chat.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "bomb.h"

std::map<std::string, int> MOB_IDS = {
    {"chicken", 10}, {"cow", 11},
    {"pig", 12}, {"sheep", 13},
    {"zombie", 32}, {"creeper", 33},
    {"skeleton", 34}, {"spider", 35},
    {"zpigman", 36}, {"tnt", 65},
    {"arrow", 80}, {"snowball", 81},
    {"egg", 82}
};

static char *next_arg(char *message, std::string &buf) {
    buf = "";
    if (*message == '\0') return message;
    while (*message == ' ') message++;
    while(*message != ' ' && *message != '\0') {
        buf += std::string(1, *message);
        message++;
    }
    return message;
}

struct Pos {
    float x, y, z;
};
static float parse_single(std::string &msg, float normal, bool &success, bool &rel) {
    rel = false;
    success = 1;
    if (msg == "") {
        success = 0;
        return 0;
    } else if (msg[0] == '~') {
        rel = true;
        msg.erase(0, 1);
        if (msg.size() == 0) {
            return normal;
        }
        std::stringstream s(msg);
        float pos;
        if (s >> pos) {
            return normal + pos;
        } else {
            success = 0;
            return 0;
        }
    } else {
        std::stringstream s(msg);
        float pos;
        if (s >> pos) {
            return pos;
        } else {
            success = 0;
            return 0;
        }
    }
}
static char *parse_pos(char *message, Minecraft *mc, Pos &pos, bool &success, bool allow_empty = false) {
    std::string content;
    message = next_arg(message, content);
    // Empty
    if (content == "") {
        // Assume set pos?
        success = allow_empty;
        return message;
    }
    bool rel = false;
    OffsetPosTranslator &offset = mc->command_server->pos_translator;
    // X
    pos.x = parse_single(content, pos.x, success, rel);
    if (!success) return message;
    if (!rel) {
        // Offset
        pos.x -= offset.x;
    }
    // Y
    message = next_arg(message, content);
    pos.y = parse_single(content, pos.y, success, rel);
    if (!success) return message;
    if (!rel) {
        // Offset
        pos.y -= offset.y;
    }
    // Z
    message = next_arg(message, content);
    pos.z = parse_single(content, pos.z, success, rel);
    if (!success) return message;
    if (!rel) {
        // Offset
        pos.z -= offset.z;
    }
    return message;
}

static std::map<std::string, std::string> emojis = {
    {"smile", "\1"},
    {"smile2", "\2"},
    {"heart", "\3"},
    {"diamond", "\4"},
    {"club", "\5"},
    {"spade", "\6"},
    {"dot", "\7"},
    {"dot2", "\10"},
    {"ring", "\11"},
    {"ring2", "\12"},
    {"male", "\13"},
    {"female", "\14"},
    {"music", "\15"},
    {"music2", "\16"},
    {"sun", "\17"},
    {"right2", "\20"},
    {"left2", "\21"},
    {"updown", "\22"},
    {"bangbang", "\23"},
    {"pilcrow", "\24"},
    {"FG6", "our all-powerful leader"},
    {"section", "\25"},
    {"rectangle", "\26"},
    {"updown2", "\27"},
    {"up", "\30"},
    {"down", "\31"},
    {"right", "\32"},
    {"left", "\33"},
    {"90", "\34"},
    {"leftright", "\35"},
    {"up2", "\36"},
    {"down2", "\37"}
    //{"", "\6"}
};
void replace_all(std::string &text, const std::string &search, const std::string &replace) {
    size_t pos = 0;
    while ((pos = text.find(search, pos)) != std::string::npos) {
        text.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}
static void replace_mojis(std::string &text) {
    for (auto i : emojis) {
        replace_all(text, ":" + i.first + ":", i.second);
    }
}

// The Actual Mod
HOOK(chat_handle_packet_send, void, (const Minecraft *minecraft_, ChatPacket *packet)) {
    Minecraft *minecraft = (Minecraft *) minecraft_;
    if (packet->message.c_str()[0] != '/' || packet->message.c_str()[1] == '/' || minecraft->level->is_client_side) {
        replace_mojis(packet->message);
        real_chat_handle_packet_send()(minecraft, packet);
    }
    // It's a command
    char *message = (char *) packet->message.c_str() + 1;
    std::string content = "";
    message = next_arg(message, content);
    // Handle the command
    if (content == "gamemode") {
        // Get gamemode
        message = next_arg(message, content);
        // Swap Game Mode
        if (content == "") {
            minecraft->setIsCreativeMode(!minecraft->is_creative_mode);
        } else if (content == "s" || content == "0" || content == "survival") {
            minecraft->setIsCreativeMode(0);
            minecraft->player->inventory->is_creative = 0;
        } else if (content == "c" || content == "1" || content == "creative" || content == "creator") {
            minecraft->setIsCreativeMode(1);
            minecraft->player->inventory->is_creative = 1;
        } else {
            minecraft->gui.addMessage("Unknown gamemode: '" + content + "', try /? gamemode for help");
            return;
        }
        // Reset inventory
        message = next_arg(message, content);
        if (content == "inv") {
            minecraft->player->inventory->clearInventoryWithDefault();
        }
    } else if (content == "clear") {
        minecraft->player->inventory->clearInventory();
    } else if (content == "inv") {
        minecraft->player->inventory->clearInventoryWithDefault();
    } else if (content == "kill_items") {
        int removed = 0;
        for (Entity *e : minecraft->level->entities) {
            if (e->isItemEntity() != 0) {
                e->remove();
                removed += 1;
            }
        }
        std::string s = "Removed " + std::to_string(removed) + " items";
        minecraft->gui.addMessage(s);
    } else if (content == "kill_all") {
        int removed = 0;
        for (Entity *e : minecraft->level->entities) {
            if (e->getEntityTypeId() != 0) {
                e->remove();
                removed += 1;
            }
        }
        std::string s = "Removed " + std::to_string(removed) + " entities";
        minecraft->gui.addMessage(s);
    } else if (content == "summon") {
        // Get id
        message = next_arg(message, content);
        int mob_id = 0;
        if (content.find_first_not_of("0123456789") == std::string::npos) {
            // It's an id
            mob_id = std::stoi(content);
        } else if (MOB_IDS.find(content) != MOB_IDS.end()) {
            mob_id = MOB_IDS[content];
        } else {
            minecraft->gui.addMessage("Unknown mob: '" + content + "', try /? summon for help");
            return;
        }
        // Get pos
        Pos pos{minecraft->player->x, minecraft->player->y, minecraft->player->z};
        bool success = true;
        message = parse_pos(message, minecraft, pos, success, true);
        if (!success) {
            minecraft->gui.addMessage("Failed to parse position");
            return;
        }
        // Summon
        Entity *entity = NULL;
        if (mob_id < 0x40) {
            entity = (Entity *) MobFactory::CreateMob(mob_id, minecraft->level);
        } else {
            entity = EntityFactory::CreateEntity(mob_id, minecraft->level);
        }
        if (entity == NULL) {
            minecraft->gui.addMessage("Failed to spawn entity");
            return;
        }
        entity->moveTo(pos.x, pos.y, pos.z, 0, 0);
        minecraft->level->addEntity(entity);
    }
}
