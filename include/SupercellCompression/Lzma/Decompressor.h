#pragma once
#include "SupercellCompression/Lzma.h"

#include "SupercellCompression/Lzma.h"
#include "SupercellCompression/interface/DecompressionInterface.h"

#include "memory/alloc.h"

#include "Alloc.h"
#include "LzmaDec.h"
#include "exception/MemoryAllocationException.h"
#include "SupercellCompression/exception/Lzma.h"

namespace sc
{
	namespace Decompressor
	{
		struct LzmaDecompressContext : public CLzmaDec {};

		const void* LzmaAlloc[] = { (void*)&sc::lzma::lzma_alloc, (void*)&sc::lzma::lzma_free };

		class Lzma : public DecompressionInterface
		{
		public:
			Lzma(uint8_t header[lzma::PROPS_SIZE], const uint64_t unpackedSize) : m_unpacked_size(unpackedSize)
			{
				m_context = new LzmaDecompressContext();
				LzmaDec_Construct(m_context);

				SRes res = LzmaDec_Allocate(m_context, header, LZMA_PROPS_SIZE, (ISzAllocPtr)&LzmaAlloc);
				if (res != SZ_OK)
				{
					throw LzmaDecompressInitException();
				}

				LzmaDec_Init(m_context);

				m_input_buffer = memalloc(Lzma::Stream_Size);
				m_output_buffer = memalloc(Lzma::Stream_Size);
			}
			~Lzma()
			{
				LzmaDec_Free(m_context, (ISzAllocPtr)&LzmaAlloc);

				if (m_input_buffer)
				{
					free(m_input_buffer);
				}

				if (m_output_buffer)
				{
					free(m_output_buffer);
				}

				delete m_context;
			}

			void decompress_stream(Stream& input, Stream& output) override
			{
				bool has_strict_bound = (m_unpacked_size != SIZE_MAX / 2) && (m_unpacked_size != SIZE_MAX);

				size_t in_position = 0, input_size = 0, out_position = 0;
				while (true)
				{
					if (in_position == input_size)
					{
						input_size = Lzma::Stream_Size;
						input.read(m_input_buffer, input_size);
						in_position = 0;
					}
					{
						SRes res;
						size_t in_processed = input_size - in_position;
						size_t out_processed = Lzma::Stream_Size - out_position;
						ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
						ELzmaStatus status;
						if (has_strict_bound && out_processed > m_unpacked_size)
						{
							out_processed = m_unpacked_size;
							finishMode = LZMA_FINISH_END;
						}

						res = LzmaDec_DecodeToBuf(m_context, m_output_buffer + out_position, &out_processed,
							m_input_buffer + in_position, &in_processed, finishMode, &status);
						in_position += in_processed;
						out_position += out_processed;
						m_unpacked_size -= out_processed;

						if (output.write(m_output_buffer, out_position) != out_position || res != SZ_OK)
							throw LzmaMissingEndMarkException();

						out_position = 0;

						if (has_strict_bound && m_unpacked_size == 0)
							// Decompression Success
							return;

						if (in_processed == 0 && out_processed == 0)
						{
							if (has_strict_bound || status != LZMA_STATUS_FINISHED_WITH_MARK)
								throw LzmaMissingEndMarkException();

							// Decompression Success
							return;
						}
					}
				}
			};

		private:
			LzmaDecompressContext* m_context;
			size_t m_unpacked_size;

			uint8_t* m_input_buffer = nullptr;
			uint8_t* m_output_buffer = nullptr;

			// -- Compression Buffer --
			static const size_t Stream_Size = 1 << 24;
		};
	}
}