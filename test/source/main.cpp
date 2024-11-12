#include "main.h"
#include "backend/interface.h"

#include "backend/sc.hpp"

#include <core/memory/ref.h>

using namespace sc::compression;

namespace
{
	Mode string_to_mode(std::string mode)
	{
		if (mode == "compress")
		{
			return Mode::Compress;
		}
		else if (mode == "decompress")
		{
			return Mode::Decompress;
		}

		throw sc::Exception("Wrong mode!");
	}

	Container string_to_container(std::string container)
	{
		if (container == "none")
		{
			return Container::None;
		}
		else if (container == "sc")
		{
			return Container::SC;
		}

		throw sc::Exception("Wrong container!");
	}

	Method string_to_method(std::string method)
	{
		if (method == "lzma")
		{
			return Method::LZMA;
		}
		else if (method == "lzham")
		{
			return Method::LZHAM;
		}
		else if (method == "zstd")
		{
			return Method::ZSTD;
		}

		throw sc::Exception("Wrong method!");
	}
}

namespace
{
	fs::path get_unique_name(fs::path basename, Mode mode, Method method, Container container)
	{
		//if (container == Container::SC)
		//{
		std::string prefix = "";
		switch (mode)
		{
		case Mode::Decompress:
			prefix = "d-";
			break;

		case Mode::Compress:
			prefix = "c-";
			break;

		default:
			break;
		}

		return fs::path(prefix).concat(basename.u16string());
		//}
	}

	sc::Ref<CommandLineInterface> InterfaceFactrory(Method method, Container container)
	{
		if (container == Container::SC)
		{
			return sc::CreateRef<ScCommandLineInterface>();
		}

		throw sc::Exception("Failed to create interface!");
	}
}

void supercell_compression_cli()
{
	Mode mode = string_to_mode("decompress");
	Method method = string_to_method("zstd");
	Container container = string_to_container("sc");

	auto processor = InterfaceFactrory(method, container);
	fs::path input_path = "C:/Users/admin/Documents/Github/SupercellCompression/WizardBG_dl.sc";

	fs::path basename = get_unique_name(input_path.filename(), mode, method, container);
	fs::path basedir = input_path.parent_path();
	fs::path output_path = basedir / basename;

	OperationContext context;

	sc::InputFileStream input_file(input_path);
	sc::OutputFileStream output_file(output_path);

	flash::Decompressor::decompress(input_file, output_file);
}

int main(int argc, const char** argv)
{
	fs::path executable = argv[0];
	fs::path executable_name = executable.stem();

	try
	{
		supercell_compression_cli();
	}
	catch (sc::Exception& exception)
	{
		std::cout << "Unknown unexpected exception: " << exception.what() << std::endl;
	}

	return 0;
}