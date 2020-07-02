#include <libwebm/mkvparser.hpp>
#include <libwebm/mkvreader.hpp>
#include <vpx/vp8.h>
#include <vpx/vp8cx.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vpx_image.h>
#include <vpx/vpx_codec.h>
#include <iostream>

struct vpx_codec_info {
	vpx_codec_iface_t* vp9_iface;
	vpx_codec_ctx_t codec_ctx;
	vpx_codec_dec_cfg dec_cfg;
	vpx_codec_flags_t flags = VPX_CODEC_USE_POSTPROC;
};

vpx_codec_info g_codec_info{};

void decode_frame(vpx_codec_info& codec, const mkvparser::Block::Frame& mkvframe, mkvparser::IMkvReader* reader)
{
	uint8_t* data = (uint8_t*)malloc(mkvframe.len);
	mkvframe.Read(reader,data);
	vpx_codec_err_t err = vpx_codec_decode(&codec.codec_ctx,data, mkvframe.len,nullptr,0);
	printf(vpx_codec_err_to_string(err));
	vpx_image_t* img = NULL;
	vpx_codec_iter_t iter = NULL;
	while ((img = vpx_codec_get_frame(&codec.codec_ctx, &iter)) != NULL) {
	}
	free(data);
}

void vpx_codec_init(vpx_codec_info& info, int width, int height, int ver)
{
	info.vp9_iface = vpx_codec_vp9_dx();
	info.dec_cfg.w = width;
	info.dec_cfg.h = height;
	info.dec_cfg.threads = 1;
	info.flags = VPX_CODEC_USE_POSTPROC;
	vpx_codec_caps_t caps = vpx_codec_get_caps(info.vp9_iface);
	vpx_codec_err_t err = vpx_codec_dec_init(&info.codec_ctx, info.vp9_iface,&info.dec_cfg, info.flags);
	printf(vpx_codec_err_to_string(err));
}

