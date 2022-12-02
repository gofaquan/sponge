#include "tcp_receiver.hh"

#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template<typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!syn_come) {
        if (!seg.header().syn) {
            return; // 没来就 return ，不接收无效 seg
        }
        if (seg.header().fin) {
            fin_come++;
        }

        syn_come = true;
        ISN = seg.header().seqno;
        _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
        return;
    }

    // 不合法的都舍弃
    if (seg.header().seqno.raw_value() <= ISN.raw_value()) {
        return;
    }
    // 接收到 fin
    if (seg.header().fin) {
        fin_come = true;
    }

    // 收到 syn 后，正常进行 seg 接收的流程
    // check point 确定对应的 unwrap 后的 下标, syn_come 是收到 syn，syn 占有 一位下标
    uint64_t checkpoint = _reassembler.stream_out().bytes_written() + syn_come;
    uint64_t index = unwrap(seg.header().seqno, ISN, checkpoint) - syn_come;
    _reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!syn_come) {
        return nullopt;  // syn 没传过来，就返回空
    }

    WrappingInt32 result = ISN + syn_come + _reassembler.stream_out().bytes_written();

    if (fin_come && _reassembler.unassembled_bytes() == 0) {
        result = result + fin_come;
    }

    return result;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }