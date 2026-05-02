#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

/* =========================================================
 * Anchor runtime — raw 64-bit value representation
 * =========================================================
 *
 * AnchorVal is a single 64-bit word.
 * Integers: raw signed 64-bit value.
 * Pointers: raw address (from malloc-backed arenas).
 * null = 0.
 *
 * All arithmetic operates directly on raw values — zero overhead vs C.
 * ========================================================= */

typedef uint64_t AnchorVal;

#define ANCHOR_NIL ((AnchorVal)0)

/* Pointer → void*: direct cast */
#define _ANCH_HPTR(v)  ((void*)(uintptr_t)(v))

/* Integer value: identity cast */
#define _ANCH_IVAL(v)  ((intptr_t)(int64_t)(v))

/* Float64: reinterpret bits as double */
#define _ANCH_FVAL(v) ({ AnchorVal _av = (v); double _fv; __builtin_memcpy(&_fv, &_av, sizeof(double)); _fv; })

/* Float32: reinterpret low 32 bits as float */
#define _ANCH_F32VAL(v) ({ uint32_t _bv = (uint32_t)(v); float _fv; __builtin_memcpy(&_fv, &_bv, sizeof(float)); _fv; })

#if defined(__GNUC__) || defined(__clang__)
#  define ANCHOR_PURE __attribute__((const))
#else
#  define ANCHOR_PURE
#endif

/* ---- Arena ---- */

typedef struct _AnchorArena {
    char*                buf;
    size_t               cap;
    size_t               used;
    size_t               checkpoint;
    struct _AnchorArena* prev;
} _AnchorArena;

#ifdef ANCHOR_MULTI_THREADED
static _Thread_local _AnchorArena* _anchor_arena_top = NULL;
#else
static _AnchorArena* _anchor_arena_top = NULL;
#endif
#define ANCHOR_DEFAULT_ARENA_CAP (1024 * 1024)

static inline AnchorVal anchor_alloc(size_t size) {
    _AnchorArena* a = _anchor_arena_top;
    if (!a) __builtin_trap();
    size_t aligned = (size + 7u) & ~7u;
    if (a->used + aligned > a->cap) __builtin_trap();
    AnchorVal r = (AnchorVal)(uintptr_t)(a->buf + a->used);
    a->used += aligned;
    return r;
}

static inline void _anchor_arena_reset(_AnchorArena* a) { a->used = 0; }

/* Wrap a raw C pointer as an AnchorVal */
static inline ANCHOR_PURE AnchorVal anchor_ext(void* p) {
    return p ? (AnchorVal)(uintptr_t)p : ANCHOR_NIL;
}

/* Unwrap AnchorVal pointer → void* */
static inline void* _anch_ptr(AnchorVal v) { return (void*)(uintptr_t)v; }

/* ---- Integer constructors / arithmetic — all raw, zero overhead vs C ---- */

static inline ANCHOR_PURE AnchorVal anchor_int(intptr_t v)  { return (AnchorVal)(int64_t)v; }
static inline ANCHOR_PURE AnchorVal anchor_add(AnchorVal a, AnchorVal b) { return a + b; }
static inline ANCHOR_PURE AnchorVal anchor_sub(AnchorVal a, AnchorVal b) { return a - b; }
static inline ANCHOR_PURE AnchorVal anchor_mul(AnchorVal a, AnchorVal b) { return a * b; }
static inline ANCHOR_PURE AnchorVal anchor_div(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a / (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_mod(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a % (int64_t)b); }

/* ---- Float constructor / arithmetic ---- */

static inline ANCHOR_PURE AnchorVal anchor_float(double v) {
    AnchorVal bits; __builtin_memcpy(&bits, &v, sizeof(double)); return bits;
}
static inline ANCHOR_PURE AnchorVal anchor_addf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) + _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_subf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) - _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_mulf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) * _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_divf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) / _ANCH_FVAL(b)); }

/* ---- Float32 constructor / arithmetic ---- */

static inline ANCHOR_PURE AnchorVal anchor_f32(float v) {
    uint32_t bits; __builtin_memcpy(&bits, &v, sizeof(float)); return (AnchorVal)bits;
}
static inline ANCHOR_PURE AnchorVal anchor_addf32(AnchorVal a, AnchorVal b) { return anchor_f32(_ANCH_F32VAL(a) + _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_subf32(AnchorVal a, AnchorVal b) { return anchor_f32(_ANCH_F32VAL(a) - _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_mulf32(AnchorVal a, AnchorVal b) { return anchor_f32(_ANCH_F32VAL(a) * _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_divf32(AnchorVal a, AnchorVal b) { return anchor_f32(_ANCH_F32VAL(a) / _ANCH_F32VAL(b)); }

/* ---- Comparisons — direct on raw values ---- */

static inline ANCHOR_PURE AnchorVal anchor_eq(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a == (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_ne(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a != (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_lt(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a <  (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_gt(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a >  (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_le(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a <= (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_ge(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a >= (int64_t)b); }

/* ---- Bitwise ---- */

static inline ANCHOR_PURE AnchorVal anchor_band(AnchorVal a, AnchorVal b)   { return a & b; }
static inline ANCHOR_PURE AnchorVal anchor_bor (AnchorVal a, AnchorVal b)   { return a | b; }
static inline ANCHOR_PURE AnchorVal anchor_bxor(AnchorVal a, AnchorVal b)   { return a ^ b; }
static inline ANCHOR_PURE AnchorVal anchor_bnot(AnchorVal a)                { return ~a; }
static inline ANCHOR_PURE AnchorVal anchor_lshift(AnchorVal a, AnchorVal b) { return a << b; }
static inline ANCHOR_PURE AnchorVal anchor_rshift(AnchorVal a, AnchorVal b) { return (AnchorVal)((uint64_t)a >> b); }

/* ---- Unsigned arithmetic ---- */

static inline ANCHOR_PURE AnchorVal anchor_addu(AnchorVal a, AnchorVal b) { return a + b; }
static inline ANCHOR_PURE AnchorVal anchor_subu(AnchorVal a, AnchorVal b) { return a - b; }
static inline ANCHOR_PURE AnchorVal anchor_mulu(AnchorVal a, AnchorVal b) { return a * b; }
static inline ANCHOR_PURE AnchorVal anchor_divu(AnchorVal a, AnchorVal b) { return a / b; }
static inline ANCHOR_PURE AnchorVal anchor_modu(AnchorVal a, AnchorVal b) { return a % b; }
static inline ANCHOR_PURE AnchorVal anchor_ltu (AnchorVal a, AnchorVal b) { return (AnchorVal)(a <  b); }
static inline ANCHOR_PURE AnchorVal anchor_gtu (AnchorVal a, AnchorVal b) { return (AnchorVal)(a >  b); }
static inline ANCHOR_PURE AnchorVal anchor_leu (AnchorVal a, AnchorVal b) { return (AnchorVal)(a <= b); }
static inline ANCHOR_PURE AnchorVal anchor_geu (AnchorVal a, AnchorVal b) { return (AnchorVal)(a >= b); }

/* ---- Float comparisons ---- */

static inline ANCHOR_PURE AnchorVal anchor_eqf(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) == _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_nef(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) != _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_ltf(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) <  _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_gtf(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) >  _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_lef(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) <= _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_gef(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) >= _ANCH_FVAL(b)); }

/* ---- Float32 comparisons ---- */

static inline ANCHOR_PURE AnchorVal anchor_eqf32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) == _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_nef32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) != _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_ltf32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) <  _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_gtf32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) >  _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_lef32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) <= _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_gef32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) >= _ANCH_F32VAL(b)); }

/* ---- Logical ---- */

static inline ANCHOR_PURE AnchorVal anchor_not(AnchorVal a)              { return (AnchorVal)(!a); }


/* struct Cons */
#define ANCHOR_SIZEOF_Cons 16
#define ANCHOR_OFFSET_Cons_car 0
#define ANCHOR_SIZE_Cons_car  8
#define ANCHOR_OFFSET_Cons_cdr 8
#define ANCHOR_SIZE_Cons_cdr  8

#include <portmidi.h>
#include <porttime.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <io.h>
#include <stdlib.h>
AnchorVal string_gt_symbol(AnchorVal s);
AnchorVal gen_chord(AnchorVal chords, AnchorVal num_chords, AnchorVal inversions, AnchorVal num_inversions, AnchorVal result);
AnchorVal copy_notes(AnchorVal src, AnchorVal dst, AnchorVal count);
AnchorVal note_add(AnchorVal notes, AnchorVal note_count, AnchorVal note);
AnchorVal note_remove(AnchorVal notes, AnchorVal note_count, AnchorVal note);
AnchorVal print_notes(AnchorVal notes, AnchorVal count);
AnchorVal identify_chord(AnchorVal notes, AnchorVal count, AnchorVal note_names, AnchorVal result);
AnchorVal find_midi_input_device(AnchorVal select);
AnchorVal read_chord(AnchorVal midi, AnchorVal buf, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal debug);
AnchorVal print_help(void);


AnchorVal string_gt_symbol(AnchorVal s) {
    int _anc_t0_raw = strlen(((const char*)_anch_ptr(s)));
    AnchorVal _anc_t0 = anchor_int((intptr_t)_anc_t0_raw);
    AnchorVal slen = _anc_t0;
    AnchorVal r = anchor_int(0);
    if (_ANCH_IVAL(anchor_lt(slen, anchor_int(8)))) {
        memcpy(((void*)_anch_ptr(anchor_ext((void*)&r))), ((void*)_anch_ptr(s)), (size_t)_ANCH_IVAL(slen));
        return r;
    } else {
        return r;
    }
    return anchor_int(0);
}

AnchorVal gen_chord(AnchorVal chords, AnchorVal num_chords, AnchorVal inversions, AnchorVal num_inversions, AnchorVal result) {
    int _anc_t1_raw = rand();
    AnchorVal _anc_t1 = anchor_int((intptr_t)_anc_t1_raw);
    AnchorVal chord_idx = anchor_mod(_anc_t1, num_chords);
    AnchorVal _anc_t2 = 0;
    __builtin_memcpy(&_anc_t2, (char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_mul(chord_idx, anchor_int(8))), 8);
    AnchorVal chord = _anc_t2;
    int _anc_t3_raw = rand();
    AnchorVal _anc_t3 = anchor_int((intptr_t)_anc_t3_raw);
    AnchorVal inversion_idx = anchor_mod(_anc_t3, num_inversions);
    AnchorVal _anc_t4 = 0;
    __builtin_memcpy(&_anc_t4, (char*)_ANCH_HPTR(inversions) + _ANCH_IVAL(anchor_mul(inversion_idx, anchor_int(16))), 8);
    AnchorVal inversion = _anc_t4;
    sprintf(((char*)_anch_ptr(result)), "%s %s", ((char*)_anch_ptr(chord)), ((char*)_anch_ptr(inversion)));
    return result;
}

AnchorVal copy_notes(AnchorVal src, AnchorVal dst, AnchorVal count) {
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, count))) {
            AnchorVal _anc_t5 = 0;
            __builtin_memcpy(&_anc_t5, (char*)_ANCH_HPTR(src) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), 8);
            { AnchorVal _anc_t6 = _anc_t5;
              __builtin_memcpy((char*)_ANCH_HPTR(dst) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), &_anc_t6, sizeof(AnchorVal)); }
            ANCHOR_NIL;
            i = anchor_add(i, anchor_int(1));
        }
    }
    return anchor_int(0);
}

AnchorVal note_add(AnchorVal notes, AnchorVal note_count, AnchorVal note) {
    AnchorVal _anc_t7 = 0;
    __builtin_memcpy(&_anc_t7, (char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), 8);
    AnchorVal count = _anc_t7;
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, count))) {
            AnchorVal _anc_t8 = 0;
            __builtin_memcpy(&_anc_t8, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), 8);
            if (_ANCH_IVAL(anchor_eq(_anc_t8, note))) {
                return anchor_int(0);
            }
            i = anchor_add(i, anchor_int(1));
        }
    }
    { AnchorVal _anc_t9 = note;
      __builtin_memcpy((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(count, anchor_int(8))), &_anc_t9, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t10 = anchor_add(count, anchor_int(1));
      __builtin_memcpy((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t10, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    return anchor_int(0);
}

AnchorVal note_remove(AnchorVal notes, AnchorVal note_count, AnchorVal note) {
    AnchorVal _anc_t11 = 0;
    __builtin_memcpy(&_anc_t11, (char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), 8);
    AnchorVal count = _anc_t11;
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, count))) {
            AnchorVal _anc_t12 = 0;
            __builtin_memcpy(&_anc_t12, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), 8);
            if (_ANCH_IVAL(anchor_eq(_anc_t12, note))) {
                {
                    AnchorVal j = i;
                    while (_ANCH_IVAL(anchor_lt(j, anchor_sub(count, anchor_int(1))))) {
                        AnchorVal _anc_t13 = 0;
                        __builtin_memcpy(&_anc_t13, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(anchor_add(j, anchor_int(1)), anchor_int(8))), 8);
                        { AnchorVal _anc_t14 = _anc_t13;
                          __builtin_memcpy((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(j, anchor_int(8))), &_anc_t14, sizeof(AnchorVal)); }
                        ANCHOR_NIL;
                        j = anchor_add(j, anchor_int(1));
                    }
                }
                { AnchorVal _anc_t15 = anchor_sub(count, anchor_int(1));
                  __builtin_memcpy((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t15, sizeof(AnchorVal)); }
                ANCHOR_NIL;
                return anchor_int(0);
            }
            i = anchor_add(i, anchor_int(1));
        }
    }
    return anchor_int(0);
}

AnchorVal print_notes(AnchorVal notes, AnchorVal count) {
    printf("Notes (%d):", (int)_ANCH_IVAL(count));
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, count))) {
            AnchorVal _anc_t16 = 0;
            __builtin_memcpy(&_anc_t16, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), 8);
            printf(" %d", (int)_ANCH_IVAL(_anc_t16));
            i = anchor_add(i, anchor_int(1));
        }
    }
    printf("\n");
    return anchor_int(0);
}

