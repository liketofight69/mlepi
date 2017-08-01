#include <string.h>
#include <ctype.h>
#include "mle.h"

// Clone cursor
int cursor_clone(cursor_t* cursor, int use_srules, cursor_t** ret_clone) {
    cursor_t* clone;
    bview_add_cursor(cursor->bview, cursor->mark->bline, cursor->mark->col, &clone);
    if (cursor->is_anchored) {
        cursor_toggle_anchor(clone, use_srules);
        mark_join(clone->anchor, cursor->anchor);
    }
    *ret_clone = clone;
    return MLE_OK;
}

// Remove cursor
int cursor_destroy(cursor_t* cursor) {
    return bview_remove_cursor(cursor->bview, cursor);
}

// Select by mark
int cursor_select_between(cursor_t* cursor, mark_t* a, mark_t* b, int use_srules) {
    cursor_drop_anchor(cursor, use_srules);
    if (mark_is_lt(a, b)) {
        mark_join(cursor->mark, a);
        mark_join(cursor->anchor, b);
    } else {
        mark_join(cursor->mark, b);
        mark_join(cursor->anchor, a);
    }
    return MLE_OK;
}

// Toggle cursor anchor
int cursor_toggle_anchor(cursor_t* cursor, int use_srules) {
    if (!cursor->is_anchored) {
        mark_clone(cursor->mark, &(cursor->anchor));
        if (use_srules) {
            cursor->sel_rule = srule_new_range(cursor->mark, cursor->anchor, 0, TB_REVERSE);
            buffer_add_srule(cursor->bview->buffer, cursor->sel_rule);
        }
        cursor->is_anchored = 1;
    } else {
        if (use_srules && cursor->sel_rule) {
            buffer_remove_srule(cursor->bview->buffer, cursor->sel_rule);
            srule_destroy(cursor->sel_rule);
            cursor->sel_rule = NULL;
        }
        mark_destroy(cursor->anchor);
        cursor->is_anchored = 0;
    }
    return MLE_OK;
}

// Drop cursor anchor
int cursor_drop_anchor(cursor_t* cursor, int use_srules) {
    if (cursor->is_anchored) return MLE_OK;
    return cursor_toggle_anchor(cursor, use_srules);
}

// Lift cursor anchor
int cursor_lift_anchor(cursor_t* cursor) {
    if (!cursor->is_anchored) return MLE_OK;
    return cursor_toggle_anchor(cursor, cursor->sel_rule ? 1 : 0);
}

// Get lo and hi marks in a is_anchored=1 cursor
int cursor_get_lo_hi(cursor_t* cursor, mark_t** ret_lo, mark_t** ret_hi) {
    if (!cursor->is_anchored) {
        return MLE_ERR;
    }
    if (mark_is_gt(cursor->anchor, cursor->mark)) {
        *ret_lo = cursor->mark;
        *ret_hi = cursor->anchor;
    } else {
        *ret_lo = cursor->anchor;
        *ret_hi = cursor->mark;
    }
    return MLE_OK;
}

// Make selection by strat
int cursor_select_by(cursor_t* cursor, const char* strat) {
    if (cursor->is_anchored) {
        return MLE_ERR;
    }
    if (strcmp(strat, "bracket") == 0) {
        return cursor_select_by_bracket(cursor);
    } else if (strcmp(strat, "word") == 0) {
        return cursor_select_by_word(cursor);
    } else if (strcmp(strat, "word_back") == 0) {
        return cursor_select_by_word_back(cursor);
    } else if (strcmp(strat, "word_forward") == 0) {
        return cursor_select_by_word_forward(cursor);
    } else if (strcmp(strat, "eol") == 0 && !mark_is_at_eol(cursor->mark)) {
        cursor_toggle_anchor(cursor, 0);
        mark_move_eol(cursor->anchor);
    } else if (strcmp(strat, "bol") == 0 && !mark_is_at_bol(cursor->mark)) {
        cursor_toggle_anchor(cursor, 0);
        mark_move_bol(cursor->anchor);
    } else if (strcmp(strat, "string") == 0) {
        return cursor_select_by_string(cursor);
    } else {
        MLE_RETURN_ERR(cursor->bview->editor, "Unrecognized cursor_select_by strat '%s'", strat);
    }
    return MLE_OK;
}

// Select by bracket
int cursor_select_by_bracket(cursor_t* cursor) {
    mark_t* orig;
    mark_clone(cursor->mark, &orig);
    if (mark_move_bracket_top(cursor->mark, MLE_BRACKET_PAIR_MAX_SEARCH) != MLBUF_OK) {
        mark_destroy(orig);
        return MLE_ERR;
    }
    cursor_toggle_anchor(cursor, 0);
    if (mark_move_bracket_pair(cursor->anchor, MLE_BRACKET_PAIR_MAX_SEARCH) != MLBUF_OK) {
        cursor_toggle_anchor(cursor, 0);
        mark_join(cursor->mark, orig);
        mark_destroy(orig);
        return MLE_ERR;
    }
    mark_move_by(cursor->mark, 1);
    mark_destroy(orig);
    return MLE_OK;
}

