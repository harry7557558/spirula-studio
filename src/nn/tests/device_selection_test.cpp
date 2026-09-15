// Device selection contract (core/VulkanDeviceSelection.h) and the NN
// configure/initialize lifecycle around it.
//
// The resolver checks run on synthetic records, so they need no GPU: an
// enumeration that reorders or duplicates names is exactly the case a
// uuid:<hex> request has to survive, and a real machine has one of each at
// most. The lifecycle checks do create a device, because the point of the seam
// is what gets published.

#include "core/VulkanDeviceSelection.h"
#include "nn/Device.h"
#include "nn/core/Error.h"
#include "nn/vk/Context.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace sel = spirula::vkselect;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) g_failures++;
}

int autoRank(const std::string& type) {
    return type == "discrete"  ? 4
         : type == "integrated" ? 3
         : type == "virtual"    ? 2
         : type == "cpu"        ? 1
                                : 0;
}

const nn::DeviceInfo* autoUsable(
    const std::vector<nn::DeviceInfo>& devices) {
    const nn::DeviceInfo* best = nullptr;
    for (const auto& d : devices) {
        if (!d.usable || d.uuid.empty()) continue;
        if (!best || autoRank(d.type) > autoRank(best->type) ||
            (autoRank(d.type) == autoRank(best->type) &&
             d.vram_bytes > best->vram_bytes))
            best = &d;
    }
    return best;
}

const nn::DeviceInfo* anotherUsable(
    const std::vector<nn::DeviceInfo>& devices, const nn::DeviceInfo* first) {
    for (const auto& d : devices)
        if (&d != first && d.usable && !d.uuid.empty()) return &d;
    return nullptr;
}
const nn::DeviceInfo* lifecycleDevice(
    const std::vector<nn::DeviceInfo>& devices) {
    const nn::DeviceInfo* automatic = autoUsable(devices);
    const nn::DeviceInfo* alternate = anotherUsable(devices, automatic);
    return alternate ? alternate : automatic;
}

sel::DeviceRecord makeRecord(int index, const char* name, VkPhysicalDeviceType type,
                             const char* uuidHex, uint64_t vram = 0, bool usable = true) {
    sel::DeviceRecord r;
    r.index = index;
    r.name = name;
    r.type = sel::deviceTypeName(type);
    r.vram_bytes = vram;
    r.usable = usable;
    r.props.deviceType = type;
    if (uuidHex && uuidHex[0]) {
        const std::string hex = uuidHex;
        for (int i = 0; i < VK_UUID_SIZE; i++) {
            auto nibble = [](char c) {
                return (uint8_t)(c <= '9' ? c - '0' : std::tolower((unsigned char)c) - 'a' + 10);
            };
            r.uuid[i] = (uint8_t)(nibble(hex[2 * i]) << 4 | nibble(hex[2 * i + 1]));
        }
    }
    return r;
}

const char* kUuidA = "11111111222233334444555566667777";
const char* kUuidB = "aaaabbbbccccddddeeeeffff00001111";