AnchorVal identify_chord(AnchorVal notes, AnchorVal count, AnchorVal note_names, AnchorVal result) {
    if (_ANCH_IVAL(anchor_ne(count, anchor_int(3)))) {
        return anchor_int(0);
    }
    AnchorVal _anc_t17 = 0;
    __builtin_memcpy(&_anc_t17, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_int(0)), 8);
    AnchorVal p0 = anchor_mod(_anc_t17, anchor_int(12));
    AnchorVal _anc_t18 = 0;
    __builtin_memcpy(&_anc_t18, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_int(8)), 8);
    AnchorVal p1 = anchor_mod(_anc_t18, anchor_int(12));
    AnchorVal _anc_t19 = 0;
    __builtin_memcpy(&_anc_t19, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_int(16)), 8);
    AnchorVal p2 = anchor_mod(_anc_t19, anchor_int(12));
    if (_ANCH_IVAL(anchor_gt(p0, p1))) {
        AnchorVal t = p0;
        p0 = p1;
        p1 = t;
    }
    if (_ANCH_IVAL(anchor_gt(p1, p2))) {
        AnchorVal t = p1;
        p1 = p2;
        p2 = t;
    }
    if (_ANCH_IVAL(anchor_gt(p0, p1))) {
        AnchorVal t = p0;
        p0 = p1;
        p1 = t;
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(p0, p1) || !!anchor_eq(p1, p2)))) {
        return anchor_int(0);
    }
    AnchorVal i1 = anchor_sub(p1, p0);
    AnchorVal i2 = anchor_sub(p2, p1);
    AnchorVal root_pc = anchor_int(0);
    AnchorVal chord_type_str = anchor_int(0);
    AnchorVal chord_type_id = anchor_int(0);
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(4)) && !!anchor_eq(i2, anchor_int(3))))) {
        root_pc = p0;
        chord_type_str = anchor_ext((void*)"maj");
        chord_type_id = anchor_int(1);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(3)) && !!anchor_eq(i2, anchor_int(5))))) {
        root_pc = p2;
        chord_type_str = anchor_ext((void*)"maj");
        chord_type_id = anchor_int(1);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(5)) && !!anchor_eq(i2, anchor_int(4))))) {
        root_pc = p1;
        chord_type_str = anchor_ext((void*)"maj");
        chord_type_id = anchor_int(1);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(3)) && !!anchor_eq(i2, anchor_int(4))))) {
        root_pc = p0;
        chord_type_str = anchor_ext((void*)"m");
        chord_type_id = anchor_int(2);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(4)) && !!anchor_eq(i2, anchor_int(5))))) {
        root_pc = p2;
        chord_type_str = anchor_ext((void*)"m");
        chord_type_id = anchor_int(2);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(5)) && !!anchor_eq(i2, anchor_int(3))))) {
        root_pc = p1;
        chord_type_str = anchor_ext((void*)"m");
        chord_type_id = anchor_int(2);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(3)) && !!anchor_eq(i2, anchor_int(3))))) {
        root_pc = p0;
        chord_type_str = anchor_ext((void*)"dim");
        chord_type_id = anchor_int(3);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(3)) && !!anchor_eq(i2, anchor_int(6))))) {
        root_pc = p2;
        chord_type_str = anchor_ext((void*)"dim");
        chord_type_id = anchor_int(3);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(6)) && !!anchor_eq(i2, anchor_int(3))))) {
        root_pc = p1;
        chord_type_str = anchor_ext((void*)"dim");
        chord_type_id = anchor_int(3);
    }
    if (_ANCH_IVAL(anchor_eq(chord_type_id, anchor_int(0)))) {
        return anchor_int(0);
    }
    AnchorVal _anc_t20;
    if (_ANCH_IVAL(anchor_eq(chord_type_id, anchor_int(1)))) {
        _anc_t20 = anchor_int(4);
    } else {
        _anc_t20 = anchor_int(3);
    }
    AnchorVal third_interval = _anc_t20;
    AnchorVal _anc_t21;
    if (_ANCH_IVAL(anchor_eq(chord_type_id, anchor_int(3)))) {
        _anc_t21 = anchor_int(6);
    } else {
        _anc_t21 = anchor_int(7);
    }
    AnchorVal fifth_interval = _anc_t21;
    AnchorVal third_pc = anchor_mod(anchor_add(root_pc, third_interval), anchor_int(12));
    AnchorVal fifth_pc = anchor_mod(anchor_add(root_pc, fifth_interval), anchor_int(12));
    AnchorVal _anc_t22 = 0;
    __builtin_memcpy(&_anc_t22, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_int(0)), 8);
    AnchorVal lowest = _anc_t22;
    {
        AnchorVal k = anchor_int(1);
        while (_ANCH_IVAL(anchor_lt(k, count))) {
            AnchorVal _anc_t23 = 0;
            __builtin_memcpy(&_anc_t23, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(k, anchor_int(8))), 8);
            if (_ANCH_IVAL(anchor_lt(_anc_t23, lowest))) {
                AnchorVal _anc_t24 = 0;
                __builtin_memcpy(&_anc_t24, (char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(k, anchor_int(8))), 8);
                lowest = _anc_t24;
            }
            k = anchor_add(k, anchor_int(1));
        }
    }
    AnchorVal lowest_pc = anchor_mod(lowest, anchor_int(12));
    AnchorVal inversion = anchor_ext((void*)"root");
    if (_ANCH_IVAL(anchor_eq(lowest_pc, third_pc))) {
        inversion = anchor_ext((void*)"1st inversion");
    }
    if (_ANCH_IVAL(anchor_eq(lowest_pc, fifth_pc))) {
        inversion = anchor_ext((void*)"2nd inversion");
    }
    AnchorVal _anc_t25 = 0;
    __builtin_memcpy(&_anc_t25, (char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_mul(root_pc, anchor_int(8))), 8);
    AnchorVal root_name = _anc_t25;
    sprintf(((char*)_anch_ptr(result)), "%s%s %s", ((char*)_anch_ptr(root_name)), ((char*)_anch_ptr(chord_type_str)), ((char*)_anch_ptr(inversion)));
    return result;
}

