#include "SupercellCompression/Astc.h"
#include "memory/alloc.h"

#include "SupercellCompression/exception/Astc/CompressException.h"
#include "SupercellCompression/exception/Astc/CompressInitException.h"
#include "SupercellCompression/exception/Astc/CompressConfigInitException.h"
#include "SupercellCompression/exception/ImageInterface/ImageAlreadyCompressedException.h"
#include <astcenc_internal_entry.h>

namespace sc
{
	namespace Compressor
	{
		void Astc::write(Image& image, AstcCompressProps props, Stream& output)
		{
			output.write(AstcFileIdentifier, sizeof(AstcFileIdentifier));

			// x y z blocks
			output.write_unsigned_byte(props.blocks_x);
			output.write_unsigned_byte(props.blocks_y);
			output.write_unsigned_byte(1);

			uint32_t width = image.width();
			uint32_t height = image.height();
			uint32_t z_dimension = 1;

			// '24-bit' integers for width / height
			output.write(&width, 3);
			output.write(&height, 3);
			output.write(&z_dimension, 3);

			Astc context(props);
			context.compress_image(image, output);
		}

		Astc::Astc(AstcCompressProps& props)
		{
			m_config = new astcenc_config();
			astcenc_error status;
			status = astcenc_config_init(
				props.profile,
				props.blocks_x, props.blocks_y, 1,
				float(props.quality), 0, m_config
			);

			status = astcenc_context_alloc(m_config, props.threads_count, &m_context);
		};

		Astc::~Astc()
		{
			astcenc_context_free(m_context);
			delete m_config;
		}

		void Astc::compress_image(Image& image, Stream& output)
		{
			if (image.is_compressed())
			{
				throw ImageAlreadyCompressedException();
			}

			astcenc_swizzle swizzle = get_astc_swizzle(image.base_type());

			uint8_t* image_data = image.data();

			astcenc_image encoder_image{};
			encoder_image.dim_x = image.width();
			encoder_image.dim_y = image.height();
			encoder_image.dim_z = 1;
			encoder_image.data = reinterpret_cast<void**>(&image_data);
			encoder_image.data_type = ASTCENC_TYPE_U8;

			const unsigned int& blocks_x = m_context->context.config.block_x;
			const unsigned int& blocks_y = m_context->context.config.block_y;

			unsigned int xblocks = (encoder_image.dim_x + blocks_x - 1) / blocks_x;
			unsigned int yblocks = (encoder_image.dim_y + blocks_y - 1) / blocks_y;

			size_t data_size = xblocks * yblocks * 16;
			uint8_t* data = memalloc(data_size);

			astcenc_error status = astcenc_compress_image(m_context, &encoder_image, &swizzle, data, data_size, 0);

			if (status != ASTCENC_SUCCESS)
			{
				free(data);
				throw AstcCompressException();
			}

			output.write(data, data_size);
			free(data);
		};
	}
}