int read_matroska(const char* file)
{
	int maj, min, build, rev;
	mkvparser::GetVersion(maj, min, build, rev);
	printf("libwebm verison: %d.%d.%d.%d\n", maj, min, build, rev);
	MkvReader reader;
	reader.Open(file);
	mkvparser::EBMLHeader header;
	long long pos;
	header.Parse(&reader,pos);
	std::cout<<"docType:\t"<<header.m_docType<<"\ndocTypeReadVersion\t"<<header.m_docTypeReadVersion
		<<"\ndocTypeVersion\t"<<header.m_docTypeVersion << "\nmaxIdLength\t" <<header.m_maxIdLength
		<< "\nmaxSizeLength\t" << header.m_maxSizeLength << "\nreadVersion\t" << header.m_readVersion
		<<"\nversion\t" <<header.m_version<<std::endl;
	mkvparser::Segment* pSegment;
	long long ret = mkvparser::Segment::CreateInstance(&reader, pos, pSegment);
	if (ret) {
		printf("\n Segment::CreateInstance() failed.");
		return -1;
	}
	pSegment->Load();
	if (ret < 0) {
		printf("\n Segment::Load() failed.");
		return -1;
	}
	const mkvparser::SegmentInfo* const pSegmentInfo = pSegment->GetInfo();

	const long long timeCodeScale = pSegmentInfo->GetTimeCodeScale();
	const long long duration_ns = pSegmentInfo->GetDuration();
	const char* const pTitle = pSegmentInfo->GetTitleAsUTF8();
	const char* const pMuxingApp = pSegmentInfo->GetMuxingAppAsUTF8();
	const char* const pWritingApp = pSegmentInfo->GetWritingAppAsUTF8();
	printf("\n");
	printf("\t   Segment Info\n");
	printf("TimeCodeScale\t\t: %lld \n", timeCodeScale);
	printf("Duration\t\t: %lld\n", duration_ns);
	const double duration_sec = double(duration_ns) / 1000000000;
	printf("Duration(secs)\t\t:%7.3f\n", duration_sec);

	if (pTitle == NULL)
		printf("Track Name\t\t: NULL\n");
	else
		printf("Track Name\t\t: %s\n", pTitle);

	if (pMuxingApp == NULL)
		printf("Muxing App\t\t: NULL\n");
	else
		printf("Muxing App\t\t: %s\n", pMuxingApp);

	if (pWritingApp == NULL)
		printf("Writing App\t\t: NULL\n");
	else
		printf("Writing App\t\t: %s\n", pWritingApp);

	// pos of segment payload
	printf("Position(Segment)\t: %lld\n", pSegment->m_start);

	// size of segment payload
	printf("Size(Segment)\t\t: %lld\n", pSegment->m_size);
	unsigned num = pSegment->GetCount();
	mkvparser::Tracks* const pTracks = pSegment->GetTracks();

	unsigned long i = 0;
	const unsigned long j = pTracks->GetTracksCount();

	enum { VIDEO_TRACK = 1, AUDIO_TRACK = 2 };

	printf("\n\t   Track Info\n");

	while (i != j) {
		const mkvparser::Track* const pTrack = pTracks->GetTrackByIndex(i++);
		if (pTrack == NULL)
			continue;
		const long long trackType = pTrack->GetType();
		const long long trackNumber = pTrack->GetNumber();
		const unsigned long long trackUid = pTrack->GetUid();
		const char* const pTrackName = pTrack->GetNameAsUTF8();

		printf("Track Type\t\t: %ld\n", trackType);
		printf("Track Number\t\t: %ld\n", trackNumber);
		printf("Track Uid\t\t: %lld\n", trackUid);

		if (pTrackName == NULL)
			printf("Track Name\t\t: NULL\n");
		else
			printf("Track Name\t\t: %s \n", pTrackName);

		const char* const pCodecId = pTrack->GetCodecId();

		if (pCodecId == NULL)
			printf("Codec Id\t\t: NULL\n");
		else
			printf("Codec Id\t\t: %s\n", pCodecId);

		const char* const pCodecName = pTrack->GetCodecNameAsUTF8();

		if (pCodecName == NULL)
			printf("Codec Name\t\t: NULL\n");
		else
			printf("Codec Name\t\t: %s\n", pCodecName);

		if (trackType == VIDEO_TRACK) {
			const mkvparser::VideoTrack* const pVideoTrack =
				static_cast<const  mkvparser::VideoTrack*>(pTrack);

			const long long width = pVideoTrack->GetWidth();
			printf("Video Width\t\t: %lld\n", width);

			const long long height = pVideoTrack->GetHeight();
			printf("Video Height\t\t: %lld\n", height);

			const double rate = pVideoTrack->GetFrameRate();
			printf("Video Rate\t\t: %f\n\n", rate);

			vpx_codec_init(g_codec_info,pVideoTrack->GetWidth(), pVideoTrack->GetHeight(),9);
		}

		if (trackType == AUDIO_TRACK) {
			const  mkvparser::AudioTrack* const pAudioTrack =
				static_cast<const  mkvparser::AudioTrack*>(pTrack);

			const long long channels = pAudioTrack->GetChannels();
			printf("Audio Channels\t\t: %lld\n", channels);

			const long long bitDepth = pAudioTrack->GetBitDepth();
			printf("Audio BitDepth\t\t: %lld\n", bitDepth);

			const double sampleRate = pAudioTrack->GetSamplingRate();
			printf("Addio Sample Rate\t: %.3f\n\n", sampleRate);
		}
	}
	printf("\n\n\t   Cluster Info\n");
	const unsigned long clusterCount = pSegment->GetCount();

	printf("Cluster Count\t: %ld\n\n", clusterCount);

	if (clusterCount == 0) {
		printf("Segment has no clusters.\n");
		delete pSegment;
		return -1;
	}
	const mkvparser::Cluster* pCluster = pSegment->GetFirst();
	while ((pCluster != NULL) && !pCluster->EOS()) {
		const long long timeCode = pCluster->GetTimeCode();
		printf("Cluster Time Code\t: %lld\n", timeCode);

		const long long time_ns = pCluster->GetTime();
		printf("Cluster Time (ns)\t: %lld\n", time_ns);

		const mkvparser::BlockEntry* pBlockEntry = pCluster->GetFirst();

		while ((pBlockEntry != NULL) && !pBlockEntry->EOS()) {
			const mkvparser::Block* const pBlock = pBlockEntry->GetBlock();
			const long long trackNum = pBlock->GetTrackNumber();
			const unsigned long tn = static_cast<unsigned long>(trackNum);
			const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(tn);
			const long long trackType = pTrack->GetType();
			const int frameCount = pBlock->GetFrameCount();
			const long long time_ns = pBlock->GetTime(pCluster);

			printf("Block\t\t:%s,%s,%15lld\n",
				   (trackType == VIDEO_TRACK) ? "V" : "A",
				   pBlock->IsKey() ? "I" : "P",
				   time_ns);

			for (int i = 0; i < frameCount; ++i) {
				const mkvparser::Block::Frame& theFrame = pBlock->GetFrame(i);
				const long size = theFrame.len;
				const long long offset = theFrame.pos;
				printf("\t\t%ld\t,%lx\n", size, offset);
				if(trackType == VIDEO_TRACK)
					decode_frame(g_codec_info,theFrame,&reader);
			}
			printf("\n");
			pBlockEntry = pCluster->GetNext(pBlockEntry);
		}

		pCluster = pSegment->GetNext(pCluster);
	}

	delete pSegment;
	return 0;
}

int main()
{

	return read_matroska("D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct.webm");
}

