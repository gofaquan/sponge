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
        s.header().seqno = _isn + _next_seqno;
        size_t size = _stream.buffer_size() > _remain_size ? _remain_size : _stream.buffer_size();
        s.payload() = _stream.read(size);

        _remain_size -= size;
        _next_seqno += size;
        _flighting_bytes += size;

        cout << s.payload().str() << "  in" << endl;
        _store.push(s);
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
    if (ackno.raw_value() <= _isn.raw_value()) {
        return;
    }
    if (ackno.raw_value() > _isn.raw_value()) {
        _wendow_size = _remain_size = window_size;
        _flighting_bytes = _next_seqno - (ackno - _isn);
    }

    while (!_store.empty() && ackno.raw_value() > _store.front().header().seqno.raw_value()) {
        cout << _store.front().payload().str() << " out,reset timer" << endl;
        _retx_attempts = 0;
        _time_left = _initial_retransmission_timeout;
        _store.pop();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _time_left -= ms_since_last_tick;
    if (_time_left <= 0) {
        if (_store.empty()) {
            _next_seqno = 0;
            _flighting_bytes = 0;
            send_empty_segment();
        } else {
            cout << _store.front().payload().str() << "  retry" << endl;
            _segments_out.push(_store.front());
        }
        _retx_attempts++;
        _time_left = _initial_retransmission_timeout << _retx_attempts;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retx_attempts; }

void TCPSender::send_empty_segment() {
    if (_next_seqno == 0) {
        TCPSegment s;
        s.header().syn = true;
        s.header().seqno = _isn;
        _flighting_bytes += 1;
        _next_seqno += 1;
        //    _store.push(s);
        _segments_out.push(s);
    }
}
