#include <benchmark/benchmark.h>
#include "../src/source/Include_i.h"
#include <memory>
#include <thread>
#include <mutex>

#define BENCHMARK_DEFAULT_REGION                     ((PCHAR) "us-west-2")
#define BENCHMARK_STREAMING_TOKEN_DURATION           (40 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define BENCHMARK_JITTER_BUFFER_CLOCK_RATE           (1000)
#define BENCHMARK_SIGNALING_MASTER_CLIENT_ID         (PCHAR) "Test_Master_ClientId"
#define BENCHMARK_SIGNALING_VIEWER_CLIENT_ID         (PCHAR) "Test_Viewer_ClientId"
#define BENCHMARK_SIGNALING_CHANNEL_NAME             (PCHAR) "ScaryTestChannel_"
#define SIGNALING_BENCHMARK_CORRELATION_ID           (PCHAR) "Test_correlation_id"
#define BENCHMARK_SIGNALING_MESSAGE_TTL              (120 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define BENCHMARK_VIDEO_FRAME_SIZE                   (120 * 1024)
#define BENCHMARK_ICE_CONFIG_INFO_POLL_PERIOD        (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define BENCHMARK_ASYNC_ICE_CONFIG_INFO_WAIT_TIMEOUT (3 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define BENCHMARK_FILE_CREDENTIALS_FILE_PATH         (PCHAR) "credsFile"
#define ASSERT_STATUS(status)                                                                                                                        \
    do {                                                                                                                                             \
        CHK_LOG_ERR(status);                                                                                                                         \
        assert((status) == STATUS_SUCCESS);                                                                                                          \
    } while (0)

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

// This comes from Producer-C, but is not exported. We are copying it here instead of making it part of the public API.
// It *MAY* become de-synchronized. If you hit issues after updating Producer-C confirm these two structs are in sync
typedef struct {
    AwsCredentialProvider credentialProvider;
    PAwsCredentials pAwsCredentials;
} StaticCredentialProvider, *PStaticCredentialProvider;

class WebRtcClientBenchmarkBase : public benchmark::Fixture {
  public:
    SIGNALING_CLIENT_HANDLE mSignalingClientHandle;

    WebRtcClientBenchmarkBase();

    PCHAR getAccessKey()
    {
        return mAccessKey;
    }

    PCHAR getSecretKey()
    {
        return mSecretKey;
    }

    PCHAR getSessionToken()
    {
        return mSessionToken;
    }

    VOID initializeSignalingClient();
    VOID deinitializeSignalingClient();
    STATUS readFrameData(PBYTE pFrame, PUINT32 pSize, UINT32 index, PCHAR frameFilePath);
    VOID connectTwoPeers(PRtcPeerConnection offerPc, PRtcPeerConnection answerPc);
    VOID addTrackToPeerConnection(PRtcPeerConnection pRtcPeerConnection, PRtcMediaStreamTrack track, PRtcRtpTransceiver* transceiver, RTC_CODEC codec,
                                  MEDIA_STREAM_TRACK_KIND kind);
    VOID getIceServers(PRtcConfiguration pRtcConfiguration);

  protected:
    virtual VOID SetUp(const ::benchmark::State& state);
    virtual VOID TearDown(const ::benchmark::State& state);
    VOID initializeJitterBuffer(UINT32, UINT32, UINT32);

    PAwsCredentialProvider mBenchmarkCredentialProvider;

    PCHAR mAccessKey;
    PCHAR mSecretKey;
    PCHAR mSessionToken;
    PCHAR mRegion;
    PCHAR mCaCertPath;
    UINT64 mStreamingRotationPeriod;

    CHAR mDefaultRegion[MAX_REGION_NAME_LEN + 1];
    BOOL mAccessKeyIdSet;
    CHAR mChannelName[MAX_CHANNEL_NAME_LEN + 1];
};

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
