#pragma once

#include <vector>
#include <string>
#include <thread>

#include "Zstd/Decompressor.h"
#include "Lzma/Decompressor.h"
#include "Lzham/Decompressor.h"

#include "io/stream.h"

#include "generic/md5.h"
#include "io/buffer_stream.h"

#include "io/memory_stream.h"
#include "exception/io/BinariesExceptions.h"

namespace sc {
	namespace ScCompression
	{
		const uint16_t SC_MAGIC = 0x4353;
		const uint32_t SCLZ_MAGIC = 0x5A4C4353;

		enum class Signature
		{
			Lzma,
			Lzham,
			Zstandard
		};

		struct MetadataAsset
		{
			std::string name;
			std::vector<char> hash;
		};

		typedef std::vector<MetadataAsset> MetadataAssetArray;

		namespace Decompressor
		{
			void read_metadata(uint8_t* buffer_end, MetadataAssetArray& metadataArray)
			{
				uint32_t asset_total_count = 0;
				bool unknown_bool = false;

				uint8_t* metadata_header = buffer_end - 6;

				// -- Strings and Hashes data --
				uint32_t strings_info_field_size = 0;
				uint8_t hash_info_field_size = 0;

				uint8_t* asset_info_ptr = nullptr;
				uint8_t* strings_ptr = nullptr;

				char info_field_size = 0;
				char metadata_flags = *(char*)metadata_header;
				if ((metadata_flags & 0xFC) == 0x24)
				{
					// -- Initial Part --
					uint8_t asset_info_field_size = *(metadata_header + 1);

					uint8_t* asset_info_offset_ptr = metadata_header - asset_info_field_size;

					uint32_t asset_info_offset;
					if (asset_info_field_size > 3)
					{
						asset_info_offset = *(uint32_t*)asset_info_offset_ptr;
					}
					else if (asset_info_field_size <= 1)
					{
						asset_info_offset = *asset_info_offset_ptr;
					}
					else
					{
						asset_info_offset = *(uint16_t*)asset_info_offset_ptr;
					}

					// -- Second Part --
					asset_info_ptr = asset_info_offset_ptr - asset_info_offset;

					char strings_bits_offset = metadata_flags & 3;
					info_field_size = 1ULL << (metadata_flags & 3);
					if (info_field_size >= 3u)
					{
						int32_t strings_data_offset = 0xFFFFFFFDULL << strings_bits_offset;
						if (info_field_size >= 8u)
						{
							uint8_t* string_data_ptr = asset_info_ptr + strings_data_offset;

							strings_info_field_size = 8;
							hash_info_field_size = 8;

							strings_ptr = &asset_info_ptr[strings_data_offset - *(uint32_t*)&asset_info_ptr[strings_data_offset]];
							strings_info_field_size = *(uint8_t*)(string_data_ptr + info_field_size);
						}
						else
						{
							hash_info_field_size = 1ULL << (metadata_flags & 3);

							strings_info_field_size = *(uint32_t*)&asset_info_ptr[strings_data_offset + info_field_size];

							uint32_t strings_array_offset = *(uint32_t*)(asset_info_ptr + strings_data_offset);

							int strings_offset = strings_data_offset - strings_array_offset;
							strings_ptr = asset_info_ptr + strings_offset;
						}

						asset_total_count = *(uint32_t*)(asset_info_ptr - info_field_size);
						unknown_bool = false;
					}
					else if (info_field_size > 1u)
					{
						hash_info_field_size = 1ULL << (metadata_flags & 3);

						int bit_offset = 0xFFFFFFFD << strings_bits_offset;
						uint8_t* strings_array_data_offset = &asset_info_ptr[bit_offset];

						strings_ptr = &strings_array_data_offset[-*(uint16_t*)strings_array_data_offset];

						strings_info_field_size = *(uint16_t*)(strings_array_data_offset + info_field_size);

						asset_total_count = *(uint16_t*)(asset_info_ptr - info_field_size);
						unknown_bool = true;
					}
					else
					{
						strings_info_field_size = asset_info_ptr[-2];

						strings_ptr = &asset_info_ptr[-3 - asset_info_ptr[-3]];

						asset_total_count = asset_info_ptr[-info_field_size];

						strings_info_field_size = 1;
						hash_info_field_size = 1;
					}
				}
				else
				{
					return;
				}

				// -- Last part --
				if (asset_total_count)
				{
					// Here we get number of strings and hashes. For some reason, in theory, there can be a different number of them.
					uint32_t strings_total_count;
					uint32_t hash_total_count;

					if (strings_info_field_size > 3)
					{
						strings_total_count = *(uint32_t*)(strings_ptr - strings_info_field_size);
					}
					else if (strings_info_field_size > 1)
					{
						strings_total_count = *(uint16_t*)(strings_ptr - strings_info_field_size);
					}
					else
					{
						strings_total_count = *(uint8_t*)(strings_ptr - strings_info_field_size);
					}

					if (unknown_bool)
					{
						if (hash_info_field_size > 1u)
							hash_total_count = *(uint16_t*)&asset_info_ptr[-info_field_size];
						else
							hash_total_count = asset_info_ptr[-info_field_size];
					}
					else
					{
						hash_total_count = *(uint32_t*)&asset_info_ptr[-info_field_size];
					}

					uint8_t* hash_flags_ptr = &asset_info_ptr[hash_total_count * info_field_size];

					for (uint32_t i = 0; asset_total_count > i; i++)
					{
						MetadataAsset asset;

						uint8_t* string_offset_ptr = strings_ptr + i * strings_info_field_size;
						int string_offset = 0;

						if (strings_total_count > i)
						{
							if (strings_info_field_size > 3)
							{
								string_offset = *(uint32_t*)string_offset_ptr;
							}
							else if (strings_info_field_size <= 1)
							{
								string_offset = *string_offset_ptr;
							}
							else
							{
								string_offset = *(uint16_t*)string_offset_ptr;
							}

							asset.name = std::string((const char*)&string_offset_ptr[-string_offset]);
						}

						if (hash_total_count > i)
						{
							uint8_t hash_flag = hash_flags_ptr[i];
							bool is_valid_hash = hash_flag >> 2 == 0x19;
							if (hash_flag >> 2 != 0x19)
								is_valid_hash = hash_flag >> 2 == 5;

							if (is_valid_hash)
							{
								uint8_t* hash_offset_ptr = &asset_info_ptr[i * info_field_size];

								int32_t hash_offset;
								if (hash_info_field_size > 3u)
								{
									hash_offset = *(uint32_t*)hash_offset_ptr;
								}
								else if (hash_info_field_size <= 1u) {
									hash_offset = *hash_offset_ptr;
								}
								else
								{
									hash_offset = *(uint16_t*)hash_offset_ptr;
								}

								uint8_t hash_length_field_size = 1ULL << (hash_flag & 3);

								uint8_t* hash_ptr = &hash_offset_ptr[-hash_offset];
								uint32_t hash_size = 0;
								if (hash_length_field_size > 3u)
								{
									hash_size = *(uint32_t*)&hash_ptr[-hash_length_field_size];
								}
								else if (hash_length_field_size > 1u)
								{
									hash_size = *(uint16_t*)&hash_ptr[-hash_length_field_size];
								}
								else
								{
									hash_size = hash_ptr[-hash_length_field_size];
								}

								asset.hash = std::vector<char>((char*)hash_ptr, (char*)hash_ptr + hash_size);
							}
						}

						metadataArray.push_back(asset);
					}
				}
			}

