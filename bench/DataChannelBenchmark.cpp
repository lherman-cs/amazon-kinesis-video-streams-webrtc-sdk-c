#include "WebRTCClientBenchmarkFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DataChannelBenchmark : public WebRtcClientBenchmarkBase {
};

BENCHMARK_DEFINE_F(DataChannelBenchmark, BM_DataChannel)(benchmark::State& state)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pDataChannel = nullptr;
    ATOMIC_BOOL remoteChannelOpened = FALSE, localChannelOpened = FALSE;
    INT64 dataSize = state.range(0);
    PBYTE data = (PBYTE) MEMALLOC(dataSize);

    DLOGI("BENCHMARK WITH %ld bytes", dataSize);
    assert(data != NULL);

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_ALL;

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        UNUSED_PARAM(pRtcDataChannel);
        DLOGI("ON DATA CHANNEL");
        dataChannelOnMessage(pRtcDataChannel, 0, [](UINT64, PRtcDataChannel, BOOL, PBYTE, UINT32) {});
        ATOMIC_STORE_BOOL((PSIZE_T) customData, TRUE);
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        DLOGI("ON OPEN");
        ATOMIC_STORE_BOOL((PSIZE_T) customData, TRUE);
    };

    ASSERT_STATUS(createPeerConnection(&configuration, &offerPc));
    ASSERT_STATUS(createPeerConnection(&configuration, &answerPc));

    ASSERT_STATUS(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteChannelOpened, onDataChannel));
    ASSERT_STATUS(createDataChannel(offerPc, (PCHAR) "Benchmark", nullptr, &pDataChannel));
    ASSERT_STATUS(dataChannelOnOpen(pDataChannel, (UINT64) &localChannelOpened, dataChannelOnOpenCallback));

    connectTwoPeers(offerPc, answerPc);

    // Busy wait until remote channel open and dtls completed
    for (auto i = 0; i <= 100 && (!ATOMIC_LOAD_BOOL(&localChannelOpened) || !ATOMIC_LOAD_BOOL(&remoteChannelOpened)); i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    assert(ATOMIC_LOAD_BOOL(&localChannelOpened) && ATOMIC_LOAD_BOOL(&remoteChannelOpened));

    UINT64 errCount = 0;
    for (auto _ : state) {
        // errCount += dataChannelSend(pDataChannel, TRUE, (PBYTE) data, dataSize) == STATUS_SUCCESS ? 0 : 1;
        ASSERT_STATUS(dataChannelSend(pDataChannel, TRUE, (PBYTE) data, dataSize));
    }
    // DLOGI("ERROR COUNT: %lu", errCount);
    state.SetBytesProcessed((((INT64) state.iterations()) - errCount) * dataSize);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
    MEMFREE(data);
}

BENCHMARK_REGISTER_F(DataChannelBenchmark, BM_DataChannel)->Range(8, 8 << 2);

/*
BENCHMARK_DEFINE_F(DataChannelBenchmark, BM_DataChannel)(benchmark::State& state)
{
    char* src = new char[state.range(0)];
    char* dst = new char[state.range(0)];
    memset(src, 'x', state.range(0));
    for (auto _ : state)
        memcpy(dst, src, state.range(0));
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
    delete[] src;
    delete[] dst;
}

BENCHMARK_REGISTER_F(DataChannelBenchmark, BM_DataChannel)->Range(8, 8 << 10)->MeasureProcessCPUTime()->UseRealTime();
*/

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
