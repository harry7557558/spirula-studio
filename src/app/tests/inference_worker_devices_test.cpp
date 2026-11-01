#include "app/InferenceWorkerDevices.h"

#include <cstdio>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

nn::DeviceInfo device(const char* uuid, const char* type) {
    nn::DeviceInfo result;
    result.uuid = uuid;
    result.type = type;
    result.usable = true;
    return result;
}
}

int main() {
    try {
        const auto primary = device("uuid:00000000010000000000000000000000", "discrete");
        const auto other = device("uuid:00000000020000000000000000000000", "discrete");
        const auto integrated = device("uuid:00000000030000000000000000000000", "integrated");
        const auto excluded = device("uuid:00000000040000000000000000000000", "cpu");
        const auto unknown = "uuid:00000000050000000000000000000000";
        const std::vector<std::string> two{primary.uuid, other.uuid};
        require(app::worker_devices_for("auto", primary.uuid, {primary, other}) == two,
            "Auto must add the other discrete GPU");
        require(app::worker_devices_for("auto", primary.uuid, {primary, integrated}).empty(),
            "Auto must not add integrated GPUs when discrete GPUs exist");
        require(app::worker_devices_for("none", primary.uuid, {primary, other}).empty(),
            "None must preserve in-process serial inference");
        const std::string explicit_list = excluded.uuid + "," + integrated.uuid + "," +
            primary.uuid + "," + unknown + "," + integrated.uuid;
        require(app::worker_devices_for(explicit_list, primary.uuid,
            {primary, integrated, excluded}) == std::vector<std::string>{primary.uuid, integrated.uuid},
            "Explicit selection must ignore primary, unknown, unsupported and duplicate GPUs");
        require(app::worker_devices_for("auto", "", {primary, other}).empty(),
            "An unfrozen primary must not select workers");
        require(app::worker_devices_for("auto", excluded.uuid, {primary, excluded}).empty(),
            "An unsupported primary must not select workers");
        require(app::worker_devices_for("auto", unknown, {primary, other}).empty(),
            "An unknown primary must not select workers");
        auto unusable = primary;
        unusable.usable = false;
        require(!app::worker_device_supported(unusable), "Unusable GPUs must be excluded");
        auto unnamed = primary;
        unnamed.uuid.clear();
        require(!app::worker_device_supported(unnamed), "GPUs without UUIDs must be excluded");
        require(!app::worker_device_supported(device(unknown, "cpu")), "CPU devices must be excluded");
        const auto second_integrated = device(unknown, "integrated");
        require(app::worker_devices_for("auto", integrated.uuid,
            {integrated, excluded, unusable, second_integrated}) ==
            std::vector<std::string>{integrated.uuid, second_integrated.uuid},
            "Auto must use integrated GPUs when no supported discrete GPU exists");
        require(app::worker_devices_for(other.uuid + "," + integrated.uuid, primary.uuid,
            {integrated, primary, other}) ==
            std::vector<std::string>{primary.uuid, integrated.uuid, other.uuid},
            "Workers must follow enumeration order after the primary without a two-GPU cap");
        std::puts("inference worker device selection checks passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
