**Code Style and Housekeeping Rules:**

1. **Maximum Line Width:** Strictly adhere to a maximum line width of 79
   characters. This applies to everything: logic, string literals, and
   comments. Wrap long expressions or string literals as needed.
2. **No Decorative Comments:** No ASCII-art banners, no separator lines, no
   `/* ------- section ------- */` decorations. An ordinary blank line
   between functions is enough. If a plain word comment is genuinely needed
   to mark a conceptual boundary, use a single line without decoration:
   ```c
   // not broken out yet into its own module
   ```
3. **Obvious Comments:** No comments stating the obvious. Only add a comment
   when there is a non-obvious *why* or a complex design decision.
4. **Empty Lines:** NO empty lines inside function bodies. Exactly one blank
   line between function definitions.
5. **Control Flow:** Prefer single-entry / single-exit. Declare a result
   variable at the top, mutate it, return at the bottom. Avoid `continue`.
   Prefer smarter loop conditions or state variables instead of `break`.
6. **Naming:** Compact local names (single letters fine). Public APIs use
   full unambiguous words in `concept_action` form. Internal helpers are
   `static` and may reverse to `action_concept`.
7. **Braces & Inline Single-Statement:** Always use `{}` for control
   flow. **Exception:** a *single* short statement may share the line with
   `if`, `for`, or `while` when the whole construct fits within 79 chars
   and reads cleanly:
   ```c
   if (pp > 0) { printf("pp: %'d", pp); }
   for (int i = 0; i < n; i++) { run(arr[i]); }
   while (*p && isspace(*p)) { p++; }
   ```
   Never put multiple statements on one line, and don't inline if it
   looks cramped:
   ```c
   if (x) { stmt1; stmt2; }           /* WRONG — multiple stmts */
   for (int i = 0; i < very_long_name && other_condition; i++) { do_something_complex(i); }
   /* WRONG — too long, should expand */
   ```
7. **Switch:**
   - `case` labels are indented 4 spaces inside `switch`.
   - Single-statement cases may be written on one line (no `{}` needed):
     ```c
     switch (kind) {
         case 'i': return decode_int(p);
         case 's': return decode_str(p);
     }
     ```
   - Cases needing local variables use `{}`:
     ```c
     case 'a': {
         int n = count_array(p);
         return decode_array(p, n);
     }
     ```
   - Fall-throughs are strongly discouraged. Always end with `break` or
     `return`.
   - Always include `default: assert(false);` (or equivalent) to catch
     unhandled cases at runtime.
8. **Pointers & Types:** Asterisk attached to the variable name:
   `char * p`, `const char * s`.
9. **Measurements:** Use `bytes` for memory capacity, `count` for element
   counts. Never `size` or `length`.

---

### Examples: DO and DON'T

#### 1. Naming & Function Signatures
**DO:**
```c
void * copy_buffer(void * data, size_t bytes);
void sort_items(struct item * arr, size_t count);

char * string_copy(char * dst, const char * src) {
    ...
}

void foo() {
    int r = bar();
}
```

**DON'T:**
```c
void * process(void * data, size_t size);
char * strcpy(char * dest, const char * src) {}
```

#### 2. Types & Parameters
**DO:**
```c
void * data, size_t bytes
```

**DON'T:**
```c
void* ptr, size_t size
```

#### 3. Control Flow & Braces
**DO:**
```c
if (b1) {
    do_something();
} else if (b2) {
    do_other();
} else {
    fallback();
}

/* Single-statement inline — OK when it fits on one line */
if (!p) { return NULL; }
if (rc) { str_push(&r, rc); r_tok++; }
```

**DON'T:**
```c
if (b1) do_something();       /* no braces — WRONG */
if (x) { stmt1; stmt2; }     /* multiple stmts on one line — WRONG */
```

#### 4. Switch
**DO:**
```c
switch (kind) {
    case 'i': decode_int(p); break;
    case 's': decode_str(p); break;
    case 'a': {
        int n = count(p);
        decode_array(p, n);
        break;
    }
    default: assert(false);
}
```

**DON'T:**
```c
switch (kind) {
case 'i':
    decode_int(p);
    /* fall through */        /* discouraged */
case 's':
    decode_str(p);
}
```

#### 5. Continuations & Smart Conditions
**DO:**
```c
int found = 0;
for (int i = 0; i < len && !found; i++) {
    if (match(i)) {
        found = 1;
    }
}
```

**DON'T:**
```c
for (int i = 0; i < len; i++) {
    if (skip(i)) { continue; }
    if (match(i)) { break; }
}
```

#### 6. Avoiding Deep Nesting
**DO:**
```c
if (a) {
    successful();
} else if (b) {
    instead();
} else {
    otherwise();
}
```

**DON'T:**
```c
if (a) {
    handle_a();
} else {
    if (b) {
        handle_b();
    } else {
        handle_other();
    }
}
```

#### 7. Function Returns (Single Entry / Single Exit)
**DO:**
```c
int process(int * data) {
    int res = 0;
    if (data) {
        res = (*data > 0) ? 1 : -1;
    }
    return res;
}
```

**DON'T:**
```c
int process(int * data) {
    if (!data) { return 0; }    /* early return — WRONG */
    return (*data > 0) ? 1 : -1;
}
```

#### 8. Empty Lines & Function Spacing
**DO:**
```c
void foo1() {
    bar1();
    bar2();
}

void foo2() {
    bar1();
    bar2();
}
```

**DON'T:**
```c
void foo1() {
    bar1();

    bar2();    /* empty line inside body — WRONG */
}
void foo2() { /* missing blank line between functions — WRONG */
    bar1();
}
```

#### 9. Comments
**DO:**
```c
if (special_case) { // obscure platform quirk — explain the why
    process();
}
```

**DON'T:**
```c
// call process function
process();   /* states the obvious — WRONG */
```