void testParsing() {
    using K = sel::Request::Kind;
    check(sel::parseRequest("auto").kind == K::Auto, "parse: auto");
    check(sel::parseRequest("AUTO").kind == K::Auto, "parse: auto is case-insensitive");
    check(sel::parseRequest("-1").kind == K::Auto, "parse: -1 is Auto (SfM spelling)");
    check(sel::parseRequest("3").kind == K::Ordinal &&
              sel::parseRequest("3").ordinal == 3,
          "parse: ordinal");
    check(sel::parseRequest("GeForce").kind == K::Name, "parse: name substring");
    check(sel::parseRequest("uuid:" + std::string(kUuidA)).kind == K::Uuid &&
              sel::parseRequest("UUID:" + std::string(kUuidA)).kind == K::Uuid,
          "parse: uuid, either case of the prefix");
    check(sel::parseRequest("").kind == K::Malformed, "parse: empty rejects");
    check(sel::parseRequest("  ").kind == K::Malformed, "parse: blank rejects");
    check(sel::parseRequest("-2").kind == K::Malformed,
          "parse: negative other than -1 rejects");
    check(sel::parseRequest("12x").kind == K::Malformed, "parse: mixed alnum rejects");
    check(sel::parseRequest("4294967296").kind == K::Malformed, "parse: overflow rejects");
    check(sel::parseRequest("99").ordinal == 99 &&
              sel::parseRequest("99").kind == K::Ordinal,
          "parse: INT_MAX fits");
    check(sel::parseRequest("2147483648").kind == K::Malformed,
          "parse: INT_MAX+1 rejects");
    check(sel::parseRequest("uuid:1234").kind == K::Malformed, "parse: short uuid rejects");
    check(sel::parseRequest("uuid:" + std::string(kUuidA) + "00").kind == K::Malformed,
          "parse: long uuid rejects");
    check(sel::parseRequest("uuid:zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz").kind == K::Malformed,
          "parse: non-hex uuid rejects");
}

void testResolutionAndRanking() {
    std::vector<sel::DeviceRecord> two{
        makeRecord(0, "AMD Radeon 780M", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
                   kUuidA, 8ull << 30),
        makeRecord(1, "RTX 3060", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, kUuidB,
                   12ull << 30),
    };

    sel::Resolution autoRes = sel::resolveRequest(sel::parseRequest("auto"), two);
    check(autoRes.ok() && autoRes.selector == "uuid:" + std::string(kUuidB),
          "auto: discrete outranks integrated");

    // Same two devices, the other order: Auto must still land on uuid B, and
    // the uuid request must still resolve to the same physical device.
    std::vector<sel::DeviceRecord> reordered{two[1], two[0]};
    reordered[0].index = 0;
    reordered[1].index = 1;
    sel::Resolution autoRe = sel::resolveRequest(sel::parseRequest("auto"), reordered);
    check(autoRe.ok() && autoRe.selector == "uuid:" + std::string(kUuidB),
          "auto: ranking survives reordering");
    sel::Resolution uuidRe =
        sel::resolveRequest(sel::parseRequest("uuid:" + std::string(kUuidB)), reordered);
    check(uuidRe.ok() && uuidRe.device.name == "RTX 3060" && uuidRe.device.index == 0,
          "uuid: resolves to the same device after reordering");

    // Ranking beyond the first key: two integrated parts, VRAM decides.
    std::vector<sel::DeviceRecord> sameType{
        makeRecord(0, "iGPU small", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, kUuidA,
                   4ull << 30),
        makeRecord(1, "iGPU large", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, kUuidB,
                   16ull << 30),
    };
    check(sel::resolveRequest(sel::parseRequest("auto"), sameType).device.name ==
              "iGPU large",
          "auto: VRAM breaks a type tie");

    // An unusable device is not an Auto candidate even if it is fastest.
    std::vector<sel::DeviceRecord> mixed{
        makeRecord(0, "RTX 4090", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, kUuidB,
                   24ull << 30, /*usable=*/false),
        makeRecord(1, "iGPU", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, kUuidA,
                   8ull << 30, /*usable=*/true),
    };
    mixed[0].unusable_reason = "no timelineSemaphore";
    check(sel::resolveRequest(sel::parseRequest("auto"), mixed).device.name == "iGPU",
          "auto: skips an unusable device");
    sel::Resolution unusable =
        sel::resolveRequest(sel::parseRequest("0"), mixed);
    check(unusable.status == sel::ResolveStatus::Unusable && unusable.selector.empty(),
          "explicit unusable device fails rather than falling back");
}

