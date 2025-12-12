use std::ffi::CString;
use std::os::raw::c_char;
use std::io::{self, Write};
use termion::event::Key;
use termion::input::TermRead;
use termion::raw::IntoRawMode;

#[link(name = "db_engine", kind = "static")]
unsafe extern "C" {
    fn initialize_database();
    fn execute_sql(sql: *const c_char) -> i32;
    fn get_last_error() -> *const c_char;
}

fn call_cpp(sql: &str) -> Result<(), String> {
    unsafe {
        let c_sql = CString::new(sql).unwrap();
        let res = execute_sql(c_sql.as_ptr());
        if res == 0 {
            Ok(())
        } else {
            let err_ptr = get_last_error();
            let err_msg = std::ffi::CStr::from_ptr(err_ptr)
                .to_string_lossy()
                .into_owned();
            Err(err_msg)
        }
    }
}

fn read_line_with_history(history: &Vec<String>, history_index: &mut usize, prompt: &str) -> String {
    let stdin = io::stdin();
    let mut stdout = io::stdout().into_raw_mode().unwrap();
    write!(stdout, "{}", prompt).unwrap();
    stdout.flush().unwrap();

    let mut input = String::new();
    let mut cursor = 0;
    let mut local_history_index = *history_index;

    for key in stdin.keys() {
        match key.unwrap() {
            Key::Char('\n') => { println!(); break; }
            Key::Char(c) => { input.insert(cursor, c); cursor += 1; write!(stdout, "{}", c).unwrap(); stdout.flush().unwrap(); }
            Key::Backspace => { 
                if cursor > 0 {
                    cursor -= 1;
                    input.remove(cursor);
                    write!(stdout, "{} {}", termion::cursor::Left(1), termion::cursor::Left(1)).unwrap();
                    stdout.flush().unwrap();
                }
            }
            Key::Up => {
                if !history.is_empty() && local_history_index > 0 {
                    local_history_index -= 1;
                    input = history[local_history_index].clone();
                    cursor = input.len();
                    write!(stdout, "\r\x1B[2K{}", prompt).unwrap();
                    write!(stdout, "{}", input).unwrap();
                    stdout.flush().unwrap();
                }
            }
            Key::Down => {
                if !history.is_empty() && local_history_index < history.len() - 1 {
                    local_history_index += 1;
                    input = history[local_history_index].clone();
                    cursor = input.len();
                    write!(stdout, "\r\x1B[2K{}", prompt).unwrap();
                    write!(stdout, "{}", input).unwrap();
                    stdout.flush().unwrap();
                } else {
                    input.clear();
                    cursor = 0;
                    write!(stdout, "\r\x1B[2K{}", prompt).unwrap();
                    stdout.flush().unwrap();
                }
            }
            _ => {}
        }
    }

    *history_index = local_history_index;
    input
}

fn main() {
    unsafe { initialize_database(); }
    println!("nanoVaultDb initialized.");

    let mut history: Vec<String> = Vec::new();
    let mut history_index = 0;

    loop {
        let mut sql = read_line_with_history(&history, &mut history_index, "nanoVaultDb> ");

        if sql.trim() == "exit" || sql.trim() == "quit" {
            break;
        }

        if !sql.trim().is_empty() {
            history.push(sql.clone());
            history_index = history.len();
        }

        while !sql.contains(';') {
            let more = read_line_with_history(&history, &mut history_index, " ...> ");
            if !more.trim().is_empty() {
                history.push(more.clone());
                history_index = history.len();
            }
            sql.push('\n');
            sql.push_str(&more);
        }

        let result = unsafe { call_cpp(&sql) };
        if let Err(e) = result {
            eprintln!("Error: {}", e);
        }
    }

    println!("Exiting nanoVaultDb shell.");
}
