#pragma once
#include <thread>

#include "SupercellCompression/Lzma.h"
#include "SupercellCompression/interface/CompressionInterface.h"

#include "memory/alloc.h"

#include "Alloc.h"
#include "LzmaEnc.h"
#include "SupercellCompression/exception/Lzma.h"

const size_t Stream_Size = (4 * 1024 * 1024); // 4MB

struct CSeqInStreamWrap
{
	ISeqInStream vt;
	sc::Stream* input;
};

struct CSeqOutStreamWrap
{
	ISeqOutStream vt;
	sc::Stream* output;
};

static SRes LzmaStreamRead(const ISeqInStream* p, void* data, size_t* size)
{
	CSeqInStreamWrap* wrap = CONTAINER_FROM_VTBL(p, CSeqInStreamWrap, vt);
	size_t bufferReadSize = (*size < Stream_Size) ? *size : Stream_Size;
	size_t readSize = wrap->input->read(data, bufferReadSize);

	*size = readSize;
	return SZ_OK;
};

static size_t LzmaStreamWrite(const ISeqOutStream* p, const void* buf, size_t size)
{
	auto* wrap = CONTAINER_FROM_VTBL(p, CSeqOutStreamWrap, vt);
	return wrap->output->write((void*)buf, size);
};

namespace sc
{
	namespace Compressor
	{
		const void* LzmaAlloc[] = { (void*)&sc::lzma::lzma_alloc, (void*)&sc::lzma::lzma_free };

		class Lzma : public CompressionInterface
		{
		public:
			struct Props
			{
				/* Compression Level: [0 - 9] */
				int level = 5;

				/* (1 << 12) <= dictSize <= (1 << 27) for 32-bit version */
				/* (1 << 12) <= dictSize <= (3 << 29) for 64-bit version */
				/*
					The maximum value for dictionary size is 1 GB = 2^30 bytes.
					Dictionary size is calculated as DictionarySize = 2^N bytes.
					For decompressing file compressed by LZMA method with dictionary
					size D = 2^N you need about D bytes of memory (RAM).
				*/
				uint32_t dict_size = (1 << 24);

				/* Number of literal context bits: [0 - 8] */
				/*
					Sometimes lc=4 gives gain for big files.
				*/
				int lc = 3;

				/* Number of literal pos bits: [0 - 4] */
				/*
					lp switch is intended for periodical data when period is
					equal 2^N. For example, for 32-bit (4 bytes)
					periodical data you can use lp=2. Often it's better to set lc0,
					if you change lp switch.
				*/
				int lp = 0;

				/* Number of pos bits: [0 - 4]*/
				/*
					pb switch is intended for periodical data
					when period is equal 2^N.
				*/
				int pb = 2;

				lzma::Mode mode = lzma::Mode::Normal;

				/* Number of fast bytes: [5 - 273]*/
				/*
					Usually big number gives a little bit better compression ratio
					and slower compression process.
				*/
				int fb = 32;

				/*Match Finder */
				lzma::BinaryMode binaryMode = lzma::BinaryMode::BinTree;

				/* 2, 3 or 4*/
				int hash_bytes_count = 4;

				/*Number of cycles for match finder: [1 - (1 << 30)]*/
				uint32_t mc = 32;

				unsigned write_end_mark = false;

				/* 1 or 2 */
				int threads = std::thread::hardware_concurrency() >= 2 ? 2 : 1;

				/* Estimated size of data that will be compressed */
				/*
					Encoder uses this value to reduce dictionary size
				*/
				uint64_t reduce_size = UINT64_MAX;

				uint64_t affinity = 0;

				/* If positive, writes the file length to a 64-bit integer, otherwise to a 32-bit integer */
				bool use_long_unpacked_length = true;
			};

		public:
			Lzma(Props& props) {
				m_context = LzmaEnc_Create((ISzAllocPtr)&LzmaAlloc);
				if (!m_context)
				{
					throw LzmaCompressInitException();
				}

				SRes res;
				res = LzmaEnc_SetProps(m_context, (CLzmaEncProps*)&props);
				if (res != SZ_OK)
				{
					throw LzmaCompressInitException();
				}

				m_use_long_unpacked_data = props.use_long_unpacked_length;
			}
			~Lzma() {
				LzmaEnc_Destroy(m_context, (ISzAllocPtr)&LzmaAlloc, (ISzAllocPtr)&LzmaAlloc);
			}

			void compress_stream(Stream& input, Stream& output)
			{
				SizeT header_length = lzma::PROPS_SIZE;
				Byte header[lzma::PROPS_SIZE];
				LzmaEnc_WriteProperties(m_context, (Byte*)&header, (SizeT*)&header_length);
				output.write(header, header_length);

				size_t file_size = input.length() - input.position();
				if (m_use_long_unpacked_data)
				{
					output.write_unsigned_long(file_size);
				}
				else
				{
					output.write_unsigned_int(static_cast<uint32_t>(file_size));
				}

				CSeqInStreamWrap inWrap;
				inWrap.vt.Read = LzmaStreamRead;
				inWrap.input = &input;

				CSeqOutStreamWrap outWrap;
				outWrap.vt.Write = LzmaStreamWrite;
				outWrap.output = &output;

				LzmaEnc_Encode(m_context, &outWrap.vt, &inWrap.vt, nullptr, (ISzAllocPtr)&LzmaAlloc, (ISzAllocPtr)&LzmaAlloc);
			}

		private:
			CLzmaEncHandle m_context;
			bool m_use_long_unpacked_data;
		};
	}
}