#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include "../vendor/minhook/include/MinHook.h"
#include "choice_state.h"
#include "music_array.h"
#include <music_symbols.h>

// This companion patches only our own DLL. Unity/game executable bytes are untouched.
using Obj = void;
using Class = void;
using Method = void;
namespace fs = std::filesystem;
struct Vec3 {
    float x = 0, y = 0, z = 0;
};
struct Quaternion {
    float x, y, z, w;
};
struct Destination {
    unsigned transform;
    std::string name, identity;
    Vec3 aim;
    bool isDoor;
};
static_assert(sizeof(Destination) == 88, "Destination ABI differs from the companion manifest");
static fs::path base;
static HMODULE game, legacy;
static void (*originalRefresh)();
static void (*originalCommand)(const std::string &);
static void (*originalFeedback)(const std::string &);
static void (*originalTick)(float);
static void (*legacySay)(const std::string &, bool);
static std::vector<Destination> *destinations;
static int *selected;
static bool *manualSpeech;
static std::string *lastSubtitle;
static ChoiceState *choices;
static bool *moving;
static void **planner;
static std::atomic<bool> *navigationMouse;
// Enabled for the user-reported first music request; cleared once vinyl is held.
static bool explicitSelection = false, musicRequested = true;
static bool (*sampleFloor)(Vec3, Vec3 &);
static void (*prepareCollision)(Obj *);
static bool (*manualInput)();
static unsigned approachHandle = 0;
static bool deckMode = false;
struct PhoneControl {
    unsigned handle;
    int line;
    std::string label;
};
static std::vector<PhoneControl> phoneControls;
static bool phoneMode = false, soundboardMode = false;
static size_t phoneIndex = 0;
static unsigned volumeSliderHandle = 0;
static float expectedVolume = 0;
static unsigned long long volumeStarted = 0;
static bool browsingClues = false;
static unsigned placingDeck = 0, placingVinyl = 0, playingDeck = 0, playingVinyl = 0;
static unsigned long long playingStarted = 0;
static unsigned long long placingStarted = 0;
static Vec3 deckAim;
static std::mutex clickMutex;
static std::string previousSubtitle;
static unsigned musicObjectHandle = 0;
static Vec3 musicAim;
static bool browseMusic = false;
static size_t recordIndex = 0;
static std::atomic<bool> clickHeld{false};
static unsigned clickTarget = 0;
static unsigned long long clickStarted = 0;
static std::atomic<unsigned long long> clickDeadline{0};
static std::atomic<bool> watchdogReady{false};

static void *(*domain_get)();
static void *(*thread_attach)(void *);
static void (*thread_detach)(void *);
static void **(*domain_get_assemblies)(void *, size_t *);
static void *(*assembly_get_image)(void *);
static const char *(*image_get_name)(void *);
static Class *(*class_from_name)(void *, const char *, const char *);
static Class *(*object_get_class)(Obj *);
static Class *(*class_get_parent)(Class *);
static const char *(*class_get_name)(Class *);
static const Method *(*class_get_methods)(Class *, void **);
static const char *(*method_get_name)(const Method *);
static unsigned (*method_get_param_count)(const Method *);
static bool (*method_is_generic)(const Method *);
static const void *(*method_get_param)(const Method *, unsigned);
static char *(*type_get_name)(const void *);
static void (*ilfree)(void *);
static const void *(*class_get_type)(Class *);
static Obj *(*type_get_object)(const void *);
static Obj *(*object_new)(Class *);
static Obj *(*string_new)(const char *);
static Obj *(*runtime_invoke)(const Method *, Obj *, void **, Obj **);
static void *(*object_unbox)(Obj *);
static int (*string_length)(Obj *);
static const wchar_t *(*string_chars)(Obj *);
static void *(*class_get_field_from_name)(Class *, const char *);
static void (*field_get_value)(Obj *, void *, void *);
static unsigned (*gchandle_new)(Obj *, bool);
static Obj *(*gchandle_get_target)(unsigned);
static void (*gchandle_free)(unsigned);

