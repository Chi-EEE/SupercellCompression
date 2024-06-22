#pragma once
#include "SupercellCompression/Lzham.h"
#include "SupercellCompression/interface/DecompressionInterface.h"
//#include <lzham.h>
#include "SupercellCompression/Lzham/lzham_config.h"

#include "exception/MemoryAllocationException.h"
#include "SupercellCompression/exception/Lzham.h"
#include "memory/alloc.h"

namespace sc
{
	namespace Decompressor
	{
		class Lzham : public DecompressionInterface
		{
		public:
			struct Props
			{
				Props()
				{
					struct_size = sizeof(lzham_decompress_params);
				}

				uint32_t struct_size;
				uint32_t dict_size_log2 = lzham::MIN_DICT_SIZE_LOG2;         // set to the log2(dictionary_size), must range between [MIN_DICT_SIZE_LOG2, MAX_DICT_SIZE_LOG2_X86] for x86 LZHAM_MAX_DICT_SIZE_LOG2_X64 for x64
				uint32_t table_update_rate = 0;								 // Controls tradeoff between ratio and decompression throughput. 0=default, or [1,LZHAM_MAX_TABLE_UPDATE_RATE], higher=faster but lower ratio.
				uint32_t decompress_flags = 0;								 // optional decompression flags (see lzham_decompress_flags enum)
				uint32_t num_seed_bytes = 0;								 // for delta compression (optional) - number of seed bytes pointed to by m_pSeed_bytes
				const void* seed = nullptr;									 // for delta compression (optional) - pointer to seed bytes buffer, must be at least m_num_seed_bytes long

				// Advanced settings - set to 0 if you don't care.
				  // 'table_max_update_interval'/'table_update_interval_slow_rate override' 'table_update_rate' and allow finer control over the table update settings.
				  // If either are non-zero they will override whatever m_table_update_rate is set to. Just leave them 0 unless you are specifically customizing them for your data.

				// def=0, typical range 12-128, controls the max interval between table updates, higher=longer max interval (faster decode/lower ratio). Was 16 in prev. releases.
				uint32_t table_max_update_interval = 64;
				// def=0, 32 or higher, scaled by 32, controls the slowing of the update update freq, higher=more rapid slowing (faster decode/lower ratio). Was 40 in prev. releases.
				uint32_t table_update_interval_slow_rate = 64;

				size_t unpacked_length;
			};
		public:
			Lzham(Props& props)
			{
				m_state = lzham_decompress_init((lzham_decompress_params*)&props);
				if (!m_state)
				{
					throw LzhamDecompressInitException();
				}

				m_unpacked_length = props.unpacked_length;

				m_input_buffer = memalloc(Lzham::Stream_Size);
				m_output_buffer = memalloc(Lzham::Stream_Size);
			}
			~Lzham()
			{
				if (m_input_buffer)
				{
					free(m_input_buffer);
				}

				if (m_output_buffer)
				{
					free(m_output_buffer);
				}

				lzham_decompress_deinit(m_state);
			}

			void decompress_stream(Stream& input, Stream& output)
			{
				size_t remain_bytes = input.length() - input.position();

				uint32_t buffer_size = 0, buffer_offset = 0;
				lzham_decompress_status_t status;
				while (true)
				{
					if (buffer_offset == buffer_size)
					{
						buffer_size = static_cast<uint32_t>(Lzham::Stream_Size < remain_bytes ? Lzham::Stream_Size : remain_bytes);
						input.read(m_input_buffer, buffer_size);

						remain_bytes -= buffer_size;

						buffer_offset = 0;
					}

					uint8_t* input_bytes = &m_input_buffer[buffer_offset];
					size_t input_bytes_length = buffer_size - buffer_offset;
					size_t output_bytes_length = Lzham::Stream_Size;

					status = lzham_decompress(m_state, input_bytes, &input_bytes_length, m_output_buffer, &output_bytes_length, remain_bytes == 0);

					if (input_bytes_length)
					{
						buffer_offset += (uint32_t)input_bytes_length;
					}

					if (output_bytes_length)
					{
						output.write(m_output_buffer, static_cast<uint32_t>(output_bytes_length));

						if (output_bytes_length > m_unpacked_length)
						{
							break;
						}
						m_unpacked_length -= output_bytes_length;
					}

					if (status >= LZHAM_DECOMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE)
						break;
				}
			}

		private:
			lzham_decompress_state_ptr m_state = nullptr;
			size_t m_unpacked_length;

			// -- Stream Buffer --
			uint8_t* m_input_buffer = nullptr;
			uint8_t* m_output_buffer = nullptr;

			static const size_t Stream_Size = 65536 * 4;
		};
	}
}