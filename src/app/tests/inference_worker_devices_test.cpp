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
        require(app::parse_workers_per_gpu(nullptr) == 1 &&
                app::parse_workers_per_gpu("1") == 1 &&
                app::parse_workers_per_gpu("2") == 2,
            "Worker multiplicity must default to one and accept positive integers");
        for (const char* value : {"", "0", "-1", "1x", "1.5", " 2", "+2", "4294967296"}) {
            bool rejected = false;
            try { app::parse_workers_per_gpu(value); }
            catch (const std::exception&) { rejected = true; }
            require(rejected, "Invalid worker multiplicity was accepted");
        }
        require(app::expand_worker_devices(two, 1) == two,
            "One worker per GPU must preserve the device list");
        require(app::expand_worker_devices(two, 2) ==
                std::vector<std::string>{primary.uuid, other.uuid, primary.uuid, other.uuid},
            "Two workers per GPU must repeat each selected device without deduplication");
        for (const char* selection : {"none", "auto"}) {
            const auto selected = app::worker_devices_for(selection, primary.uuid, {primary}, 2);
            require(app::expand_worker_devices(selected, 2) ==
                    std::vector<std::string>{primary.uuid, primary.uuid},
                "One GPU must use isolated duplicate workers when requested");
        }
        std::puts("inference worker device selection checks passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
