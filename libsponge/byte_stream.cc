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

ByteStream::ByteStream(const size_t capacity) : _buffer({}), write_count(0), read_count(0), buffer_max_size(capacity) {}

size_t ByteStream::write(const string &data) {
    if (_input_end) {
        return 0;  // 输入结束就不能再写入
    }
    size_t wl = min(data.length(), remaining_capacity());  // 选择更小的一方写入
    _buffer += data.substr(0, wl);                         // 选出 [0, wl) 子字符串
    write_count += wl;                                     // 塞入缓冲区后，写入长度加 wl
    return wl;
}

//! \param[in] len bytes will be copied from the output side of the _buffer
string ByteStream::peek_output(const size_t len) const {
    size_t cpl = min(len, buffer_size());  // 返回较小者，因为 len 很大，缓冲区可能没这么多数据返回
    return _buffer.substr(0, cpl);
}

//! \param[in] len bytes will be removed from the output side of the _buffer
void ByteStream::pop_output(const size_t len) {
    size_t e_size = min(len, buffer_size());  // 选出较小者
    _buffer.erase(0, e_size);                 // 擦除 [0,e_size) 的数据，相当于弹出
    read_count += e_size;                     // 读取长度加 e_size
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);  // 调用已经实现的先拿到数据
    pop_output(len);                // 弹出
    return res;
}

// 终止输入
void ByteStream::end_input() { _input_end = true; }
// 输入是否终止
bool ByteStream::input_ended() const { return _input_end; }
// 返回缓冲区大小
size_t ByteStream::buffer_size() const { return write_count - read_count; }
// 判断是否缓冲区为空
bool ByteStream::buffer_empty() const { return buffer_size() == 0; }
// 判断是否 EOF
bool ByteStream::eof() const { return input_ended() && buffer_empty(); }
// 返回总写入长度
size_t ByteStream::bytes_written() const { return write_count; }
// 返回总读取长度
size_t ByteStream::bytes_read() const { return read_count; }
// 返回剩余容量
size_t ByteStream::remaining_capacity() const {
    return buffer_max_size - buffer_size();  // 最大值 - 缓冲区已经存在的数据长度
}
