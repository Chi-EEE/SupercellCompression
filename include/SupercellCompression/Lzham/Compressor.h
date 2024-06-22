#pragma once
#include <thread>

#include "SupercellCompression/Lzham.h"
#include "SupercellCompression/interface/CompressionInterface.h"

#include "SupercellCompression/Lzham/lzham_config.h"

#include "exception/MemoryAllocationException.h"
#include "SupercellCompression/exception/Lzham.h"
#include "memory/alloc.h"

namespace sc
{
	namespace Compressor
	{
		class Lzham : public CompressionInterface
		{
		public:
			// Compression parameters struct.
			// IMPORTANT: The values of dict_size_log2, table_update_rate, table_max_update_interval, and table_update_interval_slow_rate MUST
			// match during compression and decompression. The codec does not verify these values for you, if you don't use the same settings during
			// decompression it will fail.
			// The seed buffer's contents and size must match the seed buffer used during decompression.
			struct Props
			{
				Props()
				{
					uint32_t threads = std::thread::hardware_concurrency();
					max_helper_threads = threads == 0 ? -1 : (threads > lzham::MAX_HELPER_THREADS ? lzham::MAX_HELPER_THREADS : threads);
				}

				uint32_t struct_size = sizeof(Props);
				uint32_t dict_size_log2 = lzham::MIN_DICT_SIZE_LOG2; // set to the log2(dictionary_size), must range between [MIN_DICT_SIZE_LOG2, MAX_DICT_SIZE_LOG2_X86] for x86 MAX_DICT_SIZE_LOG2_X64 for x64
				lzham::Level level = lzham::Level::DEFAULT;          // set to FASTEST, etc.
				uint32_t table_update_rate = 0;						 // Controls tradeoff between ratio and decompression throughput. 0=default, or [1,MAX_TABLE_UPDATE_RATE], higher=faster but lower ratio.
				int32_t max_helper_threads = -1;					 // max # of additional "helper" threads to create, must range between [-1,MAX_HELPER_THREADS], where -1=max practical
				uint32_t compress_flags = 0;						 // optional compression flags (see lzham_compress_flags enum)
				uint32_t num_seed_bytes = 0;						 // for delta compression (optional) - number of seed bytes pointed to by 'seed'
				const void* seed = nullptr;							 // for delta compression (optional) - pointer to seed bytes buffer, must be at least 'num_seed_bytes' long

				// Advanced settings - set to 0 if you don't care.
				// table_max_update_interval/m_table_update_interval_slow_rate override m_table_update_rate and allow finer control over the table update settings.
				// If either are non-zero they will override whatever m_table_update_rate is set to. Just leave them 0 unless you are specifically customizing them for your data.

				// def=0, typical range 12-128 controls the max interval between table updates, higher=longer max interval (faster decode/lower ratio). Was 16 in prev. releases.
				uint32_t table_max_update_interval = 64;

				// def=0, 32 or higher scaled by 32, controls the slowing of the update update freq, higher=more rapid slowing (faster decode/lower ratio). Was 40 in prev. releases.
				uint32_t table_update_interval_slow_rate = 64;
			};
		public:
			static void write(Stream& input, Stream& output, Props& props)
			{
				output.write(&lzham::FileIdentifier, sizeof(lzham::FileIdentifier));
				output.write_unsigned_int(props.dict_size_log2);
				output.write_unsigned_long(input.length() - input.position());

				Lzham context(props);
				context.compress_stream(input, output);
			}

		public:
			Lzham(Props& props)
			{
				m_state = lzham_compress_init((lzham_compress_params*)&props);
				if (!m_state)
				{
					throw LzhamCompressInitException();
				}

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

				lzham_compress_deinit(m_state);
			}

			void compress_stream(Stream& input, Stream& output)
			{
				uint32_t input_buffer_position = 0;
				uint32_t input_buffer_offset = 0;

				size_t remain_bytes = input.length() - input.position();

				lzham_compress_status_t status = LZHAM_COMP_STATUS_FAILED;
				while (true)
				{
					if (input_buffer_offset == input_buffer_position)
					{
						input_buffer_position = static_cast<uint32_t>(std::min<size_t>(Lzham::Stream_Size, remain_bytes));
						if (input.read(m_input_buffer, input_buffer_position) != input_buffer_position)
						{
							throw LzhamCorruptedDecompressException();
						}

						remain_bytes -= input_buffer_position;

						input_buffer_offset = 0;
					}

					uint8_t* input_bytes = &m_input_buffer[input_buffer_offset];
					size_t input_bytes_lengh = input_buffer_position - input_buffer_offset;
					size_t output_bytes_length = Lzham::Stream_Size;

					status = lzham_compress(m_state, input_bytes, &input_bytes_lengh, m_output_buffer, &output_bytes_length, remain_bytes == 0);

					if (input_bytes_lengh)
					{
						input_buffer_offset += (uint32_t)input_bytes_lengh;
					}

					if (output_bytes_length)
					{
						output.write(m_output_buffer, output_bytes_length);
					}

					if (status >= LZHAM_COMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE)
						break;
				}
			}

		private:
			lzham_compress_state_ptr m_state;

			// -- Stream Buffer --
			uint8_t* m_input_buffer = nullptr;
			uint8_t* m_output_buffer = nullptr;

			static const size_t Stream_Size = 65536 * 4;
		};
	}
}