#include "options.h"

CommandLineOptions::CommandLineOptions(int argc, char* argv[])
{
	{
		std::string operation_name = std::string(argv[1]);
		make_lowercase(operation_name);

		if (operation_name == "c" || operation_name == "compress")
		{
			operation = Operations::Compress;
		}

		if (operation_name == "d" || operation_name == "decompress")
		{
			operation = Operations::Decompress;
		}

		if (operation_name == "v" || operation_name == "convert")
		{
			operation = Operations::Convert;
		}
	}

	input_path = fs::path(argv[2]);
	output_path = fs::path(argv[3]);

	if (is_option_in(argc, argv, OptionPrefix "threads"))
	{
		threads = get_int_option(argc, argv, OptionPrefix "threads");
	}

	{
		std::string header_name = get_option(argc, argv, OptionPrefix "header");
		if (!header_name.empty())
		{
			make_lowercase(header_name);

			if (header_name == "none")
			{
				binary.header = CompressionHeader::None;
			}
			else if (header_name == "sc")
			{
				binary.header = CompressionHeader::SC;
			}
			else if (header_name == "lzham")
			{
				binary.header = CompressionHeader::LZHAM;
			}
			else if (header_name == "astc")
			{
				binary.header = CompressionHeader::ASTC;
			}
			else
			{
				std::cout << "[WARNING] An unknown type of header is specified. Instead, default is used - SC" << std::endl;
			}
		}
	}

	{
		std::string method_name = get_option(argc, argv, OptionPrefix "method");
		if (!method_name.empty())
		{
			make_lowercase(method_name);

			if (method_name == "lzma")
			{
				binary.method = CompressionMethod::LZMA;
			}
			else if (method_name == "zstd")
			{
				binary.method = CompressionMethod::ZSTD;
			}
			else if (method_name == "lzham")
			{
				binary.method = CompressionMethod::LZHAM;
			}
			else if (method_name == "astc")
			{
				binary.method = CompressionMethod::ASTC;
			}
			else
			{
				std::cout << "[WARNING] An unknown type of compression is specified. Instead, default is used - LZMA" << std::endl;
			}
		}
	}

	{
		std::string param = get_option(argc, argv, OptionPrefix "format");
		if (!param.empty())
		{
			make_lowercase(param);

			if (param == "binary")
			{
				file_format = FileFormat::Binary;
			}
			else if (param == "image")
			{
				file_format = FileFormat::Image;
			}
			else
			{
				std::cout << "[WARNING] An unknown type of file format is specified. Instead, default is used - Binary" << std::endl;
			}
		}
	}

	binary.lzma.use_long_unpacked_length = get_bool_option(argc, argv, OptionPrefix "longUnpackedLength");

	binary.sc.print_metadata = is_option_in(argc, argv, OptionPrefix "print_metadata");
}