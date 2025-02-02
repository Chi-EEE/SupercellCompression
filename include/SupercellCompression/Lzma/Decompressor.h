#pragma once

#include "SupercellCompression/Lzma.h"
#include "SupercellCompression/interface/DecompressionInterface.h"

namespace sc
{
	namespace Decompressor
	{
		struct LzmaDecompressContext;

		class Lzma : public DecompressionInterface
		{
		public:
			Lzma(uint8_t header[lzma::PROPS_SIZE], const uint64_t unpacked_size);
			~Lzma();

			void decompress_stream(Stream& input, Stream& output) override;

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