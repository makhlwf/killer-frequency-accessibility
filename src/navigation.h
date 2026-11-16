#pragma once
#include <cmath>
#include <iomanip>
#include <memory>
#include <atomic>
#include <cctype>
#include <xinput.h>
#include "route_planner.h"

namespace navigation {
using Vec3 = routing::Point;
struct Quaternion {
    float x = 0, y = 0, z = 0, w = 1;
};
struct RayHit {
    Vec3 point, normal;
    unsigned face;
    float distance;
    float uv[2];
    int collider;
};
static_assert(sizeof(RayHit) == 44);
static Vec3 position(Obj *transform) {
    auto value = invoke(transform, "get_position");
    return value ? *static_cast<Vec3 *>(object_unbox(value)) : Vec3{};
}
static std::vector<Obj *> find(Class *c) {
    if (!c)
        return {};
    auto type = type_get_object(class_get_type(c));
    void *args[] = {type};
    return array(call(method(objectClass, "FindObjectsOfType", 1), nullptr, args));
}
static std::string path(Obj *transform) {
    std::string result;
    for (int depth = 0; transform && depth < 40; ++depth) {
        auto part = text(invoke(transform, "get_name"));
        result = part + (result.empty() ? "" : "/" + result);
        transform = invoke(transform, "get_parent");
    }
    return result;
}
static std::string identity(Obj *transform) {
    auto value = invoke(transform, "GetInstanceID");
    return path(transform) + "#" +
           (value ? std::to_string(*static_cast<int *>(object_unbox(value))) : "0");
}
static Obj *player() {
    auto all = find(cls("Assembly-CSharp.dll", "", "FlatScreenPlayer"));
    return all.empty() ? nullptr : all.front();
}
static bool doorOpen(Obj *door) {
    if (!door || truth(invoke(door, "IsFullyClosed")))
        return false;
    auto amount = invoke(door, "HowOpen");
    if (amount) {
        float open = *static_cast<float *>(object_unbox(amount));
        if (std::isfinite(open) && open > .55f && open <= 1.1f)
            return true;
    }
    return truth(invoke(door, "IsFullyOpen"));
}
static Class *physics() { return cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "Physics"); }
static bool cast(Vec3 origin, Vec3 direction, float distance, RayHit &hit, int mask = -5) {
    int trigger = 1;
    void *args[] = {&origin, &direction, &hit, &distance, &mask, &trigger};
    return truth(call(method(physics(), "Raycast", 6), nullptr, args));
}
static unsigned overlapHandle = 0, playerHandle = 0, controllerHandle = 0, rootHandle = 0,
                targetHandle = 0;
