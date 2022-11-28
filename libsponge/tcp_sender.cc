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
    , _NEXT_SeqNo_MAX(_isn + 1)
    , _initial_retransmission_timeout{retx_timeout}
    , _time_left(_initial_retransmission_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    // 用 absolute 转 uint64
    return next_seqno_absolute() - unwrap(_acked, _isn, next_seqno_absolute());
}

// 发送 seg
void TCPSender::send_segment(TCPSegment s, WrappingInt32 seqno, bool syn, bool fin, size_t size) {
    s.header().syn = syn;
    s.header().fin = fin;
    s.header().seqno = seqno;
    if (!_stream.buffer_empty()) {
        s.payload() = _stream.read(size);
    }
    _next_seqno += size;
    _store.push(s);
    _segments_out.push(s);
}

void TCPSender::fill_window() {
    // 1. 判断是否能传数据
    size_t _remain_size = 0;
    if (_NEXT_SeqNo_MAX.raw_value() > next_seqno().raw_value()) {  // 防止 < 0 变很大的正数
        _remain_size = _NEXT_SeqNo_MAX.raw_value() - next_seqno().raw_value();
    }

    TCPSegment s;
    // 准备发送 syn, 未收到确认且 有空间再次发送
    if (_acked == _isn && _remain_size > 0) {
        send_segment(s, next_seqno(), true, false, 1);
        return;
    }

    // receiver 发送 ack, 同时返回给 sender 的窗口为 0
    if (_window_size == 0 && !_0_is_sent) {
        // stream 的 buffer 为空，就 send empty seg, 同时传 fin 暂停传输
        if (stream_in().buffer_empty()) {
            s.header().fin = true;
            send_segment(s, next_seqno(), false, true, 1);
        } else {
            // stream buffer 不为空，可以发 1 byte
//            s.payload() = stream_in().read(1);
            send_segment(s, next_seqno(), false, false, 1);
        }

        _0_is_sent = true;  // 只发一次
//        s.header().seqno = next_seqno();
//        _next_seqno++;
//        _store.push(s);
//        _segments_out.push(s);
    }

    if (stream_in().input_ended() && !_fin_sent && _remain_size > 0) {
        s.header().seqno = next_seqno();  // 发送的 seg 的 seqno
        if (!stream_in().buffer_empty()) {
            size_t size = min(stream_in().buffer_size(), _remain_size);
            size = min(size, TCPConfig::MAX_PAYLOAD_SIZE);
            s.payload() = stream_in().read(size);

            _next_seqno += size;  // 塞入后，下一个 seg 的 起始下标

            _remain_size -= size;
            if (_remain_size >= 1) {  // 剩余 至少一个 位置塞入 fin
                s.header().fin = true;
                _next_seqno++;
                _fin_sent = true;
            }

        } else {  // 只发送 fin
            s.header().fin = true;
            s.header().seqno = next_seqno();
            _next_seqno++;

            _fin_sent = true;
        }

        _store.push(s);         // seg 加入计时器队列，超时就准备发送
        _segments_out.push(s);  // seg 塞进 seg 队列
    }

    // byte stream 有数据，从里面拿出塞进 seg 队列
    while (!stream_in().input_ended() && !stream_in().buffer_empty() && _remain_size > 0) {
        s.header().seqno = next_seqno();  // 发送的 seg 的 seqno

        // 判断是否能把 byte stream 传完, 选较小塞入 seg
        //        cout << _remain_size << endl;
        size_t size = min(stream_in().buffer_size(), _remain_size);
        size = min(size, TCPConfig::MAX_PAYLOAD_SIZE);
        s.payload() = stream_in().read(size);

        _next_seqno += size;  // 塞入后，下一个 seg 的 起始下标

        //        cout << s.payload().str() << "  in" << endl;
        _store.push(s);         // seg 加入计时器队列，超时就准备发送
        _segments_out.push(s);  // seg 塞进 seg 队列

        _remain_size -= size;  // 及时减去 remain_size，防止 循环出错
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 1. 先进行判断ackno 合法性
    if (ackno.raw_value() <= _isn.raw_value()             // ackno 序号不合法
        || _acked.raw_value() > ackno.raw_value()         // 或者是 传来之前的 ack
        || ackno.raw_value() > next_seqno().raw_value())  // 或者是 传来的 ack 确认值 > 已发送的最大序号
    {
        return;  // 有以上就是情况不合法，直接return
    }

    if (window_size == 0) {
        _window_size = 0;
    }

    _acked = ackno;                         // 确认已获取的 最新 ackno
    _NEXT_SeqNo_MAX = ackno + window_size;  // 更新最大值

    // 更新 store 存储队列，同时 pop 掉已确认的数据
    bool refreshed = false;
    while (!_store.empty() &&
           ackno.raw_value() >= _store.front().header().seqno.raw_value() + _store.front().length_in_sequence_space()) {
        _store.pop();

        if (!refreshed) {
            // 可以设个 flag, 只执行一次
            _retx_attempts = 0;
            _time_left = _initial_retransmission_timeout;
            refreshed = true;
        }
    }

    // 试探成功，就刷新
    if (_0_is_sent) {
        _0_is_sent = false;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 不直接相减，防止 结果为负, size_t 是 unsigned 转为 很大的正数
    if (_time_left > ms_since_last_tick) {
        _time_left -= ms_since_last_tick;
    } else {
        // 到重传时间，此时 seg 重传的队列有数据，就重传最早的
        if (!_store.empty()) {
            _segments_out.push(_store.front());
        }
        // 窗口是 0，说明是试探性质的数据，不 扩大 重传时间
        if (_window_size != 0) {
            // 重传次数 +1, 计时器时间加倍
            _retx_attempts++;
        }
        // 左移, 相当于加 _retx_attempts 倍
        _time_left = _initial_retransmission_timeout << _retx_attempts;
    }
}
// 返回连续重传的次数
unsigned int TCPSender::consecutive_retransmissions() const { return _retx_attempts; }

// 发送空 seg
void TCPSender::send_empty_segment() {
    //    cout << "w34q" << endl;
    TCPSegment s;
    s.header().seqno = next_seqno();
    _store.push(s);
    _segments_out.push(s);
}
