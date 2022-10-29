#include "byte_stream.hh"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iostream>
#include <string>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):chan({}),writeLen(0),readLen(0),_capacity(capacity),isInputEnded(false) {
}

size_t ByteStream::write(const string &data) {
    if (isInputEnded) {
        return 0;
    }

    size_t wl = min(data.length(), remaining_capacity());

    for (size_t i = 0; i < wl; i++) {
        chan.push_back(data[i]);
    }

    writeLen += wl;

    return wl;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t cpl = min(len, buffer_size());
    return string(chan.begin(), chan.begin() + cpl);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {

    for (size_t i = 0; i < min(len, buffer_size()); i++) {
        chan.pop_front();
    }
//    cout << chan.size();
//    readLen +=  len;
    readLen +=  min(len, buffer_size());
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() { isInputEnded = true; }

bool ByteStream::input_ended() const { return isInputEnded; }

size_t ByteStream::buffer_size() const { return writeLen - readLen; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0 ; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return writeLen; }

size_t ByteStream::bytes_read() const { return readLen; }

size_t ByteStream::remaining_capacity() const {
    size_t _length = _capacity - (writeLen - readLen);
    return _length;
}
