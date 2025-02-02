#include "SupercellCompression/KhronosTexture.h"

namespace sc
{
	const uint8_t KhronosTexture::FileIdentifier[12] = {
		0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, '\r', '\n', 0x1A, '\n'
	};

#pragma region Constructors
	KhronosTexture::KhronosTexture(Stream& buffer)
	{
		uint32_t levels_count = read_header(buffer);

		m_levels.resize(levels_count);
		for (uint32_t level_index = 0; levels_count > level_index; level_index++)
		{
			uint32_t level_length = buffer.read_unsigned_int();

			m_levels[level_index] = new BufferStream(level_length);
			buffer.read(m_levels[level_index]->data(), level_length);
		}
	}

	KhronosTexture::KhronosTexture(glInternalFormat format, uint8_t* buffer, size_t buffer_size) : m_internal_format(format)
	{
		m_format = get_type(format);

		if (!is_compressed())
		{
			m_type = glType::GL_UNSIGNED_BYTE;
		}
		else
		{
			m_type = glType::COMPRESSED;
		}

		BufferStream* stream = new BufferStream(buffer_size);
		sc::memcopy(buffer, stream->data(), buffer_size);

		m_levels.push_back(stream);
	}

	KhronosTexture::KhronosTexture(RawImage& image, glInternalFormat format) : m_internal_format(format)
	{
		m_format = get_type(format);
		m_width = image.width();
		m_height = image.height();

		if (is_compressed())
		{
			m_type = glType::COMPRESSED;
		}
		else
		{
			m_type = glType::GL_UNSIGNED_BYTE;
		}

		MemoryStream image_data(image.data(), image.data_length());

		set_level_data(
			image_data,
			image.depth(),
			0
		);
	}

	KhronosTexture::~KhronosTexture()
	{
		for (BufferStream* level : m_levels)
		{
			if (level != nullptr) delete level;
		}
	}
#pragma endregion

#pragma region Functions
	void KhronosTexture::write(Stream& buffer)
	{
		bool is_compressed = m_type == glType::COMPRESSED;

		buffer.write(&KhronosTexture::FileIdentifier, sizeof(KhronosTexture::FileIdentifier));

		// endianess
		buffer.write_unsigned_int(0x04030201);

		// glType
		buffer.write_unsigned_int((uint32_t)m_type);

		// glTypeSize
		{
			uint32_t glTypeSize = 0;
			if (is_compressed)
			{
				glTypeSize = 1;
			}

			buffer.write_unsigned_int(glTypeSize);
		}

		// glFormat
		buffer.write_unsigned_int(is_compressed ? 0 : (uint32_t)m_format);

		// glInternalFormat
		buffer.write_unsigned_int((uint32_t)m_internal_format);

		// glBaseInternalType
		buffer.write_unsigned_int((uint32_t)m_format);

		// Width / height
		buffer.write_unsigned_int(m_width);
		buffer.write_unsigned_int(m_height);

		// pixelDepth
		buffer.write_unsigned_int(0);

		// numberOfArrayElements
		buffer.write_unsigned_int(0);

		// numberOfFaces
		buffer.write_unsigned_int(1);

		// numberOfMipmapLevels
		buffer.write_unsigned_int(static_cast<uint32_t>(m_levels.size()));

		// bytesOfKeyValueData
		buffer.write_unsigned_int(0);

		for (BufferStream* level : m_levels)
		{
			if (level == nullptr) continue;

			uint32_t image_length = static_cast<uint32_t>(level->length());
			buffer.write_unsigned_int(image_length);
			buffer.write(level->data(), image_length);
		}
	}

	void KhronosTexture::decompress_data(Stream& output, uint32_t level_index)
	{
		if (level_index >= m_levels.size()) level_index = static_cast<uint32_t>(m_levels.size()) - 1;
		BufferStream* buffer = m_levels[level_index];
		if (buffer == nullptr) return;

		uint16_t level_width = m_width / (uint16_t)pow(2, level_index);
		uint16_t level_height = m_height / (uint16_t)pow(2, level_index);

		buffer->seek(0);
		switch (compression_type())
		{
		case KhronosTextureCompression::ASTC:
			decompress_astc(*buffer, output, level_width, level_height);
			break;

		default:
			output.write(m_levels[level_index]->data(), m_levels[level_index]->length());
			break;
		}
	}