			void decompress(Stream& input, Stream& output, MetadataAssetArray* metadataArray = nullptr)
			{
				int16_t magic = input.read_unsigned_short(Endian::Big);
				if (magic != SC_MAGIC)
				{
					throw BadMagicException((uint8_t*)&SC_MAGIC, (uint8_t*)&magic, sizeof(uint16_t));
				}

				uint8_t* compressed_data_ptr = nullptr;
				size_t compressed_data_length = input.length();

				uint32_t version = input.read_unsigned_int(Endian::Big);
				if (version == 4)
				{
					version = input.read_unsigned_int(Endian::Big);

					uint8_t* buffer_end = (uint8_t*)input.data() + input.length();

					uint32_t chunk_length = swap_endian(*(uint32_t*)(buffer_end - 4));

					// metadata_length + metadata_length_bytes_size + START length
					compressed_data_length -= chunk_length;
					compressed_data_length -= 4 + 5;

					if (metadataArray)
					{
						read_metadata(buffer_end, *metadataArray);
					}
				}

				uint32_t hash_length = input.read_unsigned_int(Endian::Big);

				std::vector<uint8_t> hash(hash_length);
				input.read(hash.data(), hash_length);

				compressed_data_ptr = (uint8_t*)input.data() + input.position();
				compressed_data_length -= input.position();

				if (version == 3)
				{
					MemoryStream compressed_data(compressed_data_ptr, compressed_data_length);
					sc::Decompressor::Zstd context;
					context.decompress_stream(compressed_data, output);
				}
				else if (version == 1)
				{
					// If SCLZ
					if (*(uint32_t*)compressed_data_ptr == 0x5A4C4353)
					{
						MemoryStream compressed_data(compressed_data_ptr + 4, compressed_data_length - 4);
						sc::Decompressor::Lzham::Props props;
						props.dict_size_log2 = compressed_data.read_unsigned_byte();
						props.unpacked_length = compressed_data.read_unsigned_int();
						sc::Decompressor::Lzham context(props);
						context.decompress_stream(compressed_data, output);
					}
					else
					{
						MemoryStream compressed_data(compressed_data_ptr, compressed_data_length);

						uint8_t header[lzma::PROPS_SIZE];
						compressed_data.read(header, lzma::PROPS_SIZE);

						uint32_t unpacked_length = compressed_data.read_unsigned_int();

						sc::Decompressor::Lzma context(header, unpacked_length);
						context.decompress_stream(compressed_data, output);
					}
				}
			}
		}

