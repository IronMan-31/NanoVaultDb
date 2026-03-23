fn main() {
    println!(
        "cargo:rustc-link-search=native=/home/shivam/Desktop/learning/advanceCpp/distributed_Database"
    );
    println!("cargo:rustc-link-lib=static=db_engine");
    println!("cargo:rustc-link-lib=dylib=stdc++");
}