// Select by word-back
int cursor_select_by_word_back(cursor_t* cursor) {
    if (mark_is_at_word_bound(cursor->mark, -1)) return MLE_ERR;
    cursor_toggle_anchor(cursor, 0);
    mark_move_prev_re(cursor->mark, MLE_RE_WORD_BACK, sizeof(MLE_RE_WORD_BACK)-1);
    return MLE_OK;
}

// Select by word-forward
int cursor_select_by_word_forward(cursor_t* cursor) {
    if (mark_is_at_word_bound(cursor->mark, 1)) return MLE_ERR;
    cursor_toggle_anchor(cursor, 0);
    mark_move_next_re(cursor->mark, MLE_RE_WORD_FORWARD, sizeof(MLE_RE_WORD_FORWARD)-1);
    return MLE_OK;
}

// Select by string
int cursor_select_by_string(cursor_t* cursor) {
    mark_t* orig;
    uint32_t qchar;
    char* qre;
    mark_clone(cursor->mark, &orig);
    if (mark_move_prev_re(cursor->mark, "(?<!\\\\)[`'\"]", strlen("(?<!\\\\)[`'\"]")) != MLBUF_OK) {
        mark_destroy(orig);
        return MLE_ERR;
    }
    cursor_toggle_anchor(cursor, 0);
    mark_get_char_after(cursor->mark, &qchar);
    mark_move_by(cursor->mark, 1);
    if (qchar == '"') {
        qre = "(?<!\\\\)\"";
    } else if (qchar == '\'') {
        qre = "(?<!\\\\)'";
    } else {
        qre = "(?<!\\\\)`";
    }
    if (mark_move_next_re(cursor->anchor, qre, strlen(qre)) != MLBUF_OK) {
        cursor_toggle_anchor(cursor, 0);
        mark_join(cursor->mark, orig);
        mark_destroy(orig);
        return MLE_ERR;
    }
    mark_destroy(orig);
    return MLE_OK;
}

// Select by word
int cursor_select_by_word(cursor_t* cursor) {
    uint32_t after;
    if (mark_is_at_eol(cursor->mark)) return MLE_ERR;
    mark_get_char_after(cursor->mark, &after);
    if (!isalnum((char)after) && (char)after != '_') return MLE_ERR;
    if (!mark_is_at_word_bound(cursor->mark, -1)) {
        mark_move_prev_re(cursor->mark, MLE_RE_WORD_BACK, sizeof(MLE_RE_WORD_BACK)-1);
    }
    cursor_toggle_anchor(cursor, 0);
    mark_move_next_re(cursor->mark, MLE_RE_WORD_FORWARD, sizeof(MLE_RE_WORD_FORWARD)-1);
    return MLE_OK;
}

// Cut or copy text
int cursor_cut_copy(cursor_t* cursor, int is_cut, int use_srules, int append) {
    char* cutbuf;
    bint_t cutbuf_len;
    bint_t cur_len;
    if (!append && cursor->cut_buffer) {
        free(cursor->cut_buffer);
        cursor->cut_buffer = NULL;
    }
    if (!cursor->is_anchored) {
        use_srules = 0;
        cursor_toggle_anchor(cursor, use_srules);
        mark_move_bol(cursor->mark);
        mark_move_eol(cursor->anchor);
        mark_move_by(cursor->anchor, 1);
    }
    mark_get_between_mark(cursor->mark, cursor->anchor, &cutbuf, &cutbuf_len);
    if (append && cursor->cut_buffer) {
        cur_len = strlen(cursor->cut_buffer);
        cursor->cut_buffer = realloc(cursor->cut_buffer, cur_len + cutbuf_len + 1);
        strncat(cursor->cut_buffer, cutbuf, cutbuf_len);
        free(cutbuf);
    } else {
        cursor->cut_buffer = cutbuf;
    }
    if (is_cut) {
        mark_delete_between_mark(cursor->mark, cursor->anchor);
    }
    cursor_toggle_anchor(cursor, use_srules);
    return MLE_OK;
}

// Uncut (paste) text
int cursor_uncut(cursor_t* cursor) {
    if (!cursor->cut_buffer) return MLE_ERR;
    mark_insert_before(cursor->mark, cursor->cut_buffer, strlen(cursor->cut_buffer));
    return MLE_OK;
}

