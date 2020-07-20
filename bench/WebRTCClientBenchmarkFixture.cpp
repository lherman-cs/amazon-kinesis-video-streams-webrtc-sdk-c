#include "WebRTCClientBenchmarkFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

WebRtcClientBenchmarkBase::WebRtcClientBenchmarkBase()
    : mSignalingClientHandle(INVALID_SIGNALING_CLIENT_HANDLE_VALUE), mAccessKey(NULL), mSecretKey(NULL), mSessionToken(NULL), mRegion(NULL),
      mCaCertPath(NULL), mAccessKeyIdSet(FALSE)
{
}

VOID WebRtcClientBenchmarkBase::SetUp(const ::benchmark::State& state)
{
    UNUSED_PARAM(state);
    SET_LOGGER_LOG_LEVEL(LOG_LEVEL_INFO);
    initKvsWebRtc();

    if (NULL != (mAccessKey = getenv(ACCESS_KEY_ENV_VAR))) {
        mAccessKeyIdSet = TRUE;
    }

    mSecretKey = getenv(SECRET_KEY_ENV_VAR);
    mSessionToken = getenv(SESSION_TOKEN_ENV_VAR);

    if (NULL == (mRegion = getenv(DEFAULT_REGION_ENV_VAR))) {
        mRegion = BENCHMARK_DEFAULT_REGION;
    }

    if (NULL == (mCaCertPath = getenv(CACERT_PATH_ENV_VAR))) {
        mCaCertPath = (PCHAR) DEFAULT_KVS_CACERT_PATH;
    }

    if (mAccessKey) {
        ASSERT_STATUS(createStaticCredentialProvider(mAccessKey, 0, mSecretKey, 0, mSessionToken, 0, MAX_UINT64, &mBenchmarkCredentialProvider));
    } else {
        mBenchmarkCredentialProvider = nullptr;
    }

    // Prepare the benchmark channel name by prefixing with benchmark channel name
    // and generating random chars replacing a potentially bad characters with '.'
    STRCPY(mChannelName, BENCHMARK_SIGNALING_CHANNEL_NAME);
    UINT32 benchmarkNameLen = STRLEN(BENCHMARK_SIGNALING_CHANNEL_NAME);
    const UINT32 randSize = 16;

    PCHAR pCur = &mChannelName[benchmarkNameLen];

    for (UINT32 i = 0; i < randSize; i++) {
        *pCur++ = SIGNALING_VALID_NAME_CHARS[RAND() % (ARRAY_SIZE(SIGNALING_VALID_NAME_CHARS) - 1)];
    }

    *pCur = '\0';
}

VOID WebRtcClientBenchmarkBase::TearDown(const ::benchmark::State& state)
{
    UNUSED_PARAM(state);
    deinitKvsWebRtc();
    freeStaticCredentialProvider(&mBenchmarkCredentialProvider);
}

VOID WebRtcClientBenchmarkBase::initializeSignalingClient()
{
    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = NULL;
    signalingClientCallbacks.stateChangeFn = NULL;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_INFO;
    STRCPY(clientInfo.clientId, BENCHMARK_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.cachingPeriod = 0;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = BENCHMARK_SIGNALING_MESSAGE_TTL;
    if ((channelInfo.pRegion = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
        channelInfo.pRegion = (PCHAR) BENCHMARK_DEFAULT_REGION;
    }

    ASSERT_STATUS(
        createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, mBenchmarkCredentialProvider, &mSignalingClientHandle));

    if (!mAccessKeyIdSet) {
        mSignalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    }
}

VOID WebRtcClientBenchmarkBase::deinitializeSignalingClient()
{
    // Delete the created channel
    if (mAccessKeyIdSet) {
        ASSERT_STATUS(deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle), 0));
    }
}

STATUS WebRtcClientBenchmarkBase::readFrameData(PBYTE pFrame, PUINT32 pSize, UINT32 index, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT64 size = 0;

    CHK(pFrame != NULL && pSize != NULL, STATUS_NULL_ARG);

    SNPRINTF(filePath, MAX_PATH_LEN, "%s/frame-%03d.h264", frameFilePath, index);

    // Get the size and read into frame
    CHK_STATUS(readFile(filePath, TRUE, NULL, &size));
    CHK_STATUS(readFile(filePath, TRUE, pFrame, &size));

    *pSize = (UINT32) size;

CleanUp:

    return retStatus;
}

