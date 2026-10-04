use clak_engine::compare_and_split;

fn apply_diff(original: &str, deleted: &str, added: &str) -> String {
    let orig_chars: Vec<char> = original.chars().collect();
    let del_count = deleted.chars().count();
    assert!(
        del_count <= orig_chars.len(),
        "delete count exceeds original length"
    );
    let keep_count = orig_chars.len() - del_count;
    let mut result: String = orig_chars[..keep_count].iter().collect();
    result.push_str(added);
    result
}

#[test]
fn test_identical_strings_have_zero_diff() {
    let cases = ["", "a", "tiếng", "Việt Nam", "12345"];
    for s in cases {
        let (del, add) = compare_and_split(s, s);
        assert_eq!(del, "");
        assert_eq!(add, "");
    }
}

#[test]
fn test_empty_transitions() {
    let (del, add) = compare_and_split("", "hello");
    assert_eq!(del, "");
    assert_eq!(add, "hello");

    let (del, add) = compare_and_split("hello", "");
    assert_eq!(del, "hello");
    assert_eq!(add, "");
}

#[test]
fn test_minimal_vietnamese_diffs() {
    // verify minimal character deletion on standard telex accent transitions
    let cases = [
        ("d", "đ", "d", "đ"),
        ("a", "á", "a", "á"),
        ("o", "ơ", "o", "ơ"),
        ("u", "ư", "u", "ư"),
        ("e", "ê", "e", "ê"),
        ("tieng", "tiếng", "eng", "ếng"),
        ("nguoi", "người", "uoi", "ười"),
        ("hoang", "hoàng", "ang", "àng"),
        ("viet", "việt", "et", "ệt"),
        ("chuns", "chúng", "uns", "úng"),
    ];

    for (before, after, exp_del, exp_add) in cases {
        let (del, add) = compare_and_split(before, after);
        assert_eq!(del, exp_del, "failed del for {} -> {}", before, after);
        assert_eq!(add, exp_add, "failed add for {} -> {}", before, after);
        let reconstructed = apply_diff(before, &del, &add);
        assert_eq!(reconstructed, after);
    }
}

#[test]
fn test_diff_minimality_property() {
    // for any common prefix, deleted length must equal original minus prefix length
    let pairs = [
        ("abcde", "abcfg"),
        ("pre_hello", "pre_world"),
        ("vietnam", "vietnamese"),
        ("longprefix_a", "longprefix_b"),
    ];

    for (a, b) in pairs {
        let (del, add) = compare_and_split(a, b);
        let reconstructed = apply_diff(a, &del, &add);
        assert_eq!(reconstructed, b);

        let a_chars: Vec<char> = a.chars().collect();
        let b_chars: Vec<char> = b.chars().collect();
        let common = a_chars
            .iter()
            .zip(b_chars.iter())
            .take_while(|(x, y)| x == y)
            .count();
        assert_eq!(del.chars().count(), a_chars.len() - common);
        assert_eq!(add.chars().count(), b_chars.len() - common);
    }
}