AnchorVal find_midi_input_device(AnchorVal select) {
    if (_ANCH_IVAL(anchor_not(select))) {
        {
            AnchorVal i = anchor_int(0);
            int _anc_t26_raw;
            AnchorVal _anc_t26;
            _anc_t26_raw = Pm_CountDevices();
            _anc_t26 = anchor_int((intptr_t)_anc_t26_raw);
            while (_ANCH_IVAL(anchor_lt(i, _anc_t26))) {
                const void* _anc_t27_raw = Pm_GetDeviceInfo((int)_ANCH_IVAL(i));
                AnchorVal _anc_t27 = anchor_ext((void*)_anc_t27_raw);
                AnchorVal info = _anc_t27;
                AnchorVal _anc_t28 = 0;
                __builtin_memcpy(&_anc_t28, (char*)_ANCH_HPTR(info) + _ANCH_IVAL(anchor_int(28)), 4);
                AnchorVal is_output = _anc_t28;
                if (_ANCH_IVAL(anchor_eq(is_output, anchor_int(0)))) {
                    return i;
                }
                i = anchor_add(i, anchor_int(1));
                _anc_t26_raw = Pm_CountDevices();
                _anc_t26 = anchor_int((intptr_t)_anc_t26_raw);
            }
        }
        return anchor_int(-1);
    }
    {
        char _anc_arena__anc_t29_buf[ANCHOR_DEFAULT_ARENA_CAP];
        _AnchorArena _anc_arena__anc_t29 = {_anc_arena__anc_t29_buf, ANCHOR_DEFAULT_ARENA_CAP, 0, 0, _anchor_arena_top};
        _anchor_arena_top = &_anc_arena__anc_t29;
        printf("MIDI input devices:\n");
        {
            AnchorVal i = anchor_int(0);
            int _anc_t30_raw;
            AnchorVal _anc_t30;
            _anc_t30_raw = Pm_CountDevices();
            _anc_t30 = anchor_int((intptr_t)_anc_t30_raw);
            while (_ANCH_IVAL(anchor_lt(i, _anc_t30))) {
                const void* _anc_t31_raw = Pm_GetDeviceInfo((int)_ANCH_IVAL(i));
                AnchorVal _anc_t31 = anchor_ext((void*)_anc_t31_raw);
                AnchorVal info = _anc_t31;
                AnchorVal _anc_t32 = 0;
                __builtin_memcpy(&_anc_t32, (char*)_ANCH_HPTR(info) + _ANCH_IVAL(anchor_int(16)), 8);
                AnchorVal name = anchor_ext((char*)_anch_ptr(_anc_t32));
                AnchorVal _anc_t33 = 0;
                __builtin_memcpy(&_anc_t33, (char*)_ANCH_HPTR(info) + _ANCH_IVAL(anchor_int(28)), 4);
                AnchorVal is_output = _anc_t33;
                if (_ANCH_IVAL(anchor_eq(is_output, anchor_int(0)))) {
                    printf("  %d: \x1B[96m%s\x1B[0m\n", (int)_ANCH_IVAL(i), ((char*)_anch_ptr(name)));
                }
                i = anchor_add(i, anchor_int(1));
                _anc_t30_raw = Pm_CountDevices();
                _anc_t30 = anchor_int((intptr_t)_anc_t30_raw);
            }
        }
        printf("Enter device ID: ");
        AnchorVal buf = anchor_alloc(16);
        fgets(((char*)_anch_ptr(buf)), 16, ((void*)_anch_ptr(anchor_int((intptr_t)(stdin)))));
        int _anc_t34_raw = atoi(((char*)_anch_ptr(buf)));
        AnchorVal _anc_t34 = anchor_int((intptr_t)_anc_t34_raw);
        _anchor_arena_top = _anc_arena__anc_t29.prev;
        return _anc_t34;
        _anchor_arena_top = _anc_arena__anc_t29.prev;
    }
    return anchor_int(0);
}

