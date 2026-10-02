use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use crate::{ClakCore, Method};

#[repr(C)]
pub struct ImeAction {
    pub action_type: i32,
    pub delete_count: usize,
    pub commit_str: *const c_char,
}

pub const ACTION_FORWARD: i32 = 0;
pub const ACTION_COMMIT: i32 = 1;
pub const ACTION_REPLACE_SURROUNDING: i32 = 2;
pub const ACTION_ADDRESS_BAR_FIX: i32 = 3;
pub const ACTION_REPLACE: i32 = 4;
pub const ACTION_UINPUT_REPLACE: i32 = 4;

pub struct ClakContext {
    engine: ClakCore,
    commit_buf: CString,
    raw_buffer: String,
    last_composed: String,
    stale_surr: Option<(String, usize)>,
    just_deleted: bool,
    typed_over_selection: bool,
}

impl ClakContext {
    pub fn new(method: Method) -> Self {
        let mut engine = ClakCore::new(method);
        engine.set_auto_restore(true);
        engine.set_dict(false);
        Self {
            engine,
            commit_buf: CString::default(),
            raw_buffer: String::new(),
            last_composed: String::new(),
            stale_surr: None,
            just_deleted: false,
            typed_over_selection: false,
        }
    }

    pub fn reset(&mut self) {
        self.raw_buffer.clear();
        self.last_composed.clear();
        self.typed_over_selection = false;
    }

    pub fn process_key(
        &mut self,
        key_sym: u32,
        key_str: &str,
        has_ctrl_alt: bool,
        surrounding_text: Option<&str>,
        cursor: usize,
        anchor: usize,
    ) -> ImeAction {
        if has_ctrl_alt {
            self.reset();
            self.just_deleted = false;
            self.stale_surr = None;
            return self.forward();
        }

        if key_sym == 0xff08 {
            self.just_deleted = true;
            if let Some(text) = surrounding_text {
                self.stale_surr = Some((text.to_string(), cursor));
                let chars: Vec<char> = text.chars().collect();
                let cur = std::cmp::min(chars.len(), cursor);
                let anch = std::cmp::min(chars.len(), anchor);
                if text.is_empty() || std::cmp::min(cur, anch) == 0 {
                    self.reset();
                    return self.forward();
                }
            } else {
                self.stale_surr = None;
            }

            if cursor != anchor {
                return self.forward();
            }

            // reset state when deleting single char so no partial buffer is left
            if self.last_composed.chars().count() <= 1 {
                self.reset();
            } else {
                let mut chars: Vec<char> = self.last_composed.chars().collect();
                chars.pop();
                let remaining: String = chars.into_iter().collect();
                self.raw_buffer = decompose_to_telex(&remaining);
                self.last_composed = remaining;
            }
            return self.forward();
        }

        if key_sym == 0xff1b || key_sym == 0xff0d || key_sym == 0xff09 || (key_sym >= 0xff50 && key_sym <= 0xff57) {
            self.reset();
            self.just_deleted = false;
            self.stale_surr = None;
            return self.forward();
        }

        if key_str.is_empty() {
            return self.forward();
        }

        let key_ch = key_str.chars().next().unwrap_or('\0');
        if is_word_break(key_ch as u32) {
            self.reset();
            self.just_deleted = false;
            self.stale_surr = None;
            return self.forward();
        }

        let just_del = self.just_deleted;
        self.just_deleted = false;

        let mut has_autocomplete = false;
        if let Some(text) = surrounding_text {
            let is_stale = match &self.stale_surr {
                Some((st_text, st_cur)) => st_text == text && *st_cur == cursor,
                None => false,
            };
            self.stale_surr = None;

            let chars: Vec<char> = text.chars().collect();
            let cur = std::cmp::min(chars.len(), cursor);
            let anch = std::cmp::min(chars.len(), anchor);
            let sel_start = std::cmp::min(cur, anch);
            let sel_end = std::cmp::max(cur, anch);

            // address bar url autocomplete only applies to single-token urls without spaces or newlines
            let is_single_token = !text.is_empty() && !text.contains(' ') && !text.contains('\n');

            if sel_start == sel_end {
                self.typed_over_selection = false;
            } else if self.raw_buffer.is_empty() {
                self.reset();
                if is_single_token && sel_start == 0 && sel_end == chars.len() {
                    self.typed_over_selection = true;
                }
            }

            // browser address bar autocomplete detection
            if is_single_token && !self.raw_buffer.is_empty() && sel_start < sel_end && sel_end == chars.len() {
                let before_sel: String = chars[..sel_start].iter().collect();
                if before_sel.ends_with(&self.last_composed) {
                    has_autocomplete = true;
                }
            }

            if is_single_token && self.typed_over_selection && sel_start < sel_end {
                has_autocomplete = true;
            }

            // word seeding from surrounding text only when cursor is at word end
            let is_at_word_end = cur == chars.len() || is_word_break(chars[cur] as u32);
            let is_telex_mod = matches!(key_ch, 'a' | 'e' | 'o' | 'd' | 'w' | 's' | 'f' | 'r' | 'x' | 'j');
            if !just_del && !is_stale && !has_autocomplete && cursor == anchor && self.last_composed.is_empty() && cur > 0 && is_telex_mod && is_at_word_end {
                let mut start = cur;
                while start > 0 {
                    let prev_char = chars[start - 1];
                    if is_word_break(prev_char as u32) {
                        break;
                    }
                    start -= 1;
                }
                let old_word: String = chars[start..cur].iter().collect();
                if !old_word.is_empty() && old_word.chars().all(|c| c.is_alphabetic()) {
                    self.raw_buffer = decompose_to_telex(&old_word);
                    self.last_composed = old_word;
                }
            }
        } else {
            self.stale_surr = None;
        }

        self.raw_buffer.push_str(key_str);
        let new_word = self.engine.transform(&self.raw_buffer);

        let (deleted_part, added_part) = compare_and_split(&self.last_composed, &new_word);

        if deleted_part.is_empty() && added_part == key_str {
            self.last_composed = new_word;
            return self.forward();
        }

        let chars_to_delete = deleted_part.chars().count();
        self.last_composed = new_word;

        if has_autocomplete {
            self.typed_over_selection = false;
            return self.address_bar_fix(chars_to_delete, &added_part);
        }

        self.replace(chars_to_delete, &added_part)
    }

