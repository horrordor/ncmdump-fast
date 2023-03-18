add_rules("mode.release")
add_requires("openmp", "cryptopp", "taglib", "cjson", "zlib")


target("fastncmdump")
    set_languages("c11", "c++17")
    add_packages("openmp", "cryptopp", "taglib", "cjson", "zlib")
    set_kind("binary")
    add_files("src/*.cpp")
    set_optimize("fastest")
    add_cflags("-static", "-static-libgcc", "-static-libstdc++", "-fopenmp", "-fopenmp-simd", "-march=native" )