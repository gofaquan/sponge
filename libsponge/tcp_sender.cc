#include "tcp_sender.hh"

#include "iostream"
#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _time_left(_initial_retransmission_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _flighting_bytes; }

void TCPSender::fill_window() {
    if (_next_seqno == 0) {
        return send_empty_segment();
    }

    if (!_stream.buffer_empty()) {
        TCPSegment s;
//        cout << _stream.buffer_size() << endl;
        s.header().seqno = _isn + _next_seqno;
        size_t size = _stream.buffer_size() > _remain_size ? _remain_size : _stream.buffer_size();
        s.payload() = _stream.read(size);

        _next_seqno += size;
        _flighting_bytes += size;
        _segments_out.push(s);
    }

    // 最大的打包容量为 MAX PAYLOAD SIZE
    // 在这个方法中将stream根据可发送的windows_size的大小打包成sgement，并Push进入segment_out的队列当中
    // 关联参数： window_size

    // 首先检测目前的窗口最大值是多少
    // 将窗口最大值代表的data全部读出来
    // 根据窗口大小对data进行切片
    // 切片成segment推入_segment准备发送
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //    cout << ackno.raw_value() << endl;
    //    cout << _isn.raw_value() << endl;
    if (ackno.raw_value() > _isn.raw_value()) {
        _wendow_size = _remain_size = window_size;
//        cout << ackno - _isn << endl;
//        cout << _next_seqno << endl;

        _flighting_bytes = _next_seqno - (ackno - _isn);
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { _time_left -= ms_since_last_tick; }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {
    TCPSegment s;
    s.header().syn = true;
    s.header().seqno = _isn;
    _flighting_bytes += 1;
    _next_seqno += 1;
    _segments_out.push(s);
}