    fn forward(&self) -> ImeAction {
        ImeAction {
            action_type: ACTION_FORWARD,
            delete_count: 0,
            commit_str: std::ptr::null(),
        }
    }

    fn address_bar_fix(&mut self, delete_count: usize, text: &str) -> ImeAction {
        self.commit_buf = CString::new(text).unwrap_or_default();
        ImeAction {
            action_type: ACTION_ADDRESS_BAR_FIX,
            delete_count,
            commit_str: self.commit_buf.as_ptr(),
        }
    }

    fn replace(&mut self, delete_count: usize, text: &str) -> ImeAction {
        self.commit_buf = CString::new(text).unwrap_or_default();
        ImeAction {
            action_type: ACTION_REPLACE,
            delete_count,
            commit_str: self.commit_buf.as_ptr(),
        }
    }
}

fn is_word_break(ucs4: u32) -> bool {
    ucs4 == ' ' as u32
        || ucs4 == '\t' as u32
        || ucs4 == '\n' as u32
        || ucs4 == '\r' as u32
        || ucs4 == 0
        || (ucs4 >= 58 && ucs4 <= 64)
        || (ucs4 >= 33 && ucs4 <= 47)
        || (ucs4 >= 91 && ucs4 <= 96)
        || (ucs4 >= 123 && ucs4 <= 126)
}

