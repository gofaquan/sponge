#include "tcp_receiver.hh"

#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    WrappingInt32 seqno = seg.header().seqno;
    bool syn = seg.header().syn;
    bool fin = seg.header().fin;
    string data = seg.payload().copy();
    size_t size = seg.payload().str().size();

    if (syn && fin) {
        ISN = seqno;
        if (size > 0) {
            _reassembler.push_substring(data, unwrap(seqno, ISN, 0) - syn_come, fin);
        }
        syn_come++;
        _ackno = ISN + seg.length_in_sequence_space();
        _reassembler.stream_out().end_input();
    }

    if (fin) {
        fin_come++;
        if (_enable == true) {
            _reassembler.push_substring(seg.payload().copy(), unwrap(seqno, ISN, 0) - syn_come, fin);
        }

        if (_ackno == seqno) {
            _ackno = ISN + uint32_t(_reassembler.stream_out().bytes_written()) + syn_come + fin_come;
            _reassembler.stream_out().end_input();
        }

    } else if (syn) {
        ISN = seqno;
        if (size > 0) {
            _reassembler.push_substring(data, unwrap(seqno, ISN, 0), fin);
        }
        _ackno = ISN + seg.length_in_sequence_space();
        _enable = true;
        syn_come++;
    } else if (_enable && seqno.raw_value() > ISN.raw_value()) {
        // push 前的 缓冲区
        _reassembler.push_substring(seg.payload().copy(), unwrap(seqno, ISN, 0) - syn_come, fin);

        _ackno = ISN + uint32_t(_reassembler.stream_out().bytes_written()) + syn_come + fin_come;
        if (fin_come && _reassembler.unassembled_bytes() == 0) {
            _reassembler.stream_out().end_input();
            _enable = false;
        }
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    return _ackno.raw_value() == 0 ? optional<WrappingInt32>{} : _ackno;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }

// size_t TCPReceiver::window_size() const { return ISN + _capacity - _ackno + syn_come; }