AnchorVal read_chord(AnchorVal midi, AnchorVal buf, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal debug) {
    AnchorVal started = anchor_int(0);
    while (_ANCH_IVAL(anchor_int(1))) {
        AnchorVal _anc_t35 = 0;
        __builtin_memcpy(&_anc_t35, (char*)_ANCH_HPTR(midi) + _ANCH_IVAL(anchor_int(0)), 8);
        int _anc_t36_raw = Pm_Poll(((void*)_anch_ptr(_anc_t35)));
        AnchorVal _anc_t36 = anchor_int((intptr_t)_anc_t36_raw);
        if (_ANCH_IVAL(anchor_eq(_anc_t36, anchor_int(1)))) {
            AnchorVal _anc_t37 = 0;
            __builtin_memcpy(&_anc_t37, (char*)_ANCH_HPTR(midi) + _ANCH_IVAL(anchor_int(0)), 8);
            int _anc_t38_raw = Pm_Read(((void*)_anch_ptr(_anc_t37)), ((void*)_anch_ptr(buf)), 32);
            AnchorVal _anc_t38 = anchor_int((intptr_t)_anc_t38_raw);
            AnchorVal count = _anc_t38;
            {
                AnchorVal i = anchor_int(0);
                while (_ANCH_IVAL(anchor_lt(i, count))) {
                    AnchorVal _anc_t39 = 0;
                    __builtin_memcpy(&_anc_t39, (char*)_ANCH_HPTR(buf) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), 4);
                    AnchorVal message = _anc_t39;
                    AnchorVal status = anchor_band(message, anchor_int(255));
                    AnchorVal note = anchor_band(anchor_rshift(message, anchor_int(8)), anchor_int(255));
                    AnchorVal velocity = anchor_band(anchor_rshift(message, anchor_int(16)), anchor_int(255));
                    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(status, anchor_int(144)) && !!anchor_ne(velocity, anchor_int(0))))) {
                        note_add(active_notes, note_count, note);
                        started = anchor_int(1);
                        AnchorVal _anc_t40 = 0;
                        __builtin_memcpy(&_anc_t40, (char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), 8);
                        AnchorVal nc = _anc_t40;
                        copy_notes(active_notes, last_notes, nc);
                        { AnchorVal _anc_t41 = nc;
                          __builtin_memcpy((char*)_ANCH_HPTR(last_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t41, sizeof(AnchorVal)); }
                        ANCHOR_NIL;
                        if (_ANCH_IVAL(debug)) {
                            print_notes(active_notes, nc);
                        }
                    }
                    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(status, anchor_int(128)) || !!(AnchorVal)(!!anchor_eq(status, anchor_int(144)) && !!anchor_eq(velocity, anchor_int(0)))))) {
                        note_remove(active_notes, note_count, note);
                        if (_ANCH_IVAL(debug)) {
                            AnchorVal _anc_t42 = 0;
                            __builtin_memcpy(&_anc_t42, (char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), 8);
                            print_notes(active_notes, _anc_t42);
                        }
                        if (_ANCH_IVAL((AnchorVal)(!!started && ({ AnchorVal _anc_t43 = 0;
__builtin_memcpy(&_anc_t43, (char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), 8);
(AnchorVal)!!anchor_eq(_anc_t43, anchor_int(0)); })))) {
                            return anchor_int(0);
                        }
                    }
                    i = anchor_add(i, anchor_int(1));
                }
            }
        }
        Pt_Sleep(10);
    }
    return anchor_int(0);
}

AnchorVal print_help(void) {
    printf("Usage: chordgen [options]\n\n");
    printf("MIDI chord trainer. Displays a random chord and waits for you to play it.\n\n");
    printf("Options:\n");
    printf("  -s, --select-device   List MIDI input devices and choose one interactively\n");
    printf("  -d, --debug           Print raw MIDI note numbers on each key event\n");
    printf("  -h, --help            Show this help message\n");
    return anchor_int(0);
}