fn decompose_char_to_telex(c: char) -> (&'static str, Option<char>) {
    match c {
        'á' => ("a", Some('s')), 'à' => ("a", Some('f')), 'ả' => ("a", Some('r')), 'ã' => ("a", Some('x')), 'ạ' => ("a", Some('j')),
        'Á' => ("A", Some('s')), 'À' => ("A", Some('f')), 'Ả' => ("A", Some('r')), 'Ã' => ("A", Some('x')), 'Ạ' => ("A", Some('j')),
        'â' => ("aa", None), 'ấ' => ("aa", Some('s')), 'ầ' => ("aa", Some('f')), 'ẩ' => ("aa", Some('r')), 'ẫ' => ("aa", Some('x')), 'ậ' => ("aa", Some('j')),
        'Â' => ("AA", None), 'Ấ' => ("AA", Some('s')), 'Ầ' => ("AA", Some('f')), 'Ẩ' => ("AA", Some('r')), 'Ẫ' => ("AA", Some('x')), 'Ậ' => ("AA", Some('j')),
        'ă' => ("aw", None), 'ắ' => ("aw", Some('s')), 'ằ' => ("aw", Some('f')), 'ẳ' => ("aw", Some('r')), 'ẵ' => ("aw", Some('x')), 'ặ' => ("aw", Some('j')),
        'Ă' => ("AW", None), 'Ắ' => ("AW", Some('s')), 'Ằ' => ("AW", Some('f')), 'Ẳ' => ("AW", Some('r')), 'Ẵ' => ("AW", Some('x')), 'Ặ' => ("AW", Some('j')),
        'é' => ("e", Some('s')), 'è' => ("e", Some('f')), 'ẻ' => ("e", Some('r')), 'ẽ' => ("e", Some('x')), 'ẹ' => ("e", Some('j')),
        'É' => ("E", Some('s')), 'È' => ("E", Some('f')), 'Ẻ' => ("E", Some('r')), 'Ẽ' => ("E", Some('x')), 'Ẹ' => ("E", Some('j')),
        'ê' => ("ee", None), 'ế' => ("ee", Some('s')), 'ề' => ("ee", Some('f')), 'ể' => ("ee", Some('r')), 'ễ' => ("ee", Some('x')), 'ệ' => ("ee", Some('j')),
        'Ê' => ("EE", None), 'Ế' => ("EE", Some('s')), 'Ề' => ("EE", Some('f')), 'Ể' => ("EE", Some('r')), 'Ễ' => ("EE", Some('x')), 'Ệ' => ("EE", Some('j')),
        'í' => ("i", Some('s')), 'ì' => ("i", Some('f')), 'ỉ' => ("i", Some('r')), 'ĩ' => ("i", Some('x')), 'ị' => ("i", Some('j')),
        'Í' => ("I", Some('s')), 'Ì' => ("I", Some('f')), 'Ỉ' => ("I", Some('r')), 'Ĩ' => ("I", Some('x')), 'Ị' => ("I", Some('j')),
        'ó' => ("o", Some('s')), 'ò' => ("o", Some('f')), 'ỏ' => ("o", Some('r')), 'õ' => ("o", Some('x')), 'ọ' => ("o", Some('j')),
        'Ó' => ("O", Some('s')), 'Ò' => ("O", Some('f')), 'Ỏ' => ("O", Some('r')), 'Õ' => ("O", Some('x')), 'Ọ' => ("O", Some('j')),
        'ô' => ("oo", None), 'ố' => ("oo", Some('s')), 'ồ' => ("oo", Some('f')), 'ổ' => ("oo", Some('r')), 'ỗ' => ("oo", Some('x')), 'ộ' => ("oo", Some('j')),
        'Ô' => ("OO", None), 'Ố' => ("OO", Some('s')), 'Ồ' => ("OO", Some('f')), 'Ổ' => ("OO", Some('r')), 'Ỗ' => ("OO", Some('x')), 'Ộ' => ("OO", Some('j')),
        'ơ' => ("ow", None), 'ớ' => ("ow", Some('s')), 'ờ' => ("ow", Some('f')), 'ở' => ("ow", Some('r')), 'ỡ' => ("ow", Some('x')), 'ợ' => ("ow", Some('j')),
        'Ơ' => ("OW", None), 'Ớ' => ("OW", Some('s')), 'Ờ' => ("OW", Some('f')), 'Ở' => ("OW", Some('r')), 'Ỡ' => ("OW", Some('x')), 'Ợ' => ("OW", Some('j')),
        'ú' => ("u", Some('s')), 'ù' => ("u", Some('f')), 'ủ' => ("u", Some('r')), 'ũ' => ("u", Some('x')), 'ụ' => ("u", Some('j')),
        'Ú' => ("U", Some('s')), 'Ù' => ("U", Some('f')), 'Ủ' => ("U", Some('r')), 'Ũ' => ("U", Some('x')), 'Ụ' => ("U", Some('j')),
        'ư' => ("uw", None), 'ứ' => ("uw", Some('s')), 'ừ' => ("uw", Some('f')), 'ử' => ("uw", Some('r')), 'ữ' => ("uw", Some('x')), 'ự' => ("uw", Some('j')),
        'Ư' => ("UW", None), 'Ứ' => ("UW", Some('s')), 'Ừ' => ("UW", Some('f')), 'Ử' => ("UW", Some('r')), 'Ữ' => ("UW", Some('x')), 'Ự' => ("UW", Some('j')),
        'ý' => ("y", Some('s')), 'ỳ' => ("y", Some('f')), 'ỷ' => ("y", Some('r')), 'ỹ' => ("y", Some('x')), 'ỵ' => ("y", Some('j')),
        'Ý' => ("Y", Some('s')), 'Ỳ' => ("Y", Some('f')), 'Ỷ' => ("Y", Some('r')), 'Ỹ' => ("Y", Some('x')), 'Ỵ' => ("Y", Some('j')),
        'đ' => ("dd", None),
        'Đ' => ("Dd", None),
        _ => ("", None),
    }
}