static void log(const std::string &value) {
    std::ofstream f(base / "music-fix.log", std::ios::app);
    f << GetTickCount64() << " " << value << "\n";
}
static std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}
static std::string text(Obj *value) {
    if (!value)
        return {};
    int n = string_length(value);
    if (n < 0 || n > 200000)
        return {};
    auto chars = string_chars(value);
    int count = WideCharToMultiByte(CP_UTF8, 0, chars, n, nullptr, 0, nullptr, nullptr);
    std::string result(count, 0);
    WideCharToMultiByte(CP_UTF8, 0, chars, n, result.data(), count, nullptr, nullptr);
    return result;
}
static Class *cls(const char *assembly, const char *ns, const char *name) {
    size_t n = 0;
    auto all = domain_get_assemblies(domain_get(), &n);
    for (size_t i = 0; i < n; ++i) {
        auto image = assembly_get_image(all[i]);
        if (!strcmp(image_get_name(image), assembly))
            return class_from_name(image, ns, name);
    }
    return nullptr;
}
static const Method *method(Class *type, const char *name, unsigned n) {
    for (; type; type = class_get_parent(type)) {
        void *it = nullptr;
        while (auto m = class_get_methods(type, &it)) {
            if (strcmp(method_get_name(m), name) || method_get_param_count(m) != n ||
                method_is_generic(m))
                continue;
            if (n && (!strcmp(name, "FindObjectsOfType") ||
                      !strcmp(name, "GetComponentsInChildren") || !strcmp(name, "GetComponent"))) {
                auto value = type_get_name(method_get_param(m, 0));
                bool correct = value && !strcmp(value, "System.Type");
                if (value)
                    ilfree(value);
                if (!correct)
                    continue;
            }
            return m;
        }
    }
    return nullptr;
}
static Obj *call(const Method *m, Obj *o = nullptr, void **args = nullptr) {
    if (!m)
        return nullptr;
    Obj *exception = nullptr;
    auto r = runtime_invoke(m, o, args, &exception);
    if (exception) {
        log(std::string("Managed call failed: ") + method_get_name(m));
        return nullptr;
    }
    return r;
}
static Obj *invoke(Obj *o, const char *name, unsigned n = 0, void **args = nullptr) {
    return o ? call(method(object_get_class(o), name, n), o, args) : nullptr;
}
static Obj *field(Obj *o, const char *name) {
    if (!o)
        return nullptr;
    for (auto c = object_get_class(o); c; c = class_get_parent(c)) {
        auto f = class_get_field_from_name(c, name);
        if (f) {
            Obj *value = nullptr;
            field_get_value(o, f, &value);
            return value;
        }
    }
    return nullptr;
}
static std::vector<Obj *> array(Obj *value) { return musicArray(value); }
static std::vector<Obj *> find(const char *assembly, const char *ns, const char *name) {
    auto c = cls(assembly, ns, name);
    if (!c)
        return {};
    auto type = type_get_object(class_get_type(c));
    void *args[] = {type};
    return array(call(
        method(cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectsOfType", 1),
        nullptr, args));
}
static Vec3 position(Obj *transform) {
    auto value = invoke(transform, "get_position");
    return value ? *static_cast<Vec3 *>(object_unbox(value)) : Vec3{};
}
static float distance(Vec3 a, Vec3 b) {
    return std::hypot(a.x - b.x, a.z - b.z) + std::abs(a.y - b.y) * 12;
}
static Obj *player() {
    auto all = find("Assembly-CSharp.dll", "", "FlatScreenPlayer");
    return all.empty() ? nullptr : all.front();
}
static void say(const std::string &value) {
    log(value);
    bool previous = *manualSpeech;
    *manualSpeech = true;
    legacySay(value, true);
    *manualSpeech = previous;
}
static bool menuOpen() {
    for (auto menu : find("Assembly-CSharp.dll", "MenuSystem", "Menu")) {
        auto value = invoke(menu, "get_hasFocus");
        if (value && *static_cast<bool *>(object_unbox(value)))
            return true;
    }
    return false;
}

struct MusicTarget {
    Obj *object = nullptr;
    Obj *transform = nullptr;
    Vec3 aim;
    std::string name;
};
static std::vector<Obj *> records() {
    auto all = find("Assembly-CSharp.dll", "", "LPRecordSleeve");
    auto p = player();
    if (!p)
        return {};
    auto at = position(invoke(p, "GetRootTransform"));
    all.erase(std::remove_if(all.begin(), all.end(),
                             [&](Obj *o) {
                                 return std::hypot(at.x - position(invoke(o, "get_transform")).x,
                                                   at.z - position(invoke(o, "get_transform")).z) >
                                            2.5f ||
                                        std::abs(at.y - position(invoke(o, "get_transform")).y) >
                                            1.8f;
                             }),
              all.end());
    return all;
}
static Vec3 recordAim(Obj *record) {
    auto collider = field(record, "_boxCollider");
    auto bounds = invoke(collider, "get_bounds");
    return bounds ? *static_cast<Vec3 *>(object_unbox(bounds))
                  : position(invoke(record, "get_transform"));
}
static std::string recordName(Obj *record) {
    auto data = invoke(record, "get_recordData");
    auto title = text(field(data, "title"));
    return title.empty() ? "Music record" : title;
}
static bool truth(Obj *value) { return value && *static_cast<bool *>(object_unbox(value)); }
static bool controlled() {
    auto p = player();
    return p && truth(invoke(p, "get_controlEnabled"));
}
static Obj *heldMusic() {
    for (auto v : find("Assembly-CSharp.dll", "Interactables", "Vinyl")) {
        auto type = type_get_object(
            class_get_type(cls("Assembly-CSharp.dll", "Interactables", "Holdable")));
        void *args[] = {type};
        auto holdable = invoke(v, "GetComponent", 1, args);
        auto holder = invoke(holdable, "get_holder");
        if (holder && std::string(class_get_name(object_get_class(holder))).find("HolderPlayer") !=
                          std::string::npos)
            return v;
    }
    return nullptr;
}
static bool heldByPlayer(Obj *) { return heldMusic() != nullptr; }
static bool foreground() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}
static void aimAt(Vec3 target);
static bool faceRecord() {
    if (!controlled() || !foreground())
        return false;
    originalCommand("stop");
    auto all = records();
    if (all.empty())
        return false;
    recordIndex %= all.size();
    auto record = all[recordIndex];
    if (musicObjectHandle)
        gchandle_free(musicObjectHandle);
    musicObjectHandle = gchandle_new(record, false);
    musicAim = recordAim(record);
    aimAt(musicAim);
    browseMusic = true;
    say("Music record " + std::to_string(recordIndex + 1) + " of " + std::to_string(all.size()) +
        ". " + recordName(record) +
        ". Selected nearby record. F3 and F4 choose a record. F5 attempts pickup. F8 leaves music selection.");
    return true;
}
static bool sendMouse(DWORD flags) {
    INPUT value{};
    value.type = INPUT_MOUSE;
    value.mi.dwFlags = flags;
    return SendInput(1, &value, sizeof(value)) == 1;
}
static DWORD WINAPI releaseClick(void *) {
    watchdogReady = true;
    for (;;) {
        std::lock_guard<std::mutex> lock(clickMutex);
        if (clickHeld) {
            bool cancel = !foreground() || GetTickCount64() >= clickDeadline;
            for (int key : std::initializer_list<int>{VK_F8, VK_ESCAPE, 'W', 'A', 'S', 'D'})
                if (GetAsyncKeyState(key) & 0x8000)
                    cancel = true;
            if (cancel && sendMouse(MOUSEEVENTF_LEFTUP))
                clickHeld = false;
        }
        Sleep(5);
    }
    return 0;
}
static void pickup() {
    if (!controlled() || !foreground()) {
        say("Record interaction is unavailable while the game controls the character or is in the background.");
        return;
    }
    if (heldMusic()) {
        browseMusic = false;
        say("You are already holding a music record.");
        return;
    }
    originalCommand("stop");
    if (clickTarget || clickHeld) {
        say("Wait for the current record interaction.");
        return;
    }
    auto record = musicObjectHandle ? gchandle_get_target(musicObjectHandle) : nullptr;
    if (!record) {
        say("Select a music record first with F11.");
        return;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (pid != GetCurrentProcessId()) {
        say("Bring the game to the foreground before picking up the record.");
        return;
    }
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        say("Release the left mouse button first.");
        return;
    }
    aimAt(recordAim(record));
    if (!watchdogReady) {
        say("Record input monitor is not ready.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(clickMutex);
        if (!foreground())
            return;
        clickDeadline = GetTickCount64() + 90;
        if (!sendMouse(MOUSEEVENTF_LEFTDOWN)) {
            say("Could not send the pickup input.");
            return;
        }
        clickHeld = true;
    }
    clickTarget = gchandle_new(record, false);
    clickStarted = GetTickCount64();
    say("Picking up " + recordName(record) + ".");
}
static MusicTarget chooseTarget() {
    auto p = player();
    if (!p)
        return {};
    auto at = position(invoke(p, "GetRootTransform"));
    Obj *deck = nullptr;
    float nearest = 25;
    for (auto candidate : find("Assembly-CSharp.dll", "", "VinylPlayer")) {
        float d = distance(at, position(invoke(candidate, "get_transform")));
        if (d < nearest) {
            nearest = d;
            deck = candidate;
        }
    }
    if (!deck || heldMusic() || !musicRequested)
        return {};
    if (field(deck, "_vinyl"))
        return {};
    MusicTarget best;
    nearest = 25;
    for (auto drawer : find("Assembly-CSharp.dll", "", "RecordDrawer")) {
        auto transform = invoke(drawer, "get_transform");
        auto p0 = position(transform);
        float d = distance(at, p0);
        if (d >= nearest)
            continue;
        auto slot = field(drawer, "_firstSlotPosition");
        if (slot) {
            transform = slot;
            p0 = position(slot);
            d = distance(at, p0);
        }
        best = {drawer, transform, p0, "Music record drawer"};
        nearest = d;
    }
    if (best.object)
        return best;
    for (auto type : {"LPRecordSleeve", "RecordSleeve"})
        for (auto record : find("Assembly-CSharp.dll", "", type)) {
            auto transform = invoke(record, "get_transform");
            auto p0 = position(transform);
            float d = distance(at, p0);
            if (d < nearest) {
                best = {record, transform, p0, "Music record"};
                nearest = d;
            }
        }
    return best;
}
static MusicTarget turntable() {
    auto p = player();
    if (!p)
        return {};
    auto at = position(invoke(p, "GetRootTransform"));
    MusicTarget best;
    float nearest = 60;
    for (auto deck : find("Assembly-CSharp.dll", "", "VinylPlayer")) {
        auto transform = invoke(deck, "get_transform");
        auto aim = position(transform);
        auto dock = field(deck, "_vinylDock");
        if (dock)
            aim = position(invoke(dock, "get_transform"));
        float d = distance(at, aim);
        if (d < nearest) {
            nearest = d;
            best = {deck, transform, aim, "Turntable"};
        }
    }
    return best;
}
static MusicTarget phoneTarget() {
    if (!phoneMode || phoneControls.empty())
        return {};
    phoneIndex %= phoneControls.size();
    auto button = gchandle_get_target(phoneControls[phoneIndex].handle);
    if (!button || !truth(invoke(invoke(button, "get_gameObject"), "get_activeInHierarchy")))
        return {};
    auto transform = invoke(button, "get_transform");
    return {button, transform, position(transform),
            phoneControls[phoneIndex].label.empty()
                ? "Phone line " + std::to_string(phoneControls[phoneIndex].line)
                : phoneControls[phoneIndex].label};
}
static bool needsTurntable() {
    auto deck = turntable();
    return heldMusic() || (deck.object && field(deck.object, "_vinyl") &&
                           !truth(invoke(deck.object, "IsPlaying")));
}
static std::string turntablePrompt() {
    auto deck = turntable();
    return deck.object && field(deck.object, "_vinyl")
               ? "F5 starts the record if power is available."
               : "F5 places the held record if the game allows it.";
}
static Obj *floorApproach(const MusicTarget &deck) {
    auto p = player();
    if (!p)
        return nullptr;
    auto at = position(invoke(p, "GetRootTransform"));
    prepareCollision(field(p, "characterController"));
    Vec3 best;
    float closest = 1e9f;
    bool found = false;
    for (int i = 0; i < 16; ++i) {
        float angle = i * 6.2831853f / 16;
        Vec3 guess{deck.aim.x + std::cos(angle) * 1.35f, at.y,
                   deck.aim.z + std::sin(angle) * 1.35f},
            ground;
        if (!sampleFloor(guess, ground) || std::abs(ground.y - at.y) > .65f)
            continue;
        float d = std::hypot(at.x - ground.x, at.z - ground.z);
        if (d < closest) {
            closest = d;
            best = ground;
            found = true;
        }
    }
    if (!found)
        return nullptr;
    auto transform = approachHandle ? gchandle_get_target(approachHandle) : nullptr;
    if (!transform ||
        !truth(invoke(invoke(transform, "get_gameObject"), "get_activeInHierarchy"))) {
        auto go = object_new(cls("UnityEngine.CoreModule.dll", "UnityEngine", "GameObject"));
        auto label = string_new("Accessibility turntable floor approach");
        void *args[] = {label};
        invoke(go, ".ctor", 1, args);
        transform = invoke(go, "get_transform");
        if (!transform)
            return nullptr;
        if (approachHandle)
            gchandle_free(approachHandle);
        approachHandle = gchandle_new(transform, false);
    } // Base start() backs non-door targets off by .9m. Compensate so the final endpoint is the
      // sampled floor point.
    float length = std::hypot(best.x - at.x, best.z - at.z);
    Vec3 anchor = best;
    if (length > .1001f) {
        anchor.x += (best.x - at.x) / length * .9f;
        anchor.z += (best.z - at.z) / length * .9f;
    }
    void *args[] = {&anchor};
    invoke(transform, "set_position", 1, args);
    return transform;
}
static void refresh() {
    std::string previous = (*selected >= 0 && *selected < (int)destinations->size())
                               ? (*destinations)[*selected].identity
                               : "";
    originalRefresh();
    auto p = player();
    auto at = p ? position(invoke(p, "GetRootTransform")) : Vec3{};
    std::vector<Destination> useful;
    std::map<std::string, bool> seen;
    for (auto &d : *destinations) {
        auto key = lower(d.name);
        auto pos = position(gchandle_get_target(d.transform));
        bool keep = d.isDoor || key.find("opening alley entrance") != std::string::npos;
        for (auto word : {"map", "note", "clue", "document", "ticket", "phone", "mixer", "console",
                          "pinboard", "recordplayer", "turntable"})
            if (key.find(word) != std::string::npos)
                keep = true;
        for (auto word :
             {"towel", "toiletpaper", "paper toss", "papertoss", "notebook cover", "slider", "knob",
              "button", "handle", "jack", "socket", "fader", "switch", "meter"})
            if (key.find(word) != std::string::npos)
                keep = false;
        if (std::abs(at.y - pos.y) > 2.f)
            keep = false;
        if (d.isDoor && key != "opening alley entrance" && key != "studio door" &&
            key != "office door" && key != "basement door" && key != "restroom door" &&
            key != "station exit" && key != "kitchen door")
            keep = false;
        auto unique = d.isDoor ? key : d.identity;
        if (keep && !seen[unique]) {
            seen[unique] = true;
            useful.push_back(std::move(d));
        } else
            gchandle_free(d.transform);
    }
    std::vector<Destination> places, clues;
    for (auto &d : useful) {
        auto name = lower(d.name);
        bool clue = false;
        for (auto token : {"map", "note", "clue", "document", "ticket"})
            if (name.find(token) != std::string::npos)
                clue = true;
        if (name.find("headphone") != std::string::npos) {
            gchandle_free(d.transform);
            continue;
        }
        if (clue)
            clues.push_back(std::move(d));
        else
            places.push_back(std::move(d));
    }
    if (browsingClues) {
        for (auto &d : places)
            gchandle_free(d.transform);
        *destinations = std::move(clues);
    } else {
        if (!clues.empty()) {
            auto transform = gchandle_get_target(clues.front().transform);
            places.push_back(Destination{gchandle_new(transform, false),
                                         "Notes and clues. " + std::to_string(clues.size()) +
                                             " items. F11 opens this group",
                                         "group:clues", clues.front().aim, false});
        }
        for (auto &d : clues)
            gchandle_free(d.transform);
        *destinations = std::move(places);
    }
    *selected = 0;
    if (phoneMode) {
        auto target = phoneTarget();
        if (target.object) {
            auto floor = floorApproach(target);
            if (floor)
                destinations->insert(destinations->begin(),
                                     Destination{gchandle_new(floor, false), target.name,
                                                 "task:phone", target.aim, false});
        }
    } else if (!browsingClues && needsTurntable()) {
        auto deck = turntable();
        if (deck.object) {
            deckAim = deck.aim;
            auto floor = floorApproach(deck);
            if (floor)
                destinations->insert(destinations->begin(),
                                     Destination{gchandle_new(floor, false), "Turntable",
                                                 "task:turntable", deck.aim, false});
        }
    } else if (!browsingClues && !explicitSelection) {
        auto target = chooseTarget();
        if (target.object) {
            if (musicObjectHandle)
                gchandle_free(musicObjectHandle);
            musicObjectHandle = gchandle_new(target.object, false);
            musicAim = target.aim;
            destinations->insert(
                destinations->begin(),
                Destination{gchandle_new(target.transform, false), target.name,
                            "music-task:" + text(invoke(target.object, "get_name")), target.aim,
                            false});
        }
    }
    if (phoneMode)
        *selected = 0;
    else if (explicitSelection)
        for (int i = 0; i < (int)destinations->size(); ++i)
            if ((*destinations)[i].identity == previous)
                *selected = i;
    log("Useful destinations " + std::to_string(destinations->size()));
}
static void aimAt(Vec3 target) {
    auto p = player();
    if (!p)
        return;
    auto camera = field(p, "cameraFirstPersonTransform"), handler = field(p, "_cameraHandler");
    auto at = position(camera);
    float dx = target.x - at.x, dy = target.y - at.y, dz = target.z - at.z;
    float yaw = std::atan2(dx, dz), pitch = -std::atan2(dy, std::hypot(dx, dz));
    float sy = std::sin(yaw / 2), cy = std::cos(yaw / 2), sp = std::sin(pitch / 2),
          cp = std::cos(pitch / 2);
    Quaternion q{cy * sp, sy * cp, -sy * sp, cy * cp};
    void *args[] = {&q};
    invoke(handler, "ResetToRotation", 1, args);
}
static void feedback(const std::string &value) {
    if (value.rfind("Stopped near Phone line", 0) == 0 ||
        value.rfind("Stopped near Sound", 0) == 0) {
        auto target = phoneTarget();
        if (target.object)
            aimAt(target.aim);
        originalFeedback("At the selected control approach. " + target.name +
                         " selected. F5 presses it. F8 exits the selector.");
        return;
    }
    if (value.rfind("Stopped near Turntable", 0) == 0) {
        deckMode = true;
        aimAt(deckAim);
        originalFeedback("Stopped at the turntable approach. Facing the turntable. " +
                         turntablePrompt() + " F8 leaves turntable assistance.");
    } else if (value.rfind("Stopped near Music record", 0) == 0) {
        aimAt(musicAim);
        originalFeedback(
            value +
            " Facing the music records. Press F5 to interact, or use the game's pick-up control.");
    } else
        originalFeedback(value);
}
static void dump() {
    for (auto levels : find("Assembly-CSharp.dll", "", "SliderAudioLevels")) {
        auto slider = field(levels, "_slider");
        auto v = invoke(slider, "get_SliderNormalizedValue");
        log("MIXER " + text(invoke(levels, "get_name")) +
            " parameter=" + text(field(levels, "sliderProperyName")) +
            " value=" + (v ? std::to_string(*static_cast<float *>(object_unbox(v))) : "unknown"));
    }

    auto pp = player();
    int mode = -1;
    auto mf = class_get_field_from_name(object_get_class(pp), "currentMoveMode");
    if (mf)
        field_get_value(pp, mf, &mode);
    log("MOVE MODE " + std::to_string(mode));
    auto cam = field(pp, "cameraFirstPersonTransform");
    auto cp = position(cam);
    auto fw = invoke(cam, "get_forward");
    auto d = turntable();
    log("CAM " + std::to_string(cp.x) + "," + std::to_string(cp.y) + "," + std::to_string(cp.z) +
        " DECK " + std::to_string(d.aim.x) + "," + std::to_string(d.aim.y) + "," +
        std::to_string(d.aim.z));
    if (fw) {
        auto f = *static_cast<Vec3 *>(object_unbox(fw));
        log("FORWARD " + std::to_string(f.x) + "," + std::to_string(f.y) + "," +
            std::to_string(f.z));
    }
    auto p = player();
    auto at = p ? position(invoke(p, "GetRootTransform")) : Vec3{};
    std::ofstream f(base / "music-state.json");
    f << "{\"player\":[" << at.x << "," << at.y << "," << at.z << "],\"objects\":[";
    bool first = true;
    for (auto type : {"RecordDrawer", "LPRecordSleeve", "RecordSleeve", "VinylPlayer"})
        for (auto object : find("Assembly-CSharp.dll", "", type)) {
            auto p0 = position(invoke(object, "get_transform"));
            if (!first)
                f << ",";
            first = false;
            f << "{\"type\":" << std::quoted(type)
              << ",\"name\":" << std::quoted(text(invoke(object, "get_name"))) << ",\"position\":["
              << p0.x << "," << p0.y << "," << p0.z
              << "],\"has_record\":" << (field(object, "_vinyl") ? "true" : "false")
              << ",\"is_playing\":" << (truth(invoke(object, "IsPlaying")) ? "true" : "false")
              << "}";
        }
    f << "]}";
    log("Music inventory written");
}
static Obj *component(Obj *object, const char *ns, const char *name) {
    auto c = cls("Assembly-CSharp.dll", ns, name);
    if (!object || !c)
        return nullptr;
    auto type = type_get_object(class_get_type(c));
    void *args[] = {type};
    return invoke(object, "GetComponent", 1, args);
}
static Obj *vinylHolder(Obj *vinyl) {
    return invoke(component(vinyl, "Interactables", "Holdable"), "get_holder");
}
static bool playerHeld(Obj *vinyl) {
    auto holder = vinylHolder(vinyl);
    return holder && std::string(class_get_name(object_get_class(holder))).find("HolderPlayer") !=
                         std::string::npos;
}
static void clearPlacement() {
    if (placingDeck)
        gchandle_free(placingDeck);
    if (placingVinyl)
        gchandle_free(placingVinyl);
    placingDeck = placingVinyl = 0;
}
static bool facingTurntable(const MusicTarget &deck) {
    auto p = player();
    auto cam = field(p, "cameraFirstPersonTransform");
    auto from = position(cam);
    auto boxed = invoke(cam, "get_forward");
    if (!boxed)
        return false;
    auto f = *static_cast<Vec3 *>(object_unbox(boxed));
    float x = deck.aim.x - from.x, y = deck.aim.y - from.y, z = deck.aim.z - from.z,
          length = std::sqrt(x * x + y * y + z * z);
    return length > .01f && (x * f.x + y * f.y + z * f.z) / length > .965f;
}
static bool placeRecord() {
    if (placingDeck) {
        say("Wait for the current record placement.");
        return true;
    }
    auto vinyl = heldMusic();
    auto deck = turntable();
    if (!vinyl || !deck.object)
        return false;
    auto p = player();
    auto at = position(invoke(p, "GetRootTransform"));
    if (std::hypot(at.x - deck.aim.x, at.z - deck.aim.z) > 2.2f ||
        std::abs(at.y - deck.aim.y) > 1.8f) {
        if (deckMode) {
            say("The turntable is out of reach. Press F8, then F11 to approach it again.");
            return true;
        }
        return false;
    }
    if (!deckMode && !facingTurntable(deck))
        return false;
    originalCommand("stop");
    auto dock = field(deck.object, "_vinylDock");
    auto holdable = component(vinyl, "Interactables", "PhysicsHoldable");
    if (!dock || !holdable) {
        say("The record placement control is unavailable. You are still holding the record.");
        return true;
    }
    if (field(deck.object, "_vinyl")) {
        say("There is already a record on this turntable.");
        return true;
    }
    void *args[] = {holdable};
    if (!truth(invoke(dock, "CanDock", 1, args))) {
        say("The game is not allowing this record onto the turntable yet. The record remains in your hand.");
        return true;
    }
    if (!foreground() || !controlled() || choices->active || menuOpen() || !playerHeld(vinyl)) {
        say("Record placement cancelled because the game state changed.");
        return true;
    }
    placingDeck = gchandle_new(deck.object, false);
    placingVinyl = gchandle_new(vinyl, false);
    placingStarted = GetTickCount64();
    deckMode = true;
    invoke(dock, "Dock", 1, args);
    say("Placing the record on the turntable.");
    return true;
}
static void placementTick() {
    if (!placingDeck)
        return;
    if (!foreground() || !controlled() || choices->active || menuOpen()) {
        clearPlacement();
        return;
    }
    auto deck = gchandle_get_target(placingDeck), vinyl = gchandle_get_target(placingVinyl);
    if (deck && vinyl && field(deck, "_vinyl") == vinyl && !playerHeld(vinyl)) {
        clearPlacement();
        deckMode = false;
        say("Record placed on the turntable. Press F11, then F5 to start the record.");
        return;
    }
    if (GetTickCount64() - placingStarted > 3500) {
        clearPlacement();
        say("Placement was not confirmed. Press F11 to face the turntable, then F5 to retry.");
    }
}
static void clearPlayback() {
    if (playingDeck)
        gchandle_free(playingDeck);
    if (playingVinyl)
        gchandle_free(playingVinyl);
    playingDeck = playingVinyl = 0;
}
static bool startRecord() {
    auto deck = turntable();
    auto vinyl = deck.object ? field(deck.object, "_vinyl") : nullptr;
    if (!vinyl || heldMusic())
        return false;
    auto at = position(invoke(player(), "GetRootTransform"));
    if (std::hypot(at.x - deck.aim.x, at.z - deck.aim.z) > 2.2f ||
        std::abs(at.y - deck.aim.y) > 1.8f) {
        if (deckMode) {
            say("The turntable is out of reach. Press F8, then F11 to approach it.");
            return true;
        }
        return false;
    }
    if (!deckMode && !facingTurntable(deck))
        return false;
    if (playingDeck) {
        say("Wait for the playback check.");
        return true;
    }
    if (truth(invoke(deck.object, "IsPlaying"))) {
        say("Music is already playing on the turntable.");
        return true;
    }
    Obj *power = nullptr;
    for (auto cosmetics : find("Assembly-CSharp.dll", "", "VinylPlayerCosmetics"))
        if (field(cosmetics, "_vinylPlayer") == deck.object) {
            power = field(cosmetics, "_powerSupply");
            break;
        }
    if (!power) {
        say("Could not verify the turntable power control. Playback was not started.");
        return true;
    }
    if (!truth(invoke(power, "get_IsPowerOn"))) {
        say("The turntable has no power. Playback cannot start until station power is restored.");
        return true;
    }
    originalCommand("stop");
    if (!foreground() || !controlled() || choices->active || menuOpen() ||
        field(deck.object, "_vinyl") != vinyl) {
        say("Playback request cancelled because the game state changed.");
        return true;
    }
    playingDeck = gchandle_new(deck.object, false);
    playingVinyl = gchandle_new(vinyl, false);
    playingStarted = GetTickCount64();
    invoke(deck.object, "Play");
    say("Starting the record.");
    return true;
}
static void playbackTick() {
    if (!playingDeck)
        return;
    if (!foreground() || !controlled() || choices->active || menuOpen()) {
        clearPlayback();
        return;
    }
    auto deck = gchandle_get_target(playingDeck), vinyl = gchandle_get_target(playingVinyl);
    if (deck && vinyl && field(deck, "_vinyl") == vinyl && truth(invoke(deck, "IsPlaying"))) {
        clearPlayback();
        deckMode = false;
        say("Music is playing on the turntable. Listen for the next game instructions.");
        return;
    }
    if (GetTickCount64() - playingStarted > 3000) {
        clearPlayback();
        say("Playback was not confirmed. Press F8, then F11 and F5 to retry.");
    }
}
static void stopRecord() {
    if (!foreground()) {
        say("Bring the game to the foreground, then press Shift F5 to stop the music.");
        return;
    }
    if (menuOpen() || !controlled()) {
        say("Resume gameplay, then press Shift F5 to stop the music.");
        return;
    }
    auto p = player();
    auto at = position(invoke(p, "GetRootTransform"));
    Obj *target = nullptr;
    float closest = 15.f;
    bool foundPlaying = false;
    for (auto deck : find("Assembly-CSharp.dll", "", "VinylPlayer")) {
        if (!field(deck, "_vinyl"))
            continue;
        auto pos = position(invoke(deck, "get_transform"));
        if (std::abs(at.y - pos.y) > 2.f)
            continue;
        auto d = std::hypot(at.x - pos.x, at.z - pos.z);
        if (d >= 15.f)
            continue;
        auto status = invoke(deck, "IsPlaying");
        bool playing = truth(status);
        if ((playing && !foundPlaying) || (playing == foundPlaying && d < closest)) {
            closest = d;
            target = deck;
            foundPlaying = playing;
        }
    }
    if (!target) {
        say("No loaded turntable was found nearby.");
        return;
    }
    auto before = invoke(target, "IsPlaying");
    if (!before) {
        say("Could not check the record playback state. Press Shift F5 to retry.");
        return;
    }
    if (!*static_cast<bool *>(object_unbox(before))) {
        say("Music is already stopped.");
        return;
    }
    clearPlayback();
    invoke(target, "Stop");
    auto state = invoke(target, "IsPlaying");
    if (state && !*static_cast<bool *>(object_unbox(state)))
        say("Music stopped.");
    else
        say("The game has not confirmed that the music stopped. Press Shift F5 to retry.");
}
static std::string hierarchy(Obj *object) {
    auto t = invoke(object, "get_transform");
    std::string result;
    for (int i = 0; t && i < 12; ++i) {
        result = text(invoke(t, "get_name")) + "/" + result;
        t = invoke(t, "get_parent");
    }
    return result;
}
static void phoneDump() {
    for (auto flash : find("Assembly-CSharp.dll", "", "PhoneLineFlashing")) {
        int line = -1;
        auto f = class_get_field_from_name(object_get_class(flash), "phoneline");
        if (f)
            field_get_value(flash, f, &line);
        log("PHONE FLASH id=" + std::to_string(line) + " " + hierarchy(flash));
    }
    for (auto button : find("Assembly-CSharp.dll", "Interactables", "Button"))
        log("PHONE BUTTON " + std::string(class_get_name(object_get_class(button))) + " " +
            hierarchy(button));
    for (auto line : find("Assembly-CSharp.dll", "", "PhoneLine")) {
        auto status = invoke(line, "get_LineStatus");
        log("PHONE LEGACY " + hierarchy(line) + " state=" +
            (status ? std::to_string(*static_cast<int *>(object_unbox(status))) : "unknown"));
    }
}
static void closePhones() {
    phoneMode = false;
    soundboardMode = false;
    for (auto &item : phoneControls)
        gchandle_free(item.handle);
    phoneControls.clear();
    phoneIndex = 0;
}
static void describePhone() {
    auto target = phoneTarget();
    if (!target.object) {
        closePhones();
        say("The selected control is no longer available. Reopen the selector with Shift F6 for phones or Shift F7 for soundboard.");
        return;
    }
    std::string indicator;
    if (!soundboardMode)
        for (auto flash : find("Assembly-CSharp.dll", "", "PhoneLineFlashing")) {
            int line = -1;
            bool flashing = false;
            auto c = object_get_class(flash);
            auto f = class_get_field_from_name(c, "phoneline");
            if (f)
                field_get_value(flash, f, &line);
            if (line != phoneControls[phoneIndex].line)
                continue;
            f = class_get_field_from_name(c, "_isFlashing");
            if (f)
                field_get_value(flash, f, &flashing);
            if (flashing)
                indicator = " Indicator is flashing.";
        }
    say(target.name + " selected." + indicator +
        " F3 and F4 choose a control. F11 approaches it. F5 presses it. F8 exits the selector.");
}
static void openPhones() {
    if (!foreground() || !controlled() || menuOpen() || choices->active) {
        say("Return to gameplay and finish the current response before opening phone controls.");
        return;
    }
    originalCommand("stop");
    closePhones();
    browseMusic = deckMode = browsingClues = false;
    explicitSelection = false;
    auto at = position(invoke(player(), "GetRootTransform"));
    std::map<int, std::pair<float, Obj *>> matches;
    for (auto button : find("Assembly-CSharp.dll", "Interactables", "Button")) {
        auto path = hierarchy(button);
        if (path.find("FirstFloor_MainStudio/") == std::string::npos)
            continue;
        int line = 0;
        for (auto entry : {std::pair<const char *, int>{"PhonesLines_Button_One/", 1},
                           {"PhonesLines_Button_Two/", 2},
                           {"PhonesLines_Button_Three/", 3}})
            if (path.find(entry.first) != std::string::npos)
                line = entry.second;
        if (!line)
            continue;
        auto pos = position(invoke(button, "get_transform"));
        auto d = std::hypot(at.x - pos.x, at.z - pos.z);
        if (d > 15 || std::abs(at.y - pos.y) > 2.f)
            continue;
        if (!matches.count(line) || d < matches[line].first)
            matches[line] = {d, button};
    }
    for (auto &entry : matches)
        phoneControls.push_back({gchandle_new(entry.second.second, false), entry.first, {}});
    if (phoneControls.empty()) {
        say("No active studio phone buttons were found nearby.");
        return;
    }
    phoneMode = true;
    describePhone();
}
static void openSoundboard() {
    if (!foreground() || !controlled() || menuOpen() || choices->active) {
        say("Return to gameplay and finish the current response before opening the soundboard.");
        return;
    }
    originalCommand("stop");
    closePhones();
    browseMusic = deckMode = browsingClues = false;
    explicitSelection = false;
    auto at = position(invoke(player(), "GetRootTransform"));
    std::map<int, std::pair<float, Obj *>> matches;
    for (auto button : find("Assembly-CSharp.dll", "Interactables", "Button")) {
        auto path = hierarchy(button);
        if (path.find("FirstFloor_MainStudio/") == std::string::npos ||
            path.find("P_HeroProp_Soundboard/") == std::string::npos)
            continue;
        int id = 0;
        for (int i = 1; i <= 8; ++i)
            if (path.find("/Functional/Button_" + std::to_string(i) + "/") != std::string::npos)
                id = i;
        for (int i = 1; i <= 3; ++i)
            if (path.find("/Functional/Group" + std::to_string(i) + "/") != std::string::npos)
                id = 100 + i;
        if (!id)
            continue;
        auto pos = position(invoke(button, "get_transform"));
        auto d = std::hypot(at.x - pos.x, at.z - pos.z);
        if (d > 15 || std::abs(at.y - pos.y) > 2.f)
            continue;
        if (!matches.count(id) || d < matches[id].first)
            matches[id] = {d, button};
    }
    for (auto &entry : matches)
        phoneControls.push_back({gchandle_new(entry.second.second, false), entry.first,
                                 entry.first > 100
                                     ? "Sound group " + std::to_string(entry.first - 100)
                                     : "Sound effect button " + std::to_string(entry.first)});
    if (phoneControls.empty()) {
        say("No active soundboard buttons were found nearby.");
        return;
    }
    phoneMode = soundboardMode = true;
    describePhone();
}
static void activatePhone() {
    auto target = phoneTarget();
    if (!target.object) {
        say("The selected control is unavailable. Reopen the selector with Shift F6 for phones or Shift F7 for soundboard.");
        return;
    }
    auto at = position(invoke(player(), "GetRootTransform"));
    if (std::hypot(at.x - target.aim.x, at.z - target.aim.z) > 2.5f ||
        std::abs(at.y - target.aim.y) > 1.8f) {
        say("The selected control is out of reach. Press F11 to approach it, then F5.");
        return;
    }
    if (truth(invoke(target.object, "get_isOnCooldown"))) {
        say("This control is on cooldown. Wait briefly, then press F5.");
        return;
    }
    originalCommand("stop");
    if (!foreground() || !controlled() || menuOpen() || choices->active) {
        closePhones();
        return;
    }
    auto m = method(object_get_class(target.object), "Activate", 0);
    if (!m) {
        say("The game's button action is unavailable.");
        return;
    }
    Obj *exception = nullptr;
    runtime_invoke(m, target.object, nullptr, &exception);
    if (exception) {
        say("The game could not press the selected button.");
        return;
    }
    log(std::string(soundboardMode ? "SOUNDBOARD ACTIVATED " : "PHONE ACTIVATED ") + target.name);
    if (!choices->active && !menuOpen())
        say(target.name + " pressed.");
}
static void clearVolume() {
    if (volumeSliderHandle)
        gchandle_free(volumeSliderHandle);
    volumeSliderHandle = 0;
}
static void adjustMusicVolume(float delta) {
    if (!foreground() || !controlled() || menuOpen() || choices->active) {
        say("Return to gameplay and finish the current response before changing music volume.");
        return;
    }
    auto at = position(invoke(player(), "GetRootTransform"));
    Obj *slider = nullptr;
    float closest = 15.f;
    for (auto levels : find("Assembly-CSharp.dll", "", "SliderAudioLevels")) {
        if (text(field(levels, "sliderProperyName")) != "Record_Slider")
            continue;
        auto candidate = field(levels, "_slider");
        if (!candidate ||
            !truth(invoke(invoke(candidate, "get_gameObject"), "get_activeInHierarchy")))
            continue;
        auto pos = position(invoke(candidate, "get_transform"));
        float d = std::hypot(at.x - pos.x, at.z - pos.z);
        if (std::abs(at.y - pos.y) <= 2.f && d < closest) {
            slider = candidate;
            closest = d;
        }
    }
    if (!slider) {
        say("No studio music-volume slider was found nearby.");
        return;
    }
    auto value = invoke(slider, "get_SliderNormalizedValue");
    if (!value) {
        say("Could not read the music volume. Try again with Shift F3 or Shift F4.");
        return;
    }
    float current = *static_cast<float *>(object_unbox(value));
    if (!std::isfinite(current)) {
        say("The music-volume value is unavailable.");
        return;
    }
    if (volumeSliderHandle && gchandle_get_target(volumeSliderHandle) == slider)
        current = expectedVolume;
    float desired = std::clamp(current + delta, 0.f, 1.f);
    if (std::abs(desired - current) < .001f) {
        if (volumeSliderHandle) {
            say("The music-volume change is pending.");
            return;
        }
        say(desired < .5f ? "Music volume is already at zero percent."
                          : "Music volume is already at one hundred percent.");
        return;
    }
    auto m = method(object_get_class(slider), "MoveSliderToNormalizedValue", 1);
    if (!m) {
        say("The game's music-volume control is unavailable.");
        return;
    }
    if (!foreground() || !controlled() || menuOpen() || choices->active)
        return;
    Obj *exception = nullptr;
    void *args[] = {&desired};
    runtime_invoke(m, slider, args, &exception);
    if (exception) {
        say("The game could not move the music-volume slider.");
        return;
    }
    clearVolume();
    volumeSliderHandle = gchandle_new(slider, false);
    expectedVolume = desired;
    volumeStarted = GetTickCount64();
    log("VOLUME requested " + std::to_string(desired));
}
static void volumeTick() {
    if (!volumeSliderHandle)
        return;
    if (!foreground() || !controlled() || menuOpen() || choices->active) {
        clearVolume();
        return;
    }
    auto slider = gchandle_get_target(volumeSliderHandle);
    auto value = invoke(slider, "get_SliderNormalizedValue");
    if (value) {
        float actual = *static_cast<float *>(object_unbox(value));
        if (std::isfinite(actual) && std::abs(actual - expectedVolume) < .015f) {
            int percent = (int)std::lround(actual * 100.f);
            clearVolume();
            say("Music volume " + std::to_string(percent) + " percent.");
            return;
        }
    }
    if (GetTickCount64() - volumeStarted > 1800) {
        clearVolume();
        say("Music-volume change was not confirmed. Retry with Shift F3 or Shift F4.");
    }
}
static void command(const std::string &value) {
    if (value == "volumeup" || (value == "next" && (GetAsyncKeyState(VK_SHIFT) & 0x8000))) {
        adjustMusicVolume(.1f);
        return;
    }
    if (value == "volumedown" || (value == "previous" && (GetAsyncKeyState(VK_SHIFT) & 0x8000))) {
        adjustMusicVolume(-.1f);
        return;
    }
    if (value == "soundboard" || (value == "subtitles" && (GetAsyncKeyState(VK_SHIFT) & 0x8000))) {
        openSoundboard();
        return;
    }
    if (value == "phones" || (value == "screen" && (GetAsyncKeyState(VK_SHIFT) & 0x8000))) {
        openPhones();
        return;
    }
    if (value == "musicstop" || (value == "activate" && (GetAsyncKeyState(VK_SHIFT) & 0x8000))) {
        stopRecord();
        return;
    }
    if (value == "musicdump") {
        dump();
        return;
    }
    if (value == "phonedump") {
        phoneDump();
        return;
    }
    if (value == "clues" && !choices->active && !menuOpen() && controlled() && foreground()) {
        originalCommand("stop");
        browsingClues = true;
        explicitSelection = true;
        refresh();
        say("Notes and clues group. F3 and F4 choose a clue. F11 attempts walking to it. F8 returns to places.");
        originalCommand("navstatus");
        return;
    }
    if (value == "musicstatus") {
        auto d = turntable();
        say(heldMusic() ? "You are holding a vinyl record. F11 goes to the turntable."
            : d.object && truth(invoke(d.object, "IsPlaying"))
                ? "Music is playing on the turntable."
            : d.object && field(d.object, "_vinyl")
                ? "A record is on the turntable. Press F11, then F5 to start it."
                : "No record is on the turntable.");
        return;
    }
    if (value == "navnext" || value == "navprevious") {
        explicitSelection = true;
        browseMusic = false;
        deckMode = false;
    }
    if (value == "stop") {
        clearVolume();
        bool wasPhone = phoneMode;
        closePhones();
        clearPlayback();
        clearPlacement();
        browsingClues = false;
        browseMusic = false;
        deckMode = false;
        explicitSelection = false;
        clickDeadline = 0;
        originalCommand("stop");
        if (wasPhone)
            say("Control selector closed.");
        return;
    }
    if (!choices->active && !menuOpen() && controlled() && foreground()) {
        if (value == "navgo" && (*moving || *planner || *navigationMouse)) {
            browseMusic = false;
            originalCommand(value);
            return;
        }
        if (phoneMode) {
            if (value == "next" || value == "previous") {
                originalCommand("stop");
                if (!phoneControls.empty())
                    phoneIndex = (phoneIndex + phoneControls.size() + (value == "next" ? 1 : -1)) %
                                 phoneControls.size();
                describePhone();
                return;
            }
            if (value == "repeat") {
                describePhone();
                return;
            }
            if (value == "activate") {
                activatePhone();
                return;
            }
            if (value == "navgo") {
                auto target = phoneTarget();
                auto at = position(invoke(player(), "GetRootTransform"));
                if (!target.object) {
                    describePhone();
                    return;
                }
                if (std::hypot(at.x - target.aim.x, at.z - target.aim.z) <= 2.5f &&
                    std::abs(at.y - target.aim.y) <= 1.8f) {
                    originalCommand("stop");
                    aimAt(target.aim);
                    say(target.name + " nearby. Facing it. Press F5 to press the selected button.");
                    return;
                }
                refresh();
                if (destinations->empty() || destinations->front().identity != "task:phone") {
                    say("No clear approach to the selected control was found.");
                    return;
                }
                originalCommand(value);
                return;
            }
        }

        if (value == "navgo" && !browsingClues && *selected >= 0 &&
            *selected < (int)destinations->size() &&
            (*destinations)[*selected].identity == "group:clues") {
            browsingClues = true;
            explicitSelection = true;
            refresh();
            say("Notes and clues group. F3 and F4 choose a clue. F11 attempts walking to the selected clue. F8 returns to places and automatic music assistance.");
            originalCommand("navstatus");
            return;
        }
        if (browsingClues && (value == "next" || value == "previous")) {
            originalCommand(value == "next" ? "navnext" : "navprevious");
            return;
        }
        if (value == "navgo" && needsTurntable() && !explicitSelection) {
            browseMusic = false;
            musicRequested = false;
            auto deck = turntable();
            if (!deck.object) {
                say("No active turntable was found here.");
                return;
            }
            deckAim = deck.aim;
            auto at = position(invoke(player(), "GetRootTransform"));
            if (std::hypot(at.x - deck.aim.x, at.z - deck.aim.z) <= 1.8f &&
                std::abs(at.y - deck.aim.y) < 1.8f) {
                originalCommand("stop");
                aimAt(deck.aim);
                deckMode = true;
                say("Turntable nearby. Facing it. " + turntablePrompt() +
                    " F8 leaves turntable assistance.");
                return;
            }
            refresh();
            bool available = false;
            for (auto &d : *destinations)
                if (d.identity == "task:turntable")
                    available = true;
            if (!available) {
                say("No clear floor approach to the turntable was found. Automatic movement has not started.");
                return;
            }
            originalCommand(value);
            return;
        }
        if (value == "navgo" && !explicitSelection) {
            auto target = chooseTarget();
            auto p = player();
            if (target.object && p &&
                distance(position(invoke(p, "GetRootTransform")), target.aim) < 25) {
                auto all = records();
                if (!all.empty() &&
                    std::hypot(position(invoke(p, "GetRootTransform")).x -
                                   position(invoke(all.front(), "get_transform")).x,
                               position(invoke(p, "GetRootTransform")).z -
                                   position(invoke(all.front(), "get_transform")).z) < 2.5f) {
                    faceRecord();
                    return;
                }
            }
        }
        if (browseMusic && (value == "next" || value == "previous")) {
            auto all = records();
            if (!all.empty()) {
                recordIndex = (recordIndex + all.size() + (value == "next" ? 1 : -1)) % all.size();
                faceRecord();
                return;
            }
        }
        if (browseMusic && value == "repeat") {
            faceRecord();
            return;
        }
        if (browseMusic && value == "activate") {
            pickup();
            return;
        }
        if (value == "activate" && (placeRecord() || startRecord()))
            return;
        if (deckMode && value == "repeat") {
            say("Facing the turntable. " + turntablePrompt());
            return;
        }
    }
    if (value == "help") {
        say("Shift F3 lowers music volume by ten percent; Shift F4 raises it. Shift F7 opens the soundboard: eight effect buttons and three sound groups. F3 and F4 choose, F11 approaches, F5 presses, and F8 exits. Shift F6 opens phone controls. F3 and F4 choose a line, F11 approaches it, F5 presses it, and F8 exits. Shift F5 stops the playing record. F11 goes to the next supported music task: records first, then the turntable to place the vinyl and start playback. F11 again or F8 stops walking. Shift F11 and Control F11 browse rooms and equipment. Notes and clues are in one group; F11 opens it, then F3 and F4 choose a clue. F8 returns to the automatic task. F5 places the held record if the game allows it. F1 help. F2 repeat. F3 and F4 browse answers or menus. F5 selects or attempts interaction. F6 reads screen text. F7 subtitles. F9 automatic speech. F10 response timers. F12 reads an inspected clue.");
        return;
    }
    if (value == "music") {
        musicRequested = true;
        explicitSelection = false;
        originalCommand("navgo");
        return;
    }
    if (value == "navgo" && !choices->active && !menuOpen()) {
        auto current = lower(*lastSubtitle);
        if (current.find("record") != std::string::npos ||
            current.find("music") != std::string::npos || current.find("tape") != std::string::npos)
            musicRequested = true;
    }
    originalCommand(value);
}
static void tick(float dt) {
    originalTick(dt);
    volumeTick();
    if (phoneMode &&
        (!foreground() || !controlled() || menuOpen() || choices->active || manualInput()))
        closePhones();
    placementTick();
    playbackTick();
    if ((browseMusic || deckMode) &&
        (!foreground() || !controlled() || choices->active || menuOpen() || manualInput() ||
         (GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState('A') & 0x8000) ||
         (GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState('D') & 0x8000) ||
         (GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000) ||
         (GetAsyncKeyState(VK_UP) & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000))) {
        browseMusic = false;
        deckMode = false;
        if (clickHeld)
            clickDeadline = 0;
    }
    if (clickTarget && (choices->active || menuOpen())) {
        gchandle_free(clickTarget);
        clickTarget = 0;
        clickDeadline = 0;
    }
    if (clickTarget && GetTickCount64() - clickStarted > 200) {
        auto record = gchandle_get_target(clickTarget);
        if (record && heldByPlayer(record)) {
            gchandle_free(clickTarget);
            clickTarget = 0;
            browseMusic = false;
            say("Record picked up. The game confirms a vinyl record in your hand.");
        } else if (GetTickCount64() - clickStarted > 1600) {
            gchandle_free(clickTarget);
            clickTarget = 0;
            say("The record was not picked up. Press F11 to face it again, then F5.");
        }
    }
}
template <typename T> static bool bind(T &target, const char *name) {
    target = reinterpret_cast<T>(GetProcAddress(game, name));
    return target != nullptr;
}
static DWORD WINAPI startup(void *) {
    for (int i = 0; i < 600; ++i) {
        legacy = GetModuleHandleW(L"KillerFrequencyAccess.dll");
        game = GetModuleHandleW(L"GameAssembly.dll");
        if (legacy && game)
            break;
        Sleep(50);
    }
    if (!legacy || !game) {
        log("Base mod/game not loaded");
        return 0;
    }
    // Reject a mismatched base before touching hooks in an existing session.
    auto bytes = reinterpret_cast<char *>(legacy);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(legacy);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64 *>(bytes + dos->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != music_symbols::Timestamp ||
        nt->OptionalHeader.SizeOfImage != music_symbols::ImageSize) {
        log("Music patch not applied: base DLL build differs from manifest");
        return 0;
    }
    for (auto oldName : {L"KillerFrequencyMusicFix.dll",     L"KillerFrequencyMusicFix-v2.dll",
                         L"KillerFrequencyMusicFix-v3.dll",  L"KillerFrequencyMusicFix-v4.dll",
                         L"KillerFrequencyMusicFix-v5.dll",  L"KillerFrequencyMusicFix-v6.dll",
                         L"KillerFrequencyMusicFix-v7.dll",  L"KillerFrequencyMusicFix-v8.dll",
                         L"KillerFrequencyMusicFix-v9.dll",  L"KillerFrequencyMusicFix-v10.dll",
                         L"KillerFrequencyMusicFix-v11.dll", L"KillerFrequencyMusicFix-v12.dll",
                         L"KillerFrequencyMusicFix-v13.dll", L"KillerFrequencyMusicFix-v14.dll",
                         L"KillerFrequencyMusicFix-v15.dll", L"KillerFrequencyMusicFix-v16.dll",
                         L"KillerFrequencyMusicFix-v17.dll", L"KillerFrequencyMusicFix-v18.dll",
                         L"KillerFrequencyMusicFix-v19.dll", L"KillerFrequencyMusicFix-v20.dll",
                         L"KillerFrequencyMusicFix-v21.dll"})
        if (auto previous = GetModuleHandleW(oldName)) {
            auto disable = reinterpret_cast<MH_STATUS(WINAPI *)(void *)>(
                GetProcAddress(previous, "MH_DisableHook"));
            if (disable)
                disable(MH_ALL_HOOKS);
        }
    if (memcmp(bytes + music_symbols::Refresh, music_symbols::RefreshBytes, 16)) {
        log("Music patch not applied: base hook bytes differ from manifest. Restart with a matched package.");
        return 0;
    }
#define API(x)                                                                                     \
    if (!bind(x, "il2cpp_" #x)) {                                                                  \
        log("Missing API " #x);                                                                    \
        return 0;                                                                                  \
    }
    API(object_new);
    API(string_new);
    API(domain_get);
    API(thread_attach);
    API(thread_detach);
    API(domain_get_assemblies);
    API(assembly_get_image);
    API(image_get_name);
    API(class_from_name);
    API(object_get_class);
    API(class_get_parent);
    API(class_get_name);
    API(class_get_methods);
    API(method_get_name);
    API(method_get_param_count);
    API(method_is_generic);
    API(method_get_param);
    API(type_get_name);
    API(class_get_type);
    API(type_get_object);
    API(runtime_invoke);
    API(object_unbox);
    API(string_length);
    API(string_chars);
    API(class_get_field_from_name);
    API(field_get_value);
    API(gchandle_new);
    API(gchandle_get_target);
    API(gchandle_free);
    if (!bind(ilfree, "il2cpp_free"))
        return 0;
#undef API
    legacySay = reinterpret_cast<decltype(legacySay)>(bytes + music_symbols::Say);
    destinations = reinterpret_cast<decltype(destinations)>(bytes + music_symbols::Destinations);
    selected = reinterpret_cast<int *>(bytes + music_symbols::Selected);
    manualSpeech = reinterpret_cast<bool *>(bytes + music_symbols::ManualSpeech);
    lastSubtitle = reinterpret_cast<std::string *>(bytes + music_symbols::LastSubtitle);
    choices = reinterpret_cast<ChoiceState *>(bytes + music_symbols::Choices);
    sampleFloor = reinterpret_cast<decltype(sampleFloor)>(bytes + music_symbols::Sample);
    prepareCollision = reinterpret_cast<decltype(prepareCollision)>(bytes + music_symbols::Prepare);
    manualInput = reinterpret_cast<decltype(manualInput)>(bytes + music_symbols::ManualInput);
    moving = reinterpret_cast<bool *>(bytes + music_symbols::Moving);
    planner = reinterpret_cast<void **>(bytes + music_symbols::Planner);
    navigationMouse = reinterpret_cast<std::atomic<bool> *>(bytes + music_symbols::MouseHeld);
    auto monitor = CreateThread(nullptr, 0, releaseClick, nullptr, 0, nullptr);
    if (!monitor) {
        log("Input monitor creation failed");
        return 0;
    }
    CloseHandle(monitor);
    if (MH_Initialize() != MH_OK)
        return 0;
    if (MH_CreateHook(bytes + music_symbols::Refresh, (void *)refresh, (void **)&originalRefresh) !=
            MH_OK ||
        MH_CreateHook(bytes + music_symbols::Command, (void *)command, (void **)&originalCommand) !=
            MH_OK ||
        MH_CreateHook(bytes + music_symbols::Feedback, (void *)feedback,
                      (void **)&originalFeedback) != MH_OK ||
        MH_CreateHook(bytes + music_symbols::Tick, (void *)tick, (void **)&originalTick) != MH_OK) {
        log("Music hook creation failed");
        MH_Uninitialize();
        return 0;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        log("Music hook activation failed");
        return 0;
    }
    log("READY: live music target fix installed");
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(module, path, MAX_PATH);
        base = fs::path(path).parent_path();
        auto thread = CreateThread(nullptr, 0, startup, nullptr, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
