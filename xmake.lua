add_rules("mode.debug", "mode.release")

add_repositories("local-repo local_repository")

set_languages("cxx17")

add_requires("lzham_codec", "lzma", "zstd")
add_requires("astc-encoder", {configs = {sse41 = true, native = true, cli = false}})
add_requires("supercell_core")

target("supercell_compression")
    set_kind("$(kind)")

    add_packages("lzham_codec", "lzma", "zstd", "astc-encoder")
    add_packages("supercell_core")

    add_files("source/**.cpp")
    add_headerfiles("include/(**.h)")
    add_includedirs("include")