pub fn decompose_to_telex(word: &str) -> String {
    let mut result = String::with_capacity(word.len() + 4);
    let mut trailing_tone: Option<char> = None;

    for ch in word.chars() {
        let (base, tone) = decompose_char_to_telex(ch);
        if !base.is_empty() {
            result.push_str(base);
            if tone.is_some() {
                trailing_tone = tone;
            }
        } else {
            result.push(ch);
        }
    }

    if let Some(t) = trailing_tone {
        result.push(t);
    }

    result
}

fn compare_and_split(a: &str, b: &str) -> (String, String) {
    let a_chars: Vec<char> = a.chars().collect();
    let b_chars: Vec<char> = b.chars().collect();

    let mut prefix_len = 0;
    while prefix_len < a_chars.len()
        && prefix_len < b_chars.len()
        && a_chars[prefix_len] == b_chars[prefix_len]
    {
        prefix_len += 1;
    }

    let deleted: String = a_chars[prefix_len..].iter().collect();
    let added: String = b_chars[prefix_len..].iter().collect();
    (deleted, added)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn clak_context_new(method: i32) -> *mut ClakContext {
    let m = match method {
        0 => Method::Telex,
        1 => Method::Vni,
        2 => Method::Viqr,
        3 => Method::TeipVni,
        _ => Method::Telex,
    };
    Box::into_raw(Box::new(ClakContext::new(m)))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn clak_context_free(ctx: *mut ClakContext) {
    if !ctx.is_null() {
        drop(Box::from_raw(ctx));
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn clak_context_reset(ctx: *mut ClakContext) {
    if let Some(c) = ctx.as_mut() {
        c.reset();
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn clak_process_key(
    ctx: *mut ClakContext,
    key_sym: u32,
    key_str: *const c_char,
    has_ctrl_alt: bool,
    surrounding_text: *const c_char,
    cursor: usize,
    anchor: usize,
) -> ImeAction {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let c = match ctx.as_mut() {
            Some(ptr) => ptr,
            None => return ImeAction {
                action_type: ACTION_FORWARD,
                delete_count: 0,
                commit_str: std::ptr::null(),
            },
        };

        let k_str = if key_str.is_null() {
            ""
        } else {
            match CStr::from_ptr(key_str).to_str() {
                Ok(s) => s,
                Err(_) => "",
            }
        };

        let s_text = if surrounding_text.is_null() {
            None
        } else {
            CStr::from_ptr(surrounding_text).to_str().ok()
        };

        let action = c.process_key(key_sym, k_str, has_ctrl_alt, s_text, cursor, anchor);

        // log key action to /tmp/clak.log
        if let Ok(mut f) = std::fs::OpenOptions::new().create(true).append(true).open("/tmp/clak.log") {
            use std::io::Write;
            let act_name = match action.action_type {
                0 => "FORWARD",
                1 => "COMMIT",
                2 => "REPLACE_SURR",
                3 => "ADDR_BAR_FIX",
                4 => "REPLACE",
                _ => "UNKNOWN",
            };
            let commit = if action.commit_str.is_null() {
                ""
            } else {
                CStr::from_ptr(action.commit_str).to_str().unwrap_or("")
            };
            let mut surr_preview = String::from("none");
            if let Some(t) = s_text {
                let preview: String = t.chars().take(25).collect();
                surr_preview = format!("'{}'(c={},a={})", preview, cursor, anchor);
            }
            let _ = writeln!(
                f,
                "Key: '{}' (0x{:x}) | Surr: {} | Act: {} (del={}) -> Commit: '{}' | Raw: '{}' | Composed: '{}'",
                k_str, key_sym, surr_preview, act_name, action.delete_count, commit, c.raw_buffer, c.last_composed
            );
        }

        action
    }));

    match result {
        Ok(action) => action,
        Err(_) => ImeAction {
            action_type: ACTION_FORWARD,
            delete_count: 0,
            commit_str: std::ptr::null(),
        },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_surrounding_dd_to_d_stroke() {
        let mut ctx = ClakContext::new(Method::Telex);
        let action = ctx.process_key(b'd' as u32, "d", false, Some("d"), 1, 1);
        assert_eq!(action.action_type, ACTION_UINPUT_REPLACE);
        assert_eq!(action.delete_count, 1);
        let commit_text = unsafe { CStr::from_ptr(action.commit_str).to_str().unwrap() };
        assert_eq!(commit_text, "đ");
    }

    #[test]
    fn test_address_bar_autocomplete_selection() {
        // cursor=1, anchor=10: user typed 'd', autocomplete showed 'douyin.com'
        // we must NOT seed from surrounding text (that would double-count the 'd')
        // so raw_buffer = "d" only, transform = "d", no replacement → forward
        // the autocomplete fix happens on the second 'd': see test_address_bar_autocomplete_flow
        let mut ctx = ClakContext::new(Method::Telex);
        let action = ctx.process_key(b'd' as u32, "d", false, Some("douyin.com"), 1, 10);
        // engine did not seed from surrounding text, so 'd' alone stays as 'd' → forward
        assert_eq!(action.action_type, ACTION_FORWARD);
    }

    #[test]
    fn test_normal_english_key_forwards() {
        let mut ctx = ClakContext::new(Method::Telex);
        let action = ctx.process_key(b'k' as u32, "k", false, Some("hello "), 6, 6);
        assert_eq!(action.action_type, ACTION_FORWARD);
    }

    #[test]
    fn test_address_bar_stale_surrounding() {
        let mut ctx = ClakContext::new(Method::Telex);
        let act1 = ctx.process_key(b'd' as u32, "d", false, Some("\n"), 0, 0);
        assert_eq!(act1.action_type, ACTION_FORWARD);
        let act2 = ctx.process_key(b'd' as u32, "d", false, Some("\n"), 0, 0);
        assert_eq!(act2.action_type, ACTION_UINPUT_REPLACE);
        assert_eq!(act2.delete_count, 1);
        let commit = unsafe { CStr::from_ptr(act2.commit_str).to_str().unwrap() };
        assert_eq!(commit, "đ");
    }

    #[test]
    fn test_address_bar_autocomplete_flow() {
        let mut ctx = ClakContext::new(Method::Telex);
        let act1 = ctx.process_key(b'd' as u32, "d", false, Some("\n"), 0, 0);
        assert_eq!(act1.action_type, ACTION_FORWARD);
        let act2 = ctx.process_key(b'd' as u32, "d", false, Some("discord.com"), 1, 11);
        assert_eq!(act2.action_type, ACTION_ADDRESS_BAR_FIX);
        assert_eq!(act2.delete_count, 1);
        let commit = unsafe { CStr::from_ptr(act2.commit_str).to_str().unwrap() };
        assert_eq!(commit, "đ");
    }

    #[test]
    fn test_address_bar_typing_over_selected_url() {
        let mut ctx = ClakContext::new(Method::Telex);
        let act1 = ctx.process_key(b'd' as u32, "d", false, Some("https://www.facebook.com"), 0, 24);
        assert_eq!(act1.action_type, ACTION_FORWARD);
        let act2 = ctx.process_key(b'd' as u32, "d", false, Some("https://www.facebook.com"), 0, 24);
        assert_eq!(act2.action_type, ACTION_ADDRESS_BAR_FIX);
        assert_eq!(act2.delete_count, 1);
        let commit = unsafe { CStr::from_ptr(act2.commit_str).to_str().unwrap() };
        assert_eq!(commit, "đ");
    }

    #[test]
    fn test_address_bar_delete_and_retype() {
        let mut ctx = ClakContext::new(Method::Telex);
        let act1 = ctx.process_key(b'd' as u32, "d", false, Some("\n"), 0, 0);
        assert_eq!(act1.action_type, ACTION_FORWARD);
        let act2 = ctx.process_key(b'd' as u32, "d", false, Some("discord.com"), 1, 11);
        assert_eq!(act2.action_type, ACTION_ADDRESS_BAR_FIX);
        assert_eq!(act2.delete_count, 1);

        // dismiss autocomplete suggestion
        let act_bs1 = ctx.process_key(0xff08, "", false, Some("đangcap.vn"), 1, 10);
        assert_eq!(act_bs1.action_type, ACTION_FORWARD);

        // delete 'đ' character
        let act_bs2 = ctx.process_key(0xff08, "", false, Some("đ"), 1, 1);
        assert_eq!(act_bs2.action_type, ACTION_FORWARD);

        // address bar is empty, type 'd' again
        let act3 = ctx.process_key(b'd' as u32, "d", false, Some(""), 0, 0);
        // must forward 'd', not become 'đ'
        assert_eq!(act3.action_type, ACTION_FORWARD);
    }

    #[test]
    fn test_backspace_and_retype_with_stale_surrounding() {
        let mut ctx = ClakContext::new(Method::Telex);
        // step 1: type 'dd' -> 'đ'
        ctx.process_key(b'd' as u32, "d", false, Some(""), 0, 0);
        let act2 = ctx.process_key(b'd' as u32, "d", false, Some("d"), 1, 1);
        assert_eq!(act2.action_type, ACTION_UINPUT_REPLACE);

        // step 2: backspace deletes 'đ'
        let act_bs = ctx.process_key(0xff08, "", false, Some("đ"), 1, 1);
        assert_eq!(act_bs.action_type, ACTION_FORWARD);

        // step 3: retype 'd' while surrounding is still stale 'đ'(1, 1)
        let act3 = ctx.process_key(b'd' as u32, "d", false, Some("đ"), 1, 1);
        assert_eq!(act3.action_type, ACTION_FORWARD);
        assert_eq!(ctx.raw_buffer, "d");
        assert_eq!(ctx.last_composed, "d");

        // step 4: type second 'd' -> should become 'đ'
        let act4 = ctx.process_key(b'd' as u32, "d", false, Some("d"), 1, 1);
        assert_eq!(act4.action_type, ACTION_UINPUT_REPLACE);
        let commit4 = unsafe { CStr::from_ptr(act4.commit_str).to_str().unwrap() };
        assert_eq!(commit4, "đ");
        assert_eq!(ctx.raw_buffer, "dd");
        assert_eq!(ctx.last_composed, "đ");
    }

    #[test]
    fn test_user_scenario_dd_del_dddddd_del_dd() {
        let mut ctx = ClakContext::new(Method::Telex);
        // step 1: dd -> đ
        let _ = ctx.process_key(b'd' as u32, "d", false, Some(""), 0, 0);
        let _ = ctx.process_key(b'd' as u32, "d", false, Some("d"), 1, 1);
        assert_eq!(ctx.last_composed, "đ");
        assert_eq!(ctx.raw_buffer, "dd");

        // step 2: backspace đ
        let _ = ctx.process_key(0xff08, "", false, Some("đ"), 1, 1);
        assert_eq!(ctx.last_composed, "");
        assert_eq!(ctx.raw_buffer, "");

        // step 3: type dddddd
        for _ in "dddddd".chars() {
            let _ = ctx.process_key(b'd' as u32, "d", false, None, 0, 0);
        }
        assert_eq!(ctx.last_composed, "ddddd");
        assert_eq!(ctx.raw_buffer, "dddddd");

        // step 4: backspace all
        let count = ctx.last_composed.chars().count();
        for _ in 0..count {
            let _ = ctx.process_key(0xff08, "", false, None, 0, 0);
        }
        assert_eq!(ctx.last_composed, "");
        assert_eq!(ctx.raw_buffer, "");

        // step 5: type dd -> đ cleanly
        let a_d1 = ctx.process_key(b'd' as u32, "d", false, None, 0, 0);
        assert_eq!(a_d1.action_type, ACTION_FORWARD);
        assert_eq!(ctx.last_composed, "d");
        assert_eq!(ctx.raw_buffer, "d");

        let a_d2 = ctx.process_key(b'd' as u32, "d", false, None, 0, 0);
        assert_eq!(a_d2.action_type, ACTION_UINPUT_REPLACE);
        assert_eq!(ctx.last_composed, "đ");
        assert_eq!(ctx.raw_buffer, "dd");
    }

    #[test]
    fn test_surrounding_vietnamese_word_progression() {
        let mut ctx = ClakContext::new(Method::Telex);
        let action = ctx.process_key(b'j' as u32, "j", false, Some("viêt"), 4, 4);
        let commit_text = unsafe { CStr::from_ptr(action.commit_str).to_str().unwrap() };
        assert_eq!(action.action_type, ACTION_UINPUT_REPLACE);
        assert_eq!(action.delete_count, 2);
        assert_eq!(commit_text, "ệt");
    }

    fn simulate_typing(input: &str) -> String {
        let mut ctx = ClakContext::new(Method::Telex);
        let mut doc = String::new();
        for ch in input.chars() {
            let key_str = ch.to_string();
            let cur = doc.chars().count();
            let action = ctx.process_key(ch as u32, &key_str, false, Some(&doc), cur, cur);
            match action.action_type {
                ACTION_FORWARD => {
                    doc.push(ch);
                }
                ACTION_COMMIT => {
                    let s = unsafe { CStr::from_ptr(action.commit_str).to_str().unwrap() };
                    doc.push_str(s);
                }
                ACTION_REPLACE_SURROUNDING | ACTION_ADDRESS_BAR_FIX | ACTION_UINPUT_REPLACE => {
                    let del = action.delete_count;
                    let s = unsafe { CStr::from_ptr(action.commit_str).to_str().unwrap() };
                    let mut doc_chars: Vec<char> = doc.chars().collect();
                    for _ in 0..del {
                        doc_chars.pop();
                    }
                    doc = doc_chars.into_iter().collect();
                    doc.push_str(s);
                }
                _ => {}
            }
        }
        doc
    }

    fn simulate_typing_fallback(input: &str) -> String {
        let mut ctx = ClakContext::new(Method::Telex);
        let mut doc = String::new();
        for ch in input.chars() {
            let key_str = ch.to_string();
            let action = ctx.process_key(ch as u32, &key_str, false, None, 0, 0);
            match action.action_type {
                ACTION_FORWARD => {
                    doc.push(ch);
                }
                ACTION_COMMIT => {
                    let s = unsafe { CStr::from_ptr(action.commit_str).to_str().unwrap() };
                    doc.push_str(s);
                }
                ACTION_REPLACE_SURROUNDING | ACTION_ADDRESS_BAR_FIX | ACTION_UINPUT_REPLACE => {
                    let del = action.delete_count;
                    let s = unsafe { CStr::from_ptr(action.commit_str).to_str().unwrap() };
                    let mut doc_chars: Vec<char> = doc.chars().collect();
                    for _ in 0..del {
                        doc_chars.pop();
                    }
                    doc = doc_chars.into_iter().collect();
                    doc.push_str(s);
                }
                _ => {}
            }
        }
        doc
    }

    #[test]
    fn test_full_vietnamese_typing() {
        assert_eq!(simulate_typing("dd"), "đ");
        assert_eq!(simulate_typing("tieengs"), "tiếng");
        assert_eq!(simulate_typing("vieetj"), "việt");
        assert_eq!(simulate_typing("chaof"), "chào");
        assert_eq!(simulate_typing("nguwowif"), "người");
        assert_eq!(simulate_typing("dduwowcj"), "được");
        assert_eq!(simulate_typing("khoong"), "không");
        assert_eq!(
            simulate_typing("chaof banj tooi laf nguwowif vieetj nam"),
            "chào bạn tôi là người việt nam"
        );
        assert_eq!(simulate_typing("test"), "tét");
    }

    #[test]
    fn test_fallback_vietnamese_typing() {
        assert_eq!(simulate_typing_fallback("dd"), "đ");
        assert_eq!(simulate_typing_fallback("tieengs"), "tiếng");
        assert_eq!(simulate_typing_fallback("vieetj"), "việt");
        assert_eq!(simulate_typing_fallback("chaof"), "chào");
        assert_eq!(simulate_typing_fallback("nguwowif"), "người");
        assert_eq!(simulate_typing_fallback("dduwowcj"), "được");
        assert_eq!(simulate_typing_fallback("khoong"), "không");
        assert_eq!(simulate_typing_fallback("ddaay"), "đây");
        assert_eq!(simulate_typing_fallback("booj"), "bộ");
        assert_eq!(simulate_typing_fallback("mooj"), "mộ");
        assert_eq!(
            simulate_typing_fallback("chaof banj tooi laf nguwowif vieetj nam"),
            "chào bạn tôi là người việt nam"
        );
        assert_eq!(simulate_typing_fallback("test"), "tét");
        assert_eq!(simulate_typing_fallback("tesst"), "test");
    }

    #[test]
    fn test_chuns_progression() {
        let mut ctx = ClakContext::new(Method::Telex);
        let keys = ["c", "h", "u", "n", "s"];
        for k in keys {
            ctx.process_key(k.chars().next().unwrap() as u32, k, false, None, 0, 0);
        }
        assert_eq!(ctx.last_composed, "chún");
        ctx.process_key('g' as u32, "g", false, None, 0, 0);
        assert_eq!(ctx.last_composed, "chúng");
    }

    #[test]
    fn test_twitter_placeholder_typing_is() {
        let mut ctx = ClakContext::new(Method::Telex);
        // Twitter/Facebook modal opens with placeholder or button text in surrounding
        let act1 = ctx.process_key(b'i' as u32, "i", false, Some("Có gì đang xảy ra?\n"), 0, 0);
        assert_eq!(act1.action_type, ACTION_FORWARD);
        assert_eq!(ctx.last_composed, "i");

        let act2 = ctx.process_key(b's' as u32, "s", false, Some("i"), 1, 1);
        assert_eq!(act2.action_type, ACTION_UINPUT_REPLACE);
        assert_eq!(act2.delete_count, 1);
        let commit2 = unsafe { CStr::from_ptr(act2.commit_str).to_str().unwrap() };
        assert_eq!(commit2, "í");

        // type second s to toggle to english 'is'
        let act3 = ctx.process_key(b's' as u32, "s", false, Some("í"), 1, 1);
        assert_eq!(act3.action_type, ACTION_UINPUT_REPLACE);
        assert_eq!(act3.delete_count, 1);
        let commit3 = unsafe { CStr::from_ptr(act3.commit_str).to_str().unwrap() };
        assert_eq!(commit3, "is");
    }

    #[test]
    fn test_multiline_editor_no_false_autocomplete() {
        let mut ctx = ClakContext::new(Method::Telex);
        // multiline editor with selection (cursor != anchor) should NOT trigger address bar autocomplete
        let act = ctx.process_key(b'i' as u32, "i", false, Some("Line 1\nLine 2"), 0, 6);
        assert_eq!(act.action_type, ACTION_FORWARD);
    }

    #[test]
    fn test_twitter_draftjs_trailing_newline_is() {
        let mut ctx = ClakContext::new(Method::Telex);
        // simulate Twitter Draft.js composer where empty box has '\n' and typed word has trailing '\n'
        let act1 = ctx.process_key(b'i' as u32, "i", false, Some("\n"), 0, 0);
        assert_eq!(act1.action_type, ACTION_FORWARD);
        assert_eq!(ctx.last_composed, "i");

        // key 's' arrives with surrounding "i\n" and cursor=1
        let act2 = ctx.process_key(b's' as u32, "s", false, Some("i\n"), 1, 1);
        assert_eq!(act2.action_type, ACTION_UINPUT_REPLACE);
        assert_eq!(act2.delete_count, 1);
        let commit2 = unsafe { CStr::from_ptr(act2.commit_str).to_str().unwrap() };
        assert_eq!(commit2, "í");
    }

    #[test]
    fn test_twitter_draftjs_reseed_uaj_to_ua_with_dot() {
        let mut ctx = ClakContext::new(Method::Telex);
        // even if reset happened between u and a:
        ctx.reset();
        // user presses 'a' with surrounding "u\n"
        let act1 = ctx.process_key(b'a' as u32, "a", false, Some("u\n"), 1, 1);
        assert_eq!(act1.action_type, ACTION_FORWARD);
        assert_eq!(ctx.last_composed, "ua");

        // user presses 'j' with surrounding "ua\n"
        let act2 = ctx.process_key(b'j' as u32, "j", false, Some("ua\n"), 2, 2);
        assert_eq!(act2.action_type, ACTION_UINPUT_REPLACE);
        assert_eq!(act2.delete_count, 2);
        let commit2 = unsafe { CStr::from_ptr(act2.commit_str).to_str().unwrap() };
        assert_eq!(commit2, "ụa");
    }

    #[test]
    fn test_typing_over_selected_word_in_sentence() {
        let mut ctx = ClakContext::new(Method::Telex);
        let text = "Fcitx5 and Wayland";
        // user selects "and" (indices 7..10) and types "và" (v, a, f)
        let a1 = ctx.process_key(b'v' as u32, "v", false, Some(text), 7, 10);
        assert_eq!(a1.action_type, ACTION_FORWARD);

        // editor replaced "and" with "v", cursor at 8, anchor at 8
        let a2 = ctx.process_key(b'a' as u32, "a", false, Some("Fcitx5 v Wayland"), 8, 8);
        assert_eq!(a2.action_type, ACTION_FORWARD);

        // typing 'f' to transform "va" -> "và"
        let a3 = ctx.process_key(b'f' as u32, "f", false, Some("Fcitx5 va Wayland"), 9, 9);
        assert_eq!(a3.action_type, ACTION_REPLACE);
        assert_eq!(a3.delete_count, 1);
        let commit3 = unsafe { CStr::from_ptr(a3.commit_str).to_str().unwrap() };
        assert_eq!(commit3, "à");
    }
}
