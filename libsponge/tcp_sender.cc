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
    , _acked({_isn})
    , _end_index(_isn + 1)
    , _initial_retransmission_timeout{retx_timeout}
    , _time_left(_initial_retransmission_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _flighting_bytes; }

void TCPSender::fill_window() {
    size_t _remain_size = 0;
    if (_end_index.raw_value() > next_seqno().raw_value()) {  // 防止 < 0
        _remain_size = _end_index.raw_value() - next_seqno().raw_value();
    }
    //    cout << _end_index.raw_value() << endl;
    //    cout << next_seqno().raw_value() << endl;
    //    cout << " remain size test " << _remain_size << endl;
    if (_window_size == 0 && !_0_is_sent) {
        //    if (!stream_in().buffer_empty() && _window_size == 0 && !_0_is_sent) {
        if (stream_in().buffer_empty()) {
            _fin_ready = true;
            send_empty_segment();
        } else {
            //        cout << "cnm" << endl;
            TCPSegment s;
            s.header().seqno = next_seqno();
            s.payload() = stream_in().read(1);
            _next_seqno++;
            _flighting_bytes++;

            _store.push(s);
            _segments_out.push(s);

            _0_is_sent = true;
        }
    }

    // 准备发送 syn, 未收到确认且 有空间再次发送
    if (_acked == _isn && _remain_size > 0) {
        _syn_ready = true;
        send_empty_segment();
        _syn_ready = false;
        return;
    }
    // 准备发送 fin
    if (stream_in().input_ended()) {
        _fin_ready = true;
    }
    TCPSegment s;

    //    cout << _end_index.raw_value() << endl;
    //    cout << _remain_size << endl;
    //    cout << stream_in().buffer_size() << endl;
    if (_fin_ready && !_fin_sent && _remain_size > 0) {
        s.header().seqno = next_seqno();  // 发送的 seg 的 seqno
        if (!stream_in().buffer_empty()) {
            size_t size = min(stream_in().buffer_size(), _remain_size);
            size = min(size, TCPConfig::MAX_PAYLOAD_SIZE);
            s.payload() = stream_in().read(size);

            _next_seqno += size;       // 塞入后，下一个 seg 的 起始下标
            _flighting_bytes += size;  // 塞入后，还未 received ，还在 flighting，所以加一下

            _remain_size -= size;
            if (_remain_size >= 1) {  // 剩余 1+ 个位置塞入 fin
                s.header().fin = true;
                _next_seqno++;
                _flighting_bytes++;
                _fin_sent = true;
            }

            //        cout << s.payload().str() << "  in" << endl;
            _store.push(s);         // seg 加入计时器队列，超时就准备发送
            _segments_out.push(s);  // seg 塞进 seg 队列

        } else {
            send_empty_segment();
            _fin_sent = true;
        }
    }

    //    cout << "(!_fin_ready && !stream_in().buffer_empty() && _remain_size > 0) "
    //         << (!_fin_ready && !stream_in().buffer_empty() && _remain_size > 0) << endl;
    //    cout << stream_in().buffer_size() << endl;

    // byte stream 有数据，从里面拿出塞进 seg 队列
    while (!_fin_ready && !stream_in().buffer_empty() && _remain_size > 0) {
        s.header().seqno = next_seqno();  // 发送的 seg 的 seqno

        // 判断是否能把 byte stream 传完, 选较小塞入 seg
        //        cout << _remain_size << endl;
        size_t size = min(stream_in().buffer_size(), _remain_size);
        size = min(size, TCPConfig::MAX_PAYLOAD_SIZE);
        s.payload() = stream_in().read(size);

        _next_seqno += size;       // 塞入后，下一个 seg 的 起始下标
        _flighting_bytes += size;  // 塞入后，还未 received ，还在 flighting，所以加一下

        //        cout << s.payload().str() << "  in" << endl;
        _store.push(s);         // seg 加入计时器队列，超时就准备发送
        _segments_out.push(s);  // seg 塞进 seg 队列

        _remain_size -= size;  // 及时减去 remain_size，防止 循环出错
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

    if (window_size == 0) {
        _window_size = 0;
    }

    _acked = ackno;
    _flighting_bytes = next_seqno() - ackno;
    _end_index = ackno + window_size;

    while (!_store.empty() &&
           ackno.raw_value() >= _store.front().header().seqno.raw_value() + _store.front().length_in_sequence_space()) {
        //    while (!_store.empty() && ackno.raw_value() > _store.front().header().seqno.raw_value()) {
        //        cout << _store.front().payload().str() << " out,reset timer" << endl;
        _store.pop();

        // 会执行多次 可以设个 flag
        _retx_attempts = 0;
        _time_left = _initial_retransmission_timeout;
    }

    if (_0_is_sent) {
        _0_is_sent = false;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 不直接相减，防止 结果本来为负, 但 size_t 是 unsigned 转为 很大的正数
    if (_time_left > ms_since_last_tick) {
        _time_left -= ms_since_last_tick;
    } else {
        // 到重传时间
        if (!_store.empty()) {
            //            cout << _store.front()
            //            _next_seqno = _flighting_bytes = 0;
            //            fill_window();
            //        } else {  // 重传最小序号的 未收到 seg.payload().str() << "  retry" << endl;
            _segments_out.push(_store.front());
        }

        if (_window_size != 0) {
            // 重传次数 +1, 计时器时间加倍
            _retx_attempts++;
        }
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
    if (_fin_ready && !_fin_sent) {
        s.header().fin = true;
        s.header().seqno = next_seqno();
        _flighting_bytes++;
        _next_seqno++;
    }

    _store.push(s);
    _segments_out.push(s);
}
