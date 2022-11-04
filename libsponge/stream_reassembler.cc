#include "stream_reassembler.hh"

#include <cstddef>
#include <string>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : next_index(0), ua_cnt(0), queue(""), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (next_index >= index && next_index < index + data.length()) {
        size_t offset = next_index - index;
        string str = string(data.begin() + offset, data.end());
        _output.write(str);
        next_index += index + data.length();
    } else {
        if (queue.length() < index + 1) {
            for (size_t i = 0; i < index + 1 - queue.length(); i++) {
                queue += " ";
            }
            queue += data;
            ua_cnt += data.length();
        } else if (index + data.length() > queue.length()) {
            for (size_t offset = queue.length() - index - 1; offset < data.length(); offset++) {
                queue[offset + index] = data[offset];
            }
        }
    }

    if (eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return ua_cnt; }

bool StreamReassembler::empty() const { return _output.input_ended() && unassembled_bytes() == 0; }
