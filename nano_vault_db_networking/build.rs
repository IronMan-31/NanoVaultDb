fn main() {
    // Look for libdb_engine.a in parent directory
    println!("cargo:rustc-link-search=native=..");
    println!("cargo:rustc-link-lib=static=db_engine");

    // Link C++ standard library
    println!("cargo:rustc-link-lib=dylib=stdc++");
}