	void KhronosTexture::set_level_data(Stream& stream, Image::PixelDepth source_depth, uint32_t level_index)
	{
		// First, check if level index is ok
		// If index out of bound - create new buffer
		if (level_index >= m_levels.size())
		{
			level_index = static_cast<uint32_t>(m_levels.size());
			m_levels.resize(m_levels.size() + 1);
		};

		// Free level buffer before some changes if it exist
		if (m_levels[level_index] != nullptr)
		{
			delete m_levels[level_index];
		}

		// Level final data buffer
		BufferStream* buffer = new BufferStream();

		uint8_t* image_buffer = nullptr;
		size_t image_buffer_size = 0;

		// Second, we need to convert data base type to current texture type
		Image::PixelDepth destination_depth = depth();

		if (source_depth != destination_depth)
		{
			image_buffer_size = Image::calculate_image_length(m_width, m_height, destination_depth);
			image_buffer = memalloc(image_buffer_size);
			Image::remap(
				(uint8_t*)stream.data(), image_buffer,
				m_width, m_height,
				source_depth, destination_depth
			);
		}

		MemoryStream input_image(
			image_buffer ? image_buffer : (uint8_t*)stream.data(),
			image_buffer ? image_buffer_size : stream.length()
		);

		switch (compression_type())
		{
		case KhronosTextureCompression::ASTC:
			compress_astc(input_image, *buffer);
			break;

		default:
			buffer->resize(input_image.length());
			sc::memcopy(
				image_buffer ? image_buffer : (uint8_t*)stream.data(),
				buffer->data(),
				input_image.length()
			);
			break;
		}

		m_levels[level_index] = buffer;
	}

	void KhronosTexture::reset_level_data(uint32_t level_index)
	{
		if (level_index >= m_levels.size()) level_index = static_cast<uint32_t>(m_levels.size()) - 1;

		m_levels.erase(m_levels.begin(), m_levels.begin() + level_index);
	}
#pragma endregion

#pragma region Static Functions
	Image::PixelDepth KhronosTexture::format_depth(glInternalFormat format)
	{
		switch (format)
		{
		case sc::KhronosTexture::glInternalFormat::GL_RGBA8:
			return PixelDepth::RGBA8;
		case sc::KhronosTexture::glInternalFormat::GL_RGB8:
			return PixelDepth::RGB8;
		case sc::KhronosTexture::glInternalFormat::GL_LUMINANCE:
			return PixelDepth::LUMINANCE8;
		case sc::KhronosTexture::glInternalFormat::GL_LUMINANCE_ALPHA:
			return PixelDepth::LUMINANCE8_ALPHA8;

		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_4x4:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_5x5:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_6x6:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_8x8:
			return PixelDepth::RGBA8;

		default:
			assert(0 && "Unknown glInternalFormat");
			return PixelDepth::RGBA8;
		}
	}

	Image::BasePixelType KhronosTexture::format_type(glFormat format)
	{
		switch (format)
		{
		case sc::KhronosTexture::glFormat::GL_R:
			return BasePixelType::L;

		case sc::KhronosTexture::glFormat::GL_RG:
			return BasePixelType::LA;

		case sc::KhronosTexture::glFormat::GL_SRGB:
		case sc::KhronosTexture::glFormat::GL_RGB:
			return BasePixelType::RGB;

		case sc::KhronosTexture::glFormat::GL_SRGB_ALPHA:
		case sc::KhronosTexture::glFormat::GL_RGBA:
			return BasePixelType::RGBA;

		default:
			assert(0 && "Unknown glFormat");
			return BasePixelType::RGBA;
		}
	}

	Image::ColorSpace KhronosTexture::format_colorspace(glFormat format)
	{
		switch (format)
		{
		case sc::KhronosTexture::glFormat::GL_RGBA:
		case sc::KhronosTexture::glFormat::GL_RGB:
		case sc::KhronosTexture::glFormat::GL_RG:
		case sc::KhronosTexture::glFormat::GL_R:
			return ColorSpace::Linear;

		case sc::KhronosTexture::glFormat::GL_SRGB:
		case sc::KhronosTexture::glFormat::GL_SRGB_ALPHA:
			return ColorSpace::sRGB;

		default:
			assert(0 && "Unknown glFormat");
			return ColorSpace::Linear;
		}
	}