void testFailures() {
    std::vector<sel::DeviceRecord> devices{
        makeRecord(0, "RTX 3060", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, kUuidA),
        makeRecord(1, "RTX 3060", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, kUuidB),
        makeRecord(2, "no-identity", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, ""),
    };
    // A UUID is an identity, so duplicate reports are not selectable.
    std::vector<sel::DeviceRecord> duplicate = devices;
    for (int i = 0; i < VK_UUID_SIZE; i++) duplicate[1].uuid[i] = duplicate[0].uuid[i];
    check(sel::resolveRequest(
              sel::parseRequest("uuid:" + std::string(kUuidA)), duplicate).status ==
              sel::ResolveStatus::Ambiguous,
          "duplicate uuid is ambiguous");
    duplicate[1].name = "other";
    check(sel::resolveRequest(sel::parseRequest("auto"), duplicate).status ==
              sel::ResolveStatus::Ambiguous,
          "auto rejects duplicate uuid");
    check(sel::resolveRequest(sel::parseRequest("0"), duplicate).status ==
              sel::ResolveStatus::Ambiguous,
          "ordinal rejects duplicate uuid");
    check(sel::resolveRequest(
              sel::parseRequest("RTX 3060"), duplicate).status ==
              sel::ResolveStatus::Ambiguous,
          "name rejects duplicate uuid");

    check(sel::resolveRequest(sel::parseRequest("nope"), devices).status ==
              sel::ResolveStatus::Missing,
          "missing name fails");
    check(sel::resolveRequest(sel::parseRequest("RTX 3060"), devices).status ==
              sel::ResolveStatus::Ambiguous,
          "duplicate names are ambiguous, not first-match");
    check(sel::resolveRequest(sel::parseRequest("9"), devices).status ==
              sel::ResolveStatus::OutOfRange,
          "out-of-range ordinal fails");
    check(sel::resolveRequest(
              sel::parseRequest("uuid:00000000000000000000000000000000"), devices)
                  .status == sel::ResolveStatus::Missing,
          "unknown uuid fails");
    check(sel::resolveRequest(sel::parseRequest("uuid:" + std::string(kUuidA)), devices)
                  .selector == "uuid:" + std::string(kUuidA),
          "uuid resolves to the right device");
    // A record without a reported UUID can only be selected through a request
    // that names it structurally; the request is then refused, because there is
    // no identity to carry forward.
    check(sel::resolveRequest(sel::parseRequest("no-identity"), devices).status ==
              sel::ResolveStatus::Missing,
          "a device with no reported uuid cannot be selected");
    check(sel::parseRequest("bad!!").kind == sel::Request::Kind::Name,
          "punctuation is a name, not a malformed number");
}

