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
    , _acked({})
    , _end_index(_isn)
    , _initial_retransmission_timeout{retx_timeout}
    , _time_left(_initial_retransmission_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _flighting_bytes; }

void TCPSender::fill_window() {
    // 准备发送 syn
    if (_next_seqno == 0) {
        _syn_ready = true;
        send_empty_segment();
        return;
    }
    // 准备发送 fin
    if (stream_in().input_ended()) {
        _fin_ready = true;
        //        return ;
    }

    // byte stream 有数据，从里面拿出塞进 seg 队列
    if (!stream_in().buffer_empty()) {
        size_t _remain_size = _end_index.raw_value() - next_seqno().raw_value();
        if (_remain_size == 0) {
            return;
        }

        TCPSegment s;
        s.header().seqno = next_seqno();  // 发送的 seg 的 seqno
        // 判断是否能把 byte stream 传完, 选较小塞入 seg
        //        cout << _remain_size << endl;
        size_t size = min(stream_in().buffer_size(), _remain_size);
        s.payload() = stream_in().read(size);

        _next_seqno += size;       // 塞入后，下一个 seg 的 起始下标
        _flighting_bytes += size;  // 塞入后，还未 received ，还在 flighting，所以加一下

        if (_fin_ready) {
            s.header().fin = true;
        }

        //        cout << s.payload().str() << "  in" << endl;
        _store.push(s);         // seg 加入计时器队列，超时就准备发送
        _segments_out.push(s);  // seg 塞进 seg 队列
    } else {                    // byte steam 没有数据，判定为开始或者结束

        // 准备发送 fin
        //        _end_index == next_seqno() &&
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // ackno 序号不合法 或者是 传来之前的 ack 或者是 传来的 ack 确认值 > 已发送的最大序号 ，直接return
    if (ackno.raw_value() <= _isn.raw_value() || _acked.raw_value() > ackno.raw_value() ||
        ackno.raw_value() > next_seqno().raw_value()) {
        return;
    }

    _acked = ackno;
    _flighting_bytes = next_seqno() - ackno;
    _end_index = ackno + window_size;

    while (!_store.empty() && ackno.raw_value() > _store.front().header().seqno.raw_value()) {
        //        cout << _store.front().payload().str() << " out,reset timer" << endl;
        _retx_attempts = 0;
        _time_left = _initial_retransmission_timeout;
        _store.pop();
    }

    if (_fin_ready) {
        // 有剩余位置可以传
        if (_end_index - next_seqno() > 0 && !stream_in().buffer_empty()) {
            fill_window();
        } else {  // 没有空位，慢了就直接 fin 结束
            send_empty_segment();
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _time_left -= ms_since_last_tick;
    // 到重传时间
    if (_time_left <= 0) {
        //
        if (_store.empty()) {
            _next_seqno = _flighting_bytes = 0;
            fill_window();
        } else {  // 重传最小序号的 未收到 seg
                  //            cout << _store.front().payload().str() << "  retry" << endl;
            _segments_out.push(_store.front());
        }
        // 重传次数 +1, 计时器时间加倍
        _retx_attempts++;
        _time_left = _initial_retransmission_timeout << _retx_attempts;
    }
}
// 返回连续重传的次数
unsigned int TCPSender::consecutive_retransmissions() const { return _retx_attempts; }

void TCPSender::send_empty_segment() {
    TCPSegment s;
    // 如果是发送带 syn 的空 seg
    if (_syn_ready) {
        s.header().syn = true;
        s.header().seqno = _isn;
        _flighting_bytes++;
        _next_seqno++;
    }
    // 如果是发送带 fin 的空 seg
    if (_fin_ready) {
        s.header().fin = true;
        s.header().seqno = next_seqno();
        _flighting_bytes++;
        _next_seqno++;
    }
    _segments_out.push(s);
}