static Obj *handle(unsigned h) { return h ? gchandle_get_target(h) : nullptr; }
static float radius = .22f, height = 1.75f;
static void prepareCollision(Obj *controller) {
    auto r = invoke(controller, "get_radius"), h = invoke(controller, "get_height");
    if (r)
        radius = *static_cast<float *>(object_unbox(r)) + .025f;
    if (h)
        height = *static_cast<float *>(object_unbox(h));
    if (!overlapHandle) {
        auto c = cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "Collider");
        overlapHandle = gchandle_new(array_new(c, 128), false);
    }
}
static bool ownCollider(Obj *collider) {
    if (collider == handle(controllerHandle))
        return true;
    auto transform = invoke(collider, "get_transform");
    auto root = handle(rootHandle);
    void *args[] = {root};
    return root && truth(invoke(transform, "IsChildOf", 1, args));
}
static bool sample(Vec3 guess, Vec3 &out) {
    RayHit hit{};
    Vec3 origin{guess.x, guess.y + .65f, guess.z};
    if (!cast(origin, {0, -1, 0}, 1.55f, hit) || hit.normal.y < .65f)
        return false;
    out = hit.point;
    Vec3 half{radius, std::max(.25f, (height - .38f) / 2), radius};
    Vec3 center{out.x, out.y + .38f + half.y, out.z};
    Quaternion orientation;
    int mask = -5, trigger = 1;
    auto buffer = handle(overlapHandle);
    if (!buffer)
        return false;
    void *args[] = {&center, &half, buffer, &orientation, &mask, &trigger};
    auto boxed = call(method(physics(), "OverlapBoxNonAlloc", 6), nullptr, args);
    if (!boxed)
        return false;
    int count = *static_cast<int *>(object_unbox(boxed));
    if (count >= 128)
        return false;
    auto data = reinterpret_cast<Obj **>(static_cast<char *>(buffer) + 32);
    for (int i = 0; i < count; ++i)
        if (data[i] && !ownCollider(data[i]))
            return false;
    return true;
}
static bool edge(Vec3 a, Vec3 b) {
    float distance = routing::horizontal(a, b);
    if (distance < .001f)
        return true;
    // Check a body-height capsule along the entire edge, including diagonals.
    Vec3 bottom{a.x, std::max(a.y, b.y) + .42f, a.z},
        top{a.x, std::max(a.y, b.y) + height - radius, a.z};
    Vec3 direction{(b.x - a.x) / distance, 0, (b.z - a.z) / distance};
    RayHit hit{};
    int mask = -5, trigger = 1;
    void *args[] = {&bottom, &top, &radius, &direction, &hit, &distance, &mask, &trigger};
    return !truth(call(method(physics(), "CapsuleCast", 8), nullptr, args));
}
struct Destination {
    unsigned transform = 0;
    std::string name, identity;
    Vec3 aim;
    bool isDoor = false;
};
static std::vector<Destination> destinations;
static int selected = 0;
static std::unique_ptr<routing::Search> planner;
static std::vector<Vec3> route;
static size_t waypoint = 0;
static bool moving = false;
static unsigned generation = 0;
static unsigned long long started = 0, lastProgress = 0, lastReport = 0;
static Vec3 progressPosition;
static std::string destinationName;
static unsigned doorHandle = 0;
static unsigned long long doorStarted = 0, doorLastMove = 0;
static int doorDirection = 1;
static std::atomic<bool> mouseHeld{false};
static std::atomic<unsigned long long> releaseDeadline{0};
static std::atomic<bool> watchdogReleased{false};
static std::atomic<bool> watchdogReady{false};
static bool doorDirectionDecided = false;
static bool doorCrossing = false;
static Vec3 finalAim;
static bool mouse(DWORD flags, LONG dx = 0, LONG dy = 0) {
    INPUT value{};
    value.type = INPUT_MOUSE;
    value.mi.dwFlags = flags;
    value.mi.dx = dx;
    value.mi.dy = dy;
    return SendInput(1, &value, sizeof(value)) == 1;
}
static void releaseMouse() {
    if (mouseHeld && mouse(MOUSEEVENTF_LEFTUP))
        mouseHeld = false;
    if (doorHandle) {
        gchandle_free(doorHandle);
        doorHandle = 0;
    }
}
static DWORD WINAPI releaseWatchdog(void *) {
    for (;;) {
        if (mouseHeld) {
            DWORD pid = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &pid);
            bool cancel =
                pid != GetCurrentProcessId() || GetTickCount64() >= releaseDeadline.load();
            for (int key : std::initializer_list<int>{VK_F8, VK_ESCAPE, 'W', 'A', 'S', 'D'})
                if (GetAsyncKeyState(key) & 0x8000)
                    cancel = true;
            if (cancel && mouse(MOUSEEVENTF_LEFTUP)) {
                mouseHeld = false;
                watchdogReleased = true;
                log("NAV independent mouse release");
            }
        }
        Sleep(15);
    }
    return 0;
}
static void feedback(const std::string &message) {
    bool previous = manualSpeech;
    manualSpeech = true;
    say(message);
    manualSpeech = previous;
}
static void release(unsigned &h) {
    if (h) {
        gchandle_free(h);
        h = 0;
    }
}
static void cancel(const std::string &reason, bool announce = true) {
    bool active = moving || planner || mouseHeld;
    moving = false;
    planner.reset();
    route.clear();
    ++generation;
    releaseMouse();
    doorCrossing = false;
    release(playerHandle);
    release(controllerHandle);
    release(rootHandle);
    release(targetHandle);
    if (active && announce) {
        bool old = manualSpeech;
        manualSpeech = true;
        say(reason);
        manualSpeech = old;
    }
    log("NAV stop: " + reason);
}
static std::string friendly(std::string name) {
    for (const char *prefix :
         {"P_HeroProp_", "SM_HeroProp_", "P_Door_", "P_SetDressing_", "SM_", "P_"})
        if (name.rfind(prefix, 0) == 0)
            name.erase(0, strlen(prefix));
    for (auto &c : name)
        if (c == '_' || c == '-')
            c = ' ';
    return name;
}
enum class InteractionResult { Unavailable, Requested, Failed };
static bool movementAllowed(Obj *p) {
    auto location = invoke(p, "get_location");
    auto allowance = invoke(location, "get_allowMovement");
    return !allowance || truth(allowance);
}
static InteractionResult primaryInteraction(bool exitOnly) {
    auto p = player();
    auto manager = field(p, "_actionManager");
    auto list = invoke(manager, "get_performableInteractions");
    auto count = invoke(list, "get_Count");
    if (!count)
        return InteractionResult::Unavailable;
    for (int i = 0; i < *static_cast<int *>(object_unbox(count)); ++i) {
        void *index[] = {&i};
        auto context = invoke(list, "get_Item", 1, index);
        auto interaction = field(context, "interaction"), actor = field(context, "interactor");
        if (!interaction || !actor)
            continue;
        std::string type = class_get_name(object_get_class(interaction));
        auto name = text(invoke(interaction, "get_Name"));
        auto lower = type + " " + name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (exitOnly && lower.find("stand") == std::string::npos &&
            lower.find("exit") == std::string::npos && lower.find("leave") == std::string::npos)
            continue;
        if (!exitOnly) {
            auto observed = field(actor, "observedInteractable");
            if (!observed || invoke(interaction, "get_Interactable") != observed)
                continue;
        }
        if (lower.find("boundedmotion") != std::string::npos ||
            lower.find("grasp") != std::string::npos)
            continue;
        void *actorArgs[] = {actor};
        if (!truth(invoke(interaction, "CanPerform", 1, actorArgs)))
            continue;
        auto m = method(object_get_class(manager), "PerformInteraction", 2);
        if (!m)
            continue;
        cancel("Automatic walking stopped for interaction.", false);
        if (!truth(invoke(interaction, "CanPerform", 1, actorArgs))) {
            feedback("That interaction is no longer available.");
            return InteractionResult::Failed;
        }
        void *args[] = {interaction, actor};
        Obj *exception = nullptr;
        runtime_invoke(m, manager, args, &exception);
        if (exception) {
            feedback("The game could not perform that interaction.");
            return InteractionResult::Failed;
        }
        auto world = currentWorldUiHandle ? gchandle_get_target(currentWorldUiHandle) : nullptr;
        auto spoken = world ? localized(world, string_new(name.c_str())) : name;
        feedback(spoken.empty() ? "Interaction requested." : "Interacting. " + spoken);
        return InteractionResult::Requested;
    }
    return InteractionResult::Unavailable;
}
static void refresh() {
    std::string previous = destinations.empty() ? "" : destinations[selected].identity;
    cancel("Navigation cancelled for destination selection.", false);
    for (auto &d : destinations)
        release(d.transform);
    destinations.clear();
    selected = 0;
    auto p = player();
    if (!p)
        return;
    auto start = position(invoke(p, "GetRootTransform"));
    // The opening alley has a scripted entrance. Name it explicitly; exclude dumpsters.
    for (auto door : find(cls("Assembly-CSharp.dll", "", "DoorOpenable"))) {
        auto transform = invoke(door, "get_transform");
        auto id = path(transform);
        if (id.find("P_Door_Intro/") != std::string::npos) {
            auto parent = invoke(transform, "get_parent");
            auto handleObject = field(door, "handleObject");
            auto aim = handleObject ? position(invoke(handleObject, "get_transform"))
                                    : position(transform);
            if (!handleObject)
                aim.y += 1;
            destinations.push_back({gchandle_new(parent, false), "Opening alley entrance",
                                    identity(transform), aim, true});
            break;
        }
    }
    // Active movement anchors are authored by the game for usable locations.
    for (auto anchor : find(cls("Assembly-CSharp.dll", "Player.Movement", "MoveTarget"))) {
        auto transform = invoke(anchor, "get_transform");
        auto p0 = position(transform);
        if (routing::horizontal(start, p0) > 60)
            continue;
        auto name = text(invoke(invoke(transform, "get_parent"), "get_name"));
        if (name.empty())
            name = text(invoke(transform, "get_name"));
        destinations.push_back(
            {gchandle_new(transform, false), friendly(name), identity(transform), p0});
    }
    if (destinations.empty()) {
        for (auto door : find(cls("Assembly-CSharp.dll", "", "DoorOpenable"))) {
            auto transform = invoke(door, "get_transform");
            auto id = path(transform);
            if (id.find("Dumpster") != std::string::npos ||
                id.find("Fusebox") != std::string::npos ||
                id.find("Cabinet") != std::string::npos ||
                id.find("Container") != std::string::npos)
                continue;
            auto point = position(transform);
            if (routing::horizontal(start, point) > 35 || std::abs(start.y - point.y) > 5)
                continue;
            auto parent = invoke(transform, "get_parent");
            std::string name = friendly(text(invoke(parent, "get_name")));
            for (auto [token, label] : {std::pair{"MainStudio", "Studio door"},
                                        {"Office", "Office door"},
                                        {"Basement", "Basement door"},
                                        {"Restroom", "Restroom door"},
                                        {"WC_", "Restroom door"},
                                        {"Exterior", "Station exit"},
                                        {"Kitchen", "Kitchen door"}})
                if (id.find(token) != std::string::npos) {
                    name = label;
                    break;
                }
            auto handleObject = field(door, "handleObject");
            auto aim = handleObject ? position(invoke(handleObject, "get_transform")) : point;
            if (!handleObject)
                aim.y += 1;
            destinations.push_back(
                {gchandle_new(transform, false), name, identity(transform), aim, true});
        }
        // These are nearby clue/equipment targets, not inferred story objectives.
        for (auto holdable : find(cls("Assembly-CSharp.dll", "Interactables", "Holdable"))) {
            auto transform = invoke(holdable, "get_transform");
            auto p0 = position(transform);
            if (routing::horizontal(start, p0) > 25 || std::abs(start.y - p0.y) > 3)
                continue;
            auto name = text(invoke(holdable, "get_name"));
            if (name.empty())
                continue;
            auto lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            bool useful = false;
            for (auto word : {"map", "note", "clue", "paper", "document", "ticket", "phone",
                              "mixer", "console", "pinboard", "recordplayer"})
                if (lower.find(word) != std::string::npos) {
                    useful = true;
                    break;
                }
            if (!useful)
                continue;
            destinations.push_back(
                {gchandle_new(transform, false), friendly(name), identity(transform), p0});
        }
    }
    if (destinations.size() > 1 && destinations.front().name != "Opening alley entrance")
        std::stable_sort(
            destinations.begin(), destinations.end(), [&](const auto &a, const auto &b) {
                auto p1 = position(handle(a.transform)), p2 = position(handle(b.transform));
                return routing::horizontal(p1, start) + std::abs(p1.y - start.y) * 15 <
                       routing::horizontal(p2, start) + std::abs(p2.y - start.y) * 15;
            });
    for (int i = 0; i < (int)destinations.size(); ++i)
        if (destinations[i].identity == previous) {
            selected = i;
            break;
        }
    log("NAV destinations " + std::to_string(destinations.size()));
}
static void announce() {
    if (doorCrossing) {
        say("Walking through the opening entrance. Press F8 or F11 to cancel.");
        return;
    }
    if (mouseHeld) {
        say("Door assistance is trying the nearby door. Press F8 or F11 to cancel.");
        return;
    }
    if (!moving && !planner)
        refresh();
    if (destinations.empty()) {
        say("No navigation destinations are available here.");
        return;
    }
    auto &d = destinations[selected];
    auto p = player();
    auto distance = p ? routing::horizontal(position(invoke(p, "GetRootTransform")),
                                            position(handle(d.transform)))
                      : 0;
    say(std::string(planner  ? "Planning. "
                    : moving ? "Walking. "
                             : "Not walking. ") +
        "Destination " + std::to_string(selected + 1) + " of " +
        std::to_string(destinations.size()) + ". " + d.name + ". About " +
        std::to_string((int)distance) +
        " metres away. Press F11 to walk or stop, Shift F11 for the next destination.");
}
static void cycle(int delta) {
    refresh();
    if (destinations.empty()) {
        announce();
        return;
    }
    selected = (selected + delta + (int)destinations.size()) % (int)destinations.size();
    announce();
}
static void start() {
    if (moving || planner || mouseHeld) {
        cancel("Automatic walking cancelled.");
        return;
    }
    if (menuOpen() || choices.active) {
        say("Close the menu or finish the question before walking.");
        return;
    }
    refresh();
    if (destinations.empty()) {
        announce();
        return;
    }
    auto p = player();
    if (!p || !truth(invoke(p, "get_controlEnabled"))) {
        say("Walking is not available while the game controls the character.");
        return;
    }
    if (!movementAllowed(p)) {
        auto result = primaryInteraction(true);
        if (result == InteractionResult::Requested)
            feedback(
                "Requested leaving the current seat or interaction. Press F11 again when standing.");
        else if (result == InteractionResult::Unavailable)
            say("Walking is not available from this seat or interaction. Finish it or stand up first.");
        return;
    }
    auto controller = field(p, "characterController");
    auto root = invoke(p, "GetRootTransform");
    auto target = handle(destinations[selected].transform);
    if (!controller || !root || !target) {
        say("Destination unavailable.");
        return;
    }
    playerHandle = gchandle_new(p, false);
    controllerHandle = gchandle_new(controller, false);
    rootHandle = gchandle_new(root, false);
    targetHandle = gchandle_new(target, false);
    prepareCollision(controller);
    auto from = position(root), to = position(target);
    destinationName = destinations[selected].name;
    finalAim = destinations[selected].aim;
    if (destinationName == "Opening alley entrance") {
        bool open = false;
        for (auto door : find(cls("Assembly-CSharp.dll", "", "DoorOpenable")))
            if (identity(invoke(door, "get_transform")) == destinations[selected].identity) {
                open = doorOpen(door);
                break;
            }
        if (open) {
            to = position(target);
            to.z -= 2.8f;
            destinationName = "Inside the opening doorway";
        } else {
            to = destinations[selected].aim;
            to.z += 1.0f;
            if (routing::horizontal(from, to) < .9f) {
                cancel("Already near the entrance", false);
                feedback(
                    "You are near the opening entrance. Press F5 to try opening the door, then F11 to continue.");
                return;
            }
        }
        to.y = from.y;
    } else {
        if (destinations[selected].isDoor)
            to = destinations[selected].aim;
        float dist = routing::horizontal(from, to);
        if (dist > 1) {
            to.x += (from.x - to.x) / dist * .9f;
            to.z += (from.z - to.z) / dist * .9f;
        }
        RayHit ground{};
        if (cast({to.x, to.y + .5f, to.z}, {0, -1, 0}, 3.f, ground))
            to.y = ground.point.y;
    }
    planner = std::make_unique<routing::Search>(from, to, sample, edge);
    started = GetTickCount64();
    say("Finding a walking route to " + destinationName + ". Press F11 again to cancel.");
}
static bool manualInput() {
    for (int key : std::initializer_list<int>{'W', 'A', 'S', 'D', VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
                                              VK_ESCAPE})
        if (GetAsyncKeyState(key) & 0x8000)
            return true;
    static HMODULE input = LoadLibraryW(L"xinput1_4.dll");
    using GetState = DWORD(WINAPI *)(DWORD, XINPUT_STATE *);
    static auto state =
        input ? reinterpret_cast<GetState>(GetProcAddress(input, "XInputGetState")) : nullptr;
    if (state)
        for (DWORD i = 0; i < 4; ++i) {
            XINPUT_STATE value{};
            if (state(i, &value) == ERROR_SUCCESS &&
                (std::abs((int)value.Gamepad.sThumbLX) > 9000 ||
                 std::abs((int)value.Gamepad.sThumbLY) > 9000))
                return true;
        }
    return false;
}
static void face(Vec3 target) {
    auto p = handle(playerHandle);
    auto camera = field(p, "_cameraHandler");
    auto root = handle(rootHandle);
    auto at = position(root);
    float yaw = std::atan2(target.x - at.x, target.z - at.z);
    Quaternion q{0, std::sin(yaw / 2), 0, std::cos(yaw / 2)};
    void *args[] = {&q};
    invoke(camera, "ResetToRotation", 1, args);
}
static bool interact() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (pid != GetCurrentProcessId())
        return false;
    auto p = player();
    if (!p || menuOpen() || choices.active || !truth(invoke(p, "get_controlEnabled")))
        return false;
    auto root = invoke(p, "GetRootTransform");
    auto at = position(root);
    if (!movementAllowed(p) && primaryInteraction(true) != InteractionResult::Unavailable)
        return true;
    if (primaryInteraction(false) != InteractionResult::Unavailable)
        return true;
    Obj *closest = nullptr;
    float best = 2.5f;
    for (auto door : find(cls("Assembly-CSharp.dll", "", "DoorOpenable"))) {
        auto transform = invoke(door, "get_transform");
        auto point = position(transform);
        auto distance = routing::horizontal(at, point);
        if (std::abs(at.y - point.y) < 2 && distance < best) {
            best = distance;
            closest = door;
        }
    }
    if (!closest)
        return false;
    if (!watchdogReady) {
        say("Door assistance is unavailable because its input release monitor could not start.");
        return true;
    }
    if (doorOpen(closest)) {
        say("The nearby door is open. Use automatic walking to continue to an available destination.");
        return true;
    }
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        say("Release the left mouse button before using door assistance.");
        return true;
    }
    cancel("Walking stopped for the door.", false);
    auto target = field(closest, "handleObject");
    auto aim = target ? position(invoke(target, "get_transform"))
                      : position(invoke(closest, "get_transform"));
    if (!target)
        aim.y += 1;
    playerHandle = gchandle_new(p, false);
    rootHandle = gchandle_new(root, false);
    face(aim);
    release(playerHandle);
    release(rootHandle);
    doorHandle = gchandle_new(closest, false);
    doorStarted = doorLastMove = GetTickCount64();
    doorDirection = 1;
    doorDirectionDecided = false;
    releaseDeadline = doorStarted + 2500;
    watchdogReleased = false;
    if (!mouse(MOUSEEVENTF_LEFTDOWN)) {
        releaseMouse();
        feedback("Door assistance could not send mouse input.");
        return true;
    }
    mouseHeld = true;
    feedback(
        "Trying the nearby door using its normal mouse interaction. Press F8 or a movement key to cancel.");
    return true;
}
static void doorTick() {
    if (watchdogReleased.exchange(false)) {
        cancel("Door assistance stopped", false);
        feedback("Door assistance stopped and its mouse input was released.");
    }
    if (!mouseHeld)
        return;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    auto p = player();
    if (pid != GetCurrentProcessId() || menuOpen() || choices.active || !p ||
        !truth(invoke(p, "get_controlEnabled")) || manualInput()) {
        cancel("Door assistance stopped.");
        return;
    }
    auto door = handle(doorHandle);
    auto now = GetTickCount64();
    if (!door) {
        releaseMouse();
        return;
    }
    if (doorCrossing) {
        auto root = handle(rootHandle);
        auto entry = position(invoke(invoke(door, "get_transform"), "get_parent"));
        if (!moving || !root || position(root).z < entry.z - .45f) {
            releaseMouse();
            doorCrossing = false;
        }
        return;
    }
    if (doorOpen(door)) {
        auto transform = invoke(door, "get_transform");
        if (path(transform).find("P_Door_Intro/") != std::string::npos) {
            auto root = invoke(p, "GetRootTransform"), controller = field(p, "characterController"),
                 entry = invoke(transform, "get_parent");
            auto goal = position(entry), current = position(root);
            goal.z -= 2.8f;
            goal.y = current.y;
            if (!playerHandle)
                playerHandle = gchandle_new(p, false);
            if (!rootHandle)
                rootHandle = gchandle_new(root, false);
            if (!controllerHandle)
                controllerHandle = gchandle_new(controller, false);
            prepareCollision(controller);
            Vec3 ground;
            bool clear = sample(goal, ground) && edge(current, ground);
            if (clear) {
                goal.y = ground.y;
                release(targetHandle);
                targetHandle = gchandle_new(entry, false);
                route = {goal};
                waypoint = 0;
                moving = true;
                planner.reset();
                destinationName = "Inside the opening doorway";
                finalAim = goal;
                finalAim.z -= 1;
                started = lastProgress = lastReport = now;
                progressPosition = current;
                doorCrossing = true;
                releaseDeadline = now + 2500;
                feedback(
                    "Door open. Walking through the opening entrance. Press F11 or F8 to stop.");
                return;
            }
        } else {
            releaseMouse();
            feedback("The nearby door opened. Press F11 to continue to the selected destination.");
            return;
        }
    }
    if (now - doorStarted > 2400) {
        releaseMouse();
        feedback(
            "Door interaction finished. If it is still closed, it may be locked or require another movement.");
        return;
    }
    if (now - doorStarted > 1100 && !doorDirectionDecided) {
        doorDirectionDecided = true;
        auto progress = invoke(door, "HowOpen");
        if (!progress || std::abs(*static_cast<float *>(object_unbox(progress))) < .05f)
            doorDirection = -1;
    }
    if (now - doorLastMove > 45) {
        doorLastMove = now;
        if (!mouse(MOUSEEVENTF_MOVE, doorDirection * 14, 0))
            cancel("Door assistance could not send mouse movement.");
    }
}
static void tick(float dt) {
    doorTick();
    if (!planner && !moving)
        return;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (pid != GetCurrentProcessId()) {
        cancel("Automatic walking cancelled because the game lost focus.");
        return;
    }
    auto p = handle(playerHandle);
    auto controller = handle(controllerHandle);
    auto root = handle(rootHandle);
    auto target = handle(targetHandle);
    if (!p || !controller || !root || !target ||
        !truth(invoke(invoke(target, "get_gameObject"), "get_activeInHierarchy"))) {
        cancel("Destination is no longer available.");
        return;
    }
    if (menuOpen() || choices.active || !truth(invoke(p, "get_controlEnabled")) ||
        !movementAllowed(p) || !truth(invoke(controller, "get_enabled"))) {
        cancel("Automatic walking stopped for game interaction.");
        return;
    }
    if (manualInput()) {
        cancel("Automatic walking cancelled. Manual control.");
        return;
    }
    auto now = GetTickCount64();
    if (planner) {
        planner->step(35);
        if (!planner->finished) {
            if (now - started > 12000)
                cancel("No walking route found within the planning time.");
            return;
        }
        if (!planner->found) {
            log("NAV search expansions " + std::to_string(planner->expansions));
            cancel(
                "No clear walking route found. A door, obstacle, or different floor may block this destination.");
            return;
        }
        route = planner->route;
        log("NAV route points " + std::to_string(route.size()) + " expansions " +
            std::to_string(planner->expansions));
        planner.reset();
        moving = true;
        waypoint = 0;
        lastProgress = lastReport = now;
        progressPosition = position(root);
        feedback("Walking to " + destinationName + ". Press F11 or a movement key to stop.");
    }
    auto current = position(root);
    while (waypoint < route.size() && routing::horizontal(current, route[waypoint]) < .16f)
        ++waypoint;
    if (waypoint >= route.size()) {
        face(finalAim);
        auto name = destinationName;
        cancel("Arrived at planned stopping point", false);
        feedback(
            "Stopped near " + name +
            ". Automatic walking stopped. Check the spoken interaction prompt before continuing.");
        return;
    }
    if (now - lastProgress > 2500) {
        if (routing::horizontal(current, progressPosition) < .12f) {
            cancel("Walking blocked. Automatic movement stopped.");
            return;
        }
        lastProgress = now;
        progressPosition = current;
    }
    if (now - started > 120000) {
        cancel("Automatic walking stopped at the route time limit.");
        return;
    }
    if (dt <= 0 || dt > .25f)
        return;
    auto next = route[waypoint];
    float distance = routing::horizontal(current, next);
    float step = std::min(distance, 1.75f * dt);
    Vec3 delta{(next.x - current.x) / distance * step, -.4f * dt,
               (next.z - current.z) / distance * step};
    face(next);
    void *args[] = {&delta};
    invoke(controller, "Move", 1, args);
    if (now - lastReport > 12000) {
        lastReport = now;
        float left = 0;
        for (size_t i = waypoint + 1; i < route.size(); ++i)
            left += routing::horizontal(route[i - 1], route[i]);
        say("Walking to " + destinationName + ". About " + std::to_string((int)left) +
                " metres remaining.",
            false);
    }
}
static void dump() {
    auto p = player();
    auto actions = field(p, "_actionManager");
    auto list = invoke(actions, "get_performableInteractions");
    auto count = invoke(list, "get_Count");
    log("NAV action contexts " +
        std::to_string(count ? *static_cast<int *>(object_unbox(count)) : -1));
    if (count)
        for (int i = 0; i < *static_cast<int *>(object_unbox(count)); ++i) {
            void *args[] = {&i};
            auto context = invoke(list, "get_Item", 1, args);
            auto interaction = field(context, "interaction"),
                 interactor = field(context, "interactor");
            void *actor[] = {interactor};
            log("NAV ACTION " + std::to_string(i) + " " + text(invoke(interaction, "get_Name")) +
                " available=" + std::to_string(truth(invoke(interaction, "CanPerform", 1, actor))));
        }
    std::ofstream out(base / "navigation-scene.json");
    out << "[\n";
    bool first = true;
    for (auto transform : find(cls("UnityEngine.CoreModule.dll", "UnityEngine", "Transform"))) {
        auto p = position(transform);
        if (!first)
            out << ",\n";
        first = false;
        out << "{\"path\":" << std::quoted(path(transform)) << ",\"position\":[" << p.x << ","
            << p.y << "," << p.z << "]}";
    }
    out << "\n]";
    log("Navigation scene inventory written");
    std::ofstream types(base / "navigation-objects.json");
    types << "[\n";
    first = true;
    for (auto name : {"FlatScreenPlayer", "DoorOpenable", "InteractableItem", "FsInspectHandler"})
        for (auto object : find(cls("Assembly-CSharp.dll", "", name))) {
            auto transform = invoke(object, "get_transform");
            auto p = position(transform);
            if (!first)
                types << ",\n";
            first = false;
            types << "{\"type\":" << std::quoted(name)
                  << ",\"path\":" << std::quoted(path(transform)) << ",\"position\":[" << p.x << ","
                  << p.y << "," << p.z << "]}";
        }
    for (auto object : find(cls("Assembly-CSharp.dll", "Player.Movement", "MoveTarget"))) {
        auto transform = invoke(object, "get_transform");
        auto p = position(transform);
        if (!first)
            types << ",\n";
        first = false;
        types << "{\"type\":\"MoveTarget\",\"path\":" << std::quoted(path(transform))
              << ",\"position\":[" << p.x << "," << p.y << "," << p.z << "]}";
    }
    types << "\n]";
}
}
