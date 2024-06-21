#pragma once
#include "io/stream.h"

namespace sc
{
	namespace Compressor
	{
		class CompressionInterface
		{
		public:
			~CompressionInterface() = default;

		public:
			virtual void compress_stream(Stream& input, Stream& output) = 0;
		};
	}
}