int main(int _argc_raw, char** _argv_raw) {
    AnchorVal argc = anchor_int(_argc_raw);
    AnchorVal argv = anchor_ext((void*)_argv_raw);
    {
        char _anc_arena__anc_t44_buf[ANCHOR_DEFAULT_ARENA_CAP];
        _AnchorArena _anc_arena__anc_t44 = {_anc_arena__anc_t44_buf, ANCHOR_DEFAULT_ARENA_CAP, 0, 0, _anchor_arena_top};
        _anchor_arena_top = &_anc_arena__anc_t44;
        SetConsoleOutputCP(65001);
        int _anc_t45_raw = _fileno(((void*)_anch_ptr(anchor_int((intptr_t)(stdout)))));
        AnchorVal _anc_t45 = anchor_int((intptr_t)_anc_t45_raw);
        _setmode((int)_ANCH_IVAL(_anc_t45), 32768);
        AnchorVal debug = anchor_int(0);
        AnchorVal select_device = anchor_int(0);
        AnchorVal help = anchor_int(0);
        {
            AnchorVal i = anchor_int(1);
            while (_ANCH_IVAL(anchor_lt(i, argc))) {
                AnchorVal _anc_t46 = 0;
                __builtin_memcpy(&_anc_t46, (char*)_ANCH_HPTR(argv) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), 8);
                AnchorVal arg = _anc_t46;
                int _anc_t48_raw = strcmp(((char*)_anch_ptr(arg)), "-d");
                AnchorVal _anc_t48 = anchor_int((intptr_t)_anc_t48_raw);
                if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(_anc_t48, anchor_int(0)) || ({ int _anc_t47_raw = strcmp(((char*)_anch_ptr(arg)), "--debug");
AnchorVal _anc_t47 = anchor_int((intptr_t)_anc_t47_raw);
(AnchorVal)!!anchor_eq(_anc_t47, anchor_int(0)); })))) {
                    debug = anchor_int(1);
                }
                int _anc_t50_raw = strcmp(((char*)_anch_ptr(arg)), "-s");
                AnchorVal _anc_t50 = anchor_int((intptr_t)_anc_t50_raw);
                if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(_anc_t50, anchor_int(0)) || ({ int _anc_t49_raw = strcmp(((char*)_anch_ptr(arg)), "--select-device");
AnchorVal _anc_t49 = anchor_int((intptr_t)_anc_t49_raw);
(AnchorVal)!!anchor_eq(_anc_t49, anchor_int(0)); })))) {
                    select_device = anchor_int(1);
                }
                int _anc_t52_raw = strcmp(((char*)_anch_ptr(arg)), "-h");
                AnchorVal _anc_t52 = anchor_int((intptr_t)_anc_t52_raw);
                if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(_anc_t52, anchor_int(0)) || ({ int _anc_t51_raw = strcmp(((char*)_anch_ptr(arg)), "--help");
AnchorVal _anc_t51 = anchor_int((intptr_t)_anc_t51_raw);
(AnchorVal)!!anchor_eq(_anc_t51, anchor_int(0)); })))) {
                    help = anchor_int(1);
                }
                i = anchor_add(i, anchor_int(1));
            }
        }
        if (_ANCH_IVAL(help)) {
            int _anc_t53 = (int)_ANCH_IVAL(print_help());
            _anchor_arena_top = _anc_arena__anc_t44.prev;
            return _anc_t53;
        }
        AnchorVal note_names = anchor_alloc(96);
        { AnchorVal _anc_t54 = anchor_ext((void*)"C");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(0)), &_anc_t54, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t55 = anchor_ext((void*)"C♯");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(8)), &_anc_t55, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t56 = anchor_ext((void*)"D");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(16)), &_anc_t56, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t57 = anchor_ext((void*)"E♭");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(24)), &_anc_t57, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t58 = anchor_ext((void*)"E");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(32)), &_anc_t58, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t59 = anchor_ext((void*)"F");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(40)), &_anc_t59, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t60 = anchor_ext((void*)"F♯");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(48)), &_anc_t60, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t61 = anchor_ext((void*)"G");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(56)), &_anc_t61, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t62 = anchor_ext((void*)"A♭");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(64)), &_anc_t62, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t63 = anchor_ext((void*)"A");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(72)), &_anc_t63, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t64 = anchor_ext((void*)"B♭");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(80)), &_anc_t64, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t65 = anchor_ext((void*)"B");
          __builtin_memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(88)), &_anc_t65, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal num_chords = anchor_int(18);
        AnchorVal chords = anchor_alloc(144);
        { AnchorVal _anc_t66 = anchor_ext((void*)"Cmaj");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(0)), &_anc_t66, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t67 = anchor_ext((void*)"Cm");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(8)), &_anc_t67, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t68 = anchor_ext((void*)"Dmaj");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(16)), &_anc_t68, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t69 = anchor_ext((void*)"Dm");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(24)), &_anc_t69, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t70 = anchor_ext((void*)"E♭maj");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(32)), &_anc_t70, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t71 = anchor_ext((void*)"Em");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(40)), &_anc_t71, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t72 = anchor_ext((void*)"Edim");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(48)), &_anc_t72, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t73 = anchor_ext((void*)"Fmaj");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(56)), &_anc_t73, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t74 = anchor_ext((void*)"F♯maj");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(64)), &_anc_t74, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t75 = anchor_ext((void*)"Gmaj");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(72)), &_anc_t75, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t76 = anchor_ext((void*)"Gm");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(80)), &_anc_t76, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t77 = anchor_ext((void*)"Amaj");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(88)), &_anc_t77, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t78 = anchor_ext((void*)"Am");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(96)), &_anc_t78, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t79 = anchor_ext((void*)"Adim");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(104)), &_anc_t79, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t80 = anchor_ext((void*)"B♭maj");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(112)), &_anc_t80, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t81 = anchor_ext((void*)"Bm");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(120)), &_anc_t81, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t82 = anchor_ext((void*)"Bdim");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(128)), &_anc_t82, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t83 = anchor_ext((void*)"C♯m");
          __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(136)), &_anc_t83, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal num_inversions = anchor_int(3);
        AnchorVal inversions = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(anchor_int(3), anchor_int(16))));
        { AnchorVal _anc_t84 = anchor_ext((void*)"root");
          __builtin_memcpy((char*)_ANCH_HPTR(inversions) + _ANCH_IVAL(anchor_int(0)), &_anc_t84, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t85 = anchor_ext((void*)"1st inversion");
          __builtin_memcpy((char*)_ANCH_HPTR(inversions) + _ANCH_IVAL(anchor_int(16)), &_anc_t85, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t86 = anchor_ext((void*)"2nd inversion");
          __builtin_memcpy((char*)_ANCH_HPTR(inversions) + _ANCH_IVAL(anchor_int(32)), &_anc_t86, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal input_buffer_size = anchor_int(128);
        AnchorVal midi = anchor_alloc(8);
        AnchorVal event = anchor_alloc(256);
        AnchorVal active_notes = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(anchor_int(16), anchor_int(8))));
        AnchorVal note_count = anchor_alloc(8);
        { AnchorVal _anc_t87 = anchor_int(0);
          __builtin_memcpy((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t87, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal last_notes = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(anchor_int(16), anchor_int(8))));
        AnchorVal last_count = anchor_alloc(8);
        { AnchorVal _anc_t88 = anchor_int(0);
          __builtin_memcpy((char*)_ANCH_HPTR(last_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t88, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        long _anc_t89_raw = time((void*)0);
        AnchorVal _anc_t89 = anchor_int((intptr_t)_anc_t89_raw);
        srand((unsigned int)_ANCH_IVAL(_anc_t89));
        int _anc_t90_raw = Pm_Initialize();
        AnchorVal _anc_t90 = anchor_int((intptr_t)_anc_t90_raw);
        if (_ANCH_IVAL(anchor_ne(_anc_t90, anchor_int(0)))) {
            printf("Failed to initialize PortMidi\n");
            _anchor_arena_top = _anc_arena__anc_t44.prev;
            return 1;
        }
        AnchorVal device_id = find_midi_input_device(select_device);
        if (_ANCH_IVAL(anchor_eq(device_id, anchor_int(-1)))) {
            printf("No MIDI input device found\n");
            _anchor_arena_top = _anc_arena__anc_t44.prev;
            return 1;
        }
        int _anc_t91_raw = Pm_OpenInput(((void*)_anch_ptr(midi)), (int)_ANCH_IVAL(device_id), 0, (int)_ANCH_IVAL(input_buffer_size), (void*)0, 0);
        AnchorVal _anc_t91 = anchor_int((intptr_t)_anc_t91_raw);
        if (_ANCH_IVAL(anchor_ne(_anc_t91, anchor_int(0)))) {
            printf("Failed to open MIDI input\n");
            _anchor_arena_top = _anc_arena__anc_t44.prev;
            return 1;
        }
        while (_ANCH_IVAL(anchor_int(1))) {
            {
                size_t _anc_cp__anc_t92_used = _anchor_arena_top->used;
                size_t _anc_cp__anc_t92_prev = _anchor_arena_top->checkpoint;
                _anchor_arena_top->checkpoint = _anchor_arena_top->used;
                AnchorVal result = anchor_alloc(32);
                printf("\nPlay this chord: \x1B[96m%s\x1B[0m\n\n", ((char*)_anch_ptr(gen_chord(chords, num_chords, inversions, num_inversions, result))));
                AnchorVal correct = anchor_int(0);
                while (_ANCH_IVAL(anchor_eq(correct, anchor_int(0)))) {
                    {
                        size_t _anc_cp__anc_t93_used = _anchor_arena_top->used;
                        size_t _anc_cp__anc_t93_prev = _anchor_arena_top->checkpoint;
                        _anchor_arena_top->checkpoint = _anchor_arena_top->used;
                        { AnchorVal _anc_t94 = anchor_int(0);
                          __builtin_memcpy((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t94, sizeof(AnchorVal)); }
                        ANCHOR_NIL;
                        { AnchorVal _anc_t95 = anchor_int(0);
                          __builtin_memcpy((char*)_ANCH_HPTR(last_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t95, sizeof(AnchorVal)); }
                        ANCHOR_NIL;
                        read_chord(midi, event, active_notes, note_count, last_notes, last_count, debug);
                        AnchorVal id_result = anchor_alloc(32);
                        AnchorVal _anc_t96 = 0;
                        __builtin_memcpy(&_anc_t96, (char*)_ANCH_HPTR(last_count) + _ANCH_IVAL(anchor_int(0)), 8);
                        AnchorVal identified = identify_chord(last_notes, _anc_t96, note_names, id_result);
                        if (_ANCH_IVAL((AnchorVal)(!!identified && ({ int _anc_t97_raw = strcmp(((char*)_anch_ptr(identified)), ((char*)_anch_ptr(result)));
AnchorVal _anc_t97 = anchor_int((intptr_t)_anc_t97_raw);
(AnchorVal)!!anchor_eq(_anc_t97, anchor_int(0)); })))) {
                            printf("\x1B[92mCorrect!\x1B[0m\n");
                            correct = anchor_int(1);
                        } else {
                            if (_ANCH_IVAL(identified)) {
                                printf("That's \x1B[91m%s\x1B[0m, try again\n", ((char*)_anch_ptr(identified)));
                            } else {
                                printf("\x1B[93mUnknown chord\x1B[0m, try again\n");
                            }
                        }
                        _anchor_arena_top->used = _anc_cp__anc_t93_used;
                        _anchor_arena_top->checkpoint = _anc_cp__anc_t93_prev;
                    }
                }
                _anchor_arena_top->used = _anc_cp__anc_t92_used;
                _anchor_arena_top->checkpoint = _anc_cp__anc_t92_prev;
            }
        }
        _anchor_arena_top = _anc_arena__anc_t44.prev;
    }
    return 0;
}