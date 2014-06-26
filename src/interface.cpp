//
// (C) Jan de Vaan 2007-2010, all rights reserved. See the accompanying "License.txt" for licensed use.
//


// Implement correct linkage for win32 dlls
#if defined(WIN32) && defined(CHARLS_DLL)
#define CHARLS_IMEXPORT(returntype) __declspec(dllexport) returntype __stdcall
#else
#define CHARLS_IMEXPORT(returntype) returntype
#endif

#include "config.h"
#include "util.h"
#include "interface.h"
#include "header.h"
#include "jpegstreamreader.h"
#include "jpegstreamwriter.h"
#include "jpegmarkersegment.h"
#include <sstream>


static JLS_ERROR CheckInput(const ByteStreamInfo& uncompressedStream, const JlsParameters* pparams)
{
	if (!pparams)
		return InvalidJlsParameters;

	if (!uncompressedStream.rawStream && !uncompressedStream.rawData)
		return InvalidJlsParameters;

	if (pparams->width < 1 || pparams->width > 65535)
		return ParameterValueNotSupported;

	if (pparams->height < 1 || pparams->height > 65535)
		return ParameterValueNotSupported;

	if (uncompressedStream.rawData)
	{
		if (uncompressedStream.count < size_t(pparams->height * pparams->width * pparams->components * (pparams->bitspersample > 8 ? 2 : 1)))
			return UncompressedBufferTooSmall;
	}
	else if (!uncompressedStream.rawStream)
		return InvalidJlsParameters;

	return CheckParameterCoherent(*pparams);
}


CHARLS_IMEXPORT(JLS_ERROR) JpegLsEncodeStream(ByteStreamInfo compressedStreamInfo, size_t* pcbyteWritten, ByteStreamInfo rawStreamInfo, struct JlsParameters* pparams)
{
	if (!pcbyteWritten)
		return InvalidJlsParameters;

	JLS_ERROR parameterError = CheckInput(rawStreamInfo, pparams);
	if (parameterError != OK)
		return parameterError;

	try
	{
		JlsParameters info = *pparams;
		if (info.bytesperline == 0)
		{
			info.bytesperline = info.width * ((info.bitspersample + 7)/8);
			if (info.ilv != ILV_NONE)
			{
				info.bytesperline *= info.components;
			}
		}

		JpegStreamWriter writer;
		if (info.jfif.Ver)
		{
			writer.AddSegment(JpegMarkerSegment::CreateJpegFileInterchangeFormatMarker(info.jfif));
		}

		writer.AddSegment(JpegMarkerSegment::CreateStartOfFrameMarker(info.width, info.height, info.bitspersample, info.components));


		if (info.colorTransform != 0)
		{
			writer.AddColorTransform(info.colorTransform);
		}

		if (info.ilv == ILV_NONE)
		{
			LONG cbyteComp = info.width * info.height * ((info.bitspersample + 7) / 8);
			for (LONG component = 0; component < info.components; ++component)
			{
				writer.AddScan(rawStreamInfo, &info);
				SkipBytes(&rawStreamInfo, cbyteComp);
			}
		}
		else
		{
			writer.AddScan(rawStreamInfo, &info);
		}
	
		writer.Write(compressedStreamInfo);
		*pcbyteWritten = writer.GetBytesWritten();
		return OK;
	}
	catch (const JlsException& e)
	{
		return e._error;
	}
}


CHARLS_IMEXPORT(JLS_ERROR) JpegLsDecodeStream(ByteStreamInfo rawStream, ByteStreamInfo compressedStream, JlsParameters* info)
{
	JpegStreamReader reader(compressedStream);

	if (info)
	{
		reader.SetInfo(info);
	}

	try
	{
		reader.Read(rawStream);
		return OK;
	}
	catch (const JlsException& e)
	{
		return e._error;
	}
}


