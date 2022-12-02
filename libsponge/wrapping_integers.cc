#include "wrapping_integers.hh"

#include "iostream"
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;
long long LENGTH = 1ll << 32;
long long HALF_LENGTH = 1ll << 31;
//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32(static_cast<uint32_t>(((n + isn.raw_value()) % LENGTH)));
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // 保证 value 在 [0,LENGTH)
    uint64_t value = (n.raw_value() + LENGTH - isn.raw_value()) % LENGTH;
    // check point 是多少倍的LENGTH
    uint64_t length_count = checkpoint / LENGTH;

    if (value > checkpoint) {
        return value;
    } else {
        // 先算出暂时的长度，结果可能是 temp_length 或者 再加 length 或者 再减 length
        uint64_t temp_length = value + length_count * LENGTH;

        if (temp_length > checkpoint + HALF_LENGTH) {
            return temp_length - LENGTH;
        } else if (checkpoint > temp_length + HALF_LENGTH) {
            return temp_length + LENGTH;
        } else {
            return temp_length;
        }
    }
}