// Connect two RtcPeerConnections, and wait for them to be connected
// in the given amount of time.
VOID WebRtcClientBenchmarkBase::connectTwoPeers(PRtcPeerConnection offerPc, PRtcPeerConnection answerPc)
{
    RtcSessionDescriptionInit sdp;
    SIZE_T connectedCount = 0;

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        if (candidateStr != NULL) {
            std::thread(
                [customData](std::string candidate) {
                    RtcIceCandidateInit iceCandidate;
                    ASSERT_STATUS(deserializeRtcIceCandidateInit((PCHAR) candidate.c_str(), STRLEN(candidate.c_str()), &iceCandidate));
                    ASSERT_STATUS(addIceCandidate((PRtcPeerConnection) customData, iceCandidate.candidate));
                },
                std::string(candidateStr))
                .detach();
        }
    };

    auto onICEConnectionStateChangeHdlr = [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) -> void {
        if (newState == RTC_PEER_CONNECTION_STATE_CONNECTED) {
            ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    ASSERT_STATUS(peerConnectionOnIceCandidate(offerPc, (UINT64) answerPc, onICECandidateHdlr));
    ASSERT_STATUS(peerConnectionOnIceCandidate(answerPc, (UINT64) offerPc, onICECandidateHdlr));

    ASSERT_STATUS(peerConnectionOnConnectionStateChange(offerPc, (UINT64) &connectedCount, onICEConnectionStateChangeHdlr));
    ASSERT_STATUS(peerConnectionOnConnectionStateChange(answerPc, (UINT64) &connectedCount, onICEConnectionStateChangeHdlr));

    ASSERT_STATUS(createOffer(offerPc, &sdp));
    ASSERT_STATUS(setLocalDescription(offerPc, &sdp));
    ASSERT_STATUS(setRemoteDescription(answerPc, &sdp));

    ASSERT_STATUS(createAnswer(answerPc, &sdp));
    ASSERT_STATUS(setLocalDescription(answerPc, &sdp));
    ASSERT_STATUS(setRemoteDescription(offerPc, &sdp));

    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&connectedCount) != 2; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    assert(ATOMIC_LOAD(&connectedCount) == 2);
}

// Create track and transceiver and adds to PeerConnection
VOID WebRtcClientBenchmarkBase::addTrackToPeerConnection(PRtcPeerConnection pRtcPeerConnection, PRtcMediaStreamTrack track,
                                                         PRtcRtpTransceiver* transceiver, RTC_CODEC codec, MEDIA_STREAM_TRACK_KIND kind)
{
    MEMSET(track, 0x00, SIZEOF(RtcMediaStreamTrack));

    ASSERT_STATUS(addSupportedCodec(pRtcPeerConnection, codec));

    track->kind = kind;
    track->codec = codec;
    ASSERT_STATUS(generateJSONSafeString(track->streamId, MAX_MEDIA_STREAM_ID_LEN));
    ASSERT_STATUS(generateJSONSafeString(track->trackId, MAX_MEDIA_STREAM_ID_LEN));

    ASSERT_STATUS(addTransceiver(pRtcPeerConnection, track, NULL, transceiver));
}

STATUS awaitGetIceConfigInfoCount(SIGNALING_CLIENT_HANDLE signalingClientHandle, PUINT32 pIceConfigInfoCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 elapsed = 0;

    CHK(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingClientHandle) && pIceConfigInfoCount != NULL, STATUS_NULL_ARG);

    while (TRUE) {
        // Get the configuration count
        CHK_STATUS(signalingClientGetIceConfigInfoCount(signalingClientHandle, pIceConfigInfoCount));

        // Return OK if we have some ice configs
        CHK(*pIceConfigInfoCount == 0, retStatus);

        // Check for timeout
        CHK_ERR(elapsed <= BENCHMARK_ASYNC_ICE_CONFIG_INFO_WAIT_TIMEOUT, STATUS_OPERATION_TIMED_OUT,
                "Couldn't retrieve ICE configurations in alotted time.");

        THREAD_SLEEP(BENCHMARK_ICE_CONFIG_INFO_POLL_PERIOD);
        elapsed += BENCHMARK_ICE_CONFIG_INFO_POLL_PERIOD;
    }

CleanUp:

    return retStatus;
}

VOID WebRtcClientBenchmarkBase::getIceServers(PRtcConfiguration pRtcConfiguration)
{
    UINT32 i, j, iceConfigCount, uriCount;
    PIceConfigInfo pIceConfigInfo;

    // Assume signaling client is already created
    ASSERT_STATUS(awaitGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount));

    // Set the  STUN server
    SNPRINTF(pRtcConfiguration->iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, BENCHMARK_DEFAULT_REGION);

    for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
        ASSERT_STATUS(signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
        for (j = 0; j < pIceConfigInfo->uriCount; j++) {
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

            uriCount++;
        }
    }
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