CHARLS_IMEXPORT(JLS_ERROR) JpegLsReadHeaderStream(ByteStreamInfo rawStreamInfo, JlsParameters* pparams)
{
	try
	{
		JpegStreamReader reader(rawStreamInfo);
		reader.ReadHeader();
		reader.ReadStartOfScan(true);
		JlsParameters info = reader.GetMetadata();
		*pparams = info;
		return OK;
	}
	catch (const JlsException& e)
	{
		return e._error;
	}
}

extern "C"
{
	CHARLS_IMEXPORT(JLS_ERROR) JpegLsEncode(void* compressedData, size_t compressedLength, size_t* pcbyteWritten, const void* uncompressedData, size_t uncompressedLength, struct JlsParameters* pparams)
	{
		ByteStreamInfo rawStreamInfo = FromByteArray(uncompressedData, uncompressedLength);
		ByteStreamInfo compressedStreamInfo = FromByteArray(compressedData, compressedLength);

		return JpegLsEncodeStream(compressedStreamInfo, pcbyteWritten, rawStreamInfo, pparams);
	}


	CHARLS_IMEXPORT(JLS_ERROR) JpegLsDecode(void* uncompressedData, size_t uncompressedLength, const void* compressedData, size_t compressedLength, JlsParameters* info)
	{
		ByteStreamInfo compressedStream = FromByteArray(compressedData, compressedLength);
		ByteStreamInfo rawStreamInfo = FromByteArray(uncompressedData, uncompressedLength);

		return JpegLsDecodeStream(rawStreamInfo, compressedStream, info);
	}

	
	CHARLS_IMEXPORT(JLS_ERROR) JpegLsReadHeader(const void* compressedData, size_t compressedLength, JlsParameters* pparams)
	{
		return JpegLsReadHeaderStream(FromByteArray(compressedData, compressedLength), pparams);
	}


	CHARLS_IMEXPORT(JLS_ERROR) JpegLsVerifyEncode(const void* uncompressedData, size_t uncompressedLength, const void* compressedData, size_t compressedLength)
	{
		JlsParameters info = JlsParameters();

		JLS_ERROR error = JpegLsReadHeader(compressedData, compressedLength, &info);
		if (error != OK)
			return error;

		ByteStreamInfo rawStreamInfo = FromByteArray(uncompressedData, uncompressedLength);

		error = CheckInput(rawStreamInfo, &info);

		if (error != OK)
			return error;

		JpegStreamWriter writer;
		if (info.jfif.Ver)
		{
			writer.AddSegment(JpegMarkerSegment::CreateJpegFileInterchangeFormatMarker(info.jfif));
		}

		writer.AddSegment(JpegMarkerSegment::CreateStartOfFrameMarker(info.width, info.height, info.bitspersample, info.components));


		if (info.ilv == ILV_NONE)
		{
			LONG fieldLength = info.width * info.height * ((info.bitspersample + 7) / 8);
			for (LONG component = 0; component < info.components; ++component)
			{
				writer.AddScan(rawStreamInfo, &info);
				SkipBytes(&rawStreamInfo, fieldLength);
			}
		}
		else
		{
			writer.AddScan(rawStreamInfo, &info);
		}

		std::vector<BYTE> rgbyteCompressed(compressedLength + 16);

		memcpy(&rgbyteCompressed[0], compressedData, compressedLength);

		writer.EnableCompare(true);
		writer.Write(FromByteArray(&rgbyteCompressed[0], rgbyteCompressed.size()));

		return OK;
	}


	CHARLS_IMEXPORT(JLS_ERROR) JpegLsDecodeRect(void* uncompressedData, size_t uncompressedLength, const void* compressedData, size_t compressedLength, JlsRect roi, JlsParameters* info)
	{
		ByteStreamInfo compressedStream = FromByteArray(compressedData, compressedLength);
		JpegStreamReader reader(compressedStream);

		ByteStreamInfo rawStreamInfo = FromByteArray(uncompressedData, uncompressedLength);

		if (info)
		{
			reader.SetInfo(info);
		}

		reader.SetRect(roi);

		try
		{
			reader.Read(rawStreamInfo);
			return OK;
		}
		catch (const JlsException& e)
		{
			return e._error;
		}
	}
}