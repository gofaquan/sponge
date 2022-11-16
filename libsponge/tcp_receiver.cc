#include "tcp_receiver.hh"

#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    //    cout << seg.header().ackno << endl;
    //    cout << seg.header().seqno << endl;
    //    cout << seg.payload().copy() << endl;
    //    cout << seg.payload().str().size() << endl;

    WrappingInt32 seqno = seg.header().seqno;
    //    uint32_t ack_no = seg.header().ackno.raw_value();
    bool syn = seg.header().syn;
    bool fin = seg.header().fin;
    string data = seg.payload().copy();
    //    size_t size = seg.payload().str().size();

    if (fin) {
        if (_enable != false) {
            _reassembler.push_substring(seg.payload().copy(), unwrap(seqno, ISN, 0) - syn_come, fin);
            _ackno = _ackno + seg.length_in_sequence_space();

        } else {
            _enable = false;
        }

        _reassembler.stream_out().end_input();

        //        cout << seg.length_in_sequence_space() << endl;
    }

    if (syn) {
        ISN = seqno;
        //        if (size > 0) {
        //            _reassembler.push_substring(data, unwrap(seqno, ISN, 0), fin);
        //        }
        _ackno = ISN + seg.length_in_sequence_space();
        _enable = true;
        syn_come++;
    } else if (_enable && seqno.raw_value() > ISN.raw_value()) {
        // push 前的 缓冲区
        //        size_t o_dirty_len = _reassembler.unassembled_bytes();
        _reassembler.push_substring(seg.payload().copy(), unwrap(seqno, ISN, 0) - syn_come, fin);

        _ackno = ISN + uint32_t(_reassembler.stream_out().bytes_written()) + syn_come;

        //        size_t n_dirty_len = _reassembler.unassembled_bytes();

        //        if (seqno == _ackno) {
        //            _ackno = _ackno + size;
        //            // 大于说明有新数据进来，而且与后面的缓冲连起来了
        //            if (o_dirty_len > n_dirty_len) {
        //                _ackno = _ackno + (o_dirty_len - n_dirty_len);
        //            }
        //        }
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    return _ackno.raw_value() == 0 ? optional<WrappingInt32>{} : _ackno;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }

// size_t TCPReceiver::window_size() const { return ISN + _capacity - _ackno + syn_come; }