		namespace Compressor
		{
			static constexpr int HASH_LENGTH = 16;
			const char* metadata_delim = "START";

			struct CompressorContext
			{
				Signature signature = Signature::Zstandard;

				bool write_assets = false;
				//MetadataAssetArray assets;

				uint32_t threads_count = std::thread::hardware_concurrency() <= 0 ? 1 : std::thread::hardware_concurrency();
			};

			void write_metadata(Stream& input)
			{
				// TODO !!!!!!!!!!!!!!!!!!!!!!
				input.write(metadata_delim, sizeof(metadata_delim));

				sc::BufferStream metadata;

				// metadata_flags
				metadata.write_byte(0);

				// data_info_offset_field_size
				metadata.write_byte(0);

				input.write(metadata.data(), metadata.length());
				input.write_unsigned_int(static_cast<uint32_t>(metadata.length()), Endian::Big);
			}

			void compress(Stream& input, Stream& output, CompressorContext& context)
			{
				using namespace sc::Compressor;

				output.write_unsigned_short(SC_MAGIC);

				if (context.write_assets)
				{
					output.write_int(4, Endian::Big);
				}

				// Signature check
				switch (context.signature)
				{
				case Signature::Lzma:
				case Signature::Lzham:
					output.write_int(1, Endian::Big);
					break;
				case Signature::Zstandard:
					output.write_int(3, Endian::Big);
					break;
				}

				// Hash
				{
					md5 md_ctx;
					uint8_t hash[HASH_LENGTH];

					md_ctx.update((uint8_t*)input.data(), input.length());

					md_ctx.final(hash);

					output.write_unsigned_int(HASH_LENGTH, Endian::Big);
					output.write(&hash, HASH_LENGTH);
				}

				switch (context.signature)
				{
				case Signature::Lzma:
				{
					Lzma::Props props;
					props.level = 6;
					props.pb = 2;
					props.lc = 3;
					props.lp = 0;
					props.threads = context.threads_count > 1 ? 2 : 1;
					props.dict_size = 262144;
					props.use_long_unpacked_length = false;

					if (input.length() > 1 << 28)
						props.lc = 4;

					sc::Compressor::Lzma compression(props);
					compression.compress_stream(input, output);
				}
				break;

				case Signature::Lzham:
				{
					const uint32_t dictionary_size = 18;

					output.write_unsigned_int(SCLZ_MAGIC);
					output.write_unsigned_byte(dictionary_size);
					output.write_unsigned_int(static_cast<uint32_t>(input.length() - input.position()));

					Lzham::Props props;
					props.dict_size_log2 = dictionary_size;
					props.max_helper_threads = static_cast<uint16_t>(context.threads_count > 0 && context.threads_count < lzham::MAX_HELPER_THREADS ? context.threads_count : -1);

					sc::Compressor::Lzham compression(props);
					compression.compress_stream(input, output);
				}
				break;

				case Signature::Zstandard:
				{
					Zstd::Props props;
					props.compression_level = 16;
					props.checksum_flag = false;
					props.content_size_flag = true;
					props.workers_count = context.threads_count;

					sc::Compressor::Zstd compression(props);
					compression.compress_stream(input, output);
				}
				break;
				}

				if (context.write_assets)
				{
					write_metadata(output);
				}
			}
		}
	}
}