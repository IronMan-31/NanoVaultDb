use std::ffi::{CString, CStr};
use std::os::raw::c_char;
use std::io::{self, Write};
use std::env;
use std::path::PathBuf;

use termion::event::Key;
use termion::input::TermRead;
use termion::raw::IntoRawMode;

#[link(name = "db_engine", kind = "static")]
unsafe extern "C" {
    fn initialize_database(base_path: *const c_char);
    fn execute_sql(sql: *const c_char) -> *const c_char;
    fn free_string(s: *const c_char);
    fn get_last_error() -> *const c_char;
}

fn call_cpp(sql: &str) -> Result<String, String> {
    unsafe {
        let c_sql = CString::new(sql).unwrap();
        let result_ptr = execute_sql(c_sql.as_ptr());

        if result_ptr.is_null() {
            let err_ptr = get_last_error();
            if err_ptr.is_null() {
                Err("Unknown database error".to_string())
            } else {
                Err(CStr::from_ptr(err_ptr).to_string_lossy().into_owned())
            }
        } else {
            let result = CStr::from_ptr(result_ptr)
                .to_string_lossy()
                .into_owned();
            free_string(result_ptr);
            Ok(result)
        }
    }
}

fn redraw_line(
    stdout: &mut impl Write,
    prompt: &str,
    input: &str,
    cursor: usize,
) {
    write!(
        stdout,
        "\r{}{}{}",
        termion::clear::CurrentLine,
        prompt,
        input
    )
    .unwrap();

    let pos = prompt.len() + cursor;
    write!(stdout, "\r{}", termion::cursor::Right(pos as u16)).unwrap();
    stdout.flush().unwrap();
}

fn read_line_with_history(
    history: &[String],
    history_index: &mut usize,
    prompt: &str,
) -> String {
    let stdin = io::stdin();
    let mut stdout = io::stdout().into_raw_mode().unwrap();

    let mut input = String::new();
    let mut cursor = 0;
    let mut local_history_index = *history_index;

    redraw_line(&mut stdout, prompt, &input, cursor);

    for key in stdin.keys() {
        match key.unwrap() {
            Key::Char('\n') => {
                write!(stdout, "\r\n").unwrap();
                break;
            }

            Key::Char(c) => {
                input.insert(cursor, c);
                cursor += 1;
                redraw_line(&mut stdout, prompt, &input, cursor);
            }

            Key::Backspace => {
                if cursor > 0 {
                    cursor -= 1;
                    input.remove(cursor);
                    redraw_line(&mut stdout, prompt, &input, cursor);
                }
            }

            Key::Up => {
                if !history.is_empty() && local_history_index > 0 {
                    local_history_index -= 1;
                    input = history[local_history_index].clone();
                    cursor = input.len();
                    redraw_line(&mut stdout, prompt, &input, cursor);
                }
            }

            Key::Down => {
                if local_history_index + 1 < history.len() {
                    local_history_index += 1;
                    input = history[local_history_index].clone();
                } else {
                    local_history_index = history.len();
                    input.clear();
                }
                cursor = input.len();
                redraw_line(&mut stdout, prompt, &input, cursor);
            }

            _ => {}
        }
    }

    *history_index = local_history_index;
    input
}

fn main() {
    let exe_path = env::current_exe().expect("Failed to get exe path");

    let base_dir: PathBuf = exe_path
        .parent().unwrap()
        .parent().unwrap()
        .parent().unwrap()
        .parent().unwrap()
        .to_path_buf();

    let base_c = CString::new(base_dir.to_str().unwrap()).unwrap();

    unsafe {
        initialize_database(base_c.as_ptr());
    }

    println!("nanoVaultDb initialized.");

    let mut history: Vec<String> = Vec::new();
    let mut history_index: usize = 0;

    loop {
        let mut sql =
            read_line_with_history(&history, &mut history_index, "nanoVaultDb> ");

        if matches!(sql.trim(), "exit" | "quit") {
            break;
        }

        if !sql.trim().is_empty() {
            history.push(sql.clone());
            history_index = history.len();
        }

        while !sql.contains(';') {
            let more =
                read_line_with_history(&history, &mut history_index, " ...> ");

            if !more.trim().is_empty() {
                history.push(more.clone());
                history_index = history.len();
            }

            sql.push('\n');
            sql.push_str(&more);
        }

        match call_cpp(&sql) {
            Ok(output) => println!("{}", output),
            Err(e) => eprintln!("Error: {}", e),
        }
    }

    println!("Exiting nanoVaultDb shell.");
}