	bool KhronosTexture::format_compression(glInternalFormat format)
	{
		switch (format)
		{
		case sc::KhronosTexture::glInternalFormat::GL_RGBA8:
		case sc::KhronosTexture::glInternalFormat::GL_RGB8:
		case sc::KhronosTexture::glInternalFormat::GL_LUMINANCE:
		case sc::KhronosTexture::glInternalFormat::GL_LUMINANCE_ALPHA:
			return false;

		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_4x4:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_5x5:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_6x6:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_8x8:
			return true;

		default:
			assert(0 && "Unknown glInternalFormat");
			return false;
		}
	}

	KhronosTextureCompression KhronosTexture::format_compression_type(glInternalFormat format)
	{
		switch (format)
		{
		case sc::KhronosTexture::glInternalFormat::GL_RGBA8:
		case sc::KhronosTexture::glInternalFormat::GL_RGB8:
		case sc::KhronosTexture::glInternalFormat::GL_LUMINANCE:
		case sc::KhronosTexture::glInternalFormat::GL_LUMINANCE_ALPHA:
			return KhronosTextureCompression::None;

		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_4x4:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_5x5:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_6x6:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_8x8:
			return KhronosTextureCompression::ASTC;
		default:
			assert(0 && "Unknown glInternalFormat");
			return KhronosTextureCompression::None;
		}
	}
#pragma endregion

#pragma region Getters/Setters
	Image::BasePixelType KhronosTexture::base_type() const
	{
		return KhronosTexture::format_type(m_format);
	}

	Image::ColorSpace KhronosTexture::colorspace() const
	{
		return KhronosTexture::format_colorspace(m_format);
	}

	Image::PixelDepth KhronosTexture::depth() const
	{
		return KhronosTexture::format_depth(m_internal_format);
	};

	size_t KhronosTexture::data_length() const
	{
		return data_length(0);
	}

	size_t KhronosTexture::data_length(uint32_t level_index) const
	{
		if (level_index >= m_levels.size()) level_index = static_cast<uint32_t>(m_levels.size()) - 1;

		return m_levels[level_index]->length();
	}

	uint8_t* KhronosTexture::data() const
	{
		auto buffer = data(0);

		if (buffer != nullptr) return (uint8_t*)buffer->data();

		return nullptr;
	}

	const BufferStream* KhronosTexture::data(uint32_t level_index) const
	{
		if (level_index >= m_levels.size()) level_index = static_cast<uint32_t>(m_levels.size()) - 1;

		return m_levels[level_index];
	}

	bool KhronosTexture::is_compressed() const
	{
		return KhronosTexture::format_compression(m_internal_format);
	}

	KhronosTextureCompression KhronosTexture::compression_type()
	{
		return KhronosTexture::format_compression_type(m_internal_format);
	}

	size_t KhronosTexture::decompressed_data_length()
	{
		return decompressed_data_length(0);
	}

	size_t KhronosTexture::decompressed_data_length(uint32_t level_index)
	{
		return Image::calculate_image_length(
			m_width / (uint16_t)(pow(2, level_index)),
			m_height / (uint16_t)(pow(2, level_index)),
			depth()
		);
	}

	void KhronosTexture::decompress_data(Stream& output)
	{
		return decompress_data(output, 0);
	}

