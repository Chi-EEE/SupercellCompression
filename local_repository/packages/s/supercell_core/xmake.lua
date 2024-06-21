package("supercell_core")
    set_homepage("https://github.com/Chi-EEE/SC-Core")
    set_description("Basic and common things for general use in our projects")
    set_license("MIT")

    add_urls("https://github.com/Chi-EEE/SC-Core.git")
    add_versions("2024.06.21", "c7f62d1323317080fd763157aebccbf2bcf4ceb9")

    add_deps("cpptrace")

    on_install(function (package)
        local configs = {}
        import("package.tools.xmake").install(package, configs)
    end)
