namespace sc
{
	namespace compression
	{
		class ScCommandLineInterface : public CommandLineInterface
		{
		public:
			static void initialize(sc::ArgumentParser& parser)
			{
				parser.add_argument("-scm", "--sc-print-metadata")
					.default_value(false)
					.help("Print metadata from .sc to console when selected decompress mode");
			}

			virtual void parse(sc::ArgumentParser& parser) { }

			virtual void decompress(sc::Stream& input, sc::Stream& output) 
			{
				flash::Decompressor::decompress(input, output);
			}

			virtual void compress(sc::Stream& input, sc::Stream& output, Method method)
			{
				flash::Signature signature;

				switch (method)
				{
				case Method::LZMA:
					signature = flash::Signature::Lzma;
					break;
				case Method::LZHAM:
					signature = flash::Signature::Lzham;
					break;
				case Method::ZSTD:
					signature = flash::Signature::Zstandard;
					break;
				default:
					throw sc::Exception("Unsupported compression method!");
				}

				sc::compression::flash::Compressor::Context context;
				context.signature = signature;

				sc::compression::flash::Compressor compressor;
				compressor.compress(input, output, context);
			}
		};
	}
}