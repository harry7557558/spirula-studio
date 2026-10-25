// hierarchy_security_test.cpp -- fuzzing and malformed container security tests.

#include "lod/HierarchyFile.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static int g_failures = 0;

static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

static void write_raw_file(const std::string& path, const void* data, size_t len) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data), (std::streamsize)len);
}

int main() {
    const std::string test_path = "/tmp/test_malformed.slod";

    // Truncated header (< 128 bytes)
    {
        uint8_t tiny[64] = {};
        write_raw_file(test_path, tiny, sizeof(tiny));
        std::string err = spirula::validate_slod(test_path);
        check(!err.empty() && err.find("file too small") != std::string::npos,
              "rejects truncated file header (<128B)");

        bool threw = false;
        try { spirula::read_slod(test_path, false); } catch (const std::exception&) { threw = true; }
        check(threw, "read_slod throws cleanly on truncated file header");
    }

    // Bad magic bytes
    {
        spirula::SlodFileHeader hdr;
        hdr.magic = 0x12345678;
        write_raw_file(test_path, &hdr, sizeof(hdr));
        std::string err = spirula::validate_slod(test_path);
        check(err == "bad magic", "rejects corrupted magic bytes");
    }

    // Unsupported version
    {
        spirula::SlodFileHeader hdr;
        hdr.version = 999;
        write_raw_file(test_path, &hdr, sizeof(hdr));
        std::string err = spirula::validate_slod(test_path);
        check(!err.empty() && err.find("unsupported version") != std::string::npos,
              "rejects unsupported version");
    }

    // Negative / zero node counts
    {
        spirula::SlodFileHeader hdr;
        hdr.num_nodes = 0;
        write_raw_file(test_path, &hdr, sizeof(hdr));
        check(spirula::validate_slod(test_path) == "invalid node count",
              "rejects zero node count");

        hdr.num_nodes = -5;
        write_raw_file(test_path, &hdr, sizeof(hdr));
        check(spirula::validate_slod(test_path) == "invalid node count",
              "rejects negative node count");
    }

    // Giant node count (OOM protection)
    {
        spirula::SlodFileHeader hdr;
        hdr.num_nodes = 100000000; // 100M nodes
        write_raw_file(test_path, &hdr, sizeof(hdr));
        check(spirula::validate_slod(test_path) == "invalid node count",
              "rejects excessive node count (>50M nodes)");
    }

    // Node table offset past EOF
    {
        spirula::SlodFileHeader hdr;
        hdr.num_nodes = 10;
        hdr.node_table_offset = 128;
        // File only 128 bytes, but node table requires 128 + 10 * sizeof(SlodNodeRecord)
        write_raw_file(test_path, &hdr, sizeof(hdr));
        check(spirula::validate_slod(test_path) == "node table extends past EOF",
              "rejects node table extending past EOF");
    }

    // Malformed summary offset in node record
    {
        spirula::SlodFileHeader hdr;
        hdr.num_nodes = 1;
        hdr.root_node = 0;
        hdr.total_summary = 5;
        hdr.node_table_offset = 128;
        hdr.summary_offset = 128 + sizeof(spirula::SlodNodeRecord);
        hdr.chunk_table_offset = hdr.summary_offset + 5 * sizeof(spirula::SummaryGaussian);
        hdr.payload_offset = hdr.chunk_table_offset;

        spirula::SlodNodeRecord nr{};
        nr.parent = -1;
        nr.child_left = -1;
        nr.child_right = -1;
        nr.summary_offset = 4;
        nr.summary_count = 10; // 4 + 10 = 14 > total_summary (5)
        nr.splat_offset = 0;
        nr.splat_count = 0;

        std::vector<uint8_t> buf(hdr.chunk_table_offset, 0);
        std::memcpy(buf.data(), &hdr, sizeof(hdr));
        std::memcpy(buf.data() + hdr.node_table_offset, &nr, sizeof(nr));
        write_raw_file(test_path, buf.data(), buf.size());

        std::string err = spirula::validate_slod(test_path);
        check(!err.empty() && err.find("invalid summary offset/count") != std::string::npos,
              "rejects node record with summary offset/count exceeding total summary count");
    }

    // Chunk extending past EOF
    {
        spirula::SlodFileHeader hdr{};
        hdr.magic = spirula::kSlodMagic;
        hdr.version = spirula::kSlodVersion;
        hdr.num_nodes = 1;
        hdr.root_node = 0;
        hdr.num_chunks = 1;
        hdr.node_table_offset = 128;
        hdr.summary_offset = 128 + sizeof(spirula::SlodNodeRecord);
        hdr.chunk_table_offset = hdr.summary_offset;
        hdr.payload_offset = hdr.chunk_table_offset + sizeof(spirula::SlodChunkEntry);

        spirula::SlodNodeRecord nr{};
        nr.parent = -1;
        nr.child_left = -1;
        nr.child_right = -1;

        spirula::SlodChunkEntry ce{};
        ce.node_index = 0;
        ce.file_offset = hdr.payload_offset;
        ce.byte_size = 1000000; // extends way past file

        std::vector<uint8_t> buf(hdr.payload_offset + 10, 0);
        std::memcpy(buf.data(), &hdr, sizeof(hdr));
        std::memcpy(buf.data() + hdr.node_table_offset, &nr, sizeof(nr));
        std::memcpy(buf.data() + hdr.chunk_table_offset, &ce, sizeof(ce));
        write_raw_file(test_path, buf.data(), buf.size());

        std::string err = spirula::validate_slod(test_path);
        check(!err.empty() && err.find("extends past EOF") != std::string::npos,
              "rejects chunk entry with byte_size extending past EOF");
    }

    std::remove(test_path.c_str());

    if (g_failures == 0) std::printf("hierarchy_security_test: OK\n");
    return g_failures ? 1 : 0;
}
