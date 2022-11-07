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
    : next_index(0), ua_cnt(0), queue(""), _output(capacity), _capacity(capacity), isEOF(false), end_index(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 来的数据可以 被添加
    if (next_index >= index && next_index < index + data.length()) {
        size_t offset = next_index - index;
        string str = string(data.begin() + offset, data.end());
        queue += str;
        _output.write(str);
        next_index = index + data.length();

        string buffer = "";
        size_t cnt = 0;
        while (next_index < queue.length() && queue[next_index] != ' ') {
            buffer += queue[next_index];
            cnt++;
            next_index++;
        }
        if (buffer != "") {
            _output.write(buffer);
            ua_cnt -= cnt;
        }

    } else if (queue.length() < index + 1) {  // 来的数据可以在后面被添加
        for (size_t i = 0; i + next_index < index; i++) {
            queue += ' ';
        }
        queue += data;
        ua_cnt += data.length();
    }

    if (eof) {
        isEOF = true;
        end_index = index + data.length();
    }

    if (isEOF && next_index == end_index) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return ua_cnt; }

bool StreamReassembler::empty() const { return _output.input_ended() && unassembled_bytes() == 0; }