// Regex search and replace
int cursor_replace(cursor_t* cursor, int interactive, char* opt_regex, char* opt_replacement) {
    char* regex;
    char* replacement;
    int wrapped;
    int all;
    char* yn;
    mark_t* lo_mark;
    mark_t* hi_mark;
    mark_t* orig_mark;
    mark_t* search_mark;
    mark_t* search_mark_end;
    int anchored_before;
    srule_t* highlight;
    bline_t* bline;
    bint_t col;
    bint_t char_count;
    int pcre_rc;
    int pcre_ovector[30];
    str_t repl_backref = {0};
    int num_replacements;
    int orig_find_budge;

    if (!interactive && (!opt_regex || !opt_replacement)) {
        return MLE_ERR;
    }

    regex = NULL;
    replacement = NULL;
    wrapped = 0;
    lo_mark = NULL;
    hi_mark = NULL;
    orig_mark = NULL;
    search_mark = NULL;
    search_mark_end = NULL;
    anchored_before = 0;
    all = interactive ? 0 : 1;
    num_replacements = 0;
    mark_set_pcre_capture(&pcre_rc, pcre_ovector, 30);
    mark_set_find_budge(0, &orig_find_budge);

    do {
        if (!interactive) {
            regex = strdup(opt_regex);
            replacement = strdup(opt_replacement);
        } else {
            editor_prompt(cursor->bview->editor, "replace: Search regex?", NULL, &regex);
            if (!regex) break;
            editor_prompt(cursor->bview->editor, "replace: Replacement string?", NULL, &replacement);
            if (!replacement) break;
        }
        orig_mark = buffer_add_mark(cursor->bview->buffer, NULL, 0);
        lo_mark = buffer_add_mark(cursor->bview->buffer, NULL, 0);
        hi_mark = buffer_add_mark(cursor->bview->buffer, NULL, 0);
        search_mark = buffer_add_mark(cursor->bview->buffer, NULL, 0);
        search_mark_end = buffer_add_mark(cursor->bview->buffer, NULL, 0);
        mark_join(search_mark, cursor->mark);
        mark_join(orig_mark, cursor->mark);
        if (cursor->is_anchored) {
            anchored_before = mark_is_gt(cursor->mark, cursor->anchor);
            mark_join(lo_mark, !anchored_before ? cursor->mark : cursor->anchor);
            mark_join(hi_mark, anchored_before ? cursor->mark : cursor->anchor);
        } else {
            mark_move_beginning(lo_mark);
            mark_move_end(hi_mark);
        }
        while (1) {
            pcre_rc = 0;
            if (mark_find_next_re(search_mark, regex, strlen(regex), &bline, &col, &char_count) == MLBUF_OK
                && (mark_move_to(search_mark, bline->line_index, col) == MLBUF_OK)
                && (mark_is_gt(search_mark, lo_mark) || mark_is_eq(search_mark, lo_mark))
                && (mark_is_lt(search_mark, hi_mark))
                && (!wrapped || mark_is_lt(search_mark, orig_mark) || mark_is_eq(search_mark, orig_mark))
            ) {
                mark_move_to(search_mark_end, bline->line_index, col + char_count);
                mark_join(cursor->mark, search_mark);
                yn = NULL;
                if (all) {
                    yn = MLE_PROMPT_YES;
                } else if (interactive) {
                    highlight = srule_new_range(search_mark, search_mark_end, 0, TB_REVERSE);
                    buffer_add_srule(cursor->bview->buffer, highlight);
                    bview_rectify_viewport(cursor->bview);
                    bview_draw(cursor->bview);
                    editor_prompt(cursor->bview->editor, "replace: OK to replace? (y=yes, n=no, a=all, C-c=stop)",
                        &(editor_prompt_params_t) { .kmap = cursor->bview->editor->kmap_prompt_yna }, &yn
                    );
                    buffer_remove_srule(cursor->bview->buffer, highlight);
                    srule_destroy(highlight);
                    bview_draw(cursor->bview);
                }
                if (!yn) {
                    break;
                } else if (0 == strcmp(yn, MLE_PROMPT_YES) || 0 == strcmp(yn, MLE_PROMPT_ALL)) {
                    str_append_replace_with_backrefs(&repl_backref, search_mark->bline->data, replacement, pcre_rc, pcre_ovector, 30);
                    mark_replace_between_mark(search_mark, search_mark_end, repl_backref.data, repl_backref.len);
                    str_free(&repl_backref);
                    num_replacements += 1;
                    if (0 == strcmp(yn, MLE_PROMPT_ALL)) all = 1;
                } else {
                    mark_move_by(search_mark, 1);
                }
            } else if (!wrapped) {
                mark_join(search_mark, lo_mark);
                mark_move_by(search_mark, -1);
                wrapped = 1;
            } else {
                break;
            }
        }
    } while(0);

    if (cursor->is_anchored && lo_mark && hi_mark) {
        mark_join(cursor->mark, anchored_before ? hi_mark : lo_mark);
        mark_join(cursor->anchor, anchored_before ? lo_mark : hi_mark);
    }

    mark_set_find_budge(orig_find_budge, NULL);
    mark_set_pcre_capture(NULL, NULL, 0);
    if (regex) free(regex);
    if (replacement) free(replacement);
    if (lo_mark) mark_destroy(lo_mark);
    if (hi_mark) mark_destroy(hi_mark);
    if (orig_mark) mark_destroy(orig_mark);
    if (search_mark) mark_destroy(search_mark);
    if (search_mark_end) mark_destroy(search_mark_end);

    if (interactive) {
        MLE_SET_INFO(cursor->bview->editor, "replace: Replaced %d instance(s)", num_replacements);
        bview_rectify_viewport(cursor->bview);
        bview_draw(cursor->bview);
    }

    return MLE_OK;
}
