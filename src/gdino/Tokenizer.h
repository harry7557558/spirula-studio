#pragma once
// BERT's uncased WordPiece tokenizer (bert-base-uncased's vocab.txt), which is
// what Grounding DINO's text tower reads. Lower-casing, accent stripping for
// Latin-1, punctuation and CJK splitting, then greedy longest-match WordPiece.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace gdino {

class Tokenizer {
public:
    // Throws nn::Error when the file is unreadable or not a BERT vocabulary.
    void load(const std::string& vocab_path);
    bool loaded() const { return !vocab_.empty(); }

    // [CLS] ... [SEP], unpadded.
    std::vector<int32_t> encode(const std::string& text) const;
    const std::string& token(int32_t id) const;

    static constexpr int32_t kCls = 101, kSep = 102, kPeriod = 1012, kQuestion = 1029;

private:
    void wordpiece(const std::string& word, std::vector<int32_t>& out) const;

    std::unordered_map<std::string, int32_t> vocab_;
    std::vector<std::string>                 by_id_;
    int32_t                                  unk_ = 100;
};

}  // namespace gdino