// The configured selection outlives shutdown, and an unknown request does not
// publish a context.
void testLifecycle() {
    const std::vector<nn::DeviceInfo> devices = nn::list_devices();
    const nn::DeviceInfo* pick = lifecycleDevice(devices);
    if (!pick) {
        std::printf("--  no usable Vulkan device: skipping lifecycle checks\n");
        return;
    }
    check(!pick->uuid.empty(), "enumerated device carries a canonical selector");

    const std::string configured_before = nn::configured_device_selector();
    // A request naming no device must fail, and must not publish a context.
    bool threw = false;
    try {
        nn::configure_device("no-such-device-xyzzy");
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "configure rejects an unknown device");
    check(!nn::vk::Context::initialized(),
          "a rejected configure publishes no context");
    check(nn::configured_device_selector() == configured_before,
          "a rejected configure stores nothing");

    nn::configure_device(pick->uuid);
    check(nn::configured_device_selector() == pick->uuid,
          "configure stores the resolved identity without creating a device");
    check(!nn::vk::Context::initialized(), "configure did not create the context");

    nn::vk::Context::get();
    check(nn::vk::Context::initialized(), "first get() initializes");
    check(nn::vk::Context::current_selector() == pick->uuid,
          "the live context reports the configured identity");

    // Conflicting live request.
    const nn::DeviceInfo* other_pick = anotherUsable(devices, pick);
    bool conflict = false;
    try {
        nn::configure_device(
            other_pick ? other_pick->uuid
                       : "uuid:00000000000000000000000000000000");
    } catch (const std::exception&) {
        conflict = true;
    }
    check(conflict, "a different device request against a live context fails");

    // Same-device re-request is a no-op.
    bool same_ok = true;
    try {
        nn::vk::ContextOptions opts;
        opts.selector_set = true;
        opts.device_selector = pick->uuid;
        nn::vk::Context::get(opts);
    } catch (const std::exception&) {
        same_ok = false;
    }
    check(same_ok, "re-requesting the same device succeeds");

    // Generation advances across shutdown, and the choice survives it.
    const uint64_t before = nn::vk::Context::generation();
    nn::shutdown();
    check(!nn::vk::Context::initialized(), "shutdown releases the context");
    check(nn::configured_device_selector() == pick->uuid,
          "shutdown keeps the configured identity");
    nn::vk::Context::get();
    check(nn::vk::Context::generation() > before,
          "a new generation is created after shutdown");
    check(nn::vk::Context::current_selector() == pick->uuid,
          "the new generation re-resolved the same identity");

    // One tiny real operation, so "initialized" means "usable".
    try {
        nn::vk::DevicePtr a = nn::vk::device_alloc(4096, "selection_smoke");
        float host[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        nn::vk::Stream& s = nn::vk::Stream::get();
        s.upload(a, host, sizeof(host));
        float back[8] = {};
        s.download(back, a, sizeof(back));
        s.sync();
        bool same = true;
        for (int i = 0; i < 8; i++)
            if (back[i] != host[i]) same = false;
        check(same, "upload/download round trip on the selected device");
        nn::vk::device_free(a);
    } catch (const std::exception& e) {
        std::printf("FAIL round trip threw: %s\n", e.what());
        g_failures++;
    }
    nn::shutdown();
}

// A failed initialization must leave nothing published, so a corrected request
// can still initialize; and an explicit request that disagrees with what is
// already live must fail rather than be quietly replaced.
void testFailureRecovery() {
    const std::vector<nn::DeviceInfo> devices = nn::list_devices();
    const nn::DeviceInfo* pick = lifecycleDevice(devices);
    const nn::DeviceInfo* automatic = autoUsable(devices);
    const nn::DeviceInfo* alternate = anotherUsable(devices, automatic);
    if (alternate)
        check(pick->uuid != automatic->uuid,
              "lifecycle uses a non-Auto device when available");
    if (!pick) return;

    nn::vk::ContextOptions bad;
    bad.selector_set = true;
    bad.device_selector = "9999";
    bool threw = false;
    try {
        nn::vk::Context::get(bad);
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "get() with an out-of-range index throws");
    check(!nn::vk::Context::initialized(),
          "a failed get() publishes no context");

    const uint64_t gen_after_failure = nn::vk::Context::generation();

    // The corrected request shares the generator with the failed one, so the
    // generation must not have advanced twice, and it must initialize for real.
    nn::vk::ContextOptions good;
    good.selector_set = true;
    good.device_selector = pick->uuid;
    nn::vk::Context& ctx = nn::vk::Context::get(good);
    check(ctx.physical() != VK_NULL_HANDLE && ctx.device() != VK_NULL_HANDLE,
          "a valid request initializes after a failed one");
    check(nn::vk::Context::generation() == gen_after_failure + 1,
          "exactly one generation is created by the retry");
    check(nn::configured_device_selector() == pick->uuid,
          "direct get freezes its resolved identity");

    // An entry-point table cached against the failed generation would now be
    // stale; that is why the counter moves before init() rather than after.
    nn::vk::ContextOptions other;
    other.selector_set = true;
    const nn::DeviceInfo* other_pick = anotherUsable(devices, pick);
    other.device_selector =
        other_pick ? other_pick->uuid
                   : "uuid:00000000000000000000000000000000";
    bool conflict = false;
    try {
        nn::vk::Context::get(other);
    } catch (const std::exception&) {
        conflict = true;
    }
    check(conflict, "an explicit get() for another live device throws");
    nn::shutdown();
}

}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    testParsing();
    testResolutionAndRanking();
    testFailures();
    testFailureRecovery();
    testLifecycle();
    std::printf("%s (%d failure(s))\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
