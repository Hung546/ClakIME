use clak_engine::{compare_and_split, ClakContext, ClakCore, Method};
use proptest::prelude::*;

fn apply_diff(original: &str, deleted: &str, added: &str) -> String {
    let orig_chars: Vec<char> = original.chars().collect();
    let del_count = deleted.chars().count();
    let keep_count = orig_chars.len() - del_count;
    let mut result: String = orig_chars[..keep_count].iter().collect();
    result.push_str(added);
    result
}

proptest! {
    #[test]
    fn prop_compare_and_split_always_reconstructs(a in "\\PC*", b in "\\PC*") {
        // verify diff reconstruction invariant for any arbitrary unicode strings
        let (del, add) = compare_and_split(&a, &b);
        let reconstructed = apply_diff(&a, &del, &add);
        prop_assert_eq!(reconstructed, b);
    }

    #[test]
    fn prop_compare_and_split_is_minimal(a in "[a-zA-Z0-9_]{0,30}", b in "[a-zA-Z0-9_]{0,30}") {
        // verify delete and add counts strictly equal non-shared suffix lengths
        let (del, add) = compare_and_split(&a, &b);
        let a_chars: Vec<char> = a.chars().collect();
        let b_chars: Vec<char> = b.chars().collect();

        let common = a_chars.iter().zip(b_chars.iter()).take_while(|(x, y)| x == y).count();
        prop_assert_eq!(del.chars().count(), a_chars.len() - common);
        prop_assert_eq!(add.chars().count(), b_chars.len() - common);
    }

    #[test]
    fn prop_telex_transform_never_panics(input in "[a-zA-Z0-9\\[\\]\\\\;/'\"., -]{0,50}") {
        // verify telex transformation never crashes on arbitrary input combinations
        let core = ClakCore::new(Method::Telex);
        let result = core.transform(&input);
        prop_assert!(std::str::from_utf8(result.as_bytes()).is_ok());
    }

    #[test]
    fn prop_ime_process_key_fuzz(input in "[a-zA-Z0-9 ]{0,30}") {
        // verify state machine never panics during arbitrary typing sequences
        let mut ctx = ClakContext::new(Method::Telex);
        for ch in input.chars() {
            let s = ch.to_string();
            let sym = ch as u32;
            let action = ctx.process_key(sym, &s, false, None, 0, 0);
            if !action.commit_str.is_null() {
                let s = unsafe { std::ffi::CStr::from_ptr(action.commit_str) };
                prop_assert!(s.to_str().is_ok());
            }
        }
    }
}