	uint32_t KhronosTexture::level_count() const
	{
		return static_cast<uint32_t>(m_levels.size());
	}
#pragma endregion

#pragma region Private Functions
	uint32_t KhronosTexture::read_header(Stream& buffer)
	{
		for (uint8_t i = 0; sizeof(KhronosTexture::FileIdentifier) > i; i++)
		{
			if (buffer.read_byte() != KhronosTexture::FileIdentifier[i])
			{
				// TODO: exception
			}
		}

		// endianess
		buffer.read_unsigned_int();

		m_type = (glType)buffer.read_unsigned_int();

		// glTypeSize
		buffer.read_unsigned_int();

		glFormat format = (glFormat)buffer.read_unsigned_int();

		m_internal_format = (glInternalFormat)buffer.read_unsigned_int();
		glFormat internalBasetype = (glFormat)buffer.read_unsigned_int();

		if (format == glFormat::UNKNOWN)
		{
			m_format = internalBasetype;
		}
		else
		{
			m_format = format;
		}

		m_width = static_cast<uint16_t>(buffer.read_unsigned_int());
		m_height = static_cast<uint16_t>(buffer.read_unsigned_int());

		// pixelDepth | must be 0											//
		assert(buffer.read_unsigned_int() == 0 && "Pixel Depth != 0");		//
																			//
		// numberOfArrayElements | must be 0								// Some hardcoded/unsuported values
		assert(buffer.read_unsigned_int() == 0 && "Array elements != 0");	//
																			//
		// numberOfFaces | must be 1										//
		assert(buffer.read_unsigned_int() == 1 && "Faces number != 1");		//

		uint32_t levels_count = buffer.read_unsigned_int();

		uint32_t key_value_data_length = buffer.read_unsigned_int();

		// skip
		buffer.seek(key_value_data_length, Seek::Add);

		return levels_count;
	}

	KhronosTexture::glFormat KhronosTexture::get_type(glInternalFormat format)
	{
		switch (format)
		{
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_8x8:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_6x6:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_5x5:
		case sc::KhronosTexture::glInternalFormat::GL_COMPRESSED_RGBA_ASTC_4x4:
		case sc::KhronosTexture::glInternalFormat::GL_RGBA8:
			return glFormat::GL_RGBA;

		case sc::KhronosTexture::glInternalFormat::GL_RGB8:
			return glFormat::GL_RGB;
		case sc::KhronosTexture::glInternalFormat::GL_LUMINANCE:
			return glFormat::GL_RG;
		case sc::KhronosTexture::glInternalFormat::GL_LUMINANCE_ALPHA:
			return glFormat::GL_R;

		default:
			assert(0 && "Unknown glInternalFormat");
			return glFormat::GL_RGBA;
		}
	};
#pragma endregion

#pragma region
	void KhronosTexture::get_astc_blocks(glInternalFormat format, uint8_t& x, uint8_t& y, uint8_t& z)
	{
		switch (format)
		{
		case glInternalFormat::GL_COMPRESSED_RGBA_ASTC_4x4:
			x = 4; y = 4; z = 0;
			break;
		case glInternalFormat::GL_COMPRESSED_RGBA_ASTC_5x5:
			x = 5; y = 5; z = 0;
			break;
		case glInternalFormat::GL_COMPRESSED_RGBA_ASTC_6x6:
			x = 6; y = 6; z = 0;
			break;
		case glInternalFormat::GL_COMPRESSED_RGBA_ASTC_8x8:
			x = 8; y = 8; z = 0;
			break;
		default:
			x = 0; y = 0; z = 0;
			break;
		}
	}

	void KhronosTexture::decompress_astc(Stream& input, Stream& output, uint16_t width, uint16_t height)
	{
		using namespace Decompressor;

		uint8_t blocks_x;
		uint8_t blocks_y;
		uint8_t blocks_z;
		get_astc_blocks(m_internal_format, blocks_x, blocks_y, blocks_z);

		Astc::Props props;
		props.blocks_x = blocks_x;
		props.blocks_y = blocks_y;
		props.profile = colorspace() == ColorSpace::Linear ? astc::Profile::PRF_LDR : astc::Profile::PRF_LDR_SRGB;

		Decompressor::Astc context(props);
		context.decompress_image(
			width, height, Image::BasePixelType::RGBA,
			input, output
		);
	}

	void KhronosTexture::compress_astc(Stream& input, Stream& output)
	{
		using namespace Compressor;

		uint8_t blocks_x;
		uint8_t blocks_y;
		uint8_t blocks_z;
		get_astc_blocks(m_internal_format, blocks_x, blocks_y, blocks_z);

		Astc::Props props;
		props.blocks_x = blocks_x;
		props.blocks_y = blocks_y;

		Compressor::Astc context(props);
		context.compress_image(m_width, m_height, Image::BasePixelType::RGBA, input, output);
	}
#pragma endregion ASTC Compression